#include "e.h"
#include "e_mod_main.h"

/* local subsystem functions */
typedef struct _E_Winlist_Win E_Winlist_Win;

struct _E_Winlist_Win
{
   Evas_Object  *bg_object;
   Evas_Object  *icon_object;
   E_Client     *client;
   unsigned char was_iconified : 1;
   unsigned char was_shaded : 1;
};

static void      _e_winlist_size_adjust(void);
static Eina_Bool _e_winlist_client_add(E_Client *ec, E_Zone *zone, E_Desk *desk);
static void      _e_winlist_client_del(E_Client *ec);
static void      _e_winlist_activate_nth(int n);
static void      _e_winlist_activate(void);
static void      _e_winlist_deactivate(void);
static void      _e_winlist_show_active(void);
static Eina_Bool _e_winlist_cb_event_border_add(void *data, int type, void *event);
static Eina_Bool _e_winlist_cb_event_border_remove(void *data, int type, void *event);
static Eina_Bool _e_winlist_cb_key_down(void *data, int type, void *event);
static Eina_Bool _e_winlist_cb_key_up(void *data, int type, void *event);
static Eina_Bool _e_winlist_cb_mouse_down(void *data, int type, void *event);
static Eina_Bool _e_winlist_cb_mouse_up(void *data, int type, void *event);
static Eina_Bool _e_winlist_cb_mouse_wheel(void *data, int type, void *event);
static Eina_Bool _e_winlist_cb_mouse_move(void *data, int type, void *event);
static Eina_Bool _e_winlist_scroll_timer(void *data);
static Eina_Bool _e_winlist_animator(void *data);
#if 0
static void      _e_winlist_cb_item_mouse_in(void *data, Evas *evas,
                                             Evas_Object *obj, void *event_info);
#endif

/* local subsystem globals */
static Evas_Object *_winlist = NULL;
static E_Zone *_winlist_zone = NULL;
static Evas_Object *_bg_object = NULL;
static Evas_Object *_list_object = NULL;
static Evas_Object *_icon_object = NULL;
static Eina_List *_wins = NULL;
static Eina_List *_win_selected = NULL;
static E_Desk *_last_desk = NULL;
static int _last_pointer_x = 0;
static int _last_pointer_y = 0;
static E_Client *_last_client = NULL;
static int _hold_count = 0;
static int _hold_mod = 0;
static E_Winlist_Activate_Type _activate_type = 0;
static Eina_List *_handlers = NULL;
static Ecore_Window _input_window = 0;
static int _scroll_to = 0;
static double _scroll_align_to = 0.0;
static double _scroll_align = 0.0;
static Ecore_Timer *_scroll_timer = NULL;
static Ecore_Animator *_animator = NULL;
static E_Client *_ec_next = NULL;

static Eina_Bool
_wmclass_picked(const Eina_List *lst, const char *wmclass)
{
   const Eina_List *l;
   const char *s;

   if (!wmclass) return EINA_FALSE;

   EINA_LIST_FOREACH(lst, l, s)
     if (s == wmclass)
       return EINA_TRUE;

   return EINA_FALSE;
}

/* externally accessible functions */
int
e_winlist_init(void)
{
   return 1;
}

int
e_winlist_shutdown(void)
{
   e_winlist_hide();
   return 1;
}

int
e_winlist_show(E_Zone *zone, E_Winlist_Filter filter)
{
   int x, y, w, h;
   Evas_Object *o;
   Eina_List *l;
   E_Desk *desk;
   E_Client *ec;
   Eina_List *wmclasses = NULL;

   E_OBJECT_CHECK_RETURN(zone, 0);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, 0);

   if (_winlist) return 0;

#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_get(zone)->comp_type == E_PIXMAP_TYPE_X)
     {
        _input_window = ecore_x_window_input_new(zone->comp->man->root, 0, 0, 1, 1);
        ecore_x_window_show(_input_window);
        if (!e_grabinput_get(_input_window, 0, _input_window))
          {
             ecore_x_window_free(_input_window);
             _input_window = 0;
             return 0;
          }
     }
#endif
   if (e_comp_get(zone)->comp_type != E_PIXMAP_TYPE_X)
     {
        if (!e_comp_grab_input(e_comp_get(zone), 1, 1))
          return 0;
     }

   w = (double)zone->w * e_config->winlist_pos_size_w;
   if (w > e_config->winlist_pos_max_w) w = e_config->winlist_pos_max_w;
   else if (w < e_config->winlist_pos_min_w)
     w = e_config->winlist_pos_min_w;
   if (w > zone->w) w = zone->w;
   x = zone->x + (double)(zone->w - w) * e_config->winlist_pos_align_x;

   h = (double)zone->h * e_config->winlist_pos_size_h;
   if (h > e_config->winlist_pos_max_h) h = e_config->winlist_pos_max_h;
   else if (h < e_config->winlist_pos_min_h)
     h = e_config->winlist_pos_min_h;
   if (h > zone->h) h = zone->h;
   y = zone->y + (double)(zone->h - h) * e_config->winlist_pos_align_y;

   _winlist_zone = zone;
   e_client_move_cancel();
   e_client_resize_cancel();
   e_client_focus_track_freeze();

#ifndef HAVE_WAYLAND_ONLY
   evas_event_feed_mouse_in(zone->comp->evas, 0, NULL);
   evas_event_feed_mouse_move(zone->comp->evas, -1000000, -1000000, 0, NULL);
#endif

   evas_event_freeze(zone->comp->evas);
   o = edje_object_add(zone->comp->evas);
   _winlist = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_layer_set(_winlist, E_LAYER_CLIENT_POPUP);
   evas_object_move(_winlist, x, y);
   _bg_object = o;
   e_theme_edje_object_set(o, "base/theme/winlist",
                           "e/widgets/winlist/main");

   o = elm_box_add(o);
   _list_object = o;
   elm_box_homogeneous_set(o, 1);
   e_comp_object_util_del_list_append(_winlist, o);
   edje_object_part_swallow(_bg_object, "e.swallow.list", o);
   edje_object_part_text_set(_bg_object, "e.text.title", _("Select a window"));
   evas_object_show(o);

   _last_client = e_client_focused_get();

   desk = e_desk_current_get(_winlist_zone);
   EINA_LIST_FOREACH(e_client_focus_stack_get(), l, ec)
     {
        Eina_Bool pick;
        switch (filter)
          {
           case E_WINLIST_FILTER_CLASS_WINDOWS:
             if (!_last_client)
               pick = EINA_FALSE;
             else
               pick = _last_client->icccm.class == ec->icccm.class;
             break;
           case E_WINLIST_FILTER_CLASSES:
             pick = (!_wmclass_picked(wmclasses, ec->icccm.class));
             if (pick)
               wmclasses = eina_list_append(wmclasses, ec->icccm.class);
             break;

           default:
             pick = EINA_TRUE;
          }
        if (pick) _e_winlist_client_add(ec, _winlist_zone, desk);
     }
   eina_list_free(wmclasses);

   if (!_wins)
     {
        e_winlist_hide();
        evas_event_thaw(zone->comp->evas);
        return 1;
     }

   if (e_config->winlist_list_show_other_desk_windows ||
       e_config->winlist_list_show_other_screen_windows)
     _last_desk = e_desk_current_get(_winlist_zone);
   if (e_config->winlist_warp_while_selecting)
     ecore_evas_pointer_xy_get(_winlist_zone->comp->ee,
                            &_last_pointer_x, &_last_pointer_y);

   _e_winlist_activate_nth(1);

   if ((eina_list_count(_wins) > 1))
     {
        E_Winlist_Win *ww;

        ww = eina_list_data_get(_win_selected);
        if (ww && (ww->client == _last_client))
          e_winlist_next();
     }
   evas_event_thaw(zone->comp->evas);
   _e_winlist_size_adjust();

   E_LIST_HANDLER_APPEND(_handlers, E_EVENT_CLIENT_ADD, _e_winlist_cb_event_border_add, NULL);
   E_LIST_HANDLER_APPEND(_handlers, E_EVENT_CLIENT_REMOVE, _e_winlist_cb_event_border_remove, NULL);
   E_LIST_HANDLER_APPEND(_handlers, ECORE_EVENT_KEY_DOWN, _e_winlist_cb_key_down, NULL);
   E_LIST_HANDLER_APPEND(_handlers, ECORE_EVENT_KEY_UP, _e_winlist_cb_key_up, NULL);
   E_LIST_HANDLER_APPEND(_handlers, ECORE_EVENT_MOUSE_BUTTON_DOWN, _e_winlist_cb_mouse_down, NULL);
   E_LIST_HANDLER_APPEND(_handlers, ECORE_EVENT_MOUSE_BUTTON_UP, _e_winlist_cb_mouse_up, NULL);
   E_LIST_HANDLER_APPEND(_handlers, ECORE_EVENT_MOUSE_WHEEL, _e_winlist_cb_mouse_wheel, NULL);
   E_LIST_HANDLER_APPEND(_handlers, ECORE_EVENT_MOUSE_MOVE, _e_winlist_cb_mouse_move, NULL);

   evas_object_show(_winlist);
   return 1;
}

void
e_winlist_hide(void)
{
   E_Client *ec = NULL;
   E_Winlist_Win *ww;

   if (!_winlist) return;
   if (_win_selected)
     {
        ww = _win_selected->data;
        ec = ww->client;
     }
   evas_object_hide(_winlist);
   EINA_LIST_FREE(_wins, ww)
     {
        if ((!ec) || (ww->client != ec))
          e_object_unref(E_OBJECT(ww->client));
        free(ww);
     }
   _win_selected = NULL;
   _icon_object = NULL;

   evas_object_del(_winlist);
   e_client_focus_track_thaw();
   _winlist = NULL;
   _winlist_zone = NULL;
   _hold_count = 0;
   _hold_mod = 0;
   _activate_type = 0;

   E_FREE_LIST(_handlers, ecore_event_handler_del);

   E_FREE_FUNC(_scroll_timer, ecore_timer_del);
   E_FREE_FUNC(_animator, ecore_animator_del);

#ifndef HAVE_WAYLAND_ONLY
   if (_input_window)
     {
        e_grabinput_release(_input_window, _input_window);
        ecore_x_window_free(_input_window);
        _input_window = 0;
     }
   else
#endif
     e_comp_ungrab_input(e_comp_get(NULL), 1, 1);
   if (ec)
     {
        if (ec->shaded)
          {
             if (!ec->lock_user_shade)
               e_client_unshade(ec, ec->shade_dir);
          }
        if (e_config->winlist_list_move_after_select)
          {
             e_client_zone_set(ec, e_zone_current_get(e_util_comp_current_get()));
             e_client_desk_set(ec, e_desk_current_get(ec->zone));
          }
        else if (ec->desk)
          {
             if (!ec->sticky) e_desk_show(ec->desk);
          }
        if (!ec->lock_user_stacking)
          {
             evas_object_raise(ec->frame);
             e_client_raise_latest_set(ec);
          }
        if (ec->iconic)
          e_client_uniconify(ec);
        if (ec->shaded)
          e_client_unshade(ec, ec->shade_dir);
        if ((e_config->focus_policy != E_FOCUS_CLICK) ||
            (e_config->winlist_warp_at_end) ||
            (e_config->winlist_warp_while_selecting))
          {
             e_client_pointer_warp_to_center_now(ec);
          }
        else if (!ec->lock_focus_out)
          {
             evas_object_focus_set(ec->frame, 1);
             e_client_focus_latest_set(ec);
          }
        e_object_unref(E_OBJECT(ec));
     }
}

void
e_winlist_next(void)
{
   if (!_winlist) return;
   if (eina_list_count(_wins) == 1)
     {
        if (!_win_selected)
          {
             _win_selected = _wins;
             _e_winlist_show_active();
             _e_winlist_activate();
          }
        return;
     }
   _e_winlist_deactivate();
   if (!_win_selected)
     _win_selected = _wins;
   else
     _win_selected = _win_selected->next;
   if (!_win_selected) _win_selected = _wins;
   _e_winlist_show_active();
   _e_winlist_activate();
}

void
e_winlist_prev(void)
{
   if (!_winlist) return;
   if (eina_list_count(_wins) == 1)
     {
        if (!_win_selected)
          {
             _win_selected = _wins;
             _e_winlist_show_active();
             _e_winlist_activate();
          }
        return;
     }
   _e_winlist_deactivate();
   if (!_win_selected)
     _win_selected = _wins;
   else
     _win_selected = _win_selected->prev;
   if (!_win_selected) _win_selected = eina_list_last(_wins);
   _e_winlist_show_active();
   _e_winlist_activate();
}

static void
_e_winlist_select(E_Client *ec)
{
   Eina_Bool focus = EINA_FALSE;

   if (ec->shaded)
     {
        if (!ec->lock_user_shade)
          e_client_unshade(ec, ec->shade_dir);
     }
   if (ec->iconic)
     e_client_uniconify(ec);
   if ((!ec->lock_focus_out) &&
       (!e_config->winlist_no_warp_on_direction) &&
       ((e_config->focus_policy != E_FOCUS_CLICK) ||
       (e_config->winlist_warp_at_end) ||
       (e_config->winlist_warp_while_selecting)))
     {
       if (!e_client_pointer_warp_to_center(ec))
         focus = EINA_TRUE;
       if (_list_object && (!_animator))
         _animator = ecore_animator_add(_e_winlist_animator, NULL);
     }

   if ((!ec->lock_user_stacking) &&
       (e_config->winlist_list_raise_while_selecting))
     {
        e_client_raise_latest_set(ec);
        evas_object_raise(ec->frame);
     }
   if ((!ec->lock_focus_out) &&
       (e_config->winlist_list_focus_while_selecting))
     focus = EINA_TRUE;
   if (focus)
     evas_object_focus_set(ec->frame, 1);
}

void
e_winlist_left(E_Zone *zone)
{
   E_Client *ec;
   Eina_List *l;
   E_Desk *desk;
   E_Client *ec_orig;
   int delta = INT_MAX;
   int delta2 = INT_MAX;
   int center;

   _ec_next = NULL;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   ec_orig = e_client_focused_get();
   if (!ec_orig) return;

   center = ec_orig->x + ec_orig->w / 2;

   desk = e_desk_current_get(zone);
   EINA_LIST_FOREACH(e_client_focus_stack_get(), l, ec)
     {
        int center_next;
        int delta_next;
        int delta2_next;

        if (ec == ec_orig) continue;
        if ((!ec->icccm.accepts_focus) &&
            (!ec->icccm.take_focus)) continue;
        if (ec->netwm.state.skip_taskbar) continue;
        if (ec->user_skip_winlist) continue;
        if (ec->iconic)
          {
             if (!e_config->winlist_list_show_iconified) continue;
             if ((ec->zone != zone) &&
                 (!e_config->winlist_list_show_other_screen_iconified))
               continue;
             if ((ec->desk != desk) &&
                 (!e_config->winlist_list_show_other_desk_iconified)) continue;
          }
        else
          {
             if (ec->sticky)
               {
                  if ((ec->zone != zone) &&
                      (!e_config->winlist_list_show_other_screen_windows))
                    continue;
               }
             else
               {
                  if (ec->desk != desk)
                    {
                       if ((ec->zone) && (ec->zone != zone))
                         {
                            if (!e_config->winlist_list_show_other_screen_windows)
                              continue;
                         }
                       else if (!e_config->winlist_list_show_other_desk_windows)
                         continue;
                    }
               }
          }
        /* ec is suitable */
        center_next = ec->x + ec->w / 2;
        if (center_next >= center) continue;
        delta_next = ec_orig->x - (ec->x + ec->w);
        if (delta_next < 0) delta_next = center - center_next;
        delta2_next = abs(ec_orig->y - ec_orig->h / 2 - ec->y + ec->h/2);
        if (delta_next >= 0 && delta_next <= delta &&
            delta2_next >= 0 && delta2_next <= delta2)
          {
             _ec_next = ec;
             delta = delta_next;
             delta2 = delta2_next;
          }
     }

   if (_ec_next) _e_winlist_select(_ec_next);
}

void
e_winlist_down(E_Zone *zone)
{
   E_Client *ec;
   Eina_List *l;
   E_Desk *desk;
   E_Client *ec_orig;
   int delta = INT_MAX;
   int delta2 = INT_MAX;
   int center;

   _ec_next = NULL;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   ec_orig = e_client_focused_get();
   if (!ec_orig) return;

   center = ec_orig->y + ec_orig->h / 2;

   desk = e_desk_current_get(zone);
   EINA_LIST_FOREACH(e_client_focus_stack_get(), l, ec)
     {
        int center_next;
        int delta_next;
        int delta2_next;

        if (ec == ec_orig) continue;
        if ((!ec->icccm.accepts_focus) &&
            (!ec->icccm.take_focus)) continue;
        if (ec->netwm.state.skip_taskbar) continue;
        if (ec->user_skip_winlist) continue;
        if (ec->iconic)
          {
             if (!e_config->winlist_list_show_iconified) continue;
             if ((ec->zone != zone) &&
                 (!e_config->winlist_list_show_other_screen_iconified))
               continue;
             if ((ec->desk != desk) &&
                 (!e_config->winlist_list_show_other_desk_iconified)) continue;
          }
        else
          {
             if (ec->sticky)
               {
                  if ((ec->zone != zone) &&
                      (!e_config->winlist_list_show_other_screen_windows))
                    continue;
               }
             else
               {
                  if (ec->desk != desk)
                    {
                       if ((ec->zone) && (ec->zone != zone))
                         {
                            if (!e_config->winlist_list_show_other_screen_windows)
                              continue;
                         }
                       else if (!e_config->winlist_list_show_other_desk_windows)
                         continue;
                    }
               }
          }
        /* ec is suitable */
        center_next = ec->y + ec->h / 2;
        if (center_next <= center) continue;
        delta_next = ec->y - (ec_orig->y + ec_orig->h);
        if (delta_next < 0) delta_next = center - center_next;
        delta2_next = abs(ec_orig->x - ec_orig->w / 2 - ec->x + ec->w/2);
        if (delta_next >= 0 && delta_next <= delta &&
            delta2_next >= 0 && delta2_next <= delta2)
          {
             _ec_next = ec;
             delta = delta_next;
             delta2 = delta2_next;
          }
     }

   if (_ec_next) _e_winlist_select(_ec_next);
}

void
e_winlist_up(E_Zone *zone)
{
   E_Client *ec;
   Eina_List *l;
   E_Desk *desk;
   E_Client *ec_orig;
   int delta = INT_MAX;
   int delta2 = INT_MAX;
   int center;

   _ec_next = NULL;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   ec_orig = e_client_focused_get();
   if (!ec_orig) return;

   center = ec_orig->y + ec_orig->h / 2;

   desk = e_desk_current_get(zone);
   EINA_LIST_FOREACH(e_client_focus_stack_get(), l, ec)
     {
        int center_next;
        int delta_next;
        int delta2_next;

        if (ec == ec_orig) continue;
        if ((!ec->icccm.accepts_focus) &&
            (!ec->icccm.take_focus)) continue;
        if (ec->netwm.state.skip_taskbar) continue;
        if (ec->user_skip_winlist) continue;
        if (ec->iconic)
          {
             if (!e_config->winlist_list_show_iconified) continue;
             if ((ec->zone != zone) &&
                 (!e_config->winlist_list_show_other_screen_iconified))
               continue;
             if ((ec->desk != desk) &&
                 (!e_config->winlist_list_show_other_desk_iconified)) continue;
          }
        else
          {
             if (ec->sticky)
               {
                  if ((ec->zone != zone) &&
                      (!e_config->winlist_list_show_other_screen_windows))
                    continue;
               }
             else
               {
                  if (ec->desk != desk)
                    {
                       if ((ec->zone) && (ec->zone != zone))
                         {
                            if (!e_config->winlist_list_show_other_screen_windows)
                              continue;
                         }
                       else if (!e_config->winlist_list_show_other_desk_windows)
                         continue;
                    }
               }
          }
        /* ec is suitable */
        center_next = ec->y + ec->h / 2;
        if (center_next >= center) continue;
        delta_next = ec_orig->y - (ec->y + ec->h);
        if (delta_next < 0) delta_next = center - center_next;
        delta2_next = abs(ec_orig->x - ec_orig->w / 2 - ec->x + ec->w/2);
        if (delta_next >= 0 && delta_next <= delta &&
            delta2_next >= 0 && delta2_next <= delta2)
          {
             _ec_next = ec;
             delta = delta_next;
             delta2 = delta2_next;
          }
     }

   if (_ec_next) _e_winlist_select(_ec_next);
}

void
e_winlist_right(E_Zone *zone)
{
   E_Client *ec;
   Eina_List *l;
   E_Desk *desk;
   E_Client *ec_orig;
   int delta = INT_MAX;
   int delta2 = INT_MAX;
   int center;

   _ec_next = NULL;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   ec_orig = e_client_focused_get();
   if (!ec_orig) return;

   center = ec_orig->x + ec_orig->w / 2;

   desk = e_desk_current_get(zone);
   EINA_LIST_FOREACH(e_client_focus_stack_get(), l, ec)
     {
        int center_next;
        int delta_next;
        int delta2_next;

        if (ec == ec_orig) continue;
        if ((!ec->icccm.accepts_focus) &&
            (!ec->icccm.take_focus)) continue;
        if (ec->netwm.state.skip_taskbar) continue;
        if (ec->user_skip_winlist) continue;
        if (ec->iconic)
          {
             if (!e_config->winlist_list_show_iconified) continue;
             if ((ec->zone != zone) &&
                 (!e_config->winlist_list_show_other_screen_iconified))
               continue;
             if ((ec->desk != desk) &&
                 (!e_config->winlist_list_show_other_desk_iconified)) continue;
          }
        else
          {
             if (ec->sticky)
               {
                  if ((ec->zone != zone) &&
                      (!e_config->winlist_list_show_other_screen_windows))
                    continue;
               }
             else
               {
                  if (ec->desk != desk)
                    {
                       if ((ec->zone) && (ec->zone != zone))
                         {
                            if (!e_config->winlist_list_show_other_screen_windows)
                              continue;
                         }
                       else if (!e_config->winlist_list_show_other_desk_windows)
                         continue;
                    }
               }
          }
        /* ec is suitable */
        center_next = ec->x + ec->w / 2;
        if (center_next <= center) continue;
        delta_next = ec->x - (ec_orig->x + ec_orig->w);
        if (delta_next < 0) delta = center_next - center;
        delta2_next = abs(ec_orig->y - ec_orig->h / 2 - ec->y + ec->h/2);
        if (delta_next >= 0 && delta_next <= delta &&
            delta2_next >= 0 && delta2_next <= delta2)
          {
             _ec_next = ec;
             delta = delta_next;
             delta2 = delta2_next;
          }
     }

   if (_ec_next) _e_winlist_select(_ec_next);
}

void
e_winlist_modifiers_set(int mod, E_Winlist_Activate_Type type)
{
   if (!_winlist) return;
   _hold_mod = mod;
   _hold_count = 0;
   _activate_type = type;
   if (type == E_WINLIST_ACTIVATE_TYPE_MOUSE) _hold_count++;
   if (_hold_mod & ECORE_EVENT_MODIFIER_SHIFT) _hold_count++;
   if (_hold_mod & ECORE_EVENT_MODIFIER_CTRL) _hold_count++;
   if (_hold_mod & ECORE_EVENT_MODIFIER_ALT) _hold_count++;
   if (_hold_mod & ECORE_EVENT_MODIFIER_WIN) _hold_count++;
}

/* local subsystem functions */
static void
_e_winlist_size_adjust(void)
{
   Evas_Coord mw, mh;
   E_Zone *zone;
   int x, y, w, h;

   elm_box_recalculate(_list_object);
   edje_object_part_swallow(_bg_object, "e.swallow.list", _list_object);
   edje_object_size_min_calc(_bg_object, &mw, &mh);
   evas_object_size_hint_min_set(_list_object, -1, -1);
   edje_object_part_swallow(_bg_object, "e.swallow.list", _list_object);

   zone = _winlist_zone;
   w = (double)zone->w * e_config->winlist_pos_size_w;
   if (w < mw) w = mw;
   if (w > e_config->winlist_pos_max_w) w = e_config->winlist_pos_max_w;
   else if (w < e_config->winlist_pos_min_w)
     w = e_config->winlist_pos_min_w;
   if (w > zone->w) w = zone->w;
   x = zone->x + (double)(zone->w - w) * e_config->winlist_pos_align_x;

   h = mh;
   if (h > e_config->winlist_pos_max_h) h = e_config->winlist_pos_max_h;
   else if (h < e_config->winlist_pos_min_h)
     h = e_config->winlist_pos_min_h;
   if (h > zone->h) h = zone->h;
   y = zone->y + (double)(zone->h - h) * e_config->winlist_pos_align_y;

   evas_object_geometry_set(_winlist, x, y, w, h);
}

static Eina_Bool
_e_winlist_client_add(E_Client *ec, E_Zone *zone, E_Desk *desk)
{
   E_Winlist_Win *ww;
   Evas_Coord mw, mh;
   Evas_Object *o;

   if ((!ec->icccm.accepts_focus) &&
       (!ec->icccm.take_focus)) return EINA_FALSE;
   if (ec->netwm.state.skip_taskbar) return EINA_FALSE;
   if (ec->user_skip_winlist) return EINA_FALSE;
   if (ec->iconic)
     {
        if (!e_config->winlist_list_show_iconified) return EINA_FALSE;
        if ((ec->zone != zone) &&
            (!e_config->winlist_list_show_other_screen_iconified)) return EINA_FALSE;
        if ((ec->desk != desk) &&
            (!e_config->winlist_list_show_other_desk_iconified)) return EINA_FALSE;
     }
   else
     {
        if (ec->sticky)
          {
             if ((ec->zone != zone) &&
                 (!e_config->winlist_list_show_other_screen_windows)) return EINA_FALSE;
          }
        else
          {
             if (ec->desk != desk)
               {
                  if ((ec->zone) && (ec->zone != zone))
                    {
                       if (!e_config->winlist_list_show_other_screen_windows)
                         return EINA_FALSE;
                       if (ec->zone && ec->desk && (ec->desk != e_desk_current_get(ec->zone)))
                         {
                            if (!e_config->winlist_list_show_other_desk_windows)
                              return EINA_FALSE;
                         }
                    }
                  else if (!e_config->winlist_list_show_other_desk_windows)
                    return EINA_FALSE;
               }
          }
     }

   ww = E_NEW(E_Winlist_Win, 1);
   if (!ww) return EINA_FALSE;
   ww->client = ec;
   _wins = eina_list_append(_wins, ww);
   o = edje_object_add(ec->comp->evas);
   E_FILL(o);
   e_comp_object_util_del_list_append(_winlist, o);
   ww->bg_object = o;
   e_theme_edje_object_set(o, "base/theme/winlist",
                           "e/widgets/winlist/item");
   edje_object_part_text_set(o, "e.text.label", e_client_util_name_get(ww->client));
   evas_object_show(o);
   if (edje_object_part_exists(ww->bg_object, "e.swallow.icon"))
     {
        o = e_client_icon_add(ec, ec->comp->evas);
        ww->icon_object = o;
        e_comp_object_util_del_list_append(_winlist, o);
        edje_object_part_swallow(ww->bg_object, "e.swallow.icon", o);
        evas_object_show(o);
     }
   if (ec->shaded)
     edje_object_signal_emit(ww->bg_object, "e,state,shaded", "e");
   else if (ec->iconic)
     edje_object_signal_emit(ww->bg_object, "e,state,iconified", "e");
   else if (ec->desk != desk)
     {
        if (!((ec->sticky) && (ec->zone == zone)))
          edje_object_signal_emit(ww->bg_object, "e,state,invisible", "e");
     }

   edje_object_size_min_calc(ww->bg_object, &mw, &mh);
   E_WEIGHT(ww->bg_object, 1, 0);
   E_FILL(ww->bg_object);
   evas_object_size_hint_min_set(ww->bg_object, mw, mh);
   evas_object_size_hint_max_set(ww->bg_object, 9999, mh);
   elm_box_pack_end(_list_object, ww->bg_object);
   e_object_ref(E_OBJECT(ww->client));
   return EINA_TRUE;
}

static void
_e_winlist_client_del(E_Client *ec)
{
   E_Winlist_Win *ww;
   Eina_List *l;

   if (ec == _last_client) _last_client = NULL;
   EINA_LIST_FOREACH(_wins, l, ww)
     {
        if (ww->client == ec)
          {
             e_object_unref(E_OBJECT(ww->client));
             if (l == _win_selected)
               {
                  _win_selected = l->next;
                  if (!_win_selected) _win_selected = l->prev;
                  _e_winlist_show_active();
                  _e_winlist_activate();
               }
             e_comp_object_util_del_list_remove(_winlist, ww->bg_object);
             evas_object_del(ww->bg_object);
             if (ww->icon_object)
               {
                  e_comp_object_util_del_list_remove(_winlist, ww->icon_object);
                  evas_object_del(ww->icon_object);
               }
             E_FREE(ww);
             _wins = eina_list_remove_list(_wins, l);
             return;
          }
     }
}

static void
_e_winlist_activate_nth(int n)
{
   Eina_List *l;
   int cnt;

   _e_winlist_deactivate();
   cnt = eina_list_count(_wins);
   if (n >= cnt) n = cnt - 1;
   l = eina_list_nth_list(_wins, n);
   if (!l) return;

   _win_selected = l;
   _e_winlist_show_active();
   _e_winlist_activate();
}

static void
_e_winlist_activate(void)
{
   E_Winlist_Win *ww;
   Evas_Object *o;
   int ok = 0;

   if (!_win_selected) return;
   ww = _win_selected->data;
   edje_object_signal_emit(ww->bg_object, "e,state,selected", "e");
   if (ww->icon_object && e_icon_edje_get(ww->icon_object))
     edje_object_signal_emit(e_icon_edje_get(ww->icon_object),
                             "e,state,selected", "e");

   if ((ww->client->iconic) &&
       (e_config->winlist_list_uncover_while_selecting))
     {
        if (!ww->client->lock_user_iconify)
          e_client_uniconify(ww->client);
        ww->was_iconified = 1;
        ok = 1;
     }
   if ((!ww->client->sticky) &&
       (ww->client->desk != e_desk_current_get(_winlist_zone)) &&
       (e_config->winlist_list_jump_desk_while_selecting))
     {
        if (ww->client->desk) e_desk_show(ww->client->desk);
        ok = 1;
     }
   if ((ww->client->shaded || ww->client->shading) &&
       (ww->client->desk == e_desk_current_get(_winlist_zone)) &&
       (e_config->winlist_list_uncover_while_selecting))
     {
        if (!ww->client->lock_user_shade)
          e_client_unshade(ww->client, ww->client->shade_dir);
        ww->was_shaded = 1;
        ok = 1;
     }
   if ((!ww->client->iconic) &&
       ((ww->client->desk == e_desk_current_get(_winlist_zone)) ||
        (ww->client->sticky)))
     ok = 1;
   if (ok)
     {
        int set = 1;
        if (e_config->winlist_warp_while_selecting)
          {
            if (!e_client_pointer_warp_to_center_now(ww->client))
              {
                 evas_object_focus_set(ww->client->frame, 1);
                 set = 0;
              }
          }

        if ((!ww->client->lock_user_stacking) &&
            (e_config->winlist_list_raise_while_selecting))
          evas_object_raise(ww->client->frame);
        if ((!ww->client->lock_focus_out) &&
            (e_config->winlist_list_focus_while_selecting))
          {
             if (set)
               evas_object_focus_set(ww->client->frame, 1);
          }
     }
   edje_object_part_text_set(_bg_object, "e.text.label",
                             e_client_util_name_get(ww->client));
   if (_icon_object)
     {
        e_comp_object_util_del_list_remove(_winlist, _icon_object);
        evas_object_del(_icon_object);
        _icon_object = NULL;
     }
   if (edje_object_part_exists(_bg_object, "e.swallow.icon"))
     {
        o = e_client_icon_add(ww->client, evas_object_evas_get(_winlist));
        _icon_object = o;
        e_comp_object_util_del_list_append(_winlist, o);
        edje_object_part_swallow(_bg_object, "e.swallow.icon", o);
        evas_object_show(o);
     }

   edje_object_signal_emit(_bg_object, "e,state,selected", "e");
}

static void
_e_winlist_deactivate(void)
{
   E_Winlist_Win *ww;

   if (!_win_selected) return;
   ww = _win_selected->data;
   if (ww->was_shaded)
     {
        if (!ww->client->lock_user_shade)
          e_client_shade(ww->client, ww->client->shade_dir);
     }
   if (ww->was_iconified)
     {
        if (!ww->client->lock_user_iconify)
          e_client_iconify(ww->client);
     }
   ww->was_shaded = 0;
   ww->was_iconified = 0;
   edje_object_part_text_set(_bg_object, "e.text.label", "");
   edje_object_signal_emit(ww->bg_object, "e,state,unselected", "e");
   if (ww->icon_object && e_icon_edje_get(ww->icon_object))
     edje_object_signal_emit(e_icon_edje_get(ww->icon_object),
                             "e,state,unselected", "e");
   if (!ww->client->lock_focus_in)
     evas_object_focus_set(ww->client->frame, 0);
}

static void
_e_winlist_show_active(void)
{
   Eina_List *l;
   int i, n;

   if (!_wins) return;

   for (i = 0, l = _wins; l; l = l->next, i++)
     if (l == _win_selected) break;

   n = eina_list_count(_wins);
   if (n <= 1) return;
   _scroll_align_to = (double)i / (double)(n - 1);
   if (e_config->winlist_scroll_animate)
     {
        _scroll_to = 1;
        if (!_scroll_timer)
          _scroll_timer = ecore_timer_add(0.01, _e_winlist_scroll_timer, NULL);
        if (!_animator)
          _animator = ecore_animator_add(_e_winlist_animator, NULL);
     }
   else
     {
        _scroll_align = _scroll_align_to;
        elm_box_align_set(_list_object, 0.5, fabs(1.0 - _scroll_align));
     }
}

static void
_e_winlist_restore_desktop(void)
{
   if (_last_desk &&
       (e_config->winlist_list_show_other_desk_windows ||
        e_config->winlist_list_show_other_screen_windows))
     e_desk_show(_last_desk);
   if (e_config->winlist_warp_while_selecting)
     ecore_evas_pointer_warp(_winlist_zone->comp->ee,
                          _last_pointer_x, _last_pointer_y);
   _e_winlist_deactivate();
   _win_selected = NULL;
   e_winlist_hide();
   if (_last_client)
     {
        evas_object_focus_set(_last_client->frame, 1);
        _last_client = NULL;
     }
}

static Eina_Bool
_e_winlist_cb_event_border_add(void *data __UNUSED__, int type __UNUSED__,
                               void *event)
{
   E_Event_Client *ev = event;

   if (_e_winlist_client_add(ev->ec, _winlist_zone,
                         e_desk_current_get(_winlist_zone)))
     _e_winlist_size_adjust();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_winlist_cb_event_border_remove(void *data __UNUSED__, int type __UNUSED__,
                                  void *event)
{
   E_Event_Client *ev = event;

   _e_winlist_client_del(ev->ec);
   _e_winlist_size_adjust();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_winlist_cb_key_down(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Key *ev;

   ev = event;
   if (ev->window != _input_window) return ECORE_CALLBACK_PASS_ON;
   if (!strcmp(ev->key, "Up"))
     e_winlist_prev();
   else if (!strcmp(ev->key, "Down"))
     e_winlist_next();
   else if (!strcmp(ev->key, "Left"))
     e_winlist_prev();
   else if (!strcmp(ev->key, "Right"))
     e_winlist_next();
   else if (!strcmp(ev->key, "Return"))
     e_winlist_hide();
   else if (!strcmp(ev->key, "space"))
     e_winlist_hide();
   else if (!strcmp(ev->key, "Escape"))
     _e_winlist_restore_desktop();
   else if (!strcmp(ev->key, "1"))
     _e_winlist_activate_nth(0);
   else if (!strcmp(ev->key, "2"))
     _e_winlist_activate_nth(1);
   else if (!strcmp(ev->key, "3"))
     _e_winlist_activate_nth(2);
   else if (!strcmp(ev->key, "4"))
     _e_winlist_activate_nth(3);
   else if (!strcmp(ev->key, "5"))
     _e_winlist_activate_nth(4);
   else if (!strcmp(ev->key, "6"))
     _e_winlist_activate_nth(5);
   else if (!strcmp(ev->key, "7"))
     _e_winlist_activate_nth(6);
   else if (!strcmp(ev->key, "8"))
     _e_winlist_activate_nth(7);
   else if (!strcmp(ev->key, "9"))
     _e_winlist_activate_nth(8);
   else if (!strcmp(ev->key, "0"))
     _e_winlist_activate_nth(9);
   else
     {
        Eina_List *l;
        E_Config_Binding_Key *binding;
        E_Binding_Modifier mod;

        EINA_LIST_FOREACH(e_bindings->key_bindings, l, binding)
          {
             if (binding->action != _winlist_act) continue;

             mod = 0;

             if (ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT)
               mod |= E_BINDING_MODIFIER_SHIFT;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL)
               mod |= E_BINDING_MODIFIER_CTRL;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_ALT)
               mod |= E_BINDING_MODIFIER_ALT;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_WIN)
               mod |= E_BINDING_MODIFIER_WIN;

             if (binding->key && ((!strcmp(binding->key, ev->key)) || (!strcmp(binding->key, ev->keyname))) &&
                 ((binding->modifiers == mod) || (binding->any_mod)))
               {
                  if (!_act_winlist) continue;
                  if (_act_winlist->func.go_key)
                    _act_winlist->func.go_key(E_OBJECT(_winlist_zone), binding->params, ev);
                  else if (_act_winlist->func.go)
                    _act_winlist->func.go(E_OBJECT(_winlist_zone), binding->params);
               }
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_winlist_cb_key_up(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Key *ev;
   Eina_List *l;
   E_Config_Binding_Key *binding;
   E_Binding_Modifier mod;

   ev = event;
   if (!_winlist) return ECORE_CALLBACK_PASS_ON;
   if (_hold_mod)
     {
#define KEY_CHECK(MOD, NAME) \
        if ((_hold_mod & ECORE_EVENT_MODIFIER_##MOD) && (!strcmp(ev->key, NAME))) \
          _hold_count--, _hold_mod &= ~ECORE_EVENT_MODIFIER_##MOD
        KEY_CHECK(SHIFT, "Shift_L");
        else KEY_CHECK(SHIFT, "Shift_R");
        else KEY_CHECK(CTRL, "Control_L");
        else KEY_CHECK(CTRL, "Control_R");
        else KEY_CHECK(ALT, "Alt_L");
        else KEY_CHECK(ALT, "Alt_R");
        else KEY_CHECK(ALT, "Meta_L");
        else KEY_CHECK(ALT, "Meta_R");
        else KEY_CHECK(WIN, "Meta_L");
        else KEY_CHECK(WIN, "Meta_R");
        else KEY_CHECK(ALT, "Super_L");
        else KEY_CHECK(ALT, "Super_R");
        else KEY_CHECK(WIN, "Super_L");
        else KEY_CHECK(WIN, "Super_R");
        else KEY_CHECK(WIN, "Mode_switch");

        if ((_hold_count <= 0) || ((!_hold_mod) && (_activate_type == E_WINLIST_ACTIVATE_TYPE_KEY)))
          {
             e_winlist_hide();
             return 1;
          }
     }

   EINA_LIST_FOREACH(e_bindings->key_bindings, l, binding)
     {
        if (binding->action != _winlist_act) continue;
        mod = 0;

        if (ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT)
          mod |= E_BINDING_MODIFIER_SHIFT;
        if (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL)
          mod |= E_BINDING_MODIFIER_CTRL;
        if (ev->modifiers & ECORE_EVENT_MODIFIER_ALT)
          mod |= E_BINDING_MODIFIER_ALT;
        if (ev->modifiers & ECORE_EVENT_MODIFIER_WIN)
          mod |= E_BINDING_MODIFIER_WIN;

        if (binding->key && (!strcmp(binding->key, ev->key)) &&
            ((binding->modifiers == mod) || (binding->any_mod)))
          {
             if (!_act_winlist) continue;
             if (_act_winlist->func.end_key)
               _act_winlist->func.end_key(E_OBJECT(_winlist_zone), binding->params, ev);
             else if (_act_winlist->func.end)
               _act_winlist->func.end(E_OBJECT(_winlist_zone), binding->params);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_winlist_cb_mouse_down(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Button *ev;

   ev = event;
   if (ev->window != _input_window) return ECORE_CALLBACK_PASS_ON;
   e_bindings_mouse_down_ecore_event_handle(E_BINDING_CONTEXT_WINLIST,
                                      E_OBJECT(_winlist_zone), ev);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_winlist_cb_mouse_up(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Button *ev;

   ev = event;
   if (ev->window != _input_window) return ECORE_CALLBACK_PASS_ON;
   if (e_bindings_mouse_up_ecore_event_handle(E_BINDING_CONTEXT_WINLIST, E_OBJECT(_winlist_zone), ev))
     return ECORE_CALLBACK_RENEW;
   if (_activate_type != E_WINLIST_ACTIVATE_TYPE_MOUSE) return ECORE_CALLBACK_RENEW;
   if (!--_hold_count) e_winlist_hide();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_winlist_cb_mouse_wheel(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Wheel *ev;
   int i;

   ev = event;
   if (ev->window != _input_window) return ECORE_CALLBACK_PASS_ON;
   e_bindings_wheel_ecore_event_handle(E_BINDING_CONTEXT_WINLIST,
                                 E_OBJECT(_winlist_zone), ev);
   if (ev->z < 0) /* up */
     {
        for (i = ev->z; i < 0; i++)
          e_winlist_prev();
     }
   else if (ev->z > 0) /* down */
     {
        for (i = ev->z; i > 0; i--)
          e_winlist_next();
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_winlist_cb_mouse_move(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Event_Mouse_Move *ev;
   int x, y, w, h;

   ev = event;
   if (ev->window != _input_window) return ECORE_CALLBACK_PASS_ON;
   evas_object_geometry_get(_winlist, &x, &y, &w, &h);
   /* only feed mouse move if it's within the winlist popup */
   if (E_INSIDE(ev->x, ev->y, x, y, w, h))
     evas_event_feed_mouse_move(evas_object_evas_get(_winlist), ev->x, ev->y, ev->timestamp, NULL);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_winlist_scroll_timer(void *data __UNUSED__)
{
   if (_scroll_to)
     {
        double spd;

        spd = e_config->winlist_scroll_speed;
        _scroll_align = (_scroll_align * (1.0 - spd)) +
          (_scroll_align_to * spd);
        return 1;
     }
   _scroll_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_winlist_animator(void *data __UNUSED__)
{
   if (_scroll_to)
     {
        double da;

        da = _scroll_align - _scroll_align_to;
        if (da < 0.0) da = -da;
        if (da < 0.01)
          {
             _scroll_align = _scroll_align_to;
             _scroll_to = 0;
          }
        elm_box_align_set(_list_object, 0.5, fabs(1.0 - _scroll_align));
     }
   if (!_scroll_to) _animator = NULL;
   return _scroll_to;
}

#if 0
static void
_e_winlist_cb_item_mouse_in(void *data, Evas *evas, Evas_Object *obj, void *event_info)
{
   E_Winlist_Win *ww;
   E_Winlist_Win *lww;
   Eina_List *l;

   if (!(ww = data)) return;
   if (!_wins) return;
   EINA_LIST_FOREACH(_wins, l, lww)
     if (lww == ww) break;
   _e_winlist_deactivate();
   _win_selected = l;
   _e_winlist_show_active();
   _e_winlist_activate();
}

#endif
