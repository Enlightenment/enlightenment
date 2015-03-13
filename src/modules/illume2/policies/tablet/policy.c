#include "e_illume.h"
#include "policy.h"
#include "e.h"

/* local function prototypes */
static void _policy_border_set_focus(E_Client *ec);
static void _policy_border_move(E_Client *ec, int x, int y);
static void _policy_border_resize(E_Client *ec, int w, int h);
static void _policy_border_hide_below(E_Client *ec);
static void _policy_zone_layout_update(E_Zone *zone);
static void _policy_zone_layout_indicator(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_quickpanel(E_Client *ec);
static void _policy_zone_layout_softkey(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_keyboard(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_home_single(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_fullscreen(E_Client *ec);
static void _policy_zone_layout_app_single(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_app_dual_top(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_app_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_app_dual_left(E_Client *ec, E_Illume_Config_Zone *cz, Eina_Bool force);
static void _policy_zone_layout_dialog(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_splash(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_conformant_single(E_Client *ec, E_Illume_Config_Zone *cz);
#if 0
static void _policy_zone_layout_conformant_dual_top(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_conformant_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz);
static void _policy_zone_layout_conformant_dual_left(E_Client *ec, E_Illume_Config_Zone *cz);
#endif

static Eina_List *_pol_focus_stack;

/* local functions */
static void
_policy_border_set_focus(E_Client *ec)
{
   if (!ec) return;
   /* printf("_policy_border_set_focus %s\n", e_client_util_name_get(ec)); */

   /* if focus is locked out then get out */
   if (ec->lock_focus_out) return;

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
	     /* evas_object_raise(ec->frame); */
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
_policy_border_hide_below(E_Client *ec)
{
   E_Client *b;

   return;

   //	printf("Hide Borders Below: %s %d %d\n",
   //	       ec->icccm.name, ec->x, ec->y);

   if (!ec) return;

   /* Find the windows below this one */
   E_CLIENT_FOREACH(b)
     {
        if (e_client_util_ignored_get(b)) continue;
        if (b->layer < E_LAYER_CLIENT_BELOW) continue;
        /* skip if it's the same border */
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

#if 0
static void
_policy_border_show_below(E_Client *ec)
{
   Eina_List *l;
   E_Client *prev, *b;

   //	printf("Show Borders Below: %s %d %d\n",
   //	       ec->icccm.class, ec->x, ec->y);

   if (!ec) return;
   /* printf("_policy_border_show_below %s\n", e_client_util_name_get(ec)); */
   if (ec->icccm.transient_for)
     {
	if ((prev = e_pixmap_find_client(E_PIXMAP_TYPE_X, ec->icccm.transient_for)))
	  {
	     _policy_border_set_focus(prev);
	     return;
	  }
     }

   /* Find the windows below this one */
   E_CLIENT_FOREACH(b)
     {
        if (e_client_util_ignored_get(b)) continue;
        if (b->layer < E_LAYER_CLIENT_BELOW) continue;
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
#endif

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

   //	printf("\tLayout Indicator: %d\n", ec->zone->num);

   /* lock indicator window from dragging if we need to */
   if ((cz->mode.dual == 1) && (cz->mode.side == 0))
     ecore_x_e_illume_drag_locked_set(e_client_util_win_get(ec), 0);
   else
     ecore_x_e_illume_drag_locked_set(e_client_util_win_get(ec), 1);

   if ((ec->w != ec->zone->w) || (ec->h != cz->indicator.size))
     _policy_border_resize(ec, ec->zone->w, cz->indicator.size);

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

   if (ec->layer != POL_INDICATOR_LAYER)
     evas_object_layer_set(ec->frame, POL_INDICATOR_LAYER);
}

#define ZONE_GEOMETRY					\
  int x, y, w, h;					\
  e_zone_useful_geometry_get(ec->zone, &x, &y, &w, &h); \
  x += ec->zone->x;					\
  y += ec->zone->y;					\

static void
_border_geometry_set(E_Client *ec, int x, int y, int w, int h, int layer)
{
   if ((ec->w != w) || (ec->h != h))
     _policy_border_resize(ec, w, h);

   if ((ec->x != x) || (ec->y != y))
     _policy_border_move(ec, x, y);

   if ((int)ec->layer != layer) evas_object_layer_set(ec->frame, layer);
}

static void
_policy_zone_layout_quickpanel(E_Client *ec)
{
   int mh;

   if (!ec) return;

   e_illume_client_min_get(ec, NULL, &mh);

   if ((ec->w != ec->zone->w) || (ec->h != mh))
     _policy_border_resize(ec, ec->zone->w, mh);

   if (ec->layer != POL_QUICKPANEL_LAYER)
     evas_object_layer_set(ec->frame, POL_QUICKPANEL_LAYER);
}

static void
_policy_zone_layout_softkey(E_Client *ec, E_Illume_Config_Zone *cz)
{
   int ny;

   if ((!ec) || (!cz)) return;
   if (!ec->visible)
     {
	ecore_x_e_illume_softkey_geometry_set(ec->zone->black_win, 0, 0, 0, 0);
	return;
     }

   ZONE_GEOMETRY;

   e_illume_client_min_get(ec, NULL, &cz->softkey.size);

   /* if we are dragging, then skip it for now */
   /* NB: Disabled currently until we confirm that softkey should be draggable */
   //	if (ec->illume.drag.drag) return;

   if ((ec->w != w) || (ec->h != cz->softkey.size))
     _policy_border_resize(ec, w, cz->softkey.size);

   ny = ((ec->zone->y + ec->zone->h) - cz->softkey.size);

   /* NB: not sure why yet, but on startup the border->y is reporting
    * that it is already in this position...but it's actually not.
    * So for now, just disable the ny check until this gets sorted out */

   if ((ec->x != x) || (ec->y != ny))
     _policy_border_move(ec, x, ny);

   /* _border_geometry_set(ec, x, ny, nw, nh, ec, POL_CONFORMANT_LAYER); */

   ecore_x_e_illume_softkey_geometry_set(ec->zone->black_win,
					 ec->x, ec->y,
					 ec->w, ec->h);

   if (ec->layer != POL_SOFTKEY_LAYER) evas_object_layer_set(ec->frame, POL_SOFTKEY_LAYER);
}



#define MIN_HEIGHT 100

static Eina_Bool
_policy_layout_app_check(E_Client *ec)
{
   if (!ec)
     return EINA_FALSE;

   if (!ec->visible)
     return EINA_FALSE;

   if ((ec->desk != e_desk_current_get(ec->zone)) && (!ec->sticky))
     return EINA_FALSE;

   return EINA_TRUE;
}


static void
_policy_keyboard_restrict(E_Client *ec, int *h)
{
   int kh;

   if (ec->vkbd.state > ECORE_X_VIRTUAL_KEYBOARD_STATE_OFF)
     {
	e_illume_keyboard_safe_app_region_get(ec->zone, NULL, NULL, NULL, &kh);
	kh -= ec->zone->h - *h;
	if ((kh < *h) && (kh > MIN_HEIGHT))
	  *h = kh;
     }
}

static void
_policy_indicator_restrict(E_Illume_Config_Zone *cz, int *y, int *h)
{
   if ((cz->indicator.size) && (*y < cz->indicator.size))
     {
	*h -= cz->indicator.size;
	*y = cz->indicator.size;
     }
}

static void
_policy_softkey_restrict(E_Illume_Config_Zone *cz, int *y, int *h)
{
   if ((cz->softkey.size) && ((*y + *h) > cz->softkey.size))
     *h -= (*y + *h) - cz->softkey.size;
}

static void
_policy_zone_layout_keyboard(E_Client *ec, E_Illume_Config_Zone *cz)
{
   int ny, layer;

   if ((!ec) || (!cz) || (!ec->visible)) return;

   ZONE_GEOMETRY;

   e_illume_client_min_get(ec, NULL, &cz->vkbd.size);

   ny = ((ec->zone->y + ec->zone->h) - cz->vkbd.size);

   /* if ((ec->fullscreen) || (ec->need_fullscreen))
    *   layer = POL_FULLSCREEN_LAYER;
    * else */
   layer = POL_KEYBOARD_LAYER;
   
   _border_geometry_set(ec, x, ny, w, cz->vkbd.size, layer);
}

static void
_policy_zone_layout_home_single(E_Client *ec, E_Illume_Config_Zone *cz)
{
   if ((!ec) || (!cz) || (!ec->visible)) return;

   ZONE_GEOMETRY;
   _policy_indicator_restrict(cz, &y, &h);
   _border_geometry_set(ec, x, y, w, h, POL_HOME_LAYER);
}

static void
_policy_zone_layout_fullscreen(E_Client *ec)
{
   if (!_policy_layout_app_check(ec)) return;

   ZONE_GEOMETRY;

   _policy_keyboard_restrict(ec, &h);

   _border_geometry_set(ec, x, y, w, h, POL_FULLSCREEN_LAYER);
}

static void
_policy_zone_layout_app_single(E_Client *ec, E_Illume_Config_Zone *cz)
{
   if (!_policy_layout_app_check(ec)) return;

   ZONE_GEOMETRY;

   _policy_keyboard_restrict(ec, &h);
   _policy_indicator_restrict(cz, &y, &h);
   _policy_softkey_restrict(cz, &y, &h);

   _border_geometry_set(ec, x, y, w, h, POL_APP_LAYER);
}

static void
_policy_zone_layout_app_dual_top(E_Client *ec, E_Illume_Config_Zone *cz)
{
   E_Client *ec2;

   if (!_policy_layout_app_check(ec)) return;

   ZONE_GEOMETRY;

   _policy_keyboard_restrict(ec, &h);
   _policy_indicator_restrict(cz, &y, &h);
   _policy_softkey_restrict(cz, &y, &h);

   ec2 = e_illume_client_at_xy_get(ec->zone, x, y);
   if ((ec2) && (ec2 != ec))
     {
	if ((ec->focused) && (ec->vkbd.state > ECORE_X_VIRTUAL_KEYBOARD_STATE_OFF))
	  _border_geometry_set(ec2, x, h/2 + y, w, h/2, POL_APP_LAYER);
	else
	  y += h/2;
     }

   _border_geometry_set(ec, x, y, w, h/2, POL_APP_LAYER);
}

static void
_policy_zone_layout_app_dual_custom(E_Client *ec, E_Illume_Config_Zone *cz)
{
   E_Client *app;
   int iy, ny, nh;

   if (!_policy_layout_app_check(ec)) return;

   ZONE_GEOMETRY;

   e_illume_client_indicator_pos_get(ec->zone, NULL, &iy);

   ny = ec->zone->y;
   nh = iy;

   app = e_illume_client_at_xy_get(ec->zone, ec->zone->x, ec->zone->y);
   if ((app) && (ec != app))
     {
	ny = (iy + cz->indicator.size);
	nh = ((ec->zone->y + ec->zone->h) - ny - cz->softkey.size);
     }

   _border_geometry_set(ec, x, ny, w, nh, POL_APP_LAYER);
}

static void
_policy_zone_layout_app_dual_left(E_Client *ec, E_Illume_Config_Zone *cz, Eina_Bool force)
{
   E_Client *ec2;
   int ky, kh, nx, nw;

   if (!_policy_layout_app_check(ec)) return;

   ZONE_GEOMETRY;

   e_illume_keyboard_safe_app_region_get(ec->zone, NULL, &ky, NULL, &kh);

   if (kh >= ec->zone->h)
     kh = (kh - cz->indicator.size - cz->softkey.size);
   else
     kh = (kh - cz->indicator.size);

   nx = x;
   nw = w / 2;

   if (!force)
     {
	/* see if there is a border already there. if so, place at right */
	ec2 = e_illume_client_at_xy_get(ec->zone, nx, y);
	if ((ec2) && (ec != ec2)) nx = x + nw;
     }

   _border_geometry_set(ec, nx, y, nw, kh, POL_APP_LAYER);
}

static void
_policy_zone_layout_dialog(E_Client *ec, E_Illume_Config_Zone *cz)
{
   E_Client *parent;
   int mw, mh, nx, ny;

   /* if (!_policy_layout_app_check(ec)) return; */
   if ((!ec) || (!cz)) return;
   printf("place dialog %d - %dx%d\n", ec->placed, ec->w, ec->h);

   if (ec->placed)
     return;

   ZONE_GEOMETRY;

   mw = ec->w;
   mh = ec->h;

   if (e_illume_client_is_fixed_size(ec))
     {
	if (mw > w) mw = w;
	if (mh > h) mh = h;
     }
   else
     {
	if (w * 2/3 > ec->w)
	  mw = w * 2/3;

	if (h * 2/3 > ec->h)
	  mh = h * 2/3;
     }

   parent = e_illume_client_parent_get(ec);

   if ((!parent) || (cz->mode.dual == 1))
     {
	nx = (x + ((w - mw) / 2));
	ny = (y + ((h - mh) / 2));
     }
   else
     {
	if (mw > parent->w) mw = parent->w;
	if (mh > parent->h) mh = parent->h;

	nx = (parent->x + ((parent->w - mw) / 2));
	ny = (parent->y + ((parent->h - mh) / 2));
     }

   ec->placed = 1;
   
   _border_geometry_set(ec, nx, ny, mw, mh, POL_DIALOG_LAYER);
   printf("set geom %d %d\n", mw, mh);
}

static void
_policy_zone_layout_splash(E_Client *ec, E_Illume_Config_Zone *cz)
{
   _policy_zone_layout_dialog(ec, cz);

   if (ec->layer != POL_SPLASH_LAYER) evas_object_layer_set(ec->frame, POL_SPLASH_LAYER);
}

static void
_policy_zone_layout_conformant_single(E_Client *ec, E_Illume_Config_Zone *cz __UNUSED__)
{
   if (!_policy_layout_app_check(ec)) return;

   _border_geometry_set(ec, ec->zone->x, ec->zone->y,
			ec->zone->w, ec->zone->h,
			POL_CONFORMANT_LAYER);
}

#if 0
typedef struct _App_Desk App_Desk;

struct _App_Desk
{
  E_Desk *desk;
  const char *class;
  Eina_List *borders;
};

static Eina_List *desks = NULL;

#define EINA_LIST_FIND(_list, _item, _match)	\
  {						\
     Eina_List *_l;				\
     EINA_LIST_FOREACH(_list, _l, _item)	\
       if (_match) break;			\
  }
#endif


/* policy functions */
void
_policy_border_add(E_Client *ec)
{
   //	printf("Border added: %s\n", ec->icccm.class);

   if (!ec) return;

   /* NB: this call sets an atom on the window that specifices the zone.
    * the logic here is that any new windows created can access the zone
    * window by a 'get' call. This is useful for elementary apps as they
    * normally would not have access to the zone window. Typical use case
    * is for indicator & softkey windows so that they can send messages
    * that apply to their respective zone only. Example: softkey sends close
    * messages (or back messages to cycle focus) that should pertain just
    * to it's current zone */
   ecore_x_e_illume_zone_set(e_client_util_win_get(ec), ec->zone->black_win);

   if (e_illume_client_is_keyboard(ec))
     {
	ec->sticky = 1;
	e_hints_window_sticky_set(ec, 1);
     }
   
   if (e_illume_client_is_home(ec))
     {
	ec->sticky = 1;
	e_hints_window_sticky_set(ec, 1);
     }

   if (e_illume_client_is_indicator(ec))
     {
	ec->sticky = 1;
	e_hints_window_sticky_set(ec, 1);
     }

   if (e_illume_client_is_softkey(ec))
     {
	ec->sticky = 1;
	e_hints_window_sticky_set(ec, 1);
     }

   /* ignore stolen borders. These are typically quickpanel or keyboards */
   if (ec->stolen) return;

   /* if this is a fullscreen window, than we need to hide indicator window */
   /* NB: we could use the e_illume_client_is_fullscreen function here
    * but we save ourselves a function call this way */
   if ((ec->fullscreen) || (ec->need_fullscreen))
     {
	E_Client *ind, *sft;

	if ((ind = e_illume_client_indicator_get(ec->zone)))
	  {
	     if (ind->visible) e_illume_client_hide(ind);
	  }
	if ((sft = e_illume_client_softkey_get(ec->zone)))
	  {
	     if (e_illume_client_is_conformant(ec))
	       {
		  if (sft->visible)
		    e_illume_client_hide(sft);
		  else if (!sft->visible)
		    e_illume_client_show(sft);
	       }
	  }
     }


#if 0
   if (ec->icccm.class)
     {
   	Eina_List *l;
   	App_Desk *d;

   	EINA_LIST_FIND(desks, d, (d->class == ec->icccm.class));

   	if (!d)
   	  {
   	     d = E_NEW(App_Desk, 1);
   	     d->desk
   	  }

   	d->borders = eina_list_append(d->borders, ec);
   	e_client_desk_set(ec, d->desk);
     }
#endif

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
   //	printf("Policy Border deleted: %s\n", ec->icccm.class);

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
	     if (!ind->visible) e_illume_client_show(ind);
	  }
	_policy_zone_layout_update(ec->zone);
     }

   /* remove from focus stack */
   if ((ec->icccm.accepts_focus) || (ec->icccm.take_focus))
     _pol_focus_stack = eina_list_remove(_pol_focus_stack, ec);

   if (e_illume_client_is_softkey(ec))
     {
	E_Illume_Config_Zone *cz;

	cz = e_illume_zone_config_get(ec->zone->num);
	cz->softkey.size = 0;
	_policy_zone_layout_update(ec->zone);
     }
   else if (e_illume_client_is_indicator(ec))
     {
	E_Illume_Config_Zone *cz;

	cz = e_illume_zone_config_get(ec->zone->num);
	cz->indicator.size = 0;
	_policy_zone_layout_update(ec->zone);
     }
   /* else
    *   {
    * 	_policy_border_show_below(ec);
    *   } */
}

void
_policy_border_focus_in(E_Client *ec __UNUSED__)
{
   E_Client *ind;

   //	printf("Border focus in: %s\n", ec->icccm.name);
   if ((ec->fullscreen) || (ec->need_fullscreen))
     {
	/* try to get the Indicator on this zone */
	if ((ind = e_illume_client_indicator_get(ec->zone)))
	  {
	     /* we have the indicator, show it if needed */
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
   //	printf("Border focus out: %s\n", ec->icccm.name);

   if (!ec) return;

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

   //	printf("Border Activate: %s\n", ec->icccm.name);

   if (!ec) return;

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
   if (ec->desk != e_desk_current_get(ec->zone))
     e_desk_show(ec->desk);

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
static Eina_Bool
_policy_border_is_dialog(E_Client *ec)
{
   if (e_illume_client_is_dialog(ec))
     return EINA_TRUE;

   if (ec->e.state.centered)
     return EINA_TRUE;

   if (ec->internal)
     {
	if (ec->icccm.class)
	  {
	     if (!strncmp(ec->icccm.class, "Illume", 6))
	       return EINA_FALSE;
	     if (!strncmp(ec->icccm.class, "e_fwin", 6))
	       return EINA_FALSE;
	     if (!strncmp(ec->icccm.class, "every", 5))
	       return EINA_FALSE;

	  }
	return EINA_TRUE;
     }

   return EINA_FALSE;
}

void
_policy_border_post_fetch(E_Client *ec)
{
   if (!ec) return;

   /* NB: for this policy we disable all remembers set on a border */
   if (ec->remember) e_remember_del(ec->remember);
   ec->remember = NULL;

   if (_policy_border_is_dialog(ec))
     {
	return;
     }
   else if (e_illume_client_is_fixed_size(ec))
     {
	return;
     }
   else if (!ec->borderless)
     {
	ec->borderless = 1;
	ec->border.changed = 1;
     }
}

void
_policy_border_post_assign(E_Client *ec)
{
   if (!ec) return;

   ec->internal_no_remember = 1;

   if (_policy_border_is_dialog(ec) ||
       e_illume_client_is_fixed_size(ec))
     return;

   /* do not allow client to change these properties */
   ec->lock_client_size = 1;
   ec->lock_client_shade = 1;
   ec->lock_client_maximize = 1;
   ec->lock_client_location = 1;
   ec->lock_client_stacking = 1;

   /* do not allow the user to change these properties */
   /* ec->lock_user_location = 1;
    * ec->lock_user_size = 1;
    * ec->lock_user_shade = 1; */

   /* clear any centered states */
   /* NB: this is mainly needed for E's main config dialog */
   ec->e.state.centered = 0;

   /* lock the border type so user/client cannot change */
   ec->lock_border = 1;

   /* disable e's placement (and honoring of icccm.request_pos) */
   ec->placed = 1;
}

void
_policy_border_show(E_Client *ec)
{
   if (!ec) return;

   /* printf("_policy_border_show %s\n", e_client_util_name_get(ec)); */

   if (!ec->icccm.name) return;

   //	printf("Border Show: %s\n", ec->icccm.class);

   /* trap for special windows so we can ignore hides below them */
   if (e_illume_client_is_indicator(ec)) return;
   if (e_illume_client_is_softkey(ec)) return;
   if (e_illume_client_is_quickpanel(ec)) return;
   if (e_illume_client_is_keyboard(ec)) return;

   _policy_border_hide_below(ec);
}

/* called on E_CLIENT_HOOK_CONTAINER_LAYOUT (after e_border/eval0) */
void
_policy_zone_layout(E_Zone *zone)
{
   E_Illume_Config_Zone *cz;
   Eina_List *l;
   E_Client *ec;

   if (!zone) return;

   cz = e_illume_zone_config_get(zone->num);

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
	if (e_object_is_del(E_OBJECT(ec))) continue;

	if (ec->zone != zone) continue;

	if ((!ec->new_client) && (!ec->changes.pos) && (!ec->changes.size) &&
	    (!ec->changes.visible) &&
	    (!ec->need_shape_export) && (!ec->need_shape_merge)) continue;

	if (e_illume_client_is_indicator(ec))
	  _policy_zone_layout_indicator(ec, cz);

	else if (e_illume_client_is_quickpanel(ec))
	  _policy_zone_layout_quickpanel(ec);

	else if (e_illume_client_is_softkey(ec))
	  _policy_zone_layout_softkey(ec, cz);

	else if (e_illume_client_is_keyboard(ec))
	  _policy_zone_layout_keyboard(ec, cz);

	else if (e_illume_client_is_home(ec))
	  _policy_zone_layout_home_single(ec, cz);

	else if ((ec->fullscreen) || (ec->need_fullscreen))
	  _policy_zone_layout_fullscreen(ec);

	else if (e_illume_client_is_splash(ec))
	  _policy_zone_layout_splash(ec, cz);

	else if (_policy_border_is_dialog(ec))
	  _policy_zone_layout_dialog(ec, cz);

	else if (e_illume_client_is_conformant(ec))
	  _policy_zone_layout_conformant_single(ec, cz);

	else if (e_illume_client_is_fixed_size(ec))
	  _policy_zone_layout_dialog(ec, cz);

	else if (ec->internal && ec->icccm.class &&
		 (!strcmp(ec->icccm.class, "everything-window")))
	  {
	     if (ec->vkbd.state == ECORE_X_VIRTUAL_KEYBOARD_STATE_ON)
	       _policy_zone_layout_app_single(ec, cz);
	     /* else
	      *   _policy_zone_layout_app_dual_left(ec, cz, EINA_TRUE); */
	     if (ec->layer != POL_ACTIVATE_LAYER)
	       evas_object_layer_set(ec->frame, POL_ACTIVATE_LAYER);
	     
	     /* if (ec->layer != POL_SPLASH_LAYER)
	      *   evas_object_layer_set(ec->frame, POL_SPLASH_LAYER); */
	  }
	else if (ec->e.state.centered)
	  _policy_zone_layout_dialog(ec, cz);
	else if (!cz->mode.dual)
	  _policy_zone_layout_app_single(ec, cz);
	else
	  {
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
	       _policy_zone_layout_app_dual_left(ec, cz, EINA_FALSE);
	  }
     }
}

void
_policy_zone_move_resize(E_Zone *zone)
{
   Eina_List *l;
   E_Client *ec;

   if (!zone) return;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
	if (ec->zone != zone) continue;

	ec->changes.pos = 1;
	EC_CHANGED(ec);
     }
}

void
_policy_zone_mode_change(E_Zone *zone, Ecore_X_Atom mode)
{
   E_Illume_Config_Zone *cz;
   /* Eina_List *homes = NULL; */
   E_Client *ec;
   /* int count; */

   if (!zone) return;

   cz = e_illume_zone_config_get(zone->num);

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
#if 0 /* split home window? wtf?! go home! */
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
#endif

   _policy_zone_layout_update(zone);
}

void
_policy_zone_close(E_Zone *zone)
{
   E_Client *ec;

   if (!zone) return;

   if (!(ec = e_client_focused_get())) return;

   if (ec->zone != zone) return;

   /* close this border */
   e_client_act_close_begin(ec);
}

void
_policy_drag_start(E_Client *ec)
{
   if (!ec) return;

   if (ec->stolen) return;

   ecore_x_e_illume_drag_set(e_client_util_win_get(ec), 1);
   ecore_x_e_illume_drag_set(ec->zone->black_win, 1);
}

void
_policy_drag_end(E_Client *ec)
{
   //	printf("Drag end\n");

   if (!ec) return;

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

   //	printf("Focus back\n");

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

   //	printf("Focus forward\n");

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
   _policy_border_set_focus(ec);
}

void
_policy_property_change(Ecore_X_Event_Window_Property *event)
{
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

	if (!(zone = e_util_zone_window_find(event->win))) return;

	if (!(ec = e_illume_client_indicator_get(zone))) return;
	x = ec->x;
	y = ec->y;
	w = ec->w;
	h = ec->h;

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

	if (!(zone = e_util_zone_window_find(event->win))) return;

	if (!(ec = e_illume_client_softkey_get(zone))) return;
	x = ec->x;
	y = ec->y;
	w = ec->w;
	h = ec->h;

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

	if (!(zone = e_util_zone_window_find(event->win))) return;

	if (!(kbd = e_illume_keyboard_get())) return;
	if (!kbd->client) return;

	x = kbd->client->x;
	w = kbd->client->w;
	h = kbd->client->h;

	/* adjust Y for keyboard visibility because keyboard uses fx_offset */
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
