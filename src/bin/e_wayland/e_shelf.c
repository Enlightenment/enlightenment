#include "e.h"

/* local function prototypes */
static void _e_shelf_cb_free(E_Shelf *es);
static void _e_shelf_gadcon_min_size_request(void *data, E_Gadcon *gc, Evas_Coord w, Evas_Coord h);
static void _e_shelf_gadcon_size_request(void *data, E_Gadcon *gc, Evas_Coord w, Evas_Coord h);
static Evas_Object *_e_shelf_gadcon_frame_request(void *data, E_Gadcon_Client *gcc, const char *style);
static void _e_shelf_cb_locked_set(void *data, int lock);
static void _e_shelf_cb_urgent_show(void *data);
static void _e_shelf_gadcon_client_remove(void *data, E_Gadcon_Client *gcc);
static int _e_shelf_gadcon_client_add(void *data, const E_Gadcon_Client_Class *cc);
static void _e_shelf_cb_menu_items_append(void *data, E_Gadcon_Client *gcc, E_Menu *mn);
static void _e_shelf_menu_append(E_Shelf *es, E_Menu *mn);
static void _e_shelf_menu_pre_cb(void *data, E_Menu *m);
static void _e_shelf_menu_append(E_Shelf *es, E_Menu *mn);
static void _e_shelf_cb_menu_config(void *data, E_Menu *m, E_Menu_Item *mi);
static void _e_shelf_cb_menu_edit(void *data, E_Menu *m, E_Menu_Item *mi);
static void _e_shelf_cb_menu_contents(void *data, E_Menu *m, E_Menu_Item *mi);
static void _e_shelf_cb_confirm_dialog_yes(void *data);
static void _e_shelf_cb_menu_delete(void *data, E_Menu *m, E_Menu_Item *mi);
static void _e_shelf_menu_item_free(void *data);
static const char *_e_shelf_orient_icon_name_get(E_Shelf *s);

/* local variables */
static Eina_List *_shelves = NULL;

EINTERN int 
e_shelf_init(void)
{
   return 1;
}

EINTERN int 
e_shelf_shutdown(void)
{
   if (wl_fatal) return 1;

   while (_shelves)
     {
        E_Shelf *es;

        es = eina_list_data_get(_shelves);
        e_object_del(E_OBJECT(es));
     }

   return 1;
}

EAPI void 
e_shelf_config_update(void)
{
   Eina_List *l;
   E_Config_Shelf *cfg;
   int id = 0;

   while (_shelves)
     {
        E_Shelf *es;

        es = eina_list_data_get(_shelves);
        e_object_del(E_OBJECT(es));
     }

   EINA_LIST_FOREACH(e_config->shelves, l, cfg)
     {
        E_Zone *zone;

        if (cfg->id <= 0) cfg->id = id + 1;
        zone = e_util_container_zone_number_get(cfg->container, cfg->zone);
        if (zone) e_shelf_config_new(zone, cfg);
        id = cfg->id;
     }
}

EAPI E_Shelf *
e_shelf_config_new(E_Zone *zone, E_Config_Shelf *cfg)
{
   E_Shelf *es;

   E_OBJECT_CHECK_RETURN(zone, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, NULL);

   es = e_shelf_zone_new(zone, cfg->name, cfg->style, cfg->popup, 
                         cfg->layer, cfg->id);
   if (!es) return NULL;

   if (!cfg->hide_timeout) cfg->hide_timeout = 1.0;
   if (!cfg->hide_duration) cfg->hide_duration = 1.0;

   es->cfg = cfg;
   es->fit_along = cfg->fit_along;
   es->fit_size = cfg->fit_size;

   e_shelf_orient(es, cfg->orient);
   e_shelf_position_calc(es);
   e_shelf_populate(es);

   if (cfg->desk_show_mode)
     {
        E_Desk *desk;
        Eina_List *l;
        E_Config_Shelf_Desk *sd;

        desk = e_desk_current_get(zone);
        EINA_LIST_FOREACH(cfg->desk_list, l, sd)
          {
             if ((desk->x == sd->x) && (desk->y == sd->y))
               {
                  e_shelf_show(es);
                  break;
               }
          }
     }
   else
     e_shelf_show(es);

   e_shelf_toggle(es, EINA_FALSE);

   return es;
}

EAPI E_Shelf *
e_shelf_zone_new(E_Zone *zone, const char *name, const char *style, int popup, int layer, int id)
{
   E_Shelf *es;
   char buff[1024];
   const char *locname;

   es = E_OBJECT_ALLOC(E_Shelf, E_SHELF_TYPE, _e_shelf_cb_free);
   if (!es) return NULL;

   es->id = id;
   es->x = 0;
   es->y = 0;
   es->w = 32;
   es->h = 32;
   es->zone = zone;

   if (popup)
     {
        es->popup = e_popup_new(zone, es->x, es->y, es->w, es->h);
        e_popup_name_set(es->popup, "shelf");
        e_popup_layer_set(es->popup, layer);
        es->ee = es->popup->ee;
        es->evas = es->popup->evas;
     }
   else
     {
        es->ee = zone->container->bg_ee;
        es->evas = zone->container->bg_evas;
     }

   es->fit_along = EINA_TRUE;
   es->layer = layer;
   es->style = eina_stringshare_add(style);
   es->name = eina_stringshare_add(name);

   es->o_event = evas_object_rectangle_add(es->evas);
   evas_object_color_set(es->o_event, 255, 0, 0, 128);
   evas_object_resize(es->o_event, es->w, es->h);
   /* TODO: add callback */

   /* TODO: add handlers */

   es->o_base = edje_object_add(es->evas);
   evas_object_resize(es->o_base, es->w, es->h);
   snprintf(buff, sizeof(buff), "e/shelf/%s/base", es->style);
   if (!e_theme_edje_object_set(es->o_base, "base/theme/shelf", buff))
     e_theme_edje_object_set(es->o_base, "base/theme/shelf", 
                             "e/shelf/default/base");

   if (es->popup)
     {
        evas_object_show(es->o_event);
        evas_object_show(es->o_base);
        e_popup_edje_bg_object_set(es->popup, es->o_base);
        /* TODO: window type set */
     }
   else
     {
        evas_object_move(es->o_event, es->zone->x + es->x, 
                         es->zone->y + es->y);
        evas_object_move(es->o_base, es->zone->x + es->x, 
                         es->zone->y + es->y);
        evas_object_layer_set(es->o_event, layer);
        evas_object_layer_set(es->o_base, layer);
     }

   es->gadcon = 
     e_gadcon_swallowed_new(es->name, es->id, es->o_base, "e.swallow.content");
   locname = es->name;
   if (!name) locname = _("Shelf #");
   snprintf(buff, sizeof(buff), "%s %i", locname, es->id);
   es->gadcon->location = 
     e_gadcon_location_new(buff, E_GADCON_SITE_SHELF, 
                           _e_shelf_gadcon_client_add, es, 
                           _e_shelf_gadcon_client_remove, es);
   e_gadcon_location_register(es->gadcon->location);
// hmm dnd in ibar and ibox kill this. ok. need to look into this more   
//   es->gadcon->instant_edit = 1;
   e_gadcon_min_size_request_callback_set(es->gadcon,
					  _e_shelf_gadcon_min_size_request, es);

   e_gadcon_size_request_callback_set(es->gadcon,
				      _e_shelf_gadcon_size_request, es);
   e_gadcon_frame_request_callback_set(es->gadcon,
				       _e_shelf_gadcon_frame_request, es);
   e_gadcon_orient(es->gadcon, E_GADCON_ORIENT_TOP);
   snprintf(buff, sizeof(buff), "e,state,orientation,%s", 
	    e_shelf_orient_string_get(es));
   edje_object_signal_emit(es->o_base, buff, "e");
   edje_object_message_signal_process(es->o_base);
   e_gadcon_zone_set(es->gadcon, zone);
   e_gadcon_ecore_evas_set(es->gadcon, es->ee);
   e_gadcon_shelf_set(es->gadcon, es);

   e_gadcon_util_menu_attach_func_set(es->gadcon,
				      _e_shelf_cb_menu_items_append, es);

   e_gadcon_util_lock_func_set(es->gadcon,
			       _e_shelf_cb_locked_set, es);
   e_gadcon_util_urgent_show_func_set(es->gadcon,
				      _e_shelf_cb_urgent_show, es);

   _shelves = eina_list_append(_shelves, es);

   es->hidden = EINA_FALSE;
   /* TODO: hide step */
   es->locked = EINA_FALSE;

   /* TODO: hidden_state_size & instant_delay */

   return es;
}

EAPI void 
e_shelf_show(E_Shelf *es)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_SHELF_TYPE);

   if (es->popup)
     e_popup_show(es->popup);
   else
     {
        evas_object_show(es->o_event);
        evas_object_show(es->o_base);
     }
}

EAPI void 
e_shelf_hide(E_Shelf *es)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_SHELF_TYPE);

   if (es->popup)
     e_popup_hide(es->popup);
   else
     {
        evas_object_hide(es->o_event);
        evas_object_hide(es->o_base);
     }
}

EAPI void 
e_shelf_toggle(E_Shelf *es, Eina_Bool show)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_SHELF_TYPE);

   es->toggle = show;
   if (es->locked) return;
   es->urgent_show = EINA_FALSE;
   if ((show) && (es->hidden))
     {
        es->hidden = EINA_FALSE;
        edje_object_signal_emit(es->o_base, "e,state,visible", "e");
        /* TODO: autohide */
     }
   else if ((!show) && (!es->hidden))
     {
        es->hidden = EINA_TRUE;
        edje_object_signal_emit(es->o_base, "e,state,hidden", "e");
        /* TODO: instant delay */
     }
}

EAPI void 
e_shelf_populate(E_Shelf *es)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_SHELF_TYPE);
   e_gadcon_populate(es->gadcon);
}

EAPI void 
e_shelf_position_calc(E_Shelf *es)
{
   E_Gadcon_Orient orient = E_GADCON_ORIENT_FLOAT;
   int size = 40;

   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_SHELF_TYPE);

   if (es->cfg)
     {
        orient = es->cfg->orient;
        size = es->cfg->size * e_scale;
     }
   /* TODO: handle gadcon->orient */

   switch (orient)
     {
      case E_GADCON_ORIENT_FLOAT:
        if (!es->fit_along) es->w = es->zone->w;
        if (!es->fit_size) es->h = size;
        break;
      case E_GADCON_ORIENT_HORIZ:
        if (!es->fit_along) es->w = es->zone->w;
        if (!es->fit_size) es->h = size;
        es->x = (es->zone->w - es->w) / 2;
        break;
      case E_GADCON_ORIENT_VERT:
        if (!es->fit_along) es->h = es->zone->h;
        if (!es->fit_size) es->w = size;
        es->y = (es->zone->h - es->h) / 2;
        break;
      case E_GADCON_ORIENT_LEFT:
        if (!es->fit_along) es->h = es->zone->h;
        if (!es->fit_size) es->w = size;
        es->x = 0;
        es->y = (es->zone->h - es->h) / 2;
        break;
      case E_GADCON_ORIENT_RIGHT:
        if (!es->fit_along) es->h = es->zone->h;
        if (!es->fit_size) es->w = size;
        es->x = (es->zone->w - es->w);
        es->y = (es->zone->h - es->h) / 2;
        break;
      case E_GADCON_ORIENT_TOP:
        if (!es->fit_along) es->w = es->zone->w;
        if (!es->fit_size) es->h = size;
        es->x = (es->zone->w - es->w) / 2;
        es->y = 0;
        break;
      case E_GADCON_ORIENT_BOTTOM:
        if (!es->fit_along) es->w = es->zone->w;
        if (!es->fit_size) es->h = size;
        es->x = (es->zone->w - es->w) / 2;
        es->y = (es->zone->h - es->h);
        break;
      case E_GADCON_ORIENT_CORNER_TL:
        if (!es->fit_along) es->w = es->zone->w;
        if (!es->fit_size) es->h = size;
        es->x = 0;
        es->y = 0;
        break;
      case E_GADCON_ORIENT_CORNER_TR:
        if (!es->fit_along) es->w = es->zone->w;
        if (!es->fit_size) es->h = size;
        es->x = (es->zone->w - es->w);
        es->y = 0;
        break;
      case E_GADCON_ORIENT_CORNER_BL:
        if (!es->fit_along) es->w = es->zone->w;
        if (!es->fit_size) es->h = size;
        es->x = 0;
        es->y = (es->zone->h - es->h);
        break;
      case E_GADCON_ORIENT_CORNER_BR:
        if (!es->fit_along) es->w = es->zone->w;
        if (!es->fit_size) es->h = size;
        es->x = (es->zone->w - es->w);
        es->y = (es->zone->h - es->h);
        break;
      case E_GADCON_ORIENT_CORNER_LT:
        if (!es->fit_along) es->h = es->zone->h;
        if (!es->fit_size) es->w = size;
        es->x = 0;
        es->y = 0;
        break;
      case E_GADCON_ORIENT_CORNER_RT:
        if (!es->fit_along) es->h = es->zone->h;
        if (!es->fit_size) es->w = size;
        es->x = (es->zone->w - es->w);
        es->y = 0;
        break;
      case E_GADCON_ORIENT_CORNER_LB:
        if (!es->fit_along) es->h = es->zone->h;
        if (!es->fit_size) es->w = size;
        es->x = 0;
        es->y = (es->zone->h - es->h);
        break;
      case E_GADCON_ORIENT_CORNER_RB:
        if (!es->fit_along) es->h = es->zone->h;
        if (!es->fit_size) es->w = size;
        es->x = (es->zone->w - es->w);
        es->y = (es->zone->h - es->h);
        break;
      default:
        break;
     }

   /* TODO: hide_step, hide_origin */

   e_shelf_move_resize(es, es->x, es->y, es->w, es->h);
   if (es->hidden)
     {
        es->hidden = EINA_FALSE;
        e_shelf_toggle(es, EINA_FALSE);
     }
}

EAPI void 
e_shelf_move_resize(E_Shelf *es, int x, int y, int w, int h)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_SHELF_TYPE);

   es->x = x;
   es->y = y;
   es->w = w;
   es->h = h;
   if (es->popup)
     e_popup_move_resize(es->popup, es->x, es->y, es->w, es->h);
   else
     {
        evas_object_move(es->o_event, es->zone->x + es->x, 
                         es->zone->y + es->y);
        evas_object_move(es->o_base, es->zone->x + es->x, 
                         es->zone->y + es->y);
     }
   evas_object_resize(es->o_event, es->w, es->h);
   evas_object_resize(es->o_base, es->w, es->h);
}

EAPI Eina_List *
e_shelf_list(void)
{
   return _shelves;
}

EAPI const char *
e_shelf_orient_string_get(E_Shelf *es)
{
   const char *sig = "";

   switch (es->gadcon->orient)
     {
      case E_GADCON_ORIENT_FLOAT:
	sig = "float";
	break;
      case E_GADCON_ORIENT_HORIZ:
	sig = "horizontal";
	break;
      case E_GADCON_ORIENT_VERT:
	sig = "vertical";
	break;
      case E_GADCON_ORIENT_LEFT:
	sig = "left";
	break;
      case E_GADCON_ORIENT_RIGHT:
	sig = "right";
	break;
      case E_GADCON_ORIENT_TOP:
	sig = "top";
	break;
      case E_GADCON_ORIENT_BOTTOM:
	sig = "bottom";
	break;
      case E_GADCON_ORIENT_CORNER_TL:
	sig = "top_left";
	break;
      case E_GADCON_ORIENT_CORNER_TR:
	sig = "top_right";
	break;
      case E_GADCON_ORIENT_CORNER_BL:
	sig = "bottom_left";
	break;
      case E_GADCON_ORIENT_CORNER_BR:
	sig = "bottom_right";
	break;
      case E_GADCON_ORIENT_CORNER_LT:
	sig = "left_top";
	break;
      case E_GADCON_ORIENT_CORNER_RT:
	sig = "right_top";
	break;
      case E_GADCON_ORIENT_CORNER_LB:
	sig = "left_bottom";
	break;
      case E_GADCON_ORIENT_CORNER_RB:
	sig = "right_bottom";
	break;
      default:
	break;
     }
   return sig;
}

EAPI void 
e_shelf_orient(E_Shelf *es, E_Gadcon_Orient orient)
{
   char buf[PATH_MAX];

   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_SHELF_TYPE);

   e_gadcon_orient(es->gadcon, orient);
   snprintf(buf, sizeof(buf), "e,state,orientation,%s", 
            e_shelf_orient_string_get(es));
   edje_object_signal_emit(es->o_base, buf, "e");
   edje_object_message_signal_process(es->o_base);
   e_gadcon_location_set_icon_name(es->gadcon->location, _e_shelf_orient_icon_name_get(es));
}

EAPI void
e_shelf_locked_set(E_Shelf *es, int lock)
{
   if (lock)
     {
	e_shelf_toggle(es, 1);
	es->locked++;
     }
   else
     {
       if (es->locked > 0)
	 es->locked--;
       if (!es->locked)
	 e_shelf_toggle(es, es->toggle);
     }
}

EAPI void
e_shelf_urgent_show(E_Shelf *es)
{
   e_shelf_toggle(es, 1);
   es->urgent_show = 1;
}

/* local functions */
static void 
_e_shelf_cb_free(E_Shelf *es)
{
   _shelves = eina_list_remove(_shelves, es);
   if (es->name) eina_stringshare_del(es->name);
   if (es->style) eina_stringshare_del(es->style);
   if (es->o_event) evas_object_del(es->o_event);
   if (es->o_base) evas_object_del(es->o_base);
   free(es);
}

static void
_e_shelf_gadcon_min_size_request(void *data __UNUSED__, E_Gadcon *gc __UNUSED__, Evas_Coord w __UNUSED__, Evas_Coord h __UNUSED__)
{
   return;
}

static void
_e_shelf_gadcon_size_request(void *data, E_Gadcon *gc, Evas_Coord w, Evas_Coord h)
{
   E_Shelf *es;
   Evas_Coord nx, ny, nw, nh, ww, hh, wantw, wanth;

   es = data;
   nx = es->x;
   ny = es->y;
   nw = es->w;
   nh = es->h;
   ww = hh = 0;
   evas_object_geometry_get(gc->o_container, NULL, NULL, &ww, &hh);
   switch (gc->orient)
     {
      case E_GADCON_ORIENT_FLOAT:
      case E_GADCON_ORIENT_HORIZ:
      case E_GADCON_ORIENT_TOP:
      case E_GADCON_ORIENT_BOTTOM:
      case E_GADCON_ORIENT_CORNER_TL:
      case E_GADCON_ORIENT_CORNER_TR:
      case E_GADCON_ORIENT_CORNER_BL:
      case E_GADCON_ORIENT_CORNER_BR:
	if (!es->fit_along) w = ww;
	if (!es->fit_size) h = hh;
	break;
      case E_GADCON_ORIENT_VERT:
      case E_GADCON_ORIENT_LEFT:
      case E_GADCON_ORIENT_RIGHT:
      case E_GADCON_ORIENT_CORNER_LT:
      case E_GADCON_ORIENT_CORNER_RT:
      case E_GADCON_ORIENT_CORNER_LB:
      case E_GADCON_ORIENT_CORNER_RB:
	if (!es->fit_along) h = hh;
	if (!es->fit_size) w = ww;
	break;
      default:
	break;
     }
   e_gadcon_swallowed_min_size_set(gc, w, h);
   edje_object_size_min_calc(es->o_base, &nw, &nh);
   wantw = nw;
   wanth = nh;
   switch (gc->orient)
     {
      case E_GADCON_ORIENT_FLOAT:
	if (!es->fit_along) nw = es->w;
	if (!es->fit_size) nh = es->h;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nw != es->w) nx = es->x + ((es->w - nw) / 2);
	break;
      case E_GADCON_ORIENT_HORIZ:
	if (!es->fit_along) nw = es->w;
	if (!es->fit_size) nh = es->h;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nw != es->w) nx = es->x + ((es->w - nw) / 2);
	break;
      case E_GADCON_ORIENT_VERT:
	if (!es->fit_along) nh = es->h;
	if (!es->fit_size) nw = es->w;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nh != es->h) ny = es->y + ((es->h - nh) / 2);
	break;
      case E_GADCON_ORIENT_LEFT:
	if (!es->fit_along) nh = es->h;
	if (!es->fit_size) nw = es->w;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nh != es->h) ny = (es->zone->h - nh) / 2;
	// nx = 0;
	break;
      case E_GADCON_ORIENT_RIGHT:
	if (!es->fit_along) nh = es->h;
	if (!es->fit_size) nw = es->w;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nh != es->h) ny = (es->zone->h - nh) / 2;
	// nx = es->zone->w - nw;
	break;
      case E_GADCON_ORIENT_TOP:
	if (!es->fit_along) nw = es->w;
	if (!es->fit_size) nh = es->h;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nw != es->w) nx = (es->zone->w - nw) / 2;
	// ny = 0;
	break;
      case E_GADCON_ORIENT_BOTTOM:
	if (!es->fit_along) nw = es->w;
	if (!es->fit_size) nh = es->h;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nw != es->w) nx = (es->zone->w - nw) / 2;
	//ny = es->zone->h - nh;
	break;
      case E_GADCON_ORIENT_CORNER_TL:
	if (!es->fit_along) nw = es->w;
	if (!es->fit_size) nh = es->h;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nw != es->w) nx = 0;
	// ny = 0;
	break;
      case E_GADCON_ORIENT_CORNER_TR:
	if (!es->fit_along) nw = es->w;
	if (!es->fit_size) nh = es->h;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	nx = es->zone->w - nw;
	// ny = 0;
	break;
      case E_GADCON_ORIENT_CORNER_BL:
	if (!es->fit_along) nw = es->w;
	if (!es->fit_size) nh = es->h;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nw != es->w) nx = 0;
	// ny = es->zone->h - nh;
	break;
      case E_GADCON_ORIENT_CORNER_BR:
	if (!es->fit_along) nw = es->w;
	if (!es->fit_size) nh = es->h;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	nx = es->zone->w - nw;
	//ny = es->zone->h - nh;
	break;
      case E_GADCON_ORIENT_CORNER_LT:
	if (!es->fit_along) nh = es->h;
	if (!es->fit_size) nw = es->w;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nh != es->h) ny = 0;
	// nx = 0;
	break;
      case E_GADCON_ORIENT_CORNER_RT:
	if (!es->fit_along) nh = es->h;
	if (!es->fit_size) nw = es->w;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nh != es->h) ny = 0;
	// nx = es->zone->w - nw;
	break;
      case E_GADCON_ORIENT_CORNER_LB:
	if (!es->fit_along) nh = es->h;
	if (!es->fit_size) nw = es->w;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nh != es->h) ny = es->zone->h - nh;
	// nx = 0;
	break;
      case E_GADCON_ORIENT_CORNER_RB:
	if (!es->fit_along) nh = es->h;
	if (!es->fit_size) nw = es->w;
	if (nw > es->zone->w) nw = es->zone->w;
	if (nh > es->zone->h) nh = es->zone->h;
	if (nh != es->h) ny = es->zone->h - nh;
	// nx = es->zone->w - nw;
	break;
      default:
	break;
     }
   w -= (wantw - nw);
   h -= (wanth - nh);
   e_gadcon_swallowed_min_size_set(gc, w, h);
   e_shelf_move_resize(es, nx, ny, nw, nh);
}

static Evas_Object *
_e_shelf_gadcon_frame_request(void *data, E_Gadcon_Client *gcc, const char *style)
{
   E_Shelf *es;
   Evas_Object *o;
   char buf[PATH_MAX];

   es = data;
   o = edje_object_add(gcc->gadcon->evas);

   snprintf(buf, sizeof(buf), "e/shelf/%s/%s", es->style, style);
   if (!e_theme_edje_object_set(o, "base/theme/shelf", buf))
     {
	/* if an inset style (e.g. plain) isn't implemented for a given
	 * shelf style, fall back to the default one. no need for every
	 * theme to implement the plain style */
	snprintf(buf, sizeof(buf), "e/shelf/default/%s", style);
	if (!e_theme_edje_object_set(o, "base/theme/shelf", buf))
	  {
	     evas_object_del(o);
	     return NULL;
	  }
     }
   snprintf(buf, sizeof(buf), "e,state,orientation,%s", 
            e_shelf_orient_string_get(es));
   edje_object_signal_emit(es->o_base, buf, "e");
   edje_object_message_signal_process(o);
   return o;
}

static void
_e_shelf_cb_locked_set(void *data, int lock)
{
   E_Shelf *es;

   es = data;
   e_shelf_locked_set(es, lock);
}

static void
_e_shelf_cb_urgent_show(void *data)
{
   E_Shelf *es;

   es = data;
   e_shelf_urgent_show(es);
}

static void
_e_shelf_gadcon_client_remove(void *data, E_Gadcon_Client *gcc)
{
   E_Shelf *s;
   E_Gadcon *gc;

   s = data;
   gc = s->gadcon;
   e_gadcon_client_config_del(gc->cf, gcc->cf);
   e_gadcon_unpopulate(gc);
   e_gadcon_populate(gc);
   e_config_save_queue();
}

static int
_e_shelf_gadcon_client_add(void *data, const E_Gadcon_Client_Class *cc)
{
   E_Shelf *s;
   E_Gadcon *gc;

   s = data;
   gc = s->gadcon;
   if (!e_gadcon_client_config_new(gc, cc->name)) return 0;
   e_gadcon_unpopulate(gc);
   e_gadcon_populate(gc);
   e_config_save_queue();
   return 1;
}

static void
_e_shelf_cb_menu_items_append(void *data, E_Gadcon_Client *gcc __UNUSED__, E_Menu *mn)
{
   E_Shelf *es;

   es = data;
   _e_shelf_menu_append(es, mn);
}

static void
_e_shelf_menu_append(E_Shelf *es, E_Menu *mn)
{
   E_Menu_Item *mi;
   E_Menu *subm;
   const char *name;
   char buf[256];

   name = e_shelf_orient_string_get (es);
   snprintf(buf, sizeof(buf), "Shelf %s", name);

   e_shelf_locked_set(es, 1);

   subm = e_menu_new();
   mi = e_menu_item_new(mn);
   e_menu_item_label_set(mi, buf);
   e_util_menu_item_theme_icon_set(mi, "preferences-desktop-shelf");
   e_menu_pre_activate_callback_set(subm, _e_shelf_menu_pre_cb, es);
   e_object_free_attach_func_set(E_OBJECT(mi), _e_shelf_menu_item_free);
   e_object_data_set(E_OBJECT(mi), es);
   e_menu_item_submenu_set(mi, subm);
}

static void 
_e_shelf_menu_pre_cb(void *data, E_Menu *m) 
{
   E_Shelf *es;
   E_Menu_Item *mi;

   es = data;
   e_menu_pre_activate_callback_set(m, NULL, NULL);

   mi = e_menu_item_new(m);
   if (es->gadcon->editing)
     e_menu_item_label_set(mi, _("Stop Moving/Resizing Gadgets"));
   else
     e_menu_item_label_set(mi, _("Begin Moving/Resizing Gadgets"));
   e_util_menu_item_theme_icon_set(mi, "transform-scale");
   e_menu_item_callback_set(mi, _e_shelf_cb_menu_edit, es);

   mi = e_menu_item_new(m);
   e_menu_item_separator_set(mi, 1);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Contents"));
   e_util_menu_item_theme_icon_set(mi, "preferences-desktop-shelf");
   e_menu_item_callback_set(mi, _e_shelf_cb_menu_contents, es);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Settings"));
   e_util_menu_item_theme_icon_set(mi, "configure");
   e_menu_item_callback_set(mi, _e_shelf_cb_menu_config, es);

   mi = e_menu_item_new(m);
   e_menu_item_separator_set(mi, 1);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Delete"));
   e_util_menu_item_theme_icon_set(mi, "list-remove");
   e_menu_item_callback_set(mi, _e_shelf_cb_menu_delete, es);   
}

static void
_e_shelf_cb_menu_config(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   E_Shelf *es;

   es = data;
//   if (!es->config_dialog) e_int_shelf_config(es);
}

static void
_e_shelf_cb_menu_edit(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   E_Shelf *es;

   es = data;
   if (es->gadcon->editing)
     {
	e_gadcon_edit_end(es->gadcon);
	e_shelf_toggle(es, 0);
     }
   else
     {
	e_shelf_toggle(es, 1);
	e_gadcon_edit_begin(es->gadcon);
     }
}

static void
_e_shelf_cb_menu_contents(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   E_Shelf *es;

   es = data;
//   if (!es->gadcon->config_dialog) e_int_gadcon_config_shelf(es->gadcon);
}

static void
_e_shelf_cb_confirm_dialog_destroy(void *data)
{
   E_Shelf *es;

   es = data;
   e_object_unref(E_OBJECT(es));
}

static void
_e_shelf_cb_confirm_dialog_yes(void *data)
{
   E_Config_Shelf *cfg;
   E_Shelf *es;

   es = data;
   cfg = es->cfg;
   if (e_object_is_del(E_OBJECT(es))) return;
   e_object_del(E_OBJECT(es));
   e_config->shelves = eina_list_remove(e_config->shelves, cfg);
   if (cfg->name) eina_stringshare_del(cfg->name);
   if (cfg->style) eina_stringshare_del(cfg->style);
   E_FREE(cfg);

   e_config_save_queue();
}

static void
_e_shelf_cb_menu_delete(void *data, E_Menu *m __UNUSED__, E_Menu_Item *mi __UNUSED__)
{
   E_Shelf *es;
   E_Config_Shelf *cfg;

   es = data;
   if (e_config->cnfmdlg_disabled) 
     {
	cfg = es->cfg;
	if (e_object_is_del(E_OBJECT(es))) return;
	e_object_del(E_OBJECT(es));
	e_config->shelves = eina_list_remove(e_config->shelves, cfg);
	if (cfg->name) eina_stringshare_del(cfg->name);
	if (cfg->style) eina_stringshare_del(cfg->style);
	E_FREE(cfg);

	e_config_save_queue();
	return;
     }

   e_object_ref(E_OBJECT(es));
   /* e_confirm_dialog_show(_("Are you sure you want to delete this shelf?"), "enlightenment", */
   /* 			 _("You requested to delete this shelf.<br>" */
   /* 			      "<br>" */
   /* 			      "Are you sure you want to delete it?"), NULL, NULL, */
   /* 			 _e_shelf_cb_confirm_dialog_yes, NULL, data, NULL,  */
   /* 			 _e_shelf_cb_confirm_dialog_destroy, data); */
}

static void
_e_shelf_menu_item_free(void *data)
{
   E_Shelf *es;

   es = e_object_data_get(data);
   e_shelf_locked_set(es, 0);
}

static const char *
_e_shelf_orient_icon_name_get(E_Shelf *s)
{
   const char *name = NULL;

   switch (s->cfg->orient) 
     {
      case E_GADCON_ORIENT_LEFT:
	name = "preferences-position-left";
	break;
      case E_GADCON_ORIENT_RIGHT:
	name = "preferences-position-right";
	break;
      case E_GADCON_ORIENT_TOP:
	name = "preferences-position-top";
	break;
      case E_GADCON_ORIENT_BOTTOM:
	name = "preferences-position-bottom";
	break;
      case E_GADCON_ORIENT_CORNER_TL:
	name = "preferences-position-top-left";
	break;
      case E_GADCON_ORIENT_CORNER_TR:
	name = "preferences-position-top-right";
	break;
      case E_GADCON_ORIENT_CORNER_BL:
	name = "preferences-position-bottom-left";
	break;
      case E_GADCON_ORIENT_CORNER_BR:
	name = "preferences-position-bottom-right";
	break;
      case E_GADCON_ORIENT_CORNER_LT:
	name = "preferences-position-left-top";
	break;
      case E_GADCON_ORIENT_CORNER_RT:
	name = "preferences-position-right-top";
	break;
      case E_GADCON_ORIENT_CORNER_LB:
	name = "preferences-position-left-bottom";
	break;
      case E_GADCON_ORIENT_CORNER_RB:
	name = "preferences-position-right-bottom";
	break;
      default:
	name = "preferences-desktop-shelf";
	break;
     }
   return name;
}
