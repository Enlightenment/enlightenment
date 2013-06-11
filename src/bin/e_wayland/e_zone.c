#include "e.h"

/* local function prototypes */
static void _e_zone_cb_free(E_Zone *zone);
static void _e_zone_cb_bg_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);
static void _e_zone_cb_bg_mouse_up(void *data, Evas *evas, Evas_Object *obj, void *event);
static void _e_zone_cb_menu_deactivate(void *data EINA_UNUSED, E_Menu *m);

EINTERN int 
e_zone_init(void)
{
   return 1;
}

EINTERN int 
e_zone_shutdown(void)
{
   return 1;
}

EAPI E_Zone *
e_zone_new(E_Container *con, int num, int x, int y, int w, int h)
{
   E_Zone *zone;
   char name[40];

   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, NULL);

   zone = E_OBJECT_ALLOC(E_Zone, E_ZONE_TYPE, _e_zone_cb_free);
   if (!zone)
     {
        printf("FAILED TO ALLOCATE ZONE\n");
        return NULL;
     }

   zone->container = con;
   zone->x = x;
   zone->y = y;
   zone->w = w;
   zone->h = h;
   zone->num = num;
   zone->desk_x_count = 0;
   zone->desk_y_count = 0;
   zone->desk_x_current = 0;
   zone->desk_y_current = 0;

   zone->o_bg_clip = evas_object_rectangle_add(zone->container->bg_evas);
   evas_object_move(zone->o_bg_clip, zone->x, zone->y);
   evas_object_resize(zone->o_bg_clip, zone->w, zone->h);
   evas_object_color_set(zone->o_bg_clip, 255, 255, 255, 255);

   zone->o_bg_event = evas_object_rectangle_add(zone->container->bg_evas);
   evas_object_clip_set(zone->o_bg_event, zone->o_bg_clip);
   evas_object_move(zone->o_bg_event, zone->x, zone->y);
   evas_object_resize(zone->o_bg_event, zone->w, zone->h);
   evas_object_color_set(zone->o_bg_event, 0, 0, 0, 0);
   evas_object_repeat_events_set(zone->o_bg_event, 1);

   evas_object_event_callback_add(zone->o_bg_event, EVAS_CALLBACK_MOUSE_DOWN, 
                                  _e_zone_cb_bg_mouse_down, zone);
   evas_object_event_callback_add(zone->o_bg_event, EVAS_CALLBACK_MOUSE_UP, 
                                  _e_zone_cb_bg_mouse_up, zone);

   e_zone_desk_count_set(zone, e_config->zone_desks_x_count, 
                         e_config->zone_desks_y_count);

   /* TODO: add event handlers */

   snprintf(name, sizeof(name), "Zone %d", zone->num);
   zone->name = eina_stringshare_add(name);

   con->zones = eina_list_append(con->zones, zone);

   return zone;
}

EAPI void 
e_zone_desk_count_set(E_Zone *zone, int x, int y)
{
   E_Desk **new_desks;
   E_Desk *desk;
   int xx = 0, yy = 0, nx = 0, ny = 0;
   Eina_Bool moved = EINA_FALSE;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   xx = x;
   if (xx < 1) xx = 1;
   yy = y;
   if (yy < 1) yy = 1;

   if (zone->desk_x_current >= xx) moved = EINA_TRUE;
   if (zone->desk_y_current >= yy) moved = EINA_TRUE;

   if (moved)
     {
        nx = zone->desk_x_current;
        ny = zone->desk_y_current;
        if (zone->desk_x_current >= xx) nx = xx - 1;
        if (zone->desk_y_current >= yy) ny = yy - 1;
        if (zone->visible)
          e_desk_show(e_desk_at_xy_get(zone, nx, ny));
     }

   new_desks = malloc(xx * yy * sizeof(E_Desk *));
   for (x = 0; x < xx; x++)
     {
        for (y = 0; y < yy; y++)
          {
             if ((x < zone->desk_x_count) && (y < zone->desk_y_count))
               desk = zone->desks[x + (y * zone->desk_x_count)];
             else
               desk = e_desk_new(zone, x, y);
             new_desks[x + (y * xx)] = desk;
          }
     }

   if (zone->desks) free(zone->desks);
   zone->desks = new_desks;

   zone->desk_x_count = xx;
   zone->desk_y_count = yy;
   e_config->zone_desks_x_count = xx;
   e_config->zone_desks_y_count = yy;
   e_config_save_queue();

   /* Cannot call desk_current_get until the zone desk counts have been set
    * or else we end up with a "white background" because desk_current_get will
    * return NULL.
    */
   desk = e_desk_current_get(zone);
   if (desk)
     {
        desk->visible = 0;
        if (zone->visible) e_desk_show(desk);
     }
}

EAPI void 
e_zone_show(E_Zone *zone)
{
   int x = 0, y = 0;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if (zone->visible) return;

   evas_object_show(zone->o_bg_clip);
   evas_object_show(zone->o_bg_event);
   if (zone->o_bg) evas_object_show(zone->o_bg);

   for (x = 0; x < zone->desk_x_count; x++)
     {
        for (y = 0; y < zone->desk_y_count; y++)
          e_desk_show(zone->desks[x + (y * zone->desk_x_count)]);
     }

   zone->visible = EINA_TRUE;
}

EAPI void 
e_zone_hide(E_Zone *zone)
{
   int x = 0, y = 0;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if (!zone->visible) return;

   for (x = 0; x < zone->desk_x_count; x++)
     {
        for (y = 0; y < zone->desk_y_count; y++)
          e_desk_hide(zone->desks[x + (y * zone->desk_x_count)]);
     }

   evas_object_hide(zone->o_bg_event);
   evas_object_hide(zone->o_bg_clip);
   if (zone->o_bg) evas_object_hide(zone->o_bg);

   zone->visible = EINA_FALSE;
}

EAPI E_Zone *
e_zone_current_get(E_Container *con)
{
   Eina_List *l = NULL;
   E_Zone *zone;

   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, NULL);

   if (!starting)
     {
        int x, y;

        ecore_wl_pointer_xy_get(&x, &y);
        EINA_LIST_FOREACH(con->zones, l, zone)
          {
             if (E_INSIDE(x, y, zone->x, zone->y, zone->w, zone->h))
               return zone;
          }
     }
   if (!con->zones) return NULL;
   return (E_Zone *)eina_list_data_get(con->zones);
}

EAPI void 
e_zone_bg_reconfigure(E_Zone *zone)
{
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   e_bg_zone_update(zone, E_BG_TRANSITION_CHANGE);
}

EAPI void 
e_zone_useful_geometry_get(E_Zone *zone, int *x, int *y, int *w, int *h)
{
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if (x) *x = zone->x;
   if (y) *y = zone->y;
   if (w) *w = zone->w;
   if (h) *h = zone->h;
}

EAPI void 
e_zone_desk_count_get(E_Zone *zone, int *x, int *y)
{
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if (x) *x = zone->desk_x_count;
   if (y) *y = zone->desk_y_count;
}

/* local functions */
static void 
_e_zone_cb_free(E_Zone *zone)
{
   E_Container *con;
   int x = 0, y = 0;

   if (zone->name) eina_stringshare_del(zone->name);

   evas_object_event_callback_del(zone->o_bg_event, EVAS_CALLBACK_MOUSE_DOWN, 
                                  _e_zone_cb_bg_mouse_down);
   evas_object_event_callback_del(zone->o_bg_event, EVAS_CALLBACK_MOUSE_UP, 
                                  _e_zone_cb_bg_mouse_up);

   con = zone->container;
   con->zones = eina_list_remove(con->zones, zone);

   evas_object_del(zone->o_bg_event);
   evas_object_del(zone->o_bg_clip);
   evas_object_del(zone->o_bg);
   if (zone->o_prev_bg) evas_object_del(zone->o_prev_bg);
   if (zone->o_trans) evas_object_del(zone->o_trans);

   for (x = 0; x < zone->desk_x_count; x++)
     {
        for (y = 0; y < zone->desk_y_count; y++)
          e_object_del(E_OBJECT(zone->desks[x + (y * zone->desk_x_count)]));
     }

   free(zone->desks);
   free(zone);
}

static void 
_e_zone_cb_bg_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Zone *zone;

   if (!(zone = data)) return;
   printf("Zone Mouse Down\n");
}

static void 
_e_zone_cb_bg_mouse_up(void *data, Evas *evas, Evas_Object *obj, void *event)
{
   E_Zone *zone;
   E_Menu *m;
   int x, y;

   if (!(zone = data)) return;
   if (!(m = e_int_menus_main_new())) return;

   m->zone = zone;

   ecore_wl_pointer_xy_get(&x, &y);
   printf("Zone Mouse Up: %d %d\n", x, y);
   e_menu_post_deactivate_callback_set(m, _e_zone_cb_menu_deactivate, NULL);
   e_menu_activate_mouse(m, zone, x, y, 1, 1, E_MENU_POP_DIRECTION_AUTO, 0);
}

static void 
_e_zone_cb_menu_deactivate(void *data EINA_UNUSED, E_Menu *m)
{
   e_object_del(E_OBJECT(m));
}
