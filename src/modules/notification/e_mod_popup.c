#include "e_mod_main.h"

/* Popup function protos */
static Popup_Data *_notification_popup_new(E_Notification_Notify *n, unsigned id);
static Popup_Data *_notification_popup_find(unsigned int id);

static int         _notification_popup_place(Popup_Data *popup,
                                             int         num);
static void        _notification_popup_refresh(Popup_Data *popup);
static void        _notification_popup_del(unsigned int                 id,
                                           E_Notification_Notify_Closed_Reason reason);
static void        _notification_popdown(Popup_Data                  *popup,
                                         E_Notification_Notify_Closed_Reason reason);

#define POPUP_LIMIT 7
static int popups_displayed = 0;

/* Util function protos */
static void _notification_format_message(Popup_Data *popup);

static int next_pos = 0;

static Eina_Bool
_notification_timer_cb(Popup_Data *popup)
{
   _notification_popup_del(popup->id, E_NOTIFICATION_NOTIFY_CLOSED_REASON_EXPIRED);
   return EINA_FALSE;
}

int
notification_popup_notify(E_Notification_Notify *n,
                          unsigned int id)
{
   Popup_Data *popup = NULL;

   switch (n->urgency)
     {
      case E_NOTIFICATION_NOTIFY_URGENCY_LOW:
        if (!notification_cfg->show_low) return 0;
        break;
      case E_NOTIFICATION_NOTIFY_URGENCY_NORMAL:
        if (!notification_cfg->show_normal) return 0;
        break;
      case E_NOTIFICATION_NOTIFY_URGENCY_CRITICAL:
        if (!notification_cfg->show_critical) return 0;
        break;
      default:
        break;
     }
   if (notification_cfg->ignore_replacement)
     n->replaces_id = 0;

   if (n->replaces_id && (popup = _notification_popup_find(n->replaces_id)))
     {
        if (popup->notif)
          e_notification_notify_free(popup->notif);

        popup->notif = n;
        popup->id = id;
        _notification_popup_refresh(popup);
     }

   if (!popup)
     {
        popup = _notification_popup_new(n, id);
        if (!popup)
          {
             e_notification_notify_free(n);
             ERR("Error creating popup");
             return 0;
          }
        notification_cfg->popups = eina_list_append(notification_cfg->popups, popup);
        edje_object_signal_emit(popup->theme, "notification,new", "notification");
     }

   if (popup->timer)
     {
        ecore_timer_del(popup->timer);
        popup->timer = NULL;
     }

   if (n->timeout < 0 || notification_cfg->force_timeout)
      n->timeout = notification_cfg->timeout;
   else n->timeout = n->timeout / 1000.0;


   if (n->timeout > 0)
     popup->timer = ecore_timer_add(n->timeout, (Ecore_Task_Cb)_notification_timer_cb, popup);

   return 1;
}

void
notification_popup_shutdown(void)
{
   Popup_Data *popup;

   EINA_LIST_FREE(notification_cfg->popups, popup)
     _notification_popdown(popup, E_NOTIFICATION_NOTIFY_CLOSED_REASON_REQUESTED);
}

void
notification_popup_close(unsigned int id)
{
   _notification_popup_del(id, E_NOTIFICATION_NOTIFY_CLOSED_REASON_REQUESTED);
}

static void
_notification_theme_cb_deleted(Popup_Data *popup,
                               Evas_Object *obj __UNUSED__,
                               const char  *emission __UNUSED__,
                               const char  *source __UNUSED__)
{
   _notification_popup_refresh(popup);
   edje_object_signal_emit(popup->theme, "notification,new", "notification");
}

static void
_notification_theme_cb_close(Popup_Data *popup,
                             Evas_Object *obj __UNUSED__,
                             const char  *emission __UNUSED__,
                             const char  *source __UNUSED__)
{
   _notification_popup_del(popup->id, E_NOTIFICATION_NOTIFY_CLOSED_REASON_DISMISSED);
}

static void
_notification_theme_cb_find(Popup_Data *popup,
                            Evas_Object *obj __UNUSED__,
                            const char  *emission __UNUSED__,
                            const char  *source __UNUSED__)
{
   Eina_List *l;
   E_Border *bd;

   if (!popup->app_name) return;

   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
     {
        size_t len, test;

        len = strlen(popup->app_name);
        test = eina_strlen_bounded(bd->client.icccm.name, len + 1);

        /* We can't be sure that the app_name really match the application name.
         * Some plugin put their name instead. But this search gives some good
         * results.
         */
        if (strncasecmp(bd->client.icccm.name, popup->app_name, (test < len) ? test : len))
          continue;

        e_desk_show(bd->desk);
        e_border_show(bd);
        e_border_raise(bd);
        e_border_focus_set_with_pointer(bd);
        break;
     }
}

static Popup_Data *
_notification_popup_new(E_Notification_Notify *n, unsigned id)
{
   E_Container *con;
   Popup_Data *popup;
   char buf[PATH_MAX];
   const Eina_List *l, *screens;
   E_Screen *scr;
   E_Zone *zone = NULL;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(popups_displayed > POPUP_LIMIT, NULL);
   popup = E_NEW(Popup_Data, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(popup, NULL);
   popup->notif = n;
   popup->id = id;

   con = e_container_current_get(e_manager_current_get());
   screens = e_xinerama_screens_get();
   if (notification_cfg->dual_screen &&
       ((notification_cfg->corner == CORNER_BR) ||
       (notification_cfg->corner == CORNER_TR)))
     l = eina_list_last(screens);
   else
     l = screens;
   if (l)
     {
        scr = eina_list_data_get(l);
        EINA_SAFETY_ON_NULL_GOTO(scr, error);
        EINA_LIST_FOREACH(con->zones, l, zone)
          if ((int)zone->num == scr->screen) break;
        if (zone && ((int)zone->num != scr->screen)) goto error;
     }
   if (!zone)
     zone = e_zone_current_get(con);
   popup->zone = zone;

   /* Create the popup window */
   popup->win = e_popup_new(zone, 0, 0, 0, 0);
   e_popup_name_set(popup->win, "_e_popup_notification");
   popup->e = popup->win->evas;

   /* Setup the theme */
   snprintf(buf, sizeof(buf), "%s/e-module-notification.edj",
            notification_mod->dir);
   popup->theme = edje_object_add(popup->e);

   if (!e_theme_edje_object_set(popup->theme,
                                "base/theme/modules/notification",
                                "e/modules/notification/main"))
     if (!e_theme_edje_object_set(popup->theme,
                                  "base/theme/modules/notification",
                                  "modules/notification/main"))
       edje_object_file_set(popup->theme, buf, "modules/notification/main");

   e_popup_content_set(popup->win, popup->theme);

   evas_object_show(popup->theme);
   edje_object_signal_callback_add
     (popup->theme, "notification,deleted", "theme",
     (Edje_Signal_Cb)_notification_theme_cb_deleted, popup);
   edje_object_signal_callback_add
     (popup->theme, "notification,close", "theme",
     (Edje_Signal_Cb)_notification_theme_cb_close, popup);
   edje_object_signal_callback_add
     (popup->theme, "notification,find", "theme",
     (Edje_Signal_Cb)_notification_theme_cb_find, popup);

   _notification_popup_refresh(popup);
   next_pos = _notification_popup_place(popup, next_pos);
   e_popup_show(popup->win);
   popups_displayed++;

   return popup;
error:
   free(popup);
   return NULL;
}

static int
_notification_popup_place(Popup_Data *popup,
                          int         pos)
{
   int w, h, sw, sh;
   int gap = 10;
   int to_edge = 15;

   sw = popup->zone->w;
   sh = popup->zone->h;
   evas_object_geometry_get(popup->theme, NULL, NULL, &w, &h);

   /* XXX for now ignore placement requests */

   switch (notification_cfg->corner)
     {
      case CORNER_TL:
        e_popup_move(popup->win,
                     to_edge, to_edge + pos);
        break;
      case CORNER_TR:
        e_popup_move(popup->win,
                     sw - (w + to_edge),
                     to_edge + pos);
        break;
      case CORNER_BL:
        e_popup_move(popup->win,
                     to_edge,
                     (sh - h) - (to_edge + pos));
        break;
      case CORNER_BR:
        e_popup_move(popup->win,
                     sw - (w + to_edge),
                     (sh - h) - (to_edge + pos));
        break;
      default:
        break;
     }
   return pos + h + gap;
}

static void
_notification_popups_place(void)
{
   Popup_Data *popup;
   Eina_List *l;
   int pos = 0;

   EINA_LIST_FOREACH(notification_cfg->popups, l, popup)
     pos = _notification_popup_place(popup, pos);

   next_pos = pos;
}

static void
_notification_popup_refresh(Popup_Data *popup)
{
   const char *icon_path;
   const char *app_icon_max;
   int w, h, width = 80, height = 80;

   if (!popup) return;

   popup->app_name = popup->notif->app_name;

   if (popup->app_icon)
     {
        evas_object_del(popup->app_icon);
        popup->app_icon = NULL;
     }

   app_icon_max = edje_object_data_get(popup->theme, "app_icon_max");
   if (app_icon_max)
     {
        char *endptr;

        errno = 0;
        width = strtol(app_icon_max, &endptr, 10);
        if (errno || (width < 1) || (endptr == app_icon_max))
          {
             width = 80;
             height = 80;
          }
        else
          {
             endptr++;
             if (endptr)
               {
                  height = strtol(endptr, NULL, 10);
                  if (errno || (height < 1)) height = 80;
               }
             else height = 80;
          }
     }

   /* Check if the app specify an icon either by a path or by a hint */
   if (!popup->notif->icon.raw.data)
     {
        icon_path = popup->notif->icon.icon_path;
        if ((!icon_path) || (!icon_path[0]))
          icon_path = popup->notif->icon.icon;
        if (icon_path)
          {
             if (!strncmp(icon_path, "file://", 7)) icon_path += 7;
             if (!ecore_file_exists(icon_path))
               {
                  const char *new_path;
                  unsigned int size;

                  size = e_util_icon_size_normalize(width * e_scale);
                  new_path = efreet_icon_path_find(e_config->icon_theme, 
                                                   icon_path, size);
                  if (new_path)
                    icon_path = new_path;
                  else
                    {
                       Evas_Object *o = e_icon_add(popup->e);
                       if (!e_util_icon_theme_set(o, icon_path))
                         evas_object_del(o);
                       else
                         {
                            popup->app_icon = o;
                            w = width;
                            h = height;
                         }
                    }
               }

             if (!popup->app_icon)
               {
                  popup->app_icon = e_icon_add(popup->e);
                  if (!e_icon_file_set(popup->app_icon, icon_path))
                    {
                       evas_object_del(popup->app_icon);
                       popup->app_icon = NULL;
                    }
                  else e_icon_size_get(popup->app_icon, &w, &h);
               }
          }
     }
   else
     {
        popup->app_icon = e_notification_notify_raw_image_get(popup->notif,
                                                              popup->e);
        evas_object_image_filled_set(popup->app_icon, EINA_TRUE);
        evas_object_image_alpha_set(popup->app_icon, EINA_TRUE);
        evas_object_image_size_get(popup->app_icon, &w, &h);
     }

   if (!popup->app_icon)
     {
        char buf[PATH_MAX];

        snprintf(buf, sizeof(buf), "%s/e-module-notification.edj", 
                 notification_mod->dir);
        popup->app_icon = edje_object_add(popup->e);
        if (!e_theme_edje_object_set(popup->app_icon, 
                                     "base/theme/modules/notification",
                                     "e/modules/notification/logo"))
          if (!e_theme_edje_object_set(popup->app_icon, 
                                       "base/theme/modules/notification",
                                       "modules/notification/logo"))
            edje_object_file_set(popup->app_icon, buf, 
                                 "modules/notification/logo");
        w = width;
        h = height;
     }

   if ((w > width) || (h > height))
     {
        int v;
        v = w > h ? w : h;
        h = h * height / v;
        w = w * width / v;
     }
   edje_extern_object_min_size_set(popup->app_icon, w, h);
   edje_extern_object_max_size_set(popup->app_icon, w, h);

   edje_object_calc_force(popup->theme);
   edje_object_part_swallow(popup->theme, "notification.swallow.app_icon", 
                            popup->app_icon);
   edje_object_signal_emit(popup->theme, "notification,icon", "notification");

   /* Fill up the event message */
   _notification_format_message(popup);

   /* Compute the new size of the popup */
   edje_object_calc_force(popup->theme);
   edje_object_size_min_calc(popup->theme, &w, &h);
   w = MIN(w, popup->zone->w / 2);
   h = MIN(h, popup->zone->h / 2);
   e_popup_resize(popup->win, w, h);
   evas_object_resize(popup->theme, w, h);

   _notification_popups_place();
}

static Popup_Data *
_notification_popup_find(unsigned int id)
{
   Eina_List *l;
   Popup_Data *popup;

   if (!id) return NULL;
   EINA_LIST_FOREACH(notification_cfg->popups, l, popup)
     if (popup->id == id)
       return popup;
   return NULL;
}

static void
_notification_popup_del(unsigned int                 id,
                        E_Notification_Notify_Closed_Reason reason)
{
   Popup_Data *popup;
   Eina_List *l, *l2;
   int pos = 0;

   EINA_LIST_FOREACH_SAFE(notification_cfg->popups, l, l2, popup)
     {
        if (popup->id == id)
          {
             _notification_popdown(popup, reason);
             notification_cfg->popups = eina_list_remove_list(notification_cfg->popups, l);
          }
        else
          pos = _notification_popup_place(popup, pos);
     }

   next_pos = pos;
}

static void
_notification_popdown(Popup_Data                  *popup,
                      E_Notification_Notify_Closed_Reason reason)
{
   if (popup->timer) ecore_timer_del(popup->timer);
   e_popup_hide(popup->win);
   popups_displayed--;
   evas_object_del(popup->app_icon);
   evas_object_del(popup->theme);
   e_object_del(E_OBJECT(popup->win));
   e_notification_notify_close(popup->notif, reason);
   e_notification_notify_free(popup->notif);
   free(popup);
}

static void
_notification_format_message(Popup_Data *popup)
{
   Evas_Object *o = popup->theme;
   Eina_Strbuf *buf = eina_strbuf_new();
   edje_object_part_text_set(o, "notification.text.title",
                             popup->notif->sumary);
   /* FIXME: Filter to only include allowed markup? */
   /* We need to replace \n with <br>. FIXME: We need to handle all the
   * newline kinds, and paragraph separator. ATM this will suffice. */
   eina_strbuf_append(buf, popup->notif->body);
   eina_strbuf_replace_all(buf, "\n", "<br/>");
   edje_object_part_text_set(o, "notification.textblock.message",
                             eina_strbuf_string_get(buf));
   eina_strbuf_free(buf);
}
