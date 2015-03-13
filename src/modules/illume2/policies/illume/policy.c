#include "e_illume.h"
#include "policy.h"

/* NB: DIALOG_USES_PIXEL_BORDER is an experiment in setting dialog windows 
 * to use the 'pixel' type border. This is done because some dialogs, 
 * when shown, blend into other windows too much. Pixel border adds a 
 * little distinction between the dialog window and an app window.
 * Disable if this is not wanted */
#define DIALOG_USES_PIXEL_BORDER 1

/* local function prototypes */
static void _policy_border_set_focus(E_Client *ec);
static void _policy_border_move(E_Client *ec, int x, int y);
static void _policy_border_resize(E_Client *ec, int w, int h);
static void _policy_border_hide_above(E_Client *ec);
static void _policy_border_hide_below(E_Client *ec);
static void _policy_border_show_below(E_Client *ec);
static void _policy_zone_layout_update(E_Zone *zone);
static void _policy_zone_layout_indicator(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_quickpanel(E_Client *ec);
static void _policy_zone_layout_softkey(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_keyboard(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_home_single(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_home_dual_top(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_home_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_home_dual_left(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_fullscreen(E_Client *ec);
static void _policy_zone_layout_app_single(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_app_dual_top(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_app_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_app_dual_left(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_dialog(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_splash(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_conformant_single(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_conformant_dual_top(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_conformant_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_conformant_dual_left(E_Client *ec, E_Illume_Config_Zone *cz);

static Eina_List *_pol_focus_stack;

/* local functions */
static void 
_policy_border_set_focus(E_Client *ec) 
{
   if (!ec) return;

   /* if focus is locked out then get out */
   if (ec->lock_focus_out) return;

   /* make sure the border can accept or take focus */
   if ((ec->icccm.accepts_focus) || (ec->icccm.take_focus)) 
     {
        /* check E's focus settings */
        if ((e_config->focus_setting == E_FOCUS_NEW_WINDOW) || 
            ((ec->parent) && 
             ((e_config->focus_setting == E_FOCUS_NEW_DIALOG) || 
              ((ec->parent->focused) && 
               (e_config->focus_setting == E_FOCUS_NEW_DIALOG_IF_OWNER_FOCUSED))))) 
          {
             /* if the border was hidden due to layout, we need to unhide */
	     if (!ec->visible) e_illume_client_show(ec);

             if ((ec->iconic) && (!ec->lock_user_iconify)) 
                e_client_uniconify(ec);
             
             if (!ec->lock_user_stacking) evas_object_raise(ec->frame);

             /* focus the border */
             evas_object_focus_set(ec->frame, 1);

             /* hide the border below this one */
             _policy_border_hide_below(ec);

             /* NB: since we skip needless border evals when container layout 
              * is called (to save cpu cycles), we need to 
              * signal this border that it's focused so that the edj gets 
              * updated. 
              * 
              * This is potentially useless as THIS policy 
              * makes all windows borderless anyway, but it's in here for 
              * completeness
             e_client_focus_latest_set(ec);
             if (ec->bg_object) 
               edje_object_signal_emit(ec->bg_object, "e,state,focused", "e");
             if (ec->icon_object) 
               edje_object_signal_emit(ec->icon_object, "e,state,focused", "e");
             e_focus_event_focus_in(ec);
              */
          }
     }
}

static void 
_policy_border_move(E_Client *ec, int x, int y) 
{
   if (!ec) return;

   /* NB: Qt uses a weird window type called 'VCLSalFrame' that needs to 
    * have ec->placed set else it doesn't position correctly...
    * this could be a result of E honoring the icccm request position, 
    * not sure */

   /* NB: Seems something similar happens with elementary windows also
    * so for now just set ec->placed on all windows until this 
    * gets investigated */
   ec->placed = 1;
   ec->x = x;
   ec->y = y;
   ec->changes.pos = 1;
   EC_CHANGED(ec);
}

static void 
_policy_border_resize(E_Client *ec, int w, int h) 
{
   if (!ec) return;

   ec->w = w;
   ec->h = h;
   e_comp_object_frame_wh_unadjust(ec->frame, ec->w, ec->h, &ec->client.w, &ec->client.h);
   ec->changes.size = 1;
   EC_CHANGED(ec);
}

static void 
_policy_border_hide_above(E_Client *ec)
{
   E_Client *b;

   if (!ec) return;

   E_CLIENT_REVERSE_FOREACH(b)
     {
        if (e_client_util_ignored_get(b)) continue;
        if (b->layer <= ec->layer) break;
        /* skip if it's the same border */
        if (b == ec) continue;

        /* skip if it's not on this zone */
        if (b->zone != ec->zone) continue;

        /* skip special borders */
        if (e_illume_client_is_indicator(b)) continue;
        if (e_illume_client_is_softkey(b)) continue;
        if (e_illume_client_is_keyboard(b)) continue;
        if (e_illume_client_is_quickpanel(b)) continue;
        if (e_illume_client_is_home(b)) continue;

        e_client_iconify(b);
     }
}

static void 
_policy_border_hide_below(E_Client *ec) 
{
   E_Client *b;

   if (!ec) return;

   E_CLIENT_FOREACH(b)
     {
        if (e_client_util_ignored_get(b)) continue;
        /* break if it's the same client */
        if (b == ec) break;

        /* skip if it's not on this zone */
        if (b->zone != ec->zone) continue;

        /* skip special borders */
        if (e_illume_client_is_indicator(b)) continue;
        if (e_illume_client_is_softkey(b)) continue;
        if (e_illume_client_is_keyboard(b)) continue;
        if (e_illume_client_is_quickpanel(b)) continue;
        if (e_illume_client_is_home(b)) continue;

        if ((ec->fullscreen) || (ec->need_fullscreen)) 
          {
             if (b->visible) e_illume_client_hide(b);
          }
        else 
          {
             /* we need to check x/y position */
             if (E_CONTAINS(ec->x, ec->y, ec->w, ec->h, 
                            b->x, b->y, b->w, b->h))
               {
                  if (b->visible) e_illume_client_hide(b);
               }
          }
     }
}

static void 
_policy_border_show_below(E_Client *ec) 
{
   Eina_List *l;
   E_Client *prev, *b;

//   printf("Show Borders Below: %s %d %d\n", 
//          ec->icccm.class, ec->x, ec->y);

   if (!ec) return;

   if (ec->icccm.transient_for) 
     {
        if ((prev = e_pixmap_find_client(E_PIXMAP_TYPE_X, ec->icccm.transient_for))) 
          {
             _policy_border_set_focus(prev);
             return;
          }
     }

    E_CLIENT_FOREACH(b)
     {
        if (e_client_util_ignored_get(b)) continue;
        /* break if it's the same border */
        if (b == ec) break;

        /* skip if it's not on this zone */
        if (b->zone != ec->zone) continue;

        /* skip special borders */
        if (e_illume_client_is_indicator(b)) continue;
        if (e_illume_client_is_softkey(b)) continue;
        if (e_illume_client_is_keyboard(b)) continue;
        if (e_illume_client_is_quickpanel(b)) continue;
        if (e_illume_client_is_home(b)) continue;

        if ((ec->fullscreen) || (ec->need_fullscreen)) 
          {
             _policy_border_set_focus(b);
             return;
          }
        else 
          {
             /* need to check x/y position */
             if (E_CONTAINS(ec->x, ec->y, ec->w, ec->h, 
                            b->x, b->y, b->w, b->h))
               {
                  _policy_border_set_focus(b);
                  return;
               }
          }
     }

   /* if we reach here, then there is a problem with showing a window below
    * this one, so show previous window in stack */
   EINA_LIST_REVERSE_FOREACH(_pol_focus_stack, l, prev) 
     {
        if (prev->zone != ec->zone) continue;
        _policy_border_set_focus(prev);
        return;
     }

   /* Fallback to focusing home if all above fails */
   _policy_focus_home(ec->zone);
}

static void 
_policy_zone_layout_update(E_Zone *zone) 
{
   Eina_List *l;
   E_Client *ec;

   if (!zone) return;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        /* skip borders not on this zone */
        if (ec->zone != zone) continue;

        /* skip special windows */
        if (e_object_is_del(E_OBJECT(ec))) continue;
        if (e_illume_client_is_keyboard(ec)) continue;
        if (e_illume_client_is_quickpanel(ec)) continue;

        /* signal a changed pos here so layout gets updated */
        ec->changes.pos = 1;
        EC_CHANGED(ec);
     }
}

static void 
_policy_zone_layout_indicator(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   if ((!ec) || (!cz)) return;

   /* grab minimum indicator size */
   e_illume_client_min_get(ec, NULL, &cz->indicator.size);

   /* no point in doing anything here if indicator is hidden */
   if ((!ec->new_client) && (!ec->visible))
      {
         ecore_x_e_illume_indicator_geometry_set(ec->zone->black_win,
                                                 0, 0, 0, 0);
         return;
      }

   /* if we are dragging, then skip it for now */
   if (ec->illume.drag.drag) 
     {
        /* when dragging indicator, we need to trigger a layout update */
         ecore_x_e_illume_indicator_geometry_set(ec->zone->black_win,
                                                 0, 0, 0, 0);
        _policy_zone_layout_update(ec->zone);
        return;
     }

//   printf("\tLayout Indicator: %d\n", ec->zone->num);

   /* lock indicator window from dragging if we need to */
   if ((cz->mode.dual == 1) && (cz->mode.side == 0)) 
     ecore_x_e_illume_drag_locked_set(e_client_util_win_get(ec), 0);
   else 
     ecore_x_e_illume_drag_locked_set(e_client_util_win_get(ec), 1);

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != cz->indicator.size))
     _policy_border_resize(ec, ec->zone->w, cz->indicator.size);

   /* are we in single mode ? */
   if (!cz->mode.dual) 
     {
        /* move to 0, 0 (relative to zone) if needed */
        if ((ec->x != ec->zone->x) || (ec->y != ec->zone->y)) 
          {
             _policy_border_move(ec, ec->zone->x, ec->zone->y);
             ecore_x_e_illume_quickpanel_position_update_send(e_client_util_win_get(ec));
          }
     }
   else 
     {
        /* dual app mode top */
        if (cz->mode.side == 0) 
          {
             /* top mode...indicator is draggable so just set X.
              * in this case, the indicator itself should be responsible for 
              * sending the quickpanel position update message when it is 
              * finished dragging */
             if (ec->x != ec->zone->x) 
               _policy_border_move(ec, ec->zone->x, ec->y);
          }
        else 
          {
             /* move to 0, 0 (relative to zone) if needed */
             if ((ec->x != ec->zone->x) || (ec->y != ec->zone->y)) 
               {
                  _policy_border_move(ec, ec->zone->x, ec->zone->y);
                  ecore_x_e_illume_quickpanel_position_update_send(e_client_util_win_get(ec));
               }
          }
     }
   ecore_x_e_illume_indicator_geometry_set(ec->zone->black_win,
                                           ec->x, ec->y,
                                           ec->w, ec->h);

   /* set layer if needed */
   if (ec->layer != POL_INDICATOR_LAYER) 
     evas_object_layer_set(ec->frame, POL_INDICATOR_LAYER);
}

static void 
_policy_zone_layout_quickpanel(E_Client *ec) 
{
   int mh;

   if (!ec) return;

   /* grab minimum size */
   e_illume_client_min_get(ec, NULL, &mh);

   /* resize if needed */
   if ((ec->w != ec->zone->w) || (ec->h != mh))
     _policy_border_resize(ec, ec->zone->w, mh);

   /* set layer if needed */
   if (ec->layer != POL_QUICKPANEL_LAYER) 
     evas_object_layer_set(ec->frame, POL_QUICKPANEL_LAYER);
}

static void 
_policy_zone_layout_softkey(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   int ny;

   if (!ec) return;

   /* grab minimum softkey size */
   e_illume_client_min_get(ec, NULL, &cz->softkey.size);

   /* no point in doing anything here if softkey is hidden */
   if (!ec->visible)
     {
        ecore_x_e_illume_softkey_geometry_set(ec->zone->black_win,
                                              0, 0, 0, 0);
        return;
     }

   /* if we are dragging, then skip it for now */
   /* NB: Disabled currently until we confirm that softkey should be draggable */
//   if (ec->illume.drag.drag) return;

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != cz->softkey.size))
     _policy_border_resize(ec, ec->zone->w, cz->softkey.size);

   /* make sure it's in the correct position */
   ny = ((ec->zone->y + ec->zone->h) - cz->softkey.size);

   /* NB: not sure why yet, but on startup the border->y is reporting 
    * that it is already in this position...but it's actually not. 
    * So for now, just disable the ny check until this gets sorted out */
//   if ((ec->x != ec->zone->x) || (ec->y != ny))
     _policy_border_move(ec, ec->zone->x, ny);
   // set property for apps to find out they are
   ecore_x_e_illume_softkey_geometry_set(ec->zone->black_win,
                                         ec->x, ec->y,
                                         ec->w, ec->h);

   /* set layer if needed */
   if (ec->layer != POL_SOFTKEY_LAYER) 
     evas_object_layer_set(ec->frame, POL_SOFTKEY_LAYER);
}

static void 
_policy_zone_layout_keyboard(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   int ny, layer;

//   printf("\tLayout Keyboard\n");

   if ((!ec) || (!cz)) return;

   /* no point in adjusting size or position if it's not visible */
   if (!ec->visible) return;

   /* grab minimum keyboard size */
   e_illume_client_min_get(ec, NULL, &cz->vkbd.size);

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != cz->vkbd.size))
     _policy_border_resize(ec, ec->zone->w, cz->vkbd.size);

   /* make sure it's in the correct position */
   ny = ((ec->zone->y + ec->zone->h) - cz->vkbd.size);
   if ((ec->x != ec->zone->x) || (ec->y != ny)) 
     _policy_border_move(ec, ec->zone->x, ny);

   /* check layer according to fullscreen state */
   if ((ec->fullscreen) || (ec->need_fullscreen)) 
     layer = POL_FULLSCREEN_LAYER;
   else
     layer = POL_KEYBOARD_LAYER;

   /* set layer if needed */
   if ((int)ec->layer != layer) evas_object_layer_set(ec->frame, layer);
}

static void 
_policy_zone_layout_home_single(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   int ny, nh, indsz = 0, sftsz = 0;
   E_Client *ind, *sft;

   if ((!ec) || (!cz)) return;

   /* no point in adjusting size or position if it's not visible */
   if (!ec->visible) return;

//   printf("\tLayout Home Single: %s\n", ec->icccm.class);

   indsz = cz->indicator.size;
   sftsz = cz->softkey.size;
   if ((ind = e_illume_client_indicator_get(ec->zone)))
     {
        if (!ind->visible) indsz = 0;
     }
   if ((sft = e_illume_client_softkey_get(ec->zone)))
     {
        if (!sft->visible) sftsz = 0;
     }
   /* make sure it's the required width & height */
   if (e_illume_client_is_conformant(ec)) nh = ec->zone->h;
   else nh = (ec->zone->h - indsz - sftsz);
   
   if ((ec->w != ec->zone->w) || (ec->h != nh)) 
     _policy_border_resize(ec, ec->zone->w, nh);

   /* move to correct position (relative to zone) if needed */
   if (e_illume_client_is_conformant(ec)) ny = ec->zone->y;
   else ny = (ec->zone->y + indsz);
   if ((ec->x != ec->zone->x) || (ec->y != ny)) 
     _policy_border_move(ec, ec->zone->x, ny);

   /* set layer if needed */
   if (ec->layer != POL_HOME_LAYER) evas_object_layer_set(ec->frame, POL_HOME_LAYER);
}

static void 
_policy_zone_layout_home_dual_top(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   E_Client *home, *ind, *sft;
   int ny, nh, indsz = 0, sftsz = 0;

   if ((!ec) || (!cz)) return;

   /* no point in adjusting size or position if it's not visible */
   if (!ec->visible) return;

   indsz = cz->indicator.size;
   sftsz = cz->softkey.size;
   if ((ind = e_illume_client_indicator_get(ec->zone)))
     {
        if (!ind->visible) indsz = 0;
     }
   if ((sft = e_illume_client_softkey_get(ec->zone)))
     {
        if (!sft->visible) sftsz = 0;
     }
   /* set some defaults */
   ny = (ec->zone->y + indsz);
   nh = ((ec->zone->h - indsz - sftsz) / 2);

   /* see if there are any other home windows */
   home = e_illume_client_home_get(ec->zone);
   if (home) 
     {
        if (ec != home) ny = (home->y + nh);
     }

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != nh)) 
     _policy_border_resize(ec, ec->zone->w, nh);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != ec->zone->x) || (ec->y != ny)) 
     _policy_border_move(ec, ec->zone->x, ny);

   /* set layer if needed */
   if (ec->layer != POL_HOME_LAYER) evas_object_layer_set(ec->frame, POL_HOME_LAYER);
}

static void 
_policy_zone_layout_home_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   E_Client *home;
   int iy, ny, nh;

//   printf("\tLayout Home Dual Custom: %s\n", ec->icccm.class);

   if ((!ec) || (!cz)) return;

   /* no point in adjusting size or position if it's not visible */
   if (!ec->visible) return;

   /* grab indicator position */
   e_illume_client_indicator_pos_get(ec->zone, NULL, &iy);

   /* set some defaults */
   ny = ec->zone->y;
   nh = iy;

   /* see if there are any other home windows */
   home = e_illume_client_home_get(ec->zone);
   if ((home) && (ec != home))
     {
        ny = (iy + cz->indicator.size);
        nh = ((ec->zone->y + ec->zone->h) - ny - cz->softkey.size);
     }

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != nh)) 
     _policy_border_resize(ec, ec->zone->w, nh);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != ec->zone->x) || (ec->y != ny))
     _policy_border_move(ec, ec->zone->x, ny);

   /* set layer if needed */
   if (ec->layer != POL_HOME_LAYER) evas_object_layer_set(ec->frame, POL_HOME_LAYER);
}

static void 
_policy_zone_layout_home_dual_left(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   E_Client *home;
   int nx, nw, nh;

//   printf("\tLayout Home Dual Left: %s\n", ec->icccm.class);

   if ((!ec) || (!cz)) return;

   /* no point in adjusting size or position if it's not visible */
   if (!ec->visible) return;

   nh = (ec->zone->h - cz->indicator.size - cz->softkey.size);

   /* set some defaults */
   nx = ec->zone->x;
   nw = (ec->zone->w / 2);

   /* see if there are any other home windows */
   home = e_illume_client_home_get(ec->zone);
   if ((home) && (ec != home)) nx = (home->x + nw);

   /* make sure it's the required width & height */
   if ((ec->w != nw) || (ec->h != nh))
     _policy_border_resize(ec, nw, nh);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != nx) || (ec->y != (ec->zone->y + cz->indicator.size)))
     _policy_border_move(ec, nx, (ec->zone->y + cz->indicator.size));

   /* set layer if needed */
   if (ec->layer != POL_HOME_LAYER) evas_object_layer_set(ec->frame, POL_HOME_LAYER);
}

static void 
_policy_zone_layout_fullscreen(E_Client *ec) 
{
   int kh;

//   printf("\tLayout Fullscreen: %s\n", ec->icccm.name);

   if (!ec) return;

   /* grab keyboard safe region */
   e_illume_keyboard_safe_app_region_get(ec->zone, NULL, NULL, NULL, &kh);

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != kh))
     _policy_border_resize(ec, ec->zone->w, kh);

   /* set layer if needed */
   if (ec->layer != POL_FULLSCREEN_LAYER) 
     evas_object_layer_set(ec->frame, POL_FULLSCREEN_LAYER);
}

static void 
_policy_zone_layout_app_single(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   int ky, kh, ny, nh;

   if ((!ec) || (!cz)) return;
   if ((!ec->new_client) && (!ec->visible)) return;

//   printf("\tLayout App Single: %s\n", ec->icccm.name);

   /* grab keyboard safe region */
   e_illume_keyboard_safe_app_region_get(ec->zone, NULL, &ky, NULL, &kh);

   /* make sure it's the required width & height */
   if (kh >= ec->zone->h)
     nh = (kh - cz->indicator.size - cz->softkey.size);
   else
     nh = (kh - cz->indicator.size);

   /* resize if needed */
   if ((ec->w != ec->zone->w) || (ec->h != nh))
     _policy_border_resize(ec, ec->zone->w, nh);

   /* move to correct position (relative to zone) if needed */
   ny = (ec->zone->y + cz->indicator.size);
   if ((ec->x != ec->zone->x) || (ec->y != ny)) 
     _policy_border_move(ec, ec->zone->x, ny);

   /* set layer if needed */
   if (ec->layer != POL_APP_LAYER) evas_object_layer_set(ec->frame, POL_APP_LAYER);
}

static void 
_policy_zone_layout_app_dual_top(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   E_Client *b;
   int kh, ny, nh;

//   printf("\tLayout App Dual Top: %s\n", ec->icccm.name);

   if ((!ec) || (!cz)) return;
   if ((!ec->new_client) && (!ec->visible)) return;

   /* set a default Y position */
   ny = (ec->zone->y + cz->indicator.size);

   if ((ec->focused) && 
       (ec->vkbd.state > ECORE_X_VIRTUAL_KEYBOARD_STATE_OFF)) 
     {
        /* grab keyboard safe region */
        e_illume_keyboard_safe_app_region_get(ec->zone, NULL, NULL, NULL, &kh);
        nh = (kh - cz->indicator.size);
     }
   else 
     {
        /* make sure it's the required width & height */
        nh = ((ec->zone->h - cz->indicator.size - cz->softkey.size) / 2);
     }

   /* see if there is a border already there. if so, check placement based on 
    * virtual keyboard usage */
   b = e_illume_client_at_xy_get(ec->zone, ec->zone->x, ny);
   if ((b) && (b != ec))
     {
        /* does this border need keyboard ? */
        if ((ec->focused) && 
            (ec->vkbd.state > ECORE_X_VIRTUAL_KEYBOARD_STATE_OFF)) 
          {
             int h;

             /* move existing border to bottom if needed */
             h = ((ec->zone->h - cz->indicator.size - cz->softkey.size) / 2);
             if ((b->x != b->zone->x) || (b->y != (ny + h)))
               _policy_border_move(b, b->zone->x, (ny + h));

             /* resize existing border if needed */
             if ((b->w != b->zone->w) || (b->h != h))
               _policy_border_resize(b, b->zone->w, h);
          }
        else
          ny = b->y + nh;
     }

   /* resize if needed */
   if ((ec->w != ec->zone->w) || (ec->h != nh))
     _policy_border_resize(ec, ec->zone->w, nh);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != ec->zone->x) || (ec->y != ny)) 
     _policy_border_move(ec, ec->zone->x, ny);

   /* set layer if needed */
   if (ec->layer != POL_APP_LAYER) evas_object_layer_set(ec->frame, POL_APP_LAYER);
}

static void 
_policy_zone_layout_app_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   E_Client *app;
   int iy, ny, nh;

//   printf("\tLayout App Dual Custom: %s\n", ec->icccm.class);

   if ((!ec) || (!cz)) return;
   if ((!ec->new_client) && (!ec->visible)) return;

   /* grab indicator position */
   e_illume_client_indicator_pos_get(ec->zone, NULL, &iy);

   /* set a default position */
   ny = ec->zone->y;
   nh = iy;

   app = e_illume_client_at_xy_get(ec->zone, ec->zone->x, ec->zone->y);
   if ((app) && (ec != app))
     {
        ny = (iy + cz->indicator.size);
        nh = ((ec->zone->y + ec->zone->h) - ny - cz->softkey.size);
     }

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != nh)) 
     _policy_border_resize(ec, ec->zone->w, nh);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != ec->zone->x) || (ec->y != ny))
     _policy_border_move(ec, ec->zone->x, ny);

   /* set layer if needed */
   if (ec->layer != POL_APP_LAYER) evas_object_layer_set(ec->frame, POL_APP_LAYER);
}

static void 
_policy_zone_layout_app_dual_left(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   E_Client *b;
   int ky, kh, nx, nw;

//   printf("\tLayout App Dual Left: %s\n", ec->icccm.name);

   if ((!ec) || (!cz)) return;
   if ((!ec->new_client) && (!ec->visible)) return;

   /* grab keyboard safe region */
   e_illume_keyboard_safe_app_region_get(ec->zone, NULL, &ky, NULL, &kh);

   if (kh >= ec->zone->h)
     kh = (kh - cz->indicator.size - cz->softkey.size);
   else
     kh = (kh - cz->indicator.size);

   /* set some defaults */
   nx = ec->zone->x;
   nw = (ec->zone->w / 2);

   /* see if there is a border already there. if so, place at right */
   b = e_illume_client_at_xy_get(ec->zone, nx, (ky + cz->indicator.size));
   if ((b) && (ec != b)) nx = b->x + nw;

   /* resize if needed */
   if ((ec->w != nw) || (ec->h != kh))
     _policy_border_resize(ec, nw, kh);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != nx) || (ec->y != (ky + cz->indicator.size)))
     _policy_border_move(ec, nx, (ky + cz->indicator.size));

   /* set layer if needed */
   if (ec->layer != POL_APP_LAYER) evas_object_layer_set(ec->frame, POL_APP_LAYER);
}

static void 
_policy_zone_layout_dialog(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   E_Client *parent;
   int mw, mh, nx, ny;

//   printf("\tLayout Dialog: %s\n", ec->icccm.name);

   /* NB: This policy ignores any ICCCM requested positions and centers the 
    * dialog on it's parent (if it exists) or on the zone */

   if ((!ec) || (!cz)) return;

   mw = ec->w;
   mh = ec->h;

   /* make sure it fits in this zone */
   if (mw > ec->zone->w) mw = ec->zone->w;
   if (mh > ec->zone->h) mh = ec->zone->h;

   /* try to get this dialog's parent if it exists */
   parent = e_illume_client_parent_get(ec);

   /* if we have no parent, or we are in dual mode, then center on zone */
   /* NB: we check dual mode because if we are in dual mode, dialogs tend to 
    * be too small to be useful when positioned on the parent, so center 
    * on zone. We could check their size first here tho */
   if ((!parent) || (cz->mode.dual == 1)) 
     {
        /* no parent or dual mode, center on screen */
        nx = (ec->zone->x + ((ec->zone->w - mw) / 2));
        ny = (ec->zone->y + ((ec->zone->h - mh) / 2));
     }
   else 
     {
        /* NB: there is an assumption here that the parent has already been 
         * laid out on screen. This could be bad. Needs Testing */

        /* make sure we are not larger than the parent window */
        if (mw > parent->w) mw = parent->w;
        if (mh > parent->h) mh = parent->h;

        /* center on parent */
        nx = (parent->x + ((parent->w - mw) / 2));
        ny = (parent->y + ((parent->h - mh) / 2));
     }

   /* make sure it's the required width & height */
   if ((ec->w != mw) || (ec->h != mh))
     _policy_border_resize(ec, mw, mh);

   /* make sure it's in the correct position */
   if ((ec->x != nx) || (ec->y != ny)) 
     _policy_border_move(ec, nx, ny);

   /* set layer if needed */
   if (ec->layer != POL_DIALOG_LAYER) 
     evas_object_layer_set(ec->frame, POL_DIALOG_LAYER);
}

static void 
_policy_zone_layout_splash(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   E_Client *parent = NULL;
   int mw, mh, nx, ny;

   /* NB: This code is almost exactly the same as the dialog layout code 
    * (_policy_zone_layout_dialog) except for setting a different layer */

//   printf("\tLayout Splash: %s\n", ec->icccm.name);

   /* NB: This policy ignores any ICCCM requested positions and centers the 
    * splash screen on it's parent (if it exists) or on the zone */

   if ((!ec) || (!cz)) return;

   /* no point in adjusting size or position if it's not visible */
   if (!ec->visible) return;

   /* grab minimum size */
   e_illume_client_min_get(ec, &mw, &mh);

   /* make sure it fits in this zone */
   if (mw > ec->zone->w) mw = ec->zone->w;
   if (mh > ec->zone->h) mh = ec->zone->h;

   /* if we have no parent, or we are in dual mode, then center on zone */
   /* NB: we check dual mode because if we are in dual mode, dialogs tend to 
    * be too small to be useful when positioned on the parent, 
    * so center on zone instead */
   if ((!parent) || (cz->mode.dual == 1)) 
     {
        /* no parent or in dual mode, center on screen */
        nx = (ec->zone->x + ((ec->zone->w - mw) / 2));
        ny = (ec->zone->y + ((ec->zone->h - mh) / 2));
     }
   else 
     {
        /* NB: there is an assumption here that the parent has already been 
         * laid out on screen. This could be bad. Needs Testing */

        /* make sure we are not larger than the parent window */
        if (mw > parent->w) mw = parent->w;
        if (mh > parent->h) mh = parent->h;

        /* center on parent */
        nx = (parent->x + ((parent->w - mw) / 2));
        ny = (parent->y + ((parent->h - mh) / 2));
     }

   /* make sure it's the required width & height */
   if ((ec->w != mw) || (ec->h != mh))
     _policy_border_resize(ec, mw, mh);

   /* make sure it's in the correct position */
   if ((ec->x != nx) || (ec->y != ny)) 
     _policy_border_move(ec, nx, ny);

   /* set layer if needed */
   if (ec->layer != POL_SPLASH_LAYER) evas_object_layer_set(ec->frame, POL_SPLASH_LAYER);
}

static void 
_policy_zone_layout_conformant_single(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   if ((!ec) || (!cz)) return;
   if ((!ec->new_client) && (!ec->visible)) return;

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != ec->zone->h))
     _policy_border_resize(ec, ec->zone->w, ec->zone->h);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != ec->zone->x) || (ec->y != ec->zone->y))
     _policy_border_move(ec, ec->zone->x, ec->zone->y);

   /* set layer if needed */
   if (ec->layer != POL_CONFORMANT_LAYER) 
     evas_object_layer_set(ec->frame, POL_CONFORMANT_LAYER);
}

static void 
_policy_zone_layout_conformant_dual_top(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   int nh, ny;

   /* according to the docs I have, conformant windows are always on the 
    * bottom in dual-top mode */
   if ((!ec) || (!cz)) return;
   if ((!ec->new_client) && (!ec->visible)) return;

   /* set some defaults */
   nh = ((ec->zone->h - cz->indicator.size - cz->softkey.size) / 2);
   ny = (ec->zone->y + cz->indicator.size) + nh;
   nh += cz->softkey.size;

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != nh))
     _policy_border_resize(ec, ec->zone->w, nh);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != ec->zone->x) || (ec->y != ny))
     _policy_border_move(ec, ec->zone->x, ny);

   /* set layer if needed */
   if (ec->layer != POL_CONFORMANT_LAYER) 
     evas_object_layer_set(ec->frame, POL_CONFORMANT_LAYER);
}

static void 
_policy_zone_layout_conformant_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   int iy, nh;

//   printf("\tLayout Conformant Dual Custom: %s\n", ec->icccm.class);

   if ((!ec) || (!cz)) return;
   if ((!ec->new_client) && (!ec->visible)) return;

   /* grab indicator position */
   e_illume_client_indicator_pos_get(ec->zone, NULL, &iy);

   /* set some defaults */
   nh = ((ec->zone->y + ec->zone->h) - iy);

   /* make sure it's the required width & height */
   if ((ec->w != ec->zone->w) || (ec->h != nh)) 
     _policy_border_resize(ec, ec->zone->w, nh);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != ec->zone->x) || (ec->y != iy))
     _policy_border_move(ec, ec->zone->x, iy);

   /* set layer if needed */
   if (ec->layer != POL_CONFORMANT_LAYER) 
     evas_object_layer_set(ec->frame, POL_CONFORMANT_LAYER);
}

static void 
_policy_zone_layout_conformant_dual_left(E_Client *ec, E_Illume_Config_Zone *cz) 
{
   int nw, nx;

   /* according to the docs I have, conformant windows are always on the 
    * left in dual-left mode */
   if ((!ec) || (!cz)) return;
   if ((!ec->new_client) && (!ec->visible)) return;

   /* set some defaults */
   nw = (ec->zone->w / 2);
   nx = (ec->zone->x);

   /* make sure it's the required width & height */
   if ((ec->w != nw) || (ec->h != ec->zone->h))
     _policy_border_resize(ec, nw, ec->zone->h);

   /* move to correct position (relative to zone) if needed */
   if ((ec->x != nx) || (ec->y != ec->zone->y))
     _policy_border_move(ec, nx, ec->zone->y);

   /* set layer if needed */
   if (ec->layer != POL_CONFORMANT_LAYER) 
     evas_object_layer_set(ec->frame, POL_CONFORMANT_LAYER);
}


/* policy functions */
void 
_policy_border_add(E_Client *ec) 
{
   if (!ec) return;

//   printf("\nBorder added: %s\n", ec->icccm.class);

   /* NB: this call sets an atom on the window that specifices the zone.
    * the logic here is that any new windows created can access the zone 
    * window by a 'get' call. This is useful for elementary apps as they 
    * normally would not have access to the zone window. Typical use case 
    * is for indicator & softkey windows so that they can send messages 
    * that apply to their respective zone only. Example: softkey sends close 
    * messages (or back messages to cycle focus) that should pertain just 
    * to it's current zone */
   ecore_x_e_illume_zone_set(e_client_util_win_get(ec), ec->zone->black_win);

   /* ignore stolen borders. These are typically quickpanel or keyboards */
   if (ec->stolen) return;

   /* if this is a fullscreen window, than we need to hide indicator window */
   /* NB: we could use the e_illume_client_is_fullscreen function here 
    * but we save ourselves a function call this way */
   if ((ec->fullscreen) || (ec->need_fullscreen)) 
     {
        E_Client *ind, *sft;

        /* try to get the Indicator on this zone */
        if ((ind = e_illume_client_indicator_get(ec->zone))) 
          {
             /* we have the indicator, hide it if needed */
	     if (ind->visible) e_illume_client_hide(ind);
          }
        /* conformant - may not need softkey */
        if ((sft = e_illume_client_softkey_get(ec->zone)))
          {
             if (e_illume_client_is_conformant(ec)) 
               {
                  if (sft->visible) e_illume_client_hide(sft);
               }
             else 
               {
                  if (!sft->visible) e_illume_client_show(sft);
               }
          }
     }

   /* Add this border to our focus stack if it can accept or take focus */
   if ((ec->icccm.accepts_focus) || (ec->icccm.take_focus)) 
     _pol_focus_stack = eina_list_append(_pol_focus_stack, ec);

   if ((e_illume_client_is_softkey(ec)) || (e_illume_client_is_indicator(ec)))
     _policy_zone_layout_update(ec->zone);
   else 
     {
	/* set focus on new border if we can */
	_policy_border_set_focus(ec);
     }
}

void 
_policy_border_del(E_Client *ec) 
{
//   printf("Policy Border deleted: %s\n", ec->icccm.class);

   if (!ec) return;

   /* if this is a fullscreen window, than we need to show indicator window */
   /* NB: we could use the e_illume_client_is_fullscreen function here 
    * but we save ourselves a function call this way */
   if ((ec->fullscreen) || (ec->need_fullscreen)) 
     {
        E_Client *ind;

        /* try to get the Indicator on this zone */
        if ((ind = e_illume_client_indicator_get(ec->zone))) 
          {
             /* we have the indicator, show it if needed */
	     if (!ind->visible) e_illume_client_show(ind);
          }
        _policy_zone_layout_update(ec->zone);
     }

   /* remove from our focus stack */
   if ((ec->icccm.accepts_focus) || (ec->icccm.take_focus)) 
     _pol_focus_stack = eina_list_remove(_pol_focus_stack, ec);

   if (e_illume_client_is_softkey(ec)) 
     {
	E_Illume_Config_Zone *cz;

	/* get the config for this zone */
	cz = e_illume_zone_config_get(ec->zone->num);
	cz->softkey.size = 0;
	_policy_zone_layout_update(ec->zone);
     }
   else if (e_illume_client_is_indicator(ec)) 
     {
	E_Illume_Config_Zone *cz;

	/* get the config for this zone */
	cz = e_illume_zone_config_get(ec->zone->num);
	cz->indicator.size = 0;
	_policy_zone_layout_update(ec->zone);
     }
   else 
     {
	/* show the border below this one */
	_policy_border_show_below(ec);
     }
}

void 
_policy_border_focus_in(E_Client *ec) 
{
   E_Client *ind;

//   printf("Border focus in: %s\n", ec->icccm.name);
   if ((ec->fullscreen) || (ec->need_fullscreen)) 
     {
        /* try to get the Indicator on this zone */
        if ((ind = e_illume_client_indicator_get(ec->zone))) 
          {
             /* we have the indicator, hide it if needed */
	     if (ind->visible) e_illume_client_hide(ind);
          }
     }
   else
     {
        /* try to get the Indicator on this zone */
        if ((ind = e_illume_client_indicator_get(ec->zone))) 
          {
             /* we have the indicator, show it if needed */
	     if (!ind->visible) e_illume_client_show(ind);
          }
     }
   _policy_zone_layout_update(ec->zone);
}

void 
_policy_border_focus_out(E_Client *ec) 
{
   if (!ec) return;

//   printf("Border focus out: %s\n", ec->icccm.name);

   /* NB: if we got this focus_out event on a deleted border, we check if 
    * it is a transient (child) of another window. If it is, then we 
    * transfer focus back to the parent window */
   if (e_object_is_del(E_OBJECT(ec))) 
     {
        if (e_illume_client_is_dialog(ec)) 
          {
             E_Client *parent;

             if ((parent = e_illume_client_parent_get(ec)))
               _policy_border_set_focus(parent);
          }
     }
}

void 
_policy_border_activate(E_Client *ec) 
{
   E_Client *sft;

   if (!ec) return;

   printf("Border Activate: %s\n", ec->icccm.name);

   /* NB: stolen borders may or may not need focus call...have to test */
   if (ec->stolen) return;

   /* conformant windows hide the softkey */
   if ((sft = e_illume_client_softkey_get(ec->zone)))
     {
        if (e_illume_client_is_conformant(ec)) 
          {
	     if (sft->visible) e_illume_client_hide(sft);
          }
        else 
          {
	     if (!sft->visible) e_illume_client_show(sft);
          }
     }

   /* NB: We cannot use our set_focus function here because it does, 
    * occasionally fall through wrt E's focus policy, so cherry pick the good 
    * parts and use here :) */

   /* if the border is iconified then uniconify if allowed */
   if ((ec->iconic) && (!ec->lock_user_iconify)) 
     e_client_uniconify(ec);

   /* set very high layer for this window as it needs attention and thus 
    * should show above everything */
   evas_object_layer_set(ec->frame, POL_ACTIVATE_LAYER);

   /* if we can raise the border do it */
   if (!ec->lock_user_stacking) evas_object_raise(ec->frame);

   /* focus the border */
   evas_object_focus_set(ec->frame, 1);

   /* NB: since we skip needless border evals when container layout 
    * is called (to save cpu cycles), we need to 
    * signal this border that it's focused so that the edj gets 
    * updated. 
    * 
    * This is potentially useless as THIS policy 
    * makes all windows borderless anyway, but it's in here for 
    * completeness
   e_client_focus_latest_set(ec);
   if (ec->bg_object) 
     edje_object_signal_emit(ec->bg_object, "e,state,focused", "e");
   if (ec->icon_object) 
     edje_object_signal_emit(ec->icon_object, "e,state,focused", "e");
   e_focus_event_focus_in(ec);
    */
}

void 
_policy_border_post_fetch(E_Client *ec) 
{
//   printf("Border post fetch\n");

   if (!ec) return;

   /* NB: for this policy we disable all remembers set on a border */
   if (ec->remember) e_remember_del(ec->remember);
   ec->remember = NULL;

   /* set this border to borderless */
#ifdef DIALOG_USES_PIXEL_BORDER
   if ((e_illume_client_is_dialog(ec)) && (e_illume_client_parent_get(ec)))
     eina_stringshare_replace(&ec->bordername, "pixel");
   else
     ec->borderless = 1;
#else
   ec->borderless = 1;
#endif

   /* tell E the border has changed */
   ec->border.changed = 1;
}

void 
_policy_border_post_assign(E_Client *ec) 
{
//   printf("Border post assign\n");

   if (!ec) return;

   ec->internal_no_remember = 1;

   /* do not allow client to change these properties */
   ec->lock_client_size = 1;
   ec->lock_client_shade = 1;
   ec->lock_client_maximize = 1;
   ec->lock_client_location = 1;
   ec->lock_client_stacking = 1;

   /* do not allow the user to change these properties */
   ec->lock_user_location = 1;
   ec->lock_user_size = 1;
   ec->lock_user_shade = 1;

   /* clear any centered states */
   /* NB: this is mainly needed for E's main config dialog */
   ec->e.state.centered = 0;

   /* lock the border type so user/client cannot change */
   ec->lock_border = 1;
}

void 
_policy_border_show(E_Client *ec) 
{
   if (!ec) return;

   /* make sure we have a name so that we don't handle windows like E's root */
   if (!ec->icccm.name) return;

//   printf("Border Show: %s\n", ec->icccm.class);

   /* trap for special windows so we can ignore hides below them */
   if (e_illume_client_is_indicator(ec)) return;
   if (e_illume_client_is_softkey(ec)) return;
   if (e_illume_client_is_quickpanel(ec)) return;
   if (e_illume_client_is_keyboard(ec)) return;

   _policy_border_hide_below(ec);
}

void 
_policy_zone_layout(E_Zone *zone) 
{
   E_Illume_Config_Zone *cz;
   Eina_List *l;
   E_Client *ec;

   if (!zone) return;

//   printf("Zone Layout: %d\n", zone->num);

   /* get the config for this zone */
   cz = e_illume_zone_config_get(zone->num);

   /* loop through border list and update layout */
   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        /* skip borders that are being deleted */
        if (e_object_is_del(E_OBJECT(ec))) continue;

        /* skip borders not on this zone */
        if (ec->zone != zone) continue;

        /* only update layout for this border if it really needs it */
        if ((!ec->new_client) && (!ec->changes.pos) && (!ec->changes.size) && 
            (!ec->changes.visible) &&
            (!ec->need_shape_export) && (!ec->need_shape_merge)) continue;

        /* are we laying out an indicator ? */
        if (e_illume_client_is_indicator(ec)) 
          _policy_zone_layout_indicator(ec, cz);

        /* are we layout out a quickpanel ? */
        else if (e_illume_client_is_quickpanel(ec)) 
          _policy_zone_layout_quickpanel(ec);

        /* are we laying out a softkey ? */
        else if (e_illume_client_is_softkey(ec)) 
          _policy_zone_layout_softkey(ec, cz);

        /* are we laying out a keyboard ? */
        else if (e_illume_client_is_keyboard(ec)) 
          _policy_zone_layout_keyboard(ec, cz);

        /* are we layout out a home window ? */
        else if (e_illume_client_is_home(ec)) 
          {
             if (!cz->mode.dual)
               _policy_zone_layout_home_single(ec, cz);
             else 
               {
                  /* we are in dual-mode, check orientation */
                  if (cz->mode.side == 0) 
                    {
                       int ty;

                       e_illume_client_indicator_pos_get(ec->zone, NULL, &ty);
                       if (ty <= ec->zone->y)
                         _policy_zone_layout_home_dual_top(ec, cz);
                       else
                         _policy_zone_layout_home_dual_custom(ec, cz);
                    }
                  else 
                    _policy_zone_layout_home_dual_left(ec, cz);
               }
          }

        /* are we laying out a fullscreen window ? */
        /* NB: we could use the e_illume_client_is_fullscreen function here 
         * but we save ourselves a function call this way. */
        else if ((ec->fullscreen) || (ec->need_fullscreen)) 
          _policy_zone_layout_fullscreen(ec);

        /* are we laying out a splash screen ? */
        /* NB: we check splash before dialog so if a splash screen does not 
         * register as a splash, than the dialog trap should catch it */
        else if (e_illume_client_is_splash(ec)) 
          _policy_zone_layout_splash(ec, cz);

        /* are we laying out a dialog ? */
        else if (e_illume_client_is_dialog(ec)) 
          _policy_zone_layout_dialog(ec, cz);

        /* are we layout out a conformant window ? */
        else if (e_illume_client_is_conformant(ec)) 
          {
             if (!cz->mode.dual)
               _policy_zone_layout_conformant_single(ec, cz);
             else 
               {
                  /* we are in dual-mode, check orientation */
                  if (cz->mode.side == 0) 
                    {
                       int ty;

                       e_illume_client_indicator_pos_get(ec->zone, NULL, &ty);
                       if (ty <= ec->zone->y)
                         _policy_zone_layout_conformant_dual_top(ec, cz);
                       else
                         _policy_zone_layout_conformant_dual_custom(ec, cz);
                    }
                  else 
                    _policy_zone_layout_conformant_dual_left(ec, cz);
               }
          }

        /* must be an app */
        else 
          {
             /* are we in single mode ? */
             if (!cz->mode.dual) 
               _policy_zone_layout_app_single(ec, cz);
             else 
               {
                  /* we are in dual-mode, check orientation */
                  if (cz->mode.side == 0) 
                    {
                       int ty;

                       /* grab the indicator position so we can tell if it 
                        * is in a custom position or not (user dragged it) */
                       e_illume_client_indicator_pos_get(ec->zone, NULL, &ty);
                       if (ty <= ec->zone->y)
                         _policy_zone_layout_app_dual_top(ec, cz);
                       else
                         _policy_zone_layout_app_dual_custom(ec, cz);
                    }
                  else 
                    _policy_zone_layout_app_dual_left(ec, cz);
               }
          }
     }
}

void 
_policy_zone_move_resize(E_Zone *zone) 
{
   Eina_List *l;
   E_Client *ec;

//   printf("Zone move resize\n");

   if (!zone) return;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        /* skip borders not on this zone */
        if (ec->zone != zone) continue;

        /* signal a changed pos here so layout gets updated */
        ec->changes.pos = 1;
        EC_CHANGED(ec);
     }
}

void 
_policy_zone_mode_change(E_Zone *zone, Ecore_X_Atom mode) 
{
   E_Illume_Config_Zone *cz;
   Eina_List *homes = NULL;
   E_Client *ec;
   int count;

//   printf("Zone mode change: %d\n", zone->num);

   if (!zone) return;

   /* get the config for this zone */
   cz = e_illume_zone_config_get(zone->num);

   /* update config with new mode */
   if (mode == ECORE_X_ATOM_E_ILLUME_MODE_SINGLE) 
     cz->mode.dual = 0;
   else 
     {
        cz->mode.dual = 1;
        if (mode == ECORE_X_ATOM_E_ILLUME_MODE_DUAL_TOP) 
          cz->mode.side = 0;
        else if (mode == ECORE_X_ATOM_E_ILLUME_MODE_DUAL_LEFT) 
          cz->mode.side = 1;
     }
   e_config_save_queue();

   /* lock indicator window from dragging if we need to */
   ec = e_illume_client_indicator_get(zone);
   if (ec) 
     {
        /* only dual-top mode can drag */
        if ((cz->mode.dual == 1) && (cz->mode.side == 0))
          {
             /* only set locked if we need to */
             if (ec->illume.drag.locked != 0) 
               ecore_x_e_illume_drag_locked_set(e_client_util_win_get(ec), 0);
          }
        else
          {
             /* only set locked if we need to */
             if (ec->illume.drag.locked != 1) 
             ecore_x_e_illume_drag_locked_set(e_client_util_win_get(ec), 1);
          }
     }

   if (!(homes = e_illume_client_home_borders_get(zone))) return;

   count = eina_list_count(homes);

   /* create a new home window (if needed) for dual mode */
   if (cz->mode.dual == 1) 
     {
        if (count < 2)
          ecore_x_e_illume_home_new_send(zone->black_win);
     }
   else if (cz->mode.dual == 0) 
     {
        /* if we went to single mode, delete any extra home windows */
        if (count >= 2) 
          {
             E_Client *home;

             /* try to get a home window on this zone and remove it */
             if ((home = e_illume_client_home_get(zone)))
               ecore_x_e_illume_home_del_send(e_client_util_win_get(home));
          }
     }

   /* Need to trigger a layout update here */
   _policy_zone_layout_update(zone);
}

void 
_policy_zone_close(E_Zone *zone) 
{
   E_Client *ec;

   if (!zone) return;

   /* make sure we have a focused border */
   if (!(ec = e_client_focused_get())) return;

   /* make sure focused border is on this zone */
   if (ec->zone != zone) return;

   /* close this border */
   e_client_act_close_begin(ec);
}

void 
_policy_drag_start(E_Client *ec) 
{
//   printf("Drag start\n");

   if (!ec) return;

   /* ignore stolen borders */
   if (ec->stolen) return;

   /* set property on this border to say we are dragging */
   ecore_x_e_illume_drag_set(e_client_util_win_get(ec), 1);

   /* set property on zone window that a drag is happening */
   ecore_x_e_illume_drag_set(ec->zone->black_win, 1);
}

void 
_policy_drag_end(E_Client *ec) 
{
//   printf("Drag end\n");

   if (!ec) return;

   /* ignore stolen borders */
   if (ec->stolen) return;

   /* set property on this border to say we are done dragging */
   ecore_x_e_illume_drag_set(e_client_util_win_get(ec), 0);

   /* set property on zone window that a drag is finished */
   ecore_x_e_illume_drag_set(ec->zone->black_win, 0);
}

void 
_policy_focus_back(E_Zone *zone) 
{
   Eina_List *l, *fl = NULL;
   E_Client *ec, *fbd;

   if (!zone) return;
   if (eina_list_count(_pol_focus_stack) < 1) return;

//   printf("Focus back\n");

   EINA_LIST_REVERSE_FOREACH(_pol_focus_stack, l, ec) 
     {
        if (ec->zone != zone) continue;
        fl = eina_list_append(fl, ec);
     }

   if (!(fbd = e_client_focused_get())) return;
   if (fbd->parent) return;

   EINA_LIST_REVERSE_FOREACH(fl, l, ec) 
     {
        if ((fbd) && (ec == fbd))
          {
             E_Client *b;

             if ((l->next) && (b = l->next->data)) 
               {
                  _policy_border_set_focus(b);
                  break;
               }
             else 
               {
                  /* we've reached the end of the list. Set focus to first */
                  if ((b = eina_list_nth(fl, 0))) 
                    {
                       _policy_border_set_focus(b);
                       break;
                    }
               }
          }
     }
   eina_list_free(fl);
}

void 
_policy_focus_forward(E_Zone *zone) 
{
   Eina_List *l, *fl = NULL;
   E_Client *ec, *fbd;

   if (!zone) return;
   if (eina_list_count(_pol_focus_stack) < 1) return;

   //   printf("Focus forward\n");

   EINA_LIST_FOREACH(_pol_focus_stack, l, ec) 
     {
        if (ec->zone != zone) continue;
        fl = eina_list_append(fl, ec);
     }

   if (!(fbd = e_client_focused_get())) return;
   if (fbd->parent) return;

   EINA_LIST_FOREACH(fl, l, ec) 
     {
        if ((fbd) && (ec == fbd))
          {
             E_Client *b;

             if ((l->next) && (b = l->next->data)) 
               {
                  _policy_border_set_focus(b);
                  break;
               }
             else 
               {
                  /* we've reached the end of the list. Set focus to first */
                  if ((b = eina_list_nth(fl, 0))) 
                    {
                       _policy_border_set_focus(b);
                       break;
                    }
               }
          }
     }
   eina_list_free(fl);
}

void 
_policy_focus_home(E_Zone *zone) 
{
   E_Client *ec;

   if (!zone) return;
   if (!(ec = e_illume_client_home_get(zone))) return;

   /* if the border was hidden due to layout, we need to unhide */
   if (!ec->visible) e_illume_client_show(ec);

   if ((ec->iconic) && (!ec->lock_user_iconify)) 
     e_client_uniconify(ec);

   if (!ec->lock_user_stacking) evas_object_raise(ec->frame);

   /* hide the border(s) above this one */
   _policy_border_hide_above(ec);

   /* focus the border */
   evas_object_focus_set(ec->frame, 1);
}

void 
_policy_property_change(Ecore_X_Event_Window_Property *event) 
{
//   printf("Property Change\n");

   /* we are interested in state changes here */
   if (event->atom == ECORE_X_ATOM_NET_WM_STATE) 
     {
        E_Client *ec, *ind;

        if (!(ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, event->win))) return;

        /* not interested in stolen or invisible borders */
        if ((ec->stolen) || (!ec->visible)) return;

        /* make sure the border has a name or class */
        /* NB: this check is here because some E borders get State Changes 
         * but do not have a name/class associated with them. Not entirely sure 
         * which ones they are, but I would guess Managers, Containers, or Zones.
         * At any rate, we're not interested in those types of borders */
        if ((!ec->icccm.name) || (!ec->icccm.class)) return;

        /* NB: If we have reached this point, then it should be a fullscreen 
         * border that has toggled fullscreen on/off */

        /* try to get the Indicator on this zone */
        if (!(ind = e_illume_client_indicator_get(ec->zone))) return;

        /* if we are fullscreen, hide the indicator...else we show it */
        /* NB: we could use the e_illume_client_is_fullscreen function here 
         * but we save ourselves a function call this way */
        if ((ec->fullscreen) || (ec->need_fullscreen)) 
          {
	     if (ind->visible)
               {
                  e_illume_client_hide(ind);
                  _policy_zone_layout_update(ec->zone);
               }
          }
        else 
          {
	     if (!ind->visible)
               {
                  e_illume_client_show(ind);
                  _policy_zone_layout_update(ec->zone);
               }
          }
     }
   else if (event->atom == ECORE_X_ATOM_E_ILLUME_INDICATOR_GEOMETRY) 
     {
        Eina_List *l;
        E_Zone *zone;
        E_Client *ec;
        int x, y, w, h;

        /* make sure this property changed on a zone */
        if (!(zone = e_util_zone_window_find(event->win))) return;

        /* get the geometry */
        if (!(ec = e_illume_client_indicator_get(zone))) return;
        x = ec->x;
        y = ec->y;
        w = ec->w;
        h = ec->h;

        /* look for conformant borders */
        EINA_LIST_FOREACH(e_comp->clients, l, ec)
          {
             if (e_client_util_ignored_get(ec)) continue;
             if (ec->zone != zone) continue;
             if (!e_illume_client_is_conformant(ec)) continue;
             /* set indicator geometry on conformant window */
             /* NB: This is needed so that conformant apps get told about 
              * the indicator size/position...else they have no way of 
              * knowing that the geometry has been updated */
             ecore_x_e_illume_indicator_geometry_set(e_client_util_win_get(ec), x, y, w, h);
          }
     }
   else if (event->atom == ECORE_X_ATOM_E_ILLUME_SOFTKEY_GEOMETRY) 
     {
        Eina_List *l;
        E_Zone *zone;
        E_Client *ec;
        int x, y, w, h;

        /* make sure this property changed on a zone */
        if (!(zone = e_util_zone_window_find(event->win))) return;

        if (!(ec = e_illume_client_softkey_get(zone))) return;
        x = ec->x;
        y = ec->y;
        w = ec->w;
        h = ec->h;

        /* look for conformant borders */
        EINA_LIST_FOREACH(e_comp->clients, l, ec)
          {
             if (e_client_util_ignored_get(ec)) continue;
             if (ec->zone != zone) continue;
             if (!e_illume_client_is_conformant(ec)) continue;
             /* set softkey geometry on conformant window */
             /* NB: This is needed so that conformant apps get told about 
              * the softkey size/position...else they have no way of 
              * knowing that the geometry has been updated */
             ecore_x_e_illume_softkey_geometry_set(e_client_util_win_get(ec), x, y, w, h);
          }
     }
   else if (event->atom == ECORE_X_ATOM_E_ILLUME_KEYBOARD_GEOMETRY) 
     {
        Eina_List *l;
        E_Zone *zone;
        E_Illume_Keyboard *kbd;
        E_Client *ec;
        int x, y, w, h;

        /* make sure this property changed on a zone */
        if (!(zone = e_util_zone_window_find(event->win))) return;

        /* get the keyboard */
        if (!(kbd = e_illume_keyboard_get())) return;
        if (!kbd->client) return;

        /* get the geometry */
        x = kbd->client->x;
        w = kbd->client->w;
        h = kbd->client->h;

        /* adjust for keyboard visibility because keyboard uses fx_offset */
        y = 0;
#warning this is totally broken on so many levels
        //if (kbd->client->frame &&
          //(!e_util_strcmp(edje_object_part_state_get(kbd->client->cw->effect_obj, "mover", NULL), "custom")))
          //y = kbd->client->y;

        /* look for conformant borders */
        EINA_LIST_FOREACH(e_comp->clients, l, ec)
          {
             if (e_client_util_ignored_get(ec)) continue;
             if (ec->zone != zone) continue;
             if (!e_illume_client_is_conformant(ec)) continue;
             /* set keyboard geometry on conformant window */
             /* NB: This is needed so that conformant apps get told about 
              * the keyboard size/position...else they have no way of 
              * knowing that the geometry has been updated */
             ecore_x_e_illume_keyboard_geometry_set(e_client_util_win_get(ec), x, y, w, h);
          }
     }
   else if (event->atom == ATM_ENLIGHTENMENT_SCALE)
     {
        const Eina_List *l;
        E_Comp *comp;

        
        EINA_LIST_FOREACH(e_comp_list(), l, comp) 
          {
             Eina_List *zl;
             E_Zone *zone;

             if (event->win != comp->man->root) continue;

             EINA_LIST_FOREACH(comp->zones, zl, zone) 
               _policy_zone_layout_update(zone);
          }
     }
}
