#include "e.h"

/* TODO List:
 *
 * * fix shape callbacks to be able to be safely deleted
 * * remove duplicate bd->layer -> layers code
 *
 */

/* local subsystem functions */
static void         _e_container_free(E_Container *con);

static void    _e_container_cb_mouse_in(E_Container *con, Evas *e, Evas_Object *obj, void *event_info);
static void    _e_container_cb_mouse_down(E_Container *con, Evas *e, Evas_Object *obj, void *event_info);
static void    _e_container_cb_mouse_up(E_Container *con, Evas *e, Evas_Object *obj, void *event_info);
static void    _e_container_cb_mouse_wheel(E_Container *con, Evas *e, Evas_Object *obj, void *event_info);

static void         _e_container_shape_del(E_Container_Shape *es);
static void         _e_container_shape_free(E_Container_Shape *es);
static void         _e_container_shape_change_call(E_Container_Shape *es, E_Container_Shape_Change ch);
static void         _e_container_resize_handle(E_Container *con);
static void         _e_container_event_container_resize_free(void *data, void *ev);

EAPI int E_EVENT_CONTAINER_RESIZE = 0;
static Eina_List *handlers = NULL;

/* externally accessible functions */
EINTERN int
e_container_init(void)
{
   E_EVENT_CONTAINER_RESIZE = ecore_event_type_new();
   return 1;
}

EINTERN int
e_container_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
   return 1;
}

EAPI E_Container *
e_container_new(E_Manager *man)
{
   E_Container *con;
   Evas_Object *o;
   char name[40];
   Eina_List *l, *screens;
   int i;
   Ecore_X_Window mwin;
   E_Comp *c;
   static int container_num = 0;

   con = E_OBJECT_ALLOC(E_Container, E_CONTAINER_TYPE, _e_container_free);
   if (!con) return NULL;
   con->manager = man;
   con->manager->containers = eina_list_append(con->manager->containers, con);
   con->w = con->manager->w;
   con->h = con->manager->h;
   con->win = con->manager->win;

   c = e_comp_get(man);
   con->bg_win = c->ee_win;
   con->bg_ecore_evas = c->ee;
   con->event_win = c->ee_win;
   con->bg_evas = c->evas;

   o = evas_object_rectangle_add(con->bg_evas);
   con->bg_blank_object = o;
   evas_object_layer_set(o, E_COMP_CANVAS_LAYER_BOTTOM);
   evas_object_move(o, 0, 0);
   evas_object_resize(o, con->w, con->h);
   evas_object_color_set(o, 255, 255, 255, 255);
   evas_object_name_set(o, "container->bg_blank_object");
   evas_object_data_set(o, "e_container", con);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN, (Evas_Object_Event_Cb)_e_container_cb_mouse_down, con);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP, (Evas_Object_Event_Cb)_e_container_cb_mouse_up, con);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_IN, (Evas_Object_Event_Cb)_e_container_cb_mouse_in, con);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_WHEEL, (Evas_Object_Event_Cb)_e_container_cb_mouse_wheel, con);
   evas_object_show(o);

   con->num = container_num;
   container_num++;
   snprintf(name, sizeof(name), _("Container %d"), con->num);
   con->name = eina_stringshare_add(name);

   /* create a scratch window for putting stuff into */
   con->scratch_win = ecore_x_window_override_new(con->win, 0, 0, 7, 7);

   /* init layers */
   for (i = 0; i < 12; i++)
     {
        con->layers[i].win = ecore_x_window_input_new(con->win, 0, 0, 1, 1);
        ecore_x_window_lower(con->layers[i].win);

        if (i > 0)
          ecore_x_window_configure(con->layers[i].win,
                                   ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                                   ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                                   0, 0, 0, 0, 0,
                                   con->layers[i - 1].win, ECORE_X_WINDOW_STACK_ABOVE);
     }

   /* Put init win on top */
   if (man->initwin)
     ecore_x_window_configure(man->initwin,
                              ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                              ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                              0, 0, 0, 0, 0,
                              con->layers[11].win, ECORE_X_WINDOW_STACK_ABOVE);

   /* Put menu win on top */
   mwin = e_menu_grab_window_get();
   if (mwin)
     ecore_x_window_configure(mwin,
                              ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                              ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                              0, 0, 0, 0, 0,
                              con->layers[11].win, ECORE_X_WINDOW_STACK_ABOVE);

   screens = (Eina_List *)e_xinerama_screens_get();
   if (screens)
     {
        E_Screen *scr;
        EINA_LIST_FOREACH(screens, l, scr)
          {
             e_zone_new(con, scr->screen, scr->escreen, scr->x, scr->y, scr->w, scr->h);
          }
     }
   else
     e_zone_new(con, 0, 0, 0, 0, con->w, con->h);
   return con;
}

EAPI void
e_container_show(E_Container *con)
{
   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);

   if (con->visible) return;
   if (con->win != con->manager->win)
     ecore_x_window_show(con->win);
   con->visible = 1;
}

EAPI void
e_container_hide(E_Container *con)
{
   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);

   if (!con->visible) return;
   if (con->win != con->manager->win)
     ecore_x_window_hide(con->win);
   con->visible = 0;
}

EAPI E_Container *
e_container_current_get(E_Manager *man)
{
   Eina_List *l;
   E_Container *con;
   E_OBJECT_CHECK_RETURN(man, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(man, E_MANAGER_TYPE, NULL);

   EINA_LIST_FOREACH(man->containers, l, con)
     {
        if (!con) continue;
        if (con->visible) return con;
     }

   /* If no one is available, return the first */
   if (!man->containers) return NULL;
   l = man->containers;
   return (E_Container *)eina_list_data_get(l);
}

EAPI E_Container *
e_container_number_get(E_Manager *man, int num)
{
   Eina_List *l;
   E_Container *con;

   E_OBJECT_CHECK_RETURN(man, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(man, E_MANAGER_TYPE, NULL);
   EINA_LIST_FOREACH(man->containers, l, con)
     {
        if ((int)con->num == num) return con;
     }
   return NULL;
}

EAPI void
e_container_move(E_Container *con, int x, int y)
{
   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);
   if ((x == con->x) && (y == con->y)) return;
   con->x = x;
   con->y = y;
   if (con->win != con->manager->win)
     ecore_x_window_move(con->win, con->x, con->y);
   evas_object_move(con->bg_blank_object, con->x, con->y);
}

EAPI void
e_container_resize(E_Container *con, int w, int h)
{
   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);
   con->w = w;
   con->h = h;
   if (con->win != con->manager->win)
     ecore_x_window_resize(con->win, con->w, con->h);
   ecore_x_window_resize(con->event_win, con->w, con->h);
   if (!e_config->null_container_win)
     ecore_evas_resize(con->bg_ecore_evas, con->w, con->h);
   evas_object_resize(con->bg_blank_object, con->w, con->h);
   _e_container_resize_handle(con);
}

EAPI void
e_container_move_resize(E_Container *con, int x, int y, int w, int h)
{
   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);
   con->x = x;
   con->y = y;
   con->w = w;
   con->h = h;
   if (con->win != con->manager->win)
     ecore_x_window_move_resize(con->win, con->x, con->y, con->w, con->h);
   ecore_x_window_move_resize(con->event_win, con->x, con->y, con->w, con->h);
   if (!e_config->null_container_win)
     ecore_evas_move_resize(con->bg_ecore_evas, x, y, w, h);
   evas_object_move(con->bg_blank_object, con->x, con->y);
   evas_object_resize(con->bg_blank_object, con->w, con->h);
   _e_container_resize_handle(con);
}

EAPI void
e_container_raise(E_Container *con __UNUSED__)
{
//   E_OBJECT_CHECK(con);
//   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);
}

EAPI void
e_container_lower(E_Container *con __UNUSED__)
{
//   E_OBJECT_CHECK(con);
//   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);
}

EAPI E_Zone *
e_container_zone_at_point_get(E_Container *con, int x, int y)
{
   Eina_List *l = NULL;
   E_Zone *zone = NULL;

   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, NULL);
   EINA_LIST_FOREACH(con->zones, l, zone)
     {
        if (E_INSIDE(x, y, zone->x, zone->y, zone->w, zone->h))
          return zone;
     }
   return NULL;
}

EAPI E_Zone *
e_container_zone_number_get(E_Container *con, int num)
{
   Eina_List *l = NULL;
   E_Zone *zone = NULL;

   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, NULL);
   EINA_LIST_FOREACH(con->zones, l, zone)
     {
        if ((int)zone->num == num) return zone;
     }
   return NULL;
}

EAPI E_Zone *
e_container_zone_id_get(E_Container *con, int id)
{
   Eina_List *l = NULL;
   E_Zone *zone = NULL;

   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, NULL);
   EINA_LIST_FOREACH(con->zones, l, zone)
     {
        if (zone->id == id) return zone;
     }
   return NULL;
}

EAPI E_Desk *
e_container_desk_window_profile_get(E_Container *con,
                                    const char *profile)
{
   Eina_List *l = NULL;
   E_Zone *zone = NULL;
   int x, y;

   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, NULL);

   EINA_LIST_FOREACH(con->zones, l, zone)
     {
        for (x = 0; x < zone->desk_x_count; x++)
          {
             for (y = 0; y < zone->desk_y_count; y++)
               {
                  E_Desk *desk = e_desk_at_xy_get(zone, x, y);
                  if ((desk->window_profile) &&
                      strcmp(desk->window_profile, profile) == 0)
                    {
                       return desk;
                    }
               }
          }
     }

   return NULL;
}

EAPI E_Container_Shape *
e_container_shape_add(E_Container *con)
{
   E_Container_Shape *es;

   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, 0);

   es = E_OBJECT_ALLOC(E_Container_Shape, E_CONTAINER_SHAPE_TYPE, _e_container_shape_free);
   E_OBJECT_DEL_SET(es, _e_container_shape_del);
   es->con = con;
   con->shapes = eina_list_append(con->shapes, es);
   _e_container_shape_change_call(es, E_CONTAINER_SHAPE_ADD);
   return es;
}

EAPI void
e_container_shape_show(E_Container_Shape *es)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_CONTAINER_SHAPE_TYPE);
   if (es->visible) return;
   es->visible = 1;
   _e_container_shape_change_call(es, E_CONTAINER_SHAPE_SHOW);
}

EAPI void
e_container_shape_hide(E_Container_Shape *es)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_CONTAINER_SHAPE_TYPE);
   if (!es->visible) return;
   es->visible = 0;
   _e_container_shape_change_call(es, E_CONTAINER_SHAPE_HIDE);
}

EAPI void
e_container_shape_move(E_Container_Shape *es, int x, int y)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_CONTAINER_SHAPE_TYPE);
   if ((es->x == x) && (es->y == y)) return;
   es->x = x;
   es->y = y;
   _e_container_shape_change_call(es, E_CONTAINER_SHAPE_MOVE);
}

EAPI void
e_container_shape_resize(E_Container_Shape *es, int w, int h)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_CONTAINER_SHAPE_TYPE);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if ((es->w == w) && (es->h == h)) return;
   es->w = w;
   es->h = h;
   _e_container_shape_change_call(es, E_CONTAINER_SHAPE_RESIZE);
}

EAPI Eina_List *
e_container_shape_list_get(E_Container *con)
{
   E_OBJECT_CHECK_RETURN(con, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, NULL);
   return con->shapes;
}

EAPI void
e_container_shape_geometry_get(E_Container_Shape *es, int *x, int *y, int *w, int *h)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_CONTAINER_SHAPE_TYPE);
   if (x) *x = es->x;
   if (y) *y = es->y;
   if (w) *w = es->w;
   if (h) *h = es->h;
}

EAPI E_Container *
e_container_shape_container_get(E_Container_Shape *es)
{
   E_OBJECT_CHECK_RETURN(es, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(es, E_CONTAINER_SHAPE_TYPE, NULL);
   return es->con;
}

EAPI void
e_container_shape_change_callback_add(E_Container *con, E_Container_Shape_Cb func, void *data)
{
   E_Container_Shape_Callback *cb;

   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);
   cb = calloc(1, sizeof(E_Container_Shape_Callback));
   if (!cb) return;
   cb->func = func;
   cb->data = data;
   con->shape_change_cb = eina_list_append(con->shape_change_cb, cb);
}

EAPI void
e_container_shape_change_callback_del(E_Container *con, E_Container_Shape_Cb func, void *data)
{
   Eina_List *l = NULL;
   E_Container_Shape_Callback *cb = NULL;

   /* FIXME: if we call this from within a callback we are in trouble */
   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);
   EINA_LIST_FOREACH(con->shape_change_cb, l, cb)
     {
        if ((cb->func == func) && (cb->data == data))
          {
             con->shape_change_cb = eina_list_remove_list(con->shape_change_cb, l);
             free(cb);
             return;
          }
     }
}

EAPI void
e_container_shape_rects_set(E_Container_Shape *es, Eina_Rectangle *rects, int num)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_CONTAINER_SHAPE_TYPE);
   E_FREE(es->shape_rects);
   es->shape_rects_num = 0;
   if ((rects) && (num == 1) &&
       (rects[0].x == 0) &&
       (rects[0].y == 0) &&
       ((int)rects[0].w == es->w) &&
       ((int)rects[0].h == es->h))
     {
        free(rects);
     }
   else if (rects)
     {
        es->shape_rects = rects;
        es->shape_rects_num = num;
     }
   _e_container_shape_change_call(es, E_CONTAINER_SHAPE_RECTS);
}

EAPI void
e_container_shape_input_rects_set(E_Container_Shape *es, Eina_Rectangle *rects, int num)
{
   E_OBJECT_CHECK(es);
   E_OBJECT_TYPE_CHECK(es, E_CONTAINER_SHAPE_TYPE);
   E_FREE(es->shape_input_rects);
   es->shape_input_rects_num = 0;
   if ((rects) && (num == 1) &&
       (rects[0].x == 0) &&
       (rects[0].y == 0) &&
       ((int)rects[0].w == es->w) &&
       ((int)rects[0].h == es->h))
     {
        free(rects);
     }
   else if (rects)
     {
        es->shape_input_rects = rects;
        es->shape_input_rects_num = num;
     }
   _e_container_shape_change_call(es, E_CONTAINER_SHAPE_INPUT_RECTS);
}

EAPI void
e_container_shape_solid_rect_set(E_Container_Shape *es, int x, int y, int w, int h)
{
   es->solid_rect.x = x;
   es->solid_rect.y = y;
   es->solid_rect.w = w;
   es->solid_rect.h = h;
}

EAPI void
e_container_shape_solid_rect_get(E_Container_Shape *es, int *x, int *y, int *w, int *h)
{
   if (x) *x = es->solid_rect.x;
   if (y) *y = es->solid_rect.y;
   if (w) *w = es->solid_rect.w;
   if (h) *h = es->solid_rect.h;
}

/* layers
 * 0 = desktop
 * 50 = below
 * 100 = normal
 * 150 = above
 * 200 = fullscreen
 * 250 = fullscreen
 * 300 = fullscreen
 * 350 = stuff over fullscreen
 * 400 = stuff over stuff
 * 450 = yet more stuff on top
 */
EAPI int
e_container_borders_count(E_Container *con)
{
   return con->clients;
}

static int
_e_container_layer_map(E_Layer layer)
{
   int pos = 0;

   pos = 1 + (layer / 50);
   if (pos > 10) pos = 10;
   return pos;
}

EAPI void
e_container_border_add(E_Border *bd)
{
   int pos = _e_container_layer_map(bd->layer);
   bd->zone->container->clients++;
   bd->zone->container->layers[pos].clients =
     eina_list_append(bd->zone->container->layers[pos].clients, bd);
   e_hints_client_list_set();
}

EAPI void
e_container_border_remove(E_Border *bd)
{
   int i;

   if (!bd->zone) return;
   /* FIXME: Could revert to old behaviour, ->layer is consistent
    * with pos now. */
   for (i = 0; i < 11; i++)
     {
        bd->zone->container->layers[i].clients =
          eina_list_remove(bd->zone->container->layers[i].clients, bd);
     }
   bd->zone->container->clients--;
   bd->zone = NULL;
   e_hints_client_list_set();
}

EAPI void
e_container_window_raise(E_Container *con, Ecore_X_Window win, E_Layer layer)
{
   int pos = _e_container_layer_map(layer);
   ecore_x_window_configure(win,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0,
                            con->layers[pos + 1].win, ECORE_X_WINDOW_STACK_BELOW);
}

EAPI void
e_container_window_lower(E_Container *con, Ecore_X_Window win, E_Layer layer)
{
   int pos = _e_container_layer_map(layer);
   ecore_x_window_configure(win,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0,
                            con->layers[pos].win, ECORE_X_WINDOW_STACK_ABOVE);
}

EAPI E_Border *
e_container_border_raise(E_Border *bd)
{
   E_Border *above = NULL;
   Eina_List *l;
   int pos = 0, i;

   if (!bd->zone) return NULL;
   /* Remove from old layer */
   for (i = 0; i < 11; i++)
     {
        bd->zone->container->layers[i].clients =
          eina_list_remove(bd->zone->container->layers[i].clients, bd);
     }

   /* Add to new layer */
   pos = _e_container_layer_map(bd->layer);

   ecore_x_window_configure(bd->win,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0,
                            bd->zone->container->layers[pos + 1].win, ECORE_X_WINDOW_STACK_BELOW);

   bd->zone->container->layers[pos].clients =
     eina_list_append(bd->zone->container->layers[pos].clients, bd);

   /* Find the window below this one */
   l = eina_list_data_find_list(bd->zone->container->layers[pos].clients, bd);
   if (eina_list_prev(l))
     above = eina_list_data_get(eina_list_prev(l));
   else
     {
        /* Need to check the layers below */
        for (i = pos - 1; i >= 0; i--)
          {
             if ((bd->zone->container->layers[i].clients) &&
                 (l = eina_list_last(bd->zone->container->layers[i].clients)))
               {
                  above = eina_list_data_get(l);
                  break;
               }
          }
     }

   e_hints_client_stacking_set();
   return above;
}

EAPI E_Border *
e_container_border_lower(E_Border *bd)
{
   E_Border *below = NULL;
   Eina_List *l;
   int pos = 0, i;

   if (!bd->zone) return NULL;
   /* Remove from old layer */
   for (i = 0; i < 11; i++)
     {
        bd->zone->container->layers[i].clients =
          eina_list_remove(bd->zone->container->layers[i].clients, bd);
     }

   /* Add to new layer */
   pos = _e_container_layer_map(bd->layer);

   ecore_x_window_configure(bd->win,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0,
                            bd->zone->container->layers[pos].win, ECORE_X_WINDOW_STACK_ABOVE);

   bd->zone->container->layers[pos].clients =
     eina_list_prepend(bd->zone->container->layers[pos].clients, bd);

   /* Find the window above this one */
   l = eina_list_data_find_list(bd->zone->container->layers[pos].clients, bd);
   if (eina_list_next(l))
     below = eina_list_data_get(eina_list_next(l));
   else
     {
        /* Need to check the layers above */
        for (i = pos + 1; i < 11; i++)
          {
             if (bd->zone->container->layers[i].clients)
               {
                  below = eina_list_data_get(bd->zone->container->layers[i].clients);
                  break;
               }
          }
     }

   e_hints_client_stacking_set();
   return below;
}

EAPI void
e_container_border_stack_above(E_Border *bd, E_Border *above)
{
   int pos = 0, i;

   if (!bd->zone) return;
   /* Remove from old layer */
   for (i = 0; i < 11; i++)
     {
        bd->zone->container->layers[i].clients =
          eina_list_remove(bd->zone->container->layers[i].clients, bd);
     }

   /* Add to new layer */
   bd->layer = above->layer;
   pos = _e_container_layer_map(bd->layer);

   ecore_x_window_configure(bd->win,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0,
                            above->win, ECORE_X_WINDOW_STACK_ABOVE);

   bd->zone->container->layers[pos].clients =
     eina_list_append_relative(bd->zone->container->layers[pos].clients, bd, above);
}

EAPI void
e_container_border_stack_below(E_Border *bd, E_Border *below)
{
   int pos = 0, i;

   if (!bd->zone) return;
   /* Remove from old layer */
   for (i = 0; i < 11; i++)
     {
        bd->zone->container->layers[i].clients =
          eina_list_remove(bd->zone->container->layers[i].clients, bd);
     }

   /* Add to new layer */
   bd->layer = below->layer;
   pos = _e_container_layer_map(bd->layer);

   ecore_x_window_configure(bd->win,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0,
                            below->win, ECORE_X_WINDOW_STACK_BELOW);

   bd->zone->container->layers[pos].clients =
     eina_list_prepend_relative(bd->zone->container->layers[pos].clients, bd, below);
}

static E_Border_List *
_e_container_border_list_new(E_Container *con)
{
   E_Border_List *list = NULL;
   E_Border *bd;
   int i;
   Eina_List *l;

   if (!(list = E_NEW(E_Border_List, 1))) return NULL;
   list->container = con;
   e_object_ref(E_OBJECT(list->container));
   eina_array_step_set(&(list->client_array), sizeof(list->client_array), 256);
   for (i = 0; i < 11; i++)
     {
        EINA_LIST_FOREACH(con->layers[i].clients, l, bd)
          eina_array_push(&(list->client_array), bd);
     }
   return list;
}

static E_Border *
_e_container_border_list_jump(E_Border_List *list, int dir)
{
   E_Border *bd;

   if ((list->pos < 0) ||
       (list->pos >= (int)eina_array_count(&(list->client_array))))
     return NULL;
   bd = eina_array_data_get(&(list->client_array), list->pos);
   list->pos += dir;
   return bd;
}

EAPI E_Border_List *
e_container_border_list_first(E_Container *con)
{
   E_Border_List *list = NULL;

   list = _e_container_border_list_new(con);
   list->pos = 0;
   return list;
}

EAPI E_Border_List *
e_container_border_list_last(E_Container *con)
{
   E_Border_List *list = NULL;

   list = _e_container_border_list_new(con);
   list->pos = eina_array_count(&(list->client_array)) - 1;
   return list;
}

EAPI E_Border *
e_container_border_list_next(E_Border_List *list)
{
   return _e_container_border_list_jump(list, 1);
}

EAPI E_Border *
e_container_border_list_prev(E_Border_List *list)
{
   return _e_container_border_list_jump(list, -1);
}

EAPI void
e_container_border_list_free(E_Border_List *list)
{
   e_object_unref(E_OBJECT(list->container));
   eina_array_flush(&(list->client_array));
   free(list);
}

EAPI void
e_container_all_freeze(void)
{
   Eina_List *l, *ll;
   E_Manager *man;
   E_Container *con;

   EINA_LIST_FOREACH(e_manager_list(), l, man)
     {
        EINA_LIST_FOREACH(man->containers, ll, con)
          {
             evas_event_freeze(con->bg_evas);
          }
     }
}

EAPI void
e_container_all_thaw(void)
{
   Eina_List *l, *ll;
   E_Manager *man;
   E_Container *con;

   EINA_LIST_FOREACH(e_manager_list(), l, man)
     {
        EINA_LIST_FOREACH(man->containers, ll, con)
          {
             evas_event_thaw(con->bg_evas);
          }
     }
}

EAPI E_Container *
e_container_evas_object_container_get(Evas_Object *obj)
{
   Evas *evas;
   Evas_Object *wobj;
   E_Container *con;

   if (!obj) return NULL;
   evas = evas_object_evas_get(obj);
   wobj = evas_object_name_find(evas, "container->bg_blank_object");
   if (!wobj) return NULL;
   con = evas_object_data_get(wobj, "e_container");
   return con;
}

/* local subsystem functions */
static void
_e_container_free(E_Container *con)
{
   Eina_List *l;
   int i;

   ecore_x_window_free(con->scratch_win);
   /* We can't use e_object_del here, because border adds a ref to itself
    * when it is removed, and the ref is never unref'ed */
   for (i = 0; i < 11; i++)
     {
        ecore_x_window_free(con->layers[i].win);
/* FIXME: had to disable this as it was freeing already freed items during
 * looping (particularly remember/lock config dialogs). this is just
 * disabled until we put in some special handling for this
 *
        EINA_LiST_FOREACH(con->layers[i].clients, l, tmp)
          {
             e_object_free(E_OBJECT(tmp));
          }
 */
     }
   l = con->zones;
   con->zones = NULL;
   E_FREE_LIST(l, e_object_del);
   con->manager->containers = eina_list_remove(con->manager->containers, con);
   e_canvas_del(con->bg_ecore_evas);
   ecore_evas_free(con->bg_ecore_evas);
   if (con->manager->win != con->win)
     {
        ecore_x_window_free(con->win);
     }
   if (con->name) eina_stringshare_del(con->name);
   free(con);
}

static void
_e_container_cb_mouse_in(E_Container *con EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Border *bd;

   bd = e_border_focused_get();
   if (bd && (!bd->border_menu)) e_focus_event_mouse_out(bd);
}

static void
_e_container_cb_mouse_down(E_Container *con, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   e_bindings_mouse_down_evas_event_handle(E_BINDING_CONTEXT_CONTAINER, E_OBJECT(con), event_info);
}

static void
_e_container_cb_mouse_up(E_Container *con, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   e_bindings_mouse_up_evas_event_handle(E_BINDING_CONTEXT_CONTAINER, E_OBJECT(con), event_info);
}

static void
_e_container_cb_mouse_wheel(E_Container *con, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   e_bindings_wheel_evas_event_handle(E_BINDING_CONTEXT_CONTAINER, E_OBJECT(con), event_info);
}

static void
_e_container_shape_del(E_Container_Shape *es)
{
   if (es->comp_win) es->comp_win->shape = NULL;
   _e_container_shape_change_call(es, E_CONTAINER_SHAPE_DEL);
}

static void
_e_container_shape_free(E_Container_Shape *es)
{
   es->con->shapes = eina_list_remove(es->con->shapes, es);
   free(es->shape_rects);
   free(es->shape_input_rects);
   free(es);
}

static void
_e_container_shape_change_call(E_Container_Shape *es, E_Container_Shape_Change ch)
{
   Eina_List *l = NULL;
   E_Container_Shape_Callback *cb = NULL;

   if ((!es) || (!es->con) || (!es->con->shape_change_cb)) return;
   EINA_LIST_FOREACH(es->con->shape_change_cb, l, cb)
     {
        if (!cb) continue;
        cb->func(cb->data, es, ch);
     }
}

static int
_e_container_cb_zone_sort(const void *data1, const void *data2)
{
   const E_Zone *z1, *z2;

   z1 = data1;
   z2 = data2;

   return z2->num - z1->num;
}

static void
_e_container_resize_handle(E_Container *con)
{
   E_Event_Container_Resize *ev;
   Eina_List *l, *screens, *zones = NULL, *ll;
   E_Zone *zone;
   E_Screen *scr;
   int i;

   ev = calloc(1, sizeof(E_Event_Container_Resize));
   ev->container = con;
   e_object_ref(E_OBJECT(con));

   e_xinerama_update();
   screens = (Eina_List *)e_xinerama_screens_get();

   if (screens)
     {
        EINA_LIST_FOREACH(con->zones, l, zone)
          zones = eina_list_append(zones, zone);
        con->zones = NULL;
        EINA_LIST_FOREACH(screens, l, scr)
          {
             zone = NULL;

             printf("@@@ SCREENS: %i %i | %i %i %ix%i\n",
                    scr->screen, scr->escreen, scr->x, scr->y, scr->w, scr->h);
             EINA_LIST_FOREACH(zones, ll, zone)
               {
                  if (zone->id == scr->escreen) break;
                  zone = NULL;
               }
             if (zone)
               {
                  printf("@@@ FOUND ZONE %i %i [%p]\n", zone->num, zone->id, zone);
                  e_zone_move_resize(zone, scr->x, scr->y, scr->w, scr->h);
                  zones = eina_list_remove(zones, zone);
                  con->zones = eina_list_append(con->zones, zone);
                  zone->num = scr->screen;
               }
             else
               {
                  zone = e_zone_new(con, scr->screen, scr->escreen,
                                    scr->x, scr->y, scr->w, scr->h);
                  printf("@@@ NEW ZONE = %p\n", zone);
               }
          }
        con->zones = eina_list_sort(con->zones, eina_list_count(con->zones),
                                    _e_container_cb_zone_sort);
        if (zones)
          {
             E_Zone *spare_zone = NULL;

             if (con->zones) spare_zone = con->zones->data;

             EINA_LIST_FREE(zones, zone)
               {
                  E_Border_List *bl;
                  E_Border *bd;

                  /* delete any shelves on this zone */
                  bl = e_container_border_list_first(zone->container);
                  while ((bd = e_container_border_list_next(bl)))
                    {
                       if (bd->zone == zone)
                         {
                            if (spare_zone) e_border_zone_set(bd, spare_zone);
                            else
                              printf("EEEK! should not be here - but no\n"
                                     "spare zones exist to move this\n"
                                     "window to!!! help!\n");
                         }
                    }
                  e_container_border_list_free(bl);
                  e_object_del(E_OBJECT(zone));
               }
          }
        e_shelf_config_update();
     }
   else
     {
        E_Zone *z;

        z = e_container_zone_number_get(con, 0);
        if (z)
          {
             e_zone_move_resize(z, 0, 0, con->w, con->h);
             e_shelf_zone_move_resize_handle(z);
          }
     }

   ecore_event_add(E_EVENT_CONTAINER_RESIZE, ev, _e_container_event_container_resize_free, NULL);

   for (i = 0; i < 11; i++)
     {
        Eina_List *tmp = NULL;
        E_Border *bd;

        /* Make temporary list as e_border_res_change_geometry_restore
         * rearranges the order. */
        EINA_LIST_FOREACH(con->layers[i].clients, l, bd)
          tmp = eina_list_append(tmp, bd);

        EINA_LIST_FOREACH(tmp, l, bd)
          {
             e_border_res_change_geometry_save(bd);
             e_border_res_change_geometry_restore(bd);
          }

        eina_list_free(tmp);
     }
}

static void
_e_container_event_container_resize_free(void *data __UNUSED__, void *ev)
{
   E_Event_Container_Resize *e;

   e = ev;
   e_object_unref(E_OBJECT(e->container));
   free(e);
}

