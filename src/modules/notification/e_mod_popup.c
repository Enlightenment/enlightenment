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


#define POPUP_GAP 10
#define POPUP_TO_EDGE 15
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

static Popup_Data *
_notification_popup_merge(E_Notification_Notify *n)
{
   Eina_List *l;
   Popup_Data *popup;
   char *body_final;
   size_t len;

   if (!n->app_name) return NULL;

   EINA_LIST_FOREACH(notification_cfg->popups, l, popup)
     {
        if (!popup->notif) continue;
        if (popup->notif->app_name == n->app_name) break;
     }

   if (!popup)
     {
        /* printf("- no poup to merge\n"); */
        return NULL;
     }

   if (n->summary && (n->summary != popup->notif->summary))
     {
        /* printf("- summary doesn match, %s, %s\n", str1, str2); */
        return NULL;
     }

   /* TODO  p->n is not fallback alert..*/
   /* TODO  both allow merging */

   len = strlen(popup->notif->body);
   len += strlen(n->body);
   len += 5; /* \xE2\x80\xA9 or <PS/> */
   body_final = malloc(len + 1);
   if (body_final)
     {
        /* Hack to allow e to include markup */
        snprintf(body_final, len + 1, "%s<ps/>%s", popup->notif->body, n->body);

        /* printf("set body %s\n", body_final); */

        eina_stringshare_replace(&n->body, body_final);

        e_object_del(E_OBJECT(popup->notif));
        popup->notif = n;
        free(body_final);
     }

   return popup;
}

static void
_notification_reshuffle_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Popup_Data *popup;
   Eina_List *l, *l2;
   int pos = 0;

   EINA_LIST_FOREACH_SAFE(notification_cfg->popups, l, l2, popup)
     {
        if (popup->theme == obj)
          {
             popup->pending = 0;
             _notification_popdown(popup, 0);
             notification_cfg->popups = eina_list_remove_list(notification_cfg->popups, l);
          }
        else
          pos = _notification_popup_place(popup, pos);
     }
   next_pos = pos;
}

void
notification_popup_notify(E_Notification_Notify *n,
                          unsigned int id)
{
   Popup_Data *popup = NULL;

   switch (n->urgency)
     {
      case E_NOTIFICATION_NOTIFY_URGENCY_LOW:
        if (!notification_cfg->show_low) return;
        break;
      case E_NOTIFICATION_NOTIFY_URGENCY_NORMAL:
        if (!notification_cfg->show_normal) return;
        break;
      case E_NOTIFICATION_NOTIFY_URGENCY_CRITICAL:
        if (!notification_cfg->show_critical) return;
        break;
      default:
        break;
     }
   if (notification_cfg->ignore_replacement)
     n->replaces_id = 0;

   if (n->replaces_id && (popup = _notification_popup_find(n->replaces_id)))
     {
        if (popup->notif)
          e_object_del(E_OBJECT(popup->notif));

        popup->notif = n;
        popup->id = id;
        _notification_popup_refresh(popup);
        _notification_reshuffle_cb(NULL, NULL, NULL, NULL);
     }
   else if (!n->replaces_id)
     {
        if ((popup = _notification_popup_merge(n)))
          {
             _notification_popup_refresh(popup);
             _notification_reshuffle_cb(NULL, NULL, NULL, NULL);
          }
     }

   if (!popup)
     {
        popup = _notification_popup_new(n, id);
        if (!popup)
          {
             e_object_del(E_OBJECT(n));
             ERR("Error creating popup");
             return;
          }
        notification_cfg->popups = eina_list_append(notification_cfg->popups, popup);
        edje_object_signal_emit(popup->theme, "notification,new", "notification");
     }

   E_FREE_FUNC(popup->timer, ecore_timer_del);

   if (n->timeout < 0 || notification_cfg->force_timeout)
      n->timeout = notification_cfg->timeout * 1000.0;


   if (n->timeout > 0)
     popup->timer = ecore_timer_loop_add((double)n->timeout / 1000.0,
                                         (Ecore_Task_Cb)_notification_timer_cb,
                                         popup);
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
                               Evas_Object *obj EINA_UNUSED,
                               const char  *emission EINA_UNUSED,
                               const char  *source EINA_UNUSED)
{
   _notification_popup_refresh(popup);
   edje_object_signal_emit(popup->theme, "notification,new", "notification");
}

static void
_notification_theme_cb_close(Popup_Data *popup,
                             Evas_Object *obj EINA_UNUSED,
                             const char  *emission EINA_UNUSED,
                             const char  *source EINA_UNUSED)
{
   if (popup->pending) return;
   _notification_popup_del(popup->id, E_NOTIFICATION_NOTIFY_CLOSED_REASON_DISMISSED);
}

static void
_notification_theme_cb_find(Popup_Data *popup,
                            Evas_Object *obj EINA_UNUSED,
                            const char  *emission EINA_UNUSED,
                            const char  *source EINA_UNUSED)
{
   const Eina_List *l;
   E_Client *ec;

   if (!popup->app_name) return;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        size_t len, test;
        const char *name;

        if (e_client_util_ignored_get(ec)) continue;
        len = strlen(popup->app_name);
        name = e_client_util_name_get(ec);
        if (!name) continue;
        test = eina_strlen_bounded(name, len + 1);

        /* We can't be sure that the app_name really match the application name.
         * Some plugin put their name instead. But this search gives some good
         * results.
         */
        if (strncasecmp(name, popup->app_name, (test < len) ? test : len))
          continue;

        e_desk_show(ec->desk);
        evas_object_show(ec->frame);
        evas_object_raise(ec->frame);
        e_client_focus_set_with_pointer(ec);
        break;
     }
}

static void
_notification_theme_cb_anchor(Popup_Data *popup EINA_UNUSED,
                              Evas_Object *obj EINA_UNUSED,
                              const char  *emission,
                              const char  *source EINA_UNUSED)
{
   if (!strncmp(emission, "anchor,mouse,clicked,1,",
                strlen("anchor,mouse,clicked,1,")))
     {
        const char *href = emission + strlen("anchor,mouse,clicked,1,");
        Eina_Strbuf *buf = eina_strbuf_new();

        if (buf)
          {
             const char *s;

             eina_strbuf_append(buf, href);
             s = eina_strbuf_string_get(buf);
             if ((s) && (*s == '"'))
               {
                  eina_strbuf_remove(buf, 0, 1);
                  s = eina_strbuf_string_get(buf);
                  if ((s) && (strlen(s) > 0) && (s[strlen(s) - 1] == '"'))
                    eina_strbuf_replace_last(buf, "\"", "");
               }
             if ((s) && (*s == '\''))
               {
                  eina_strbuf_remove(buf, 0, 1);
                  s = eina_strbuf_string_get(buf);
                  if ((s) && (strlen(s) > 0) && (s[strlen(s) - 1] == '\''))
                    eina_strbuf_replace_last(buf, "'", "");
               }
             printf("NOT: clicked=[%s]\n", eina_strbuf_string_get(buf));
             e_util_open(eina_strbuf_string_get(buf), NULL);
             eina_strbuf_free(buf);
          }
     }
}

static void
_notification_theme_cb_action(Popup_Data *popup,
                              Evas_Object *obj,
                              const char  *emission EINA_UNUSED,
                              const char  *source EINA_UNUSED)
{
   const char *action = evas_object_data_get(obj, "action");

   if (action)
     {
        printf("NOT: action=[%s]\n", action);
        e_notification_notify_action(popup->notif, action);
     }
}


static void
_notification_popup_place_coords_get(int zw, int zh, int ow, int oh, int pos, int *x, int *y)
{
   /* XXX for now ignore placement requests */

   switch (notification_cfg->corner)
     {
      case CORNER_TL:
        *x = 15, *y = 15 + pos;
        break;
      case CORNER_TR:
        *x = zw - (ow + 15), *y = 15 + pos;
        break;
      case CORNER_BL:
        *x = 15, *y = (zh - oh) - (15 + pos);
        break;
      case CORNER_BR:
        *x = zw - (ow + 15), *y = (zh - oh) - (15 + pos);
        break;
     }
}

static void
_notification_popup_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Popup_Data *popup = data;

   popup->win = NULL;
}

static Evas_Object *
_cb_item_provider(void *data, Evas_Object *obj EINA_UNUSED, const char *part, const char *item)
{
   Popup_Data *popup = data;

   printf("NOT: PROVIDER.... [%s] item: [%s]\n", part, item);
//   if (!strcmp(part, "notification.textblock.message"))
     {
        Evas_Object *o = e_icon_add(popup->e);

        e_icon_file_set(o, item);
        return o;
     }
   return NULL;
}

static Popup_Data *
_notification_popup_new(E_Notification_Notify *n, unsigned id)
{
   Popup_Data *popup;
   Eina_List *l;
   int pos = next_pos;
   E_Zone *zone = NULL;

   switch (notification_cfg->dual_screen)
     {
      case POPUP_DISPLAY_POLICY_FIRST:
        zone = eina_list_data_get(e_comp->zones);
        break;
      case POPUP_DISPLAY_POLICY_CURRENT:
      case POPUP_DISPLAY_POLICY_ALL:
        zone = e_zone_current_get();
        break;
      case POPUP_DISPLAY_POLICY_MULTI:
        if ((notification_cfg->corner == CORNER_BR) ||
            (notification_cfg->corner == CORNER_TR))
          zone = eina_list_last_data_get(e_comp->zones);
        else
          zone = eina_list_data_get(e_comp->zones);
        break;
     }

   /* prevent popups if they would go offscreen
    * FIXME: this can be improved...
    */
   if (next_pos + 30 >= zone->h) return NULL;
   popup = E_NEW(Popup_Data, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(popup, NULL);
   popup->notif = n;
   popup->id = id;
   popup->e = e_comp->evas;

   /* Setup the theme */
   popup->theme = edje_object_add(popup->e);
   edje_object_item_provider_set(popup->theme, _cb_item_provider, popup);
   e_theme_edje_object_set(popup->theme,
                           "base/theme/modules/notification",
                           "e/modules/notification/main");

   /* Create the popup window */
   popup->win = e_comp_object_util_add(popup->theme, E_COMP_OBJECT_TYPE_POPUP);
   edje_object_signal_emit(popup->win, "e,state,shadow,off", "e");
   evas_object_layer_set(popup->win, E_LAYER_POPUP);
   evas_object_event_callback_add(popup->win, EVAS_CALLBACK_DEL, _notification_popup_del_cb, popup);

   edje_object_signal_callback_add
     (popup->theme, "notification,deleted", "*",
     (Edje_Signal_Cb)_notification_theme_cb_deleted, popup);
   edje_object_signal_callback_add
     (popup->theme, "notification,close", "*",
     (Edje_Signal_Cb)_notification_theme_cb_close, popup);
   edje_object_signal_callback_add
     (popup->theme, "notification,find", "*",
     (Edje_Signal_Cb)_notification_theme_cb_find, popup);
   edje_object_signal_callback_add
     (popup->theme, "anchor,mouse,clicked,1,*", "notification.textblock.message",
     (Edje_Signal_Cb)_notification_theme_cb_anchor, popup);

   _notification_popup_refresh(popup);
   next_pos = _notification_popup_place(popup, next_pos);
   evas_object_show(popup->win);
   if (notification_cfg->dual_screen == POPUP_DISPLAY_POLICY_ALL)
     {
        EINA_LIST_FOREACH(e_comp->zones, l, zone)
          {
             Evas_Object *o;
             int x = 0, y = 0, w = 0, h = 0;

             if (zone == e_comp_object_util_zone_get(popup->win)) continue;
             o = e_comp_object_util_mirror_add(popup->theme);
             o = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_POPUP);
             edje_object_signal_emit(o, "e,state,shadow,off", "e");
             evas_object_name_set(o, "notification_mirror");
             evas_object_data_set(o, "zone", zone);
             evas_object_geometry_get(popup->win, NULL, NULL, &w, &h);
             evas_object_resize(o, w, h);
             evas_object_layer_set(o, E_LAYER_POPUP);
             _notification_popup_place_coords_get(zone->w, zone->h, w, h, pos, &x, &y);
             evas_object_move(o, zone->x + x, zone->y + y);
             evas_object_show(o);
             popup->mirrors = eina_list_append(popup->mirrors, o);
          }
     }
   popups_displayed++;

   return popup;
}

static int
_notification_popup_place(Popup_Data *popup, int pos)
{
   int x = 0, y = 0, w = 0, h = 0;
   Eina_List *l;
   Evas_Object *o;
   E_Zone *zone;

   if (!popup->win) return pos;
   evas_object_geometry_get(popup->win, NULL, NULL, &w, &h);
   zone = e_comp_object_util_zone_get(popup->win);
   if (!zone) return pos;
   _notification_popup_place_coords_get(zone->w, zone->h, w, h, pos, &x, &y);
   evas_object_move(popup->win, x, y);
   EINA_LIST_FOREACH(popup->mirrors, l, o)
     {
        zone = evas_object_data_get(o, "zone");
        _notification_popup_place_coords_get(zone->w, zone->h, w, h, pos, &x, &y);
        evas_object_move(o, zone->x + x, zone->y + y);
     }
   return pos + h + 10;
}

static void
_notification_popup_refresh(Popup_Data *popup)
{
   const char *app_icon_max;
   int w, h, width = 80, height = 80;
   E_Zone *zone;
   Evas_Object *o;

   if (!popup) return;

   popup->app_name = popup->notif->app_name;

   EINA_LIST_FREE(popup->actions, o)
     {
        evas_object_del(o);
     }
   if (popup->action_box)
     {
        e_comp_object_util_del_list_remove(popup->win, popup->action_box);
        E_FREE_FUNC(popup->action_box, evas_object_del);
        edje_object_signal_emit(popup->theme, "e,state,actions,hide", "e");
     }

   if (popup->app_icon)
     {
        e_comp_object_util_del_list_remove(popup->win, popup->app_icon);
        E_FREE_FUNC(popup->app_icon, evas_object_del);
     }

   if (popup->desktop_icon)
     {
        e_comp_object_util_del_list_remove(popup->win, popup->desktop_icon);
        E_FREE_FUNC(popup->desktop_icon, evas_object_del);
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
             if (!endptr[0])
               height = width;
             else
               {
                  endptr++;
                  if (endptr[0])
                    {
                       height = strtol(endptr, NULL, 10);
                       if (errno || (height < 1)) height = width;
                    }
                  else height = width;
               }
          }
     }

   /* Check if the app specify an icon either by a path or by a hint */
   if (!popup->notif->icon.raw.data)
     {
        const char *icon_path;

        icon_path = popup->notif->icon.icon_path;
        if ((!icon_path) || (!icon_path[0]))
          icon_path = popup->notif->icon.icon;
        if (icon_path && icon_path[0])
          {
             Efreet_Uri *uri = NULL;

             if (icon_path[0] == '/')
               uri = efreet_uri_decode(icon_path);
             if ((!uri) || strcmp(uri->protocol, "file") || (uri->path[0] != '/'))
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
                       o = e_icon_add(popup->e);
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
                  e_icon_file_set(popup->app_icon, uri ? uri->path : icon_path);
                  // XXX: FIXME: this disallows for async to work
                  // e_icon_size_get(popup->app_icon, &w, &h);
                  w = width;
                  h = height;
               }
             efreet_uri_free(uri);
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
        popup->app_icon = edje_object_add(popup->e);
        e_theme_edje_object_set(popup->app_icon,
                                "base/theme/modules/notification",
                                "e/modules/notification/logo");
        w = width;
        h = height;
     }

   e_comp_object_util_del_list_append(popup->win, popup->app_icon);
   if ((w > width) || (h > height))
     {
        int v;
        v = w > h ? w : h;
        h = h * height / v;
        w = w * width / v;
     }
   evas_object_size_hint_min_set(popup->app_icon, w, h);
   evas_object_size_hint_max_set(popup->app_icon, w, h);

   edje_object_part_swallow(popup->theme, "notification.swallow.app_icon",
                            popup->app_icon);
   edje_object_signal_emit(popup->theme, "notification,icon", "notification");

   if ((popup->notif->desktop_entry) &&
       (edje_object_part_exists(popup->theme, "notification.swallow.desktop_icon")))
     {
        Efreet_Desktop *desktop;
        unsigned int size;
        const char *icon_path;
        char buf[1024];

        snprintf(buf, sizeof(buf), "%s.desktop", popup->notif->desktop_entry);
        desktop = efreet_util_desktop_file_id_find(buf);
        if (!desktop)
          { // some apps name their desktops with capitals - err... Firefox
             char *buf2 = strdup(buf);

             if (buf2)
               {
                  eina_str_tolower(&buf2);
                  desktop = efreet_util_desktop_file_id_find(buf2);
                  free(buf2);
               }
          }
        if ((desktop) && (desktop->icon))
          {
             size = e_util_icon_size_normalize(width * e_scale);
             icon_path = efreet_icon_path_find(e_config->icon_theme,
                                               desktop->icon, size);
             efreet_desktop_free(desktop);

             o = e_icon_add(popup->e);
             if (!e_util_icon_theme_set(o, icon_path))
               evas_object_del(o);
             else
               {
                  popup->desktop_icon = o;
                  edje_object_part_swallow(popup->theme,
                                           "notification.swallow.desktop_icon",
                                           popup->desktop_icon);
                  evas_object_show(o);
                  e_comp_object_util_del_list_append(popup->win, o);
               }
          }
     }

   if (popup->notif->category)
     {
        char buf[1024];

        snprintf(buf, sizeof(buf), "e,category,%s", popup->notif->category);
        edje_object_signal_emit(popup->theme, buf, "e");
     }
   if (popup->notif->urgency == E_NOTIFICATION_NOTIFY_URGENCY_LOW)
     edje_object_signal_emit(popup->theme, "e,urgency,low", "e");
   else if (popup->notif->urgency == E_NOTIFICATION_NOTIFY_URGENCY_NORMAL)
     edje_object_signal_emit(popup->theme, "e,urgency,normal", "e");
   else if (popup->notif->urgency == E_NOTIFICATION_NOTIFY_URGENCY_CRITICAL)
     edje_object_signal_emit(popup->theme, "e,urgency,critical", "e");

   /* Fill up the event message */
   _notification_format_message(popup);

   if (popup->notif->actions)
     {
        int i;

        o = popup->action_box = elm_box_add(e_comp->elm);
        elm_box_homogeneous_set(o, EINA_TRUE);
        elm_box_horizontal_set(o, EINA_TRUE);
        e_comp_object_util_del_list_append(popup->win, o);
        for (i = 0; popup->notif->actions[i].action; i++)
          {
             o = edje_object_add(popup->e);
             e_theme_edje_object_set(o,
                                     "base/theme/modules/notification",
                                     "e/modules/notification/action");
             evas_object_data_set(o, "action", popup->notif->actions[i].action);
             edje_object_part_text_unescaped_set(o, "e.text.label",
                                                 popup->notif->actions[i].label);
             edje_object_signal_callback_add
               (o, "e,action,clicked", "e",
                (Edje_Signal_Cb)_notification_theme_cb_action, popup);
             edje_object_size_min_calc(o, &w, &h);
             evas_object_size_hint_min_set(o, w, h);
             printf("NOT: act %ix%i\n", w, h);
             elm_box_pack_end(popup->action_box, o);
             evas_object_show(o);
          }
        evas_smart_objects_calculate(popup->e);
        edje_message_signal_process();
        evas_smart_objects_calculate(popup->e);
        evas_object_size_hint_min_get(popup->action_box, &w, &h);
        printf("NOT: actbox %ix%i\n", w, h);
        edje_object_part_swallow(popup->theme, "notification.swallow.actions", popup->action_box);
        edje_object_signal_emit(popup->theme, "e,state,actions,show", "e");
     }

   /* Compute the new size of the popup */
   edje_object_calc_force(popup->theme);
   edje_object_size_min_calc(popup->theme, &w, &h);
   printf("NOT: min %ix%i\n", w, h);
   if ((zone = e_comp_object_util_zone_get(popup->win)))
     {
        w = MIN(w, zone->w / 2);
        h = MIN(h, zone->h / 2);
     }
   evas_object_resize(popup->win, w, h);

   if (popup->notif->sound_file)
     {
        e_sound_file_play(popup->notif->sound_file, 1.0);
     }
// we don't do sound themes/schemes .. but we know about it...
//   else if (popup->notif->sound_name)
//     {
//     }
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
   Eina_List *l;

   EINA_LIST_FOREACH(notification_cfg->popups, l, popup)
     {
        if (popup->id == id)
          {
             popup->pending = 1;
             evas_object_event_callback_add(popup->theme, EVAS_CALLBACK_DEL, _notification_reshuffle_cb, NULL);
             _notification_popdown(popup, reason);
             break;
          }
     }
}

static void
_notification_popdown(Popup_Data                  *popup,
                      E_Notification_Notify_Closed_Reason reason)
{
   E_FREE_FUNC(popup->timer, ecore_timer_del);
   E_FREE_LIST(popup->mirrors, evas_object_del);
   if (popup->win)
     {
       edje_object_signal_callback_del
         (popup->theme, "notification,deleted", "*",
          (Edje_Signal_Cb)_notification_theme_cb_deleted);
       edje_object_signal_callback_del
         (popup->theme, "notification,close", "*",
          (Edje_Signal_Cb)_notification_theme_cb_close);
       edje_object_signal_callback_del
         (popup->theme, "notification,find", "*",
          (Edje_Signal_Cb)_notification_theme_cb_find);
       edje_object_signal_callback_del
         (popup->theme, "anchor,mouse,clicked,1,*", "notification.textblock.message",
          (Edje_Signal_Cb)_notification_theme_cb_anchor);
        evas_object_event_callback_del_full(popup->win, EVAS_CALLBACK_DEL, _notification_popup_del_cb, popup);
        evas_object_hide(popup->win);
        evas_object_del(popup->win);
     }
   if (popup->notif)
     {
        e_notification_notify_close(popup->notif, reason);
        e_object_del(E_OBJECT(popup->notif));
     }
   popup->notif = NULL;
   if (popup->pending) return;
   popups_displayed--;
   free(popup);
   e_comp_shape_queue();
}

static void
_notification_format_message(Popup_Data *popup)
{
   Evas_Object *o = popup->theme;
   Eina_Strbuf *buf = eina_strbuf_new();

   printf("NOT: set message... [%s]\n", popup->notif->body);
   edje_object_part_text_unescaped_set(o, "notification.text.title",
                                       popup->notif->summary);
   /* FIXME: Filter to only include allowed markup? */
   /* We need to replace \n with <ps/>. FIXME: We need to handle all the
   * newline kinds, and paragraph separator. ATM this will suffice. */
   eina_strbuf_append(buf, popup->notif->body);
   eina_strbuf_replace_all(buf, "\n", "<br/>");
   // message is thge shadow sizer part
   edje_object_part_text_set(o, "message",
                             eina_strbuf_string_get(buf));
   edje_object_part_text_set(o, "notification.textblock.message",
                             eina_strbuf_string_get(buf));
   eina_strbuf_free(buf);
}
