#include "e.h"

/* E_Desk is a child object of E_Zone. A desk is essentially a background
 * and an associated set of client windows. Each zone can have an arbitrary
 * number of desktops.
 */

static void      _e_desk_free(E_Desk *desk);
static void      _e_desk_event_desk_show_free(void *data, void *ev);
static void      _e_desk_event_desk_before_show_free(void *data, void *ev);
static void      _e_desk_event_desk_after_show_free(void *data, void *ev);
static void      _e_desk_event_desk_deskshow_free(void *data, void *ev);
static void      _e_desk_event_desk_name_change_free(void *data, void *ev);
static void      _e_desk_show_begin(E_Desk *desk, int dx, int dy);
static void      _e_desk_hide_begin(E_Desk *desk, int dx, int dy);
static void      _e_desk_event_desk_window_profile_change_free(void *data, void *ev);
static void      _e_desk_window_profile_change_protocol_set(void);

static E_Desk_Flip_Cb _e_desk_flip_cb = NULL;
static void *_e_desk_flip_data = NULL;

E_API int E_EVENT_DESK_SHOW = 0;
E_API int E_EVENT_DESK_BEFORE_SHOW = 0;
E_API int E_EVENT_DESK_AFTER_SHOW = 0;
E_API int E_EVENT_DESK_DESKSHOW = 0;
E_API int E_EVENT_DESK_NAME_CHANGE = 0;
E_API int E_EVENT_DESK_WINDOW_PROFILE_CHANGE = 0;

EINTERN int
e_desk_init(void)
{
   E_EVENT_DESK_SHOW = ecore_event_type_new();
   E_EVENT_DESK_BEFORE_SHOW = ecore_event_type_new();
   E_EVENT_DESK_AFTER_SHOW = ecore_event_type_new();
   E_EVENT_DESK_DESKSHOW = ecore_event_type_new();
   E_EVENT_DESK_NAME_CHANGE = ecore_event_type_new();
   E_EVENT_DESK_WINDOW_PROFILE_CHANGE = ecore_event_type_new();
   return 1;
}

EINTERN int
e_desk_shutdown(void)
{
   return 1;
}

E_API E_Desk *
e_desk_new(E_Zone *zone, int x, int y)
{
   E_Desk *desk;
   Eina_List *l;
   E_Config_Desktop_Name *cfname;
   E_Config_Desktop_Window_Profile *cfprof;
   char name[40];
   int ok = 0;

   E_OBJECT_CHECK_RETURN(zone, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, NULL);

   desk = E_OBJECT_ALLOC(E_Desk, E_DESK_TYPE, _e_desk_free);
   if (!desk) return NULL;

   desk->zone = zone;
   desk->x = x;
   desk->y = y;

   /* Get current desktop's name */
   EINA_LIST_FOREACH(e_config->desktop_names, l, cfname)
     {
        if ((cfname->zone >= 0) &&
            ((int)zone->num != cfname->zone)) continue;
        if ((cfname->desk_x != desk->x) || (cfname->desk_y != desk->y))
          continue;
        desk->name = eina_stringshare_ref(cfname->name);
        ok = 1;
        break;
     }

   if (!ok)
     {
        snprintf(name, sizeof(name), _(e_config->desktop_default_name), x, y);
        desk->name = eina_stringshare_add(name);
     }
   /* Get window profile name for current desktop */
   ok = 0;
   EINA_LIST_FOREACH(e_config->desktop_window_profiles, l, cfprof)
     {
        if ((cfprof->zone >= 0) &&
            ((int)zone->num != cfprof->zone)) continue;
        if ((cfprof->desk_x != desk->x) || (cfprof->desk_y != desk->y))
          continue;
        desk->window_profile = eina_stringshare_ref(cfprof->profile);
        ok = 1;
        break;
     }

   if (!ok)
     desk->window_profile = eina_stringshare_ref(e_config->desktop_default_window_profile);
   return desk;
}

E_API E_Client *
e_desk_client_top_visible_get(const E_Desk *desk)
{
   E_Client *ec;

   E_OBJECT_CHECK_RETURN(desk, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(desk, E_DESK_TYPE, NULL);

   E_CLIENT_REVERSE_FOREACH(ec)
     if (e_client_util_desk_visible(ec, desk) && evas_object_visible_get(ec->frame)) return ec;
   return NULL;
}

E_API void
e_desk_name_set(E_Desk *desk, const char *name)
{
   E_Event_Desk_Name_Change *ev;

   E_OBJECT_CHECK(desk);
   E_OBJECT_TYPE_CHECK(desk, E_DESK_TYPE);

   eina_stringshare_replace(&desk->name, name);

   ev = E_NEW(E_Event_Desk_Name_Change, 1);
   ev->desk = desk;
   e_object_ref(E_OBJECT(desk));
   ecore_event_add(E_EVENT_DESK_NAME_CHANGE, ev,
                   _e_desk_event_desk_name_change_free, NULL);
}

E_API void
e_desk_name_add(int zone, int desk_x, int desk_y, const char *name)
{
   E_Config_Desktop_Name *cfname;

   e_desk_name_del(zone, desk_x, desk_y);

   cfname = E_NEW(E_Config_Desktop_Name, 1);
   cfname->zone = zone;
   cfname->desk_x = desk_x;
   cfname->desk_y = desk_y;
   if (name) cfname->name = eina_stringshare_add(name);
   else cfname->name = NULL;
   e_config->desktop_names = eina_list_append(e_config->desktop_names, cfname);
}

E_API void
e_desk_name_del(int zone, int desk_x, int desk_y)
{
   Eina_List *l = NULL;
   E_Config_Desktop_Name *cfname = NULL;

   EINA_LIST_FOREACH(e_config->desktop_names, l, cfname)
     {
        if ((cfname->zone == zone) &&
            (cfname->desk_x == desk_x) && (cfname->desk_y == desk_y))
          {
             e_config->desktop_names =
               eina_list_remove_list(e_config->desktop_names, l);
             if (cfname->name) eina_stringshare_del(cfname->name);
             E_FREE(cfname);
             break;
          }
     }
}

E_API void
e_desk_name_update(void)
{
   const Eina_List *z, *l;
   E_Zone *zone;
   E_Desk *desk;
   E_Config_Desktop_Name *cfname;
   int d_x, d_y, ok;
   char name[40];

   EINA_LIST_FOREACH(e_comp->zones, z, zone)
     {
        for (d_x = 0; d_x < zone->desk_x_count; d_x++)
          {
             for (d_y = 0; d_y < zone->desk_y_count; d_y++)
               {
                  desk = zone->desks[d_x + zone->desk_x_count * d_y];
                  ok = 0;

                  EINA_LIST_FOREACH(e_config->desktop_names, l, cfname)
                    {
                       if ((cfname->zone >= 0) &&
                           ((int)zone->num != cfname->zone)) continue;
                       if ((cfname->desk_x != d_x) ||
                           (cfname->desk_y != d_y)) continue;
                       e_desk_name_set(desk, cfname->name);
                       ok = 1;
                       break;
                    }

                  if (!ok)
                    {
                       snprintf(name, sizeof(name),
                                _(e_config->desktop_default_name),
                                d_x, d_y);
                       e_desk_name_set(desk, name);
                    }
               }
          }
     }
}

E_API void
e_desk_show(E_Desk *desk)
{
   E_Event_Desk_Show *ev;
   E_Event_Desk_Before_Show *eev;
   E_Event_Desk_After_Show *eeev;
   Edje_Message_Int_Set *msg;
   Eina_List *l;
   E_Shelf *es;
   int was_zone = 0, x, y, dx = 0, dy = 0;

   E_OBJECT_CHECK(desk);
   E_OBJECT_TYPE_CHECK(desk, E_DESK_TYPE);
   if (desk->visible) return;

   eev = E_NEW(E_Event_Desk_Before_Show, 1);
   eev->desk = e_desk_current_get(desk->zone);
   e_object_ref(E_OBJECT(eev->desk));
   ecore_event_add(E_EVENT_DESK_BEFORE_SHOW, eev,
                   _e_desk_event_desk_before_show_free, NULL);

   for (x = 0; x < desk->zone->desk_x_count; x++)
     {
        for (y = 0; y < desk->zone->desk_y_count; y++)
          {
             E_Desk *desk2;

             desk2 = e_desk_at_xy_get(desk->zone, x, y);
             if (desk2->visible)
               {
                  desk2->visible = 0;
                  if (e_config->desk_flip_wrap)
                    {
                       /* current desk (desk2) is last desk, switching to first desk (desk) */
                       if ((!desk->x) && (!desk->y) && (desk2->x + 1 == desk->zone->desk_x_count) && (desk2->y + 1 == desk->zone->desk_y_count))
                         {
                            dx = (desk->x != desk2->x) ? 1 : 0;
                            dy = (desk->y != desk2->y) ? 1 : 0;
                         }
                       /* current desk (desk2) is first desk, switching to last desk (desk) */
                       else if ((!desk2->x) && (!desk2->y) && (desk->x + 1 == desk->zone->desk_x_count) && (desk->y + 1 == desk->zone->desk_y_count))
                         {
                            dx = (desk->x != desk2->x) ? -1 : 0;
                            dy = (desk->y != desk2->y) ? -1 : 0;
                         }
                    }
                  if ((!dx) && (!dy))
                    {
                       dx = desk->x - desk2->x;
                       dy = desk->y - desk2->y;
                    }
                  _e_desk_hide_begin(desk2, dx, dy);
                  break;
               }
          }
     }

   desk->zone->desk_x_prev = desk->zone->desk_x_current;
   desk->zone->desk_y_prev = desk->zone->desk_y_current;
   desk->zone->desk_x_current = desk->x;
   desk->zone->desk_y_current = desk->y;
   desk->visible = 1;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msg->count = 4;
   msg->val[0] = desk->x;
   msg->val[1] = desk->zone->desk_x_count;
   msg->val[2] = desk->y;
   msg->val[3] = desk->zone->desk_y_count;
   if (desk->zone->bg_object)
     edje_object_message_send(desk->zone->bg_object, EDJE_MESSAGE_INT_SET, 0, msg);

   if (desk->zone->bg_object) was_zone = 1;
   _e_desk_show_begin(desk, dx, dy);
   if (!e_config->desk_flip_animate_type)
     {
        if (e_config->focus_last_focused_per_desktop)
          e_desk_last_focused_focus(desk);
        else if (e_client_focused_get())
          evas_object_focus_set(e_client_focused_get()->frame, 0);
     }

   if (was_zone)
     e_bg_zone_update(desk->zone, E_BG_TRANSITION_DESK);
   else
     e_bg_zone_update(desk->zone, E_BG_TRANSITION_START);

   ev = E_NEW(E_Event_Desk_Show, 1);
   ev->desk = desk;
   e_object_ref(E_OBJECT(desk));
   ecore_event_add(E_EVENT_DESK_SHOW, ev, _e_desk_event_desk_show_free, NULL);

   EINA_LIST_FOREACH(e_shelf_list(), l, es)
     {
        if (es->zone != desk->zone) continue;
        if (e_shelf_desk_visible(es, desk))
          e_shelf_show(es);
        else
          e_shelf_hide(es);
     }

   if (e_config->desk_flip_animate_mode == 0)
     {
        eeev = E_NEW(E_Event_Desk_After_Show, 1);
        eeev->desk = e_desk_current_get(desk->zone);
        e_object_ref(E_OBJECT(eeev->desk));
        ecore_event_add(E_EVENT_DESK_AFTER_SHOW, eeev,
                        _e_desk_event_desk_after_show_free, NULL);
     }
   e_zone_edge_flip_eval(desk->zone);
}

E_API void
e_desk_deskshow(E_Zone *zone)
{
   E_Client *ec;
   E_Desk *desk;
   E_Event_Desk_Show *ev;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   desk = e_desk_current_get(zone);
   if (desk->deskshow_toggle)
     {
        /* uniconify raises windows and changes stacking order
         * go top-down to avoid skipping windows
         */
        E_CLIENT_REVERSE_FOREACH(ec)
          {
             if (e_client_util_ignored_get(ec)) continue;
             if (ec->desk != desk) continue;
             if (ec->deskshow)
               {
                  ec->deskshow = 0;
                  e_client_uniconify(ec);
               }
          }
     }
   else
     {
        /*
         * iconify raises, so we have to start from the bottom so we are going forward
         */
        E_CLIENT_FOREACH(ec)
          {
             if (e_client_util_ignored_get(ec)) continue;
             if (ec->desk != desk) continue;
             if (ec->iconic) continue;
             if (ec->netwm.state.skip_taskbar) continue;
             if (ec->user_skip_winlist) continue;
             ec->deskshow = 1;
             e_client_iconify(ec);
          }
     }
   desk->deskshow_toggle = !desk->deskshow_toggle;
   ev = E_NEW(E_Event_Desk_Show, 1);
   ev->desk = desk;
   e_object_ref(E_OBJECT(desk));
   ecore_event_add(E_EVENT_DESK_DESKSHOW, ev,
                   _e_desk_event_desk_deskshow_free, NULL);
}

E_API E_Client *
e_desk_last_focused_focus(E_Desk *desk)
{
   Eina_List *l = NULL;
   E_Client *ec, *ecs = NULL;

   EINA_LIST_FOREACH(e_client_focus_stack_get(), l, ec)
     {
        if ((!ec->iconic) && (evas_object_visible_get(ec->frame)) &&
            ((ec->desk == desk) || ((ec->zone == desk->zone) && ec->sticky)) &&
            (ec->icccm.accepts_focus || ec->icccm.take_focus) &&
            (ec->netwm.type != E_WINDOW_TYPE_DOCK) &&
            (ec->netwm.type != E_WINDOW_TYPE_TOOLBAR) &&
            (ec->netwm.type != E_WINDOW_TYPE_MENU) &&
            (ec->netwm.type != E_WINDOW_TYPE_SPLASH) &&
            (ec->netwm.type != E_WINDOW_TYPE_DESKTOP))
          {
             /* this was the window last focused in this desktop */
             if (!ec->lock_focus_out)
               {
                  if (ec->sticky)
                    {
                       ecs = ec;
                       continue;
                    }
                  e_client_focus_set_with_pointer(ec);
                  if (e_config->raise_on_revert_focus)
                    evas_object_raise(ec->frame);
                  return ec;
               }
          }
     }
   if (ecs && evas_object_visible_get(ecs->frame))
     {
        e_client_focus_set_with_pointer(ecs);
        if (e_config->raise_on_revert_focus)
          evas_object_raise(ecs->frame);
        return ecs;
     }
   if (e_client_focused_get())
     evas_object_focus_set(e_client_focused_get()->frame, 0);
   return NULL;
}

E_API void
e_desk_row_add(E_Zone *zone)
{
   e_zone_desk_count_set(zone, zone->desk_x_count, zone->desk_y_count + 1);
}

E_API void
e_desk_row_remove(E_Zone *zone)
{
   e_zone_desk_count_set(zone, zone->desk_x_count, zone->desk_y_count - 1);
}

E_API void
e_desk_col_add(E_Zone *zone)
{
   e_zone_desk_count_set(zone, zone->desk_x_count + 1, zone->desk_y_count);
}

E_API void
e_desk_col_remove(E_Zone *zone)
{
   e_zone_desk_count_set(zone, zone->desk_x_count - 1, zone->desk_y_count);
}

E_API E_Desk *
e_desk_current_get(E_Zone *zone)
{
   E_OBJECT_CHECK_RETURN(zone, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, NULL);

   return e_desk_at_xy_get(zone, zone->desk_x_current, zone->desk_y_current);
}

E_API E_Desk *
e_desk_at_xy_get(E_Zone *zone, int x, int y)
{
   E_OBJECT_CHECK_RETURN(zone, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, NULL);

   if ((x >= zone->desk_x_count) || (y >= zone->desk_y_count))
     return NULL;
   else if ((x < 0) || (y < 0))
     return NULL;

   if (!zone->desks) return NULL;
   return zone->desks[x + (y * zone->desk_x_count)];
}

E_API E_Desk *
e_desk_at_pos_get(E_Zone *zone, int pos)
{
   int x, y;

   E_OBJECT_CHECK_RETURN(zone, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, NULL);

   y = pos / zone->desk_x_count;
   x = pos - (y * zone->desk_x_count);

   if ((x >= zone->desk_x_count) || (y >= zone->desk_y_count))
     return NULL;

   return zone->desks[x + (y * zone->desk_x_count)];
}

E_API void
e_desk_xy_get(E_Desk *desk, int *x, int *y)
{
   E_OBJECT_CHECK(desk);
   E_OBJECT_TYPE_CHECK(desk, E_DESK_TYPE);

   if (x) *x = desk->x;
   if (y) *y = desk->y;
}

E_API void
e_desk_next(E_Zone *zone)
{
   int x, y;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if ((zone->desk_x_count < 2) && (zone->desk_y_count < 2))
     return;

   x = zone->desk_x_current;
   y = zone->desk_y_current;

   x++;
   if (x >= zone->desk_x_count)
     {
        x = 0;
        y++;
        if (y >= zone->desk_y_count) y = 0;
     }

   e_desk_show(e_desk_at_xy_get(zone, x, y));
}

E_API void
e_desk_prev(E_Zone *zone)
{
   int x, y;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if ((zone->desk_x_count < 2) && (zone->desk_y_count < 2))
     return;

   x = zone->desk_x_current;
   y = zone->desk_y_current;

   x--;
   if (x < 0)
     {
        x = zone->desk_x_count - 1;
        y--;
        if (y < 0) y = zone->desk_y_count - 1;
     }
   e_desk_show(e_desk_at_xy_get(zone, x, y));
}

E_API void
e_desk_window_profile_set(E_Desk *desk,
                          const char *profile)
{
   E_Event_Desk_Window_Profile_Change *ev;

   E_OBJECT_CHECK(desk);
   E_OBJECT_TYPE_CHECK(desk, E_DESK_TYPE);

   eina_stringshare_replace(&desk->window_profile, profile);

   ev = E_NEW(E_Event_Desk_Window_Profile_Change, 1);
   ev->desk = desk;
   e_object_ref(E_OBJECT(desk));
   ecore_event_add(E_EVENT_DESK_WINDOW_PROFILE_CHANGE, ev,
                   _e_desk_event_desk_window_profile_change_free, NULL);
}

E_API void
e_desk_window_profile_add(int zone,
                          int desk_x,
                          int desk_y,
                          const char *profile)
{
   E_Config_Desktop_Window_Profile *cfprof;

   e_desk_window_profile_del(zone, desk_x, desk_y);

   cfprof = E_NEW(E_Config_Desktop_Window_Profile, 1);
   cfprof->zone = zone;
   cfprof->desk_x = desk_x;
   cfprof->desk_y = desk_y;
   cfprof->profile = eina_stringshare_add(profile);
   e_config->desktop_window_profiles = eina_list_append(e_config->desktop_window_profiles, cfprof);
}

E_API void
e_desk_window_profile_del(int zone,
                          int desk_x,
                          int desk_y)
{
   Eina_List *l = NULL;
   E_Config_Desktop_Window_Profile *cfprof = NULL;

   EINA_LIST_FOREACH(e_config->desktop_window_profiles, l, cfprof)
     {
        if (!((cfprof->zone == zone) &&
              (cfprof->desk_x == desk_x) &&
              (cfprof->desk_y == desk_y)))
          continue;

        e_config->desktop_window_profiles =
          eina_list_remove_list(e_config->desktop_window_profiles, l);
        eina_stringshare_del(cfprof->profile);
        free(cfprof);
        break;
     }
}

E_API void
e_desk_window_profile_update(void)
{
   const Eina_List *z, *l;
   E_Zone *zone;
   E_Desk *desk;
   E_Config_Desktop_Window_Profile *cfprof;
   int d_x, d_y, ok;

   _e_desk_window_profile_change_protocol_set();

   if (!(e_config->use_desktop_window_profile))
     return;

   EINA_LIST_FOREACH(e_comp->zones, z, zone)
     {
        for (d_x = 0; d_x < zone->desk_x_count; d_x++)
          {
             for (d_y = 0; d_y < zone->desk_y_count; d_y++)
               {
                  desk = zone->desks[d_x + zone->desk_x_count * d_y];
                  ok = 0;

                  EINA_LIST_FOREACH(e_config->desktop_window_profiles, l, cfprof)
                    {
                       if ((cfprof->zone >= 0) &&
                           ((int)zone->num != cfprof->zone)) continue;
                       if ((cfprof->desk_x != d_x) ||
                           (cfprof->desk_y != d_y)) continue;
                       e_desk_window_profile_set(desk, cfprof->profile);
                       ok = 1;
                       break;
                    }

                  if (!ok)
                    {
                       e_desk_window_profile_set
                         (desk, e_config->desktop_default_window_profile);
                    }
               }
          }
     }
}

E_API void
e_desk_flip_cb_set(E_Desk_Flip_Cb cb, const void *data)
{
   _e_desk_flip_cb = cb;
   _e_desk_flip_data = (void*)data;
}

E_API void
e_desk_flip_end(E_Desk *desk)
{
   E_Event_Desk_After_Show *ev;
   E_Client *ec;

   ev = E_NEW(E_Event_Desk_After_Show, 1);
   ev->desk = desk;
   e_object_ref(E_OBJECT(ev->desk));
   ecore_event_add(E_EVENT_DESK_AFTER_SHOW, ev,
                   _e_desk_event_desk_after_show_free, NULL);

   e_comp_shape_queue();
   if (!e_config->focus_last_focused_per_desktop) return;
   if ((e_config->focus_policy == E_FOCUS_MOUSE) ||
       (e_config->focus_policy == E_FOCUS_SLOPPY))
     {
        ec = e_client_focused_get();
        /* only set focus/warp pointer if currently focused window
         * is on same screen (user hasn't switched screens during transition)
         */
        if (ec && ec->desk && (ec->desk->zone != desk->zone)) return;
     }
   if (starting) return;
   ec = e_desk_last_focused_focus(desk);
   if ((e_config->focus_policy != E_FOCUS_MOUSE) && (!ec))
     {
        /* we didn't previously have a focused window on this desk
         * but we should, so this is probably the first time the
         * user has flipped to this desk. let's be helpful and
         * focus a random window!
         */
         E_CLIENT_REVERSE_FOREACH(ec)
           {
              /* start with top and go down... */
              if (e_client_util_ignored_get(ec)) continue;
              if (!e_client_util_desk_visible(ec, desk)) continue;
              if (ec->iconic) continue;
              evas_object_focus_set(ec->frame, 1);
              if (e_config->raise_on_revert_focus)
                evas_object_raise(ec->frame);
              return;
           }
     }
}

E_API unsigned int
e_desks_count(void)
{
   Eina_List *l;
   E_Zone *zone;
   unsigned int count = 0;

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        int cx = 0, cy = 0;

        e_zone_desk_count_get(zone, &cx, &cy);
        count += cx * cy;
     }
   return count;
}

static void
_e_desk_free(E_Desk *desk)
{
   eina_stringshare_del(desk->name);
   desk->name = NULL;
   free(desk);
}

static void
_e_desk_event_desk_show_free(void *data EINA_UNUSED, void *event)
{
   E_Event_Desk_Show *ev;

   ev = event;
   e_object_unref(E_OBJECT(ev->desk));
   free(ev);
}

static void
_e_desk_event_desk_before_show_free(void *data EINA_UNUSED, void *event)
{
   E_Event_Desk_Before_Show *ev;

   ev = event;
   e_object_unref(E_OBJECT(ev->desk));
   free(ev);
}

static void
_e_desk_event_desk_after_show_free(void *data EINA_UNUSED, void *event)
{
   E_Event_Desk_After_Show *ev;

   ev = event;
   e_object_unref(E_OBJECT(ev->desk));
   free(ev);
}

static void
_e_desk_event_desk_deskshow_free(void *data EINA_UNUSED, void *event)
{
   E_Event_Desk_Show *ev;

   ev = event;
   e_object_unref(E_OBJECT(ev->desk));
   free(ev);
}

static void
_e_desk_event_desk_name_change_free(void *data EINA_UNUSED, void *event)
{
   E_Event_Desk_Name_Change *ev = event;
   e_object_unref(E_OBJECT(ev->desk));
   free(ev);
}

static void
_e_desk_event_desk_window_profile_change_free(void *data EINA_UNUSED, void *event)
{
   E_Event_Desk_Window_Profile_Change *ev = event;
   e_object_unref(E_OBJECT(ev->desk));
   E_FREE(ev);
}

static Eina_Bool
_e_desk_transition_setup(E_Client *ec, int dx, int dy, int state)
{
   e_comp_object_effect_set(ec->frame, e_config->desk_flip_animate_type ?: "none");
   if (e_config->desk_flip_animate_type)
     {
        /* set geoms */
        e_comp_object_effect_params_set(ec->frame, 1, (int[]){ec->x - ec->zone->x, ec->y - ec->zone->y, ec->w, ec->h, ec->zone->w, ec->zone->h, dx, dy}, 8);
        e_comp_object_effect_params_set(ec->frame, 0, (int[]){state}, 1);
     }

   return !!e_config->desk_flip_animate_type;
}

static void
_e_desk_show_end(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   E_Client *ec = data;

   ec->desk->animate_count--;
   e_client_comp_hidden_set(ec, ec->shaded);
   e_comp_object_effect_unclip(ec->frame);
   ec->hidden = 0;
   if (!ec->visible) evas_object_show(ec->frame);
   if (ec->desk != e_desk_current_get(ec->zone)) return;
   if (!ec->desk->animate_count) e_desk_flip_end(ec->desk);
}

static void
_e_desk_hide_end(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   E_Client *ec = data;

   ec->desk->animate_count--;
   ec->hidden = 1;
   evas_object_hide(ec->frame);
}

static void
_e_desk_show_begin(E_Desk *desk, int dx, int dy)
{
   E_Client *ec;

   if (dx < 0) dx = -1;
   if (dx > 0) dx = 1;
   if (dy < 0) dy = -1;
   if (dy > 0) dy = 1;

   desk->animate_count = 0;
   if (_e_desk_flip_cb && e_config->desk_flip_animate_type)
     {
        _e_desk_flip_cb(_e_desk_flip_data, desk, dx, dy, 1);
        return;
     }
   E_CLIENT_FOREACH(ec)
     {
        if (e_client_util_ignored_get(ec) || (ec->desk->zone != desk->zone) || (ec->iconic)) continue;
        if (ec->moving)
          {
             e_client_desk_set(ec, desk);
             evas_object_show(ec->frame);
             continue;
          }
        if ((ec->desk != desk) || (ec->sticky)) continue;
        if ((!starting) && (!ec->new_client) && _e_desk_transition_setup(ec, dx, dy, 1))
          {
             e_comp_object_effect_stop(ec->frame, _e_desk_hide_end);
             e_comp_object_effect_start(ec->frame, _e_desk_show_end, ec);
             desk->animate_count++;
          }
        else
          ec->hidden = 0;

        e_client_comp_hidden_set(ec, ec->hidden || ec->shaded);
        evas_object_show(ec->frame);
     }
   if ((!e_config->desk_flip_animate_type) || (!desk->animate_count))
     e_desk_flip_end(desk);
}

static void
_e_desk_hide_begin(E_Desk *desk, int dx, int dy)
{
   E_Client *ec;

   if (dx < 0) dx = -1;
   if (dx > 0) dx = 1;
   if (dy < 0) dy = -1;
   if (dy > 0) dy = 1;

   desk->animate_count = 0;
   if (_e_desk_flip_cb && e_config->desk_flip_animate_type)
     {
        _e_desk_flip_cb(_e_desk_flip_data, desk, dx, dy, 0);
        return;
     }
   E_CLIENT_FOREACH(ec)
     {
        if (e_client_util_ignored_get(ec) || (ec->desk->zone != desk->zone) || (ec->iconic)) continue;
        if (ec->moving) continue;
        if ((ec->desk != desk) || (ec->sticky)) continue;
        if ((!starting) && (!ec->new_client) && _e_desk_transition_setup(ec, -dx, -dy, 0))
          {
             e_comp_object_effect_stop(ec->frame, _e_desk_show_end);
             e_comp_object_effect_start(ec->frame, _e_desk_hide_end, ec);
             desk->animate_count++;
          }
        else
          {
             ec->hidden = 1;
             evas_object_show(ec->frame);
             evas_object_hide(ec->frame);
          }
        e_client_comp_hidden_set(ec, EINA_TRUE);
     }
}

static void
_e_desk_window_profile_change_protocol_set(void)
{
#ifndef HAVE_WAYLAND_ONLY
  ecore_x_e_window_profile_supported_set(e_comp->root, e_config->use_desktop_window_profile);
#endif
}
