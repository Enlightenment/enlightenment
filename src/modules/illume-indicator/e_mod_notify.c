#include "e.h"
#include "e_mod_main.h"
#include "e_mod_notify.h"

/* local function prototypes */
static unsigned int _e_mod_notify_cb_add(void *data EINA_UNUSED, E_Notification_Notify *n);
static void _e_mod_notify_cb_del(void *data EINA_UNUSED, unsigned int id);
static Ind_Notify_Win *_e_mod_notify_find(unsigned int id);
static void _e_mod_notify_refresh(Ind_Notify_Win *nwin);
static Ind_Notify_Win *_e_mod_notify_new(E_Notification_Notify *n, unsigned id);
static Eina_Bool _e_mod_notify_cb_timeout(void *data);
static void _e_mod_notify_cb_free(Ind_Notify_Win *nwin);
static void _e_mod_notify_cb_resize(E_Win *win);

/* local variables */
static Eina_List *_nwins = NULL;
static int _notify_id = 0;

static const E_Notification_Server_Info info = {
   .name = "illume-indicator",
   .vendor = "Enlightenment",
   .version = "0.17",
   .spec_version = "1.2",
   .capabilities = { "body", NULL }
};

int
e_mod_notify_init(void)
{
   /* init notification subsystem */
   if (!e_notification_server_register(&info, _e_mod_notify_cb_add,
                                       _e_mod_notify_cb_del, NULL))
     return 0;
   return 1;
}

int
e_mod_notify_shutdown(void)
{
   Ind_Notify_Win *nwin;
   Eina_List *l, *l2;

   EINA_LIST_FOREACH_SAFE(_nwins, l, l2, nwin)
     e_object_del(E_OBJECT(nwin));

   e_notification_server_unregister();

   return 1;
}

static unsigned int
_e_mod_notify_cb_add(void *data EINA_UNUSED, E_Notification_Notify *n)
{
   Ind_Notify_Win *nwin = NULL;

   _notify_id++;

   if (n->replaces_id && (nwin = _e_mod_notify_find(n->replaces_id)))
     {
        if (nwin->notify)
          e_object_del(E_OBJECT(nwin->notify));
        nwin->notify = n;
        nwin->id = _notify_id;
        _e_mod_notify_refresh(nwin);
     }

   if (!nwin)
     {
        nwin = _e_mod_notify_new(n, _notify_id);
        EINA_SAFETY_ON_NULL_RETURN_VAL(nwin, 0);
     }

   /* show it */
   ecore_x_e_illume_quickpanel_state_send(nwin->zone->black_win,
                                          ECORE_X_ILLUME_QUICKPANEL_STATE_ON);

   if (nwin->timer) ecore_timer_del(nwin->timer);
   nwin->timer = NULL;

   if (n->timeout < 0) n->timeout = 3000.0;
   n->timeout = n->timeout / 1000.0;

   if (n->timeout > 0)
     nwin->timer = ecore_timer_add(n->timeout, _e_mod_notify_cb_timeout, nwin);

   return _notify_id;
}

static void
_e_mod_notify_cb_del(void *data EINA_UNUSED, unsigned int id)
{
   Ind_Notify_Win *nwin = _e_mod_notify_find(id);
   if (!nwin)
     return;

   e_object_del(E_OBJECT(nwin));
}

static
Ind_Notify_Win *
_e_mod_notify_find(unsigned int id)
{
   const Eina_List *l;
   Ind_Notify_Win *nwin;

   EINA_LIST_FOREACH(_nwins, l, nwin)
     if (nwin->id == id)
       return nwin;
   return NULL;
}

static void
_e_mod_notify_refresh(Ind_Notify_Win *nwin)
{
   const char *icon;
   Evas_Coord mw, mh;
   int size;

   if (!nwin) return;

   if (nwin->o_icon)
     {
        edje_object_part_unswallow(nwin->o_base, nwin->o_icon);
        evas_object_del(nwin->o_icon);
     }

   size = (48 * e_scale);
   if (nwin->notify->icon.raw.data)
     {
        nwin->o_icon = e_notification_notify_raw_image_get(nwin->notify,
                                                           nwin->win->evas);
        if (nwin->o_icon)
          evas_object_image_fill_set(nwin->o_icon, 0, 0, size, size);
     }
   else if (nwin->notify->icon.icon[0])
     {
        icon = nwin->notify->icon.icon;
        if (!strncmp(icon, "file://", 7))
          {
             icon += 7;
             nwin->o_icon = e_util_icon_add(icon, nwin->win->evas);
          }
        else
           nwin->o_icon = e_util_icon_theme_icon_add(icon, size, nwin->win->evas);
     }

   if (nwin->o_icon)
     {
        evas_object_resize(nwin->o_icon, size, size);
        evas_object_size_hint_min_set(nwin->o_icon, size, size);
        evas_object_size_hint_max_set(nwin->o_icon, size, size);
        edje_object_part_swallow(nwin->o_base, "e.swallow.icon", nwin->o_icon);
     }

   edje_object_part_text_set(nwin->o_base, "e.text.title", nwin->notify->summary);
   edje_object_part_text_set(nwin->o_base, "e.text.message", nwin->notify->body);


   edje_object_calc_force(nwin->o_base);
   edje_object_size_min_calc(nwin->o_base, &mw, &mh);

   evas_object_size_hint_min_set(nwin->o_base, mw, mh);
   evas_object_size_hint_min_set(nwin->win, nwin->zone->w, mh);
}

static Ind_Notify_Win *
_e_mod_notify_new(E_Notification_Notify *n, unsigned id)
{
   Ind_Notify_Win *nwin;
   Ecore_X_Window_State states[2];
   E_Zone *zone;

   nwin = E_OBJECT_ALLOC(Ind_Notify_Win, IND_NOTIFY_WIN_TYPE,
                         _e_mod_notify_cb_free);
   if (!nwin) return NULL;
   _nwins = eina_list_append(_nwins, nwin);
   nwin->notify = n;
   nwin->id = id;

   zone = e_util_zone_current_get(e_manager_current_get());
   nwin->zone = zone;

   nwin->win = e_win_new(zone->comp);
   nwin->win->data = nwin;

   e_win_name_class_set(nwin->win, "Illume-Notify", "Illume-Notify");
   e_win_no_remember_set(nwin->win, EINA_TRUE);
   evas_object_resize_callback_set(nwin->win, _e_mod_notify_cb_resize);

   ecore_x_e_illume_quickpanel_set(nwin->win->evas_win, EINA_TRUE);
   ecore_x_e_illume_quickpanel_priority_major_set(nwin->win->evas_win, n->urgency);
   ecore_x_e_illume_quickpanel_zone_set(nwin->win->evas_win, zone->num);

   states[0] = ECORE_X_WINDOW_STATE_SKIP_TASKBAR;
   states[1] = ECORE_X_WINDOW_STATE_SKIP_PAGER;
   ecore_x_netwm_window_state_set(nwin->win->evas_win, states, 2);
   ecore_x_icccm_hints_set(nwin->win->evas_win, 0, 0, 0, 0, 0, 0, 0);

   nwin->o_base = edje_object_add(nwin->win->evas);
   if (!e_theme_edje_object_set(nwin->o_base,
                                "base/theme/modules/illume-indicator",
                                "modules/illume-indicator/notify"))
     {
        char buff[PATH_MAX];

        snprintf(buff, sizeof(buff),
                 "%s/e-module-illume-indicator.edj", _ind_mod_dir);
        edje_object_file_set(nwin->o_base, buff,
                             "modules/illume-indicator/notify");
     }
   evas_object_move(nwin->o_base, 0, 0);
   evas_object_show(nwin->o_base);

   _e_mod_notify_refresh(nwin);

   evas_object_show(nwin->win);
   e_client_zone_set(nwin->win->client, zone);
   nwin->win->client->user_skip_winlist = 1;

   return nwin;
}

static Eina_Bool
_e_mod_notify_cb_timeout(void *data)
{
   Ind_Notify_Win *nwin;

   if (!(nwin = data)) return EINA_FALSE;

   /* hide it */
   ecore_x_e_illume_quickpanel_state_send(nwin->zone->black_win,
                                          ECORE_X_ILLUME_QUICKPANEL_STATE_OFF);
   e_object_del(E_OBJECT(nwin));
   return EINA_FALSE;
}

static void
_e_mod_notify_cb_free(Ind_Notify_Win *nwin)
{
   if (nwin->timer) ecore_timer_del(nwin->timer);
   nwin->timer = NULL;
   if (nwin->o_icon) evas_object_del(nwin->o_icon);
   nwin->o_icon = NULL;
   if (nwin->o_base) evas_object_del(nwin->o_base);
   nwin->o_base = NULL;
   if (nwin->win) e_object_del(E_OBJECT(nwin->win));
   nwin->win = NULL;
   e_notification_notify_close(nwin->notify,
                               E_NOTIFICATION_NOTIFY_CLOSED_REASON_REQUESTED);
   e_object_del(E_OBJECT(nwin->notify));
   _nwins = eina_list_remove(_nwins, nwin);
   E_FREE(nwin);
}

static void
_e_mod_notify_cb_resize(E_Win *win)
{
   Ind_Notify_Win *nwin;

   if (!(nwin = win->data)) return;
   if (nwin->o_base) evas_object_resize(nwin->o_base, win->w, win->h);
}
