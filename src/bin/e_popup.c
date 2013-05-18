#include "e.h"

/* local subsystem globals */
static Eina_List *_e_popup_list = NULL;
static E_Popup *autoclose_popup = NULL;
static Evas_Object *event_rect = NULL;
static Ecore_Event_Handler *key_handler = NULL;

/* local subsystem functions */

static void
_e_popup_autoclose_cleanup(void)
{
   if (autoclose_popup)
     {
        e_grabinput_release(0, e_comp_get(autoclose_popup)->ee_win);
        autoclose_popup->autoclose = 0;
        if (autoclose_popup->del_cb)
          autoclose_popup->del_cb(autoclose_popup->cb_data, autoclose_popup);
        else
          E_FREE_FUNC(autoclose_popup, e_object_del);
     }
   autoclose_popup = NULL;
   E_FREE_FUNC(event_rect, evas_object_del);
   E_FREE_FUNC(key_handler, ecore_event_handler_del);
}

static void
_e_popup_free(E_Popup *pop)
{
   e_object_unref(E_OBJECT(pop->zone));
   if (pop->autoclose)
     _e_popup_autoclose_cleanup();
   E_FREE_FUNC(pop->shape, e_object_del);
   E_FREE_LIST(pop->objects, evas_object_del);
   pop->zone->popups = eina_list_remove(pop->zone->popups, pop);
   eina_stringshare_del(pop->name);
   _e_popup_list = eina_list_remove(_e_popup_list, pop);
   free(pop);
}

static Eina_Bool
_e_popup_autoclose_key_down_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Key *ev = event;
   Eina_Bool del = EINA_TRUE;

   if (autoclose_popup->key_cb)
     del = !autoclose_popup->key_cb(autoclose_popup->cb_data, ev);
   if (del) _e_popup_autoclose_cleanup();
   return ECORE_CALLBACK_RENEW;
}

static void
_e_popup_autoclose_mouse_up_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _e_popup_autoclose_cleanup();
}

static void
_e_popup_autoclose_setup(E_Popup *pop)
{
   E_FREE_FUNC(autoclose_popup, e_object_del);
   E_FREE_FUNC(event_rect, evas_object_del);

   event_rect = evas_object_rectangle_add(e_comp_get(pop)->evas);
   evas_object_color_set(event_rect, 0, 0, 0, 0);
   evas_object_resize(event_rect, pop->zone->container->w, pop->zone->container->h);
   evas_object_event_callback_add(event_rect, EVAS_CALLBACK_MOUSE_UP, _e_popup_autoclose_mouse_up_cb, NULL);
   if (pop->comp_layer == E_COMP_CANVAS_LAYER_LAYOUT)
     {
        e_layout_pack(e_comp_get(pop)->layout, event_rect);
        e_layout_child_lower_below(event_rect, pop->cw->effect_obj);
     }
   else
     evas_object_layer_set(event_rect, pop->comp_layer - 1);
   evas_object_show(event_rect);
   key_handler = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, _e_popup_autoclose_key_down_cb, NULL);
   e_grabinput_get(0, 0, e_comp_get(pop)->ee_win);
   autoclose_popup = pop;
}

static void
_e_popup_delay_del_cb(E_Popup *pop)
{
   e_popup_hide(pop);
   if (pop->cw) e_comp_win_del(pop->cw);
}

/* externally accessible functions */

EINTERN int
e_popup_init(void)
{
   return 1;
}

EINTERN int
e_popup_shutdown(void)
{
   _e_popup_autoclose_cleanup();
   return 1;
}

EAPI E_Popup *
e_popup_new(E_Zone *zone, int x, int y, int w, int h)
{
   E_Popup *pop;

   pop = E_OBJECT_ALLOC(E_Popup, E_POPUP_TYPE, _e_popup_free);
   if (!pop) return NULL;
   e_object_delay_del_set(E_OBJECT(pop), _e_popup_delay_del_cb);
   pop->zone = zone;
   pop->ecore_evas = zone->container->bg_ecore_evas;
   pop->zx = pop->zone->x;
   pop->zy = pop->zone->y;
   pop->x = x;
   pop->y = y;
   pop->w = w;
   pop->h = h;
   pop->layer = E_LAYER_POPUP;
   pop->comp_layer = E_COMP_CANVAS_LAYER_LAYOUT;

   pop->evas = e_comp_get(zone)->evas;
   pop->shape = e_container_shape_add(zone->container);
   e_object_ref(E_OBJECT(pop->zone));
   pop->zone->popups = eina_list_append(pop->zone->popups, pop);
   _e_popup_list = eina_list_append(_e_popup_list, pop);
   return pop;
}

EAPI void
e_popup_content_set(E_Popup *pop, Evas_Object *obj)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);

   if (pop->content) evas_object_del(pop->content);
   pop->content = obj;
   evas_object_data_set(obj, "eobj", pop);
   evas_object_move(obj, pop->zone->x + pop->x, pop->zone->y + pop->y);
   evas_object_resize(obj, pop->w, pop->h);
   evas_object_show(obj);
   e_popup_layer_set(pop, pop->comp_layer, pop->layer);
   e_popup_ignore_events_set(pop, pop->ignore_events);
   if (pop->visible)
     e_comp_win_moveresize(pop->cw, pop->zone->x + pop->x, pop->zone->y + pop->y, pop->w, pop->h);
}

EAPI void
e_popup_show(E_Popup *pop)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);

   if (pop->visible) return;

   pop->visible = 1;
   e_comp_win_moveresize(pop->cw, pop->zone->x + pop->x, pop->zone->y + pop->y, pop->w, pop->h);
   e_comp_win_show(pop->cw);
   if (pop->autoclose) _e_popup_autoclose_setup(pop);
}

EAPI void
e_popup_hide(E_Popup *pop)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);
   if (!pop->visible) return;
   pop->visible = 0;
   if (pop->cw)
     {
        e_comp_win_hide(pop->cw);
        if (e_object_is_del(E_OBJECT(pop)))
          e_comp_win_del(pop->cw);
     }
   if (!pop->autoclose) return;
   if (e_object_is_del(E_OBJECT(pop))) return;
   _e_popup_autoclose_cleanup();
}

EAPI void
e_popup_move(E_Popup *pop, int x, int y)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);
   if ((pop->x == x) && (pop->y == y) &&
       (pop->zone->x == pop->zx) && (pop->zone->y == pop->zy)) return;
   pop->zx = pop->zone->x;
   pop->zy = pop->zone->y;
   pop->x = x;
   pop->y = y;
   if (!pop->cw) return;
   e_comp_win_move(pop->cw, pop->zone->x + pop->x, pop->zone->y + pop->y);
}

EAPI void
e_popup_resize(E_Popup *pop, int w, int h)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);
   if ((pop->w == w) && (pop->h == h)) return;
   pop->w = w;
   pop->h = h;
   if (!pop->cw) return;
   e_comp_win_resize(pop->cw, pop->w, pop->h);
}

EAPI void
e_popup_move_resize(E_Popup *pop, int x, int y, int w, int h)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);
   if ((pop->x == x) && (pop->y == y) &&
       (pop->w == w) && (pop->h == h) &&
       (pop->zone->x == pop->zx) && (pop->zone->y == pop->zy)) return;
   pop->x = x;
   pop->y = y;
   pop->w = w;
   pop->h = h;
   if (!pop->cw) return;
   e_comp_win_moveresize(pop->cw, pop->zone->x + x, pop->zone->y + y, w, h);
}

EAPI void
e_popup_ignore_events_set(E_Popup *pop, int ignore)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);

   ignore = !!ignore;
   pop->ignore_events = ignore;
   if (pop->cw)
     e_comp_win_ignore_events_set(pop->cw, ignore);
}

EAPI void
e_popup_layer_set(E_Popup *pop, E_Comp_Canvas_Layer comp_layer, E_Layer layer)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);
   pop->comp_layer = comp_layer;
   pop->layer = layer;
   if (!pop->content) return;
   if (comp_layer == E_COMP_CANVAS_LAYER_LAYOUT)
     E_LAYER_LAYOUT_ADD(pop->content, layer);
   else
     E_LAYER_SET_ABOVE(pop->content, comp_layer);
}

EAPI void
e_popup_name_set(E_Popup *pop, const char *name)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);
   eina_stringshare_replace(&pop->name, name);
}

EAPI void
e_popup_object_add(E_Popup *pop, Evas_Object *obj)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);

   pop->objects = eina_list_append(pop->objects, obj);
}

EAPI void
e_popup_object_remove(E_Popup *pop, Evas_Object *obj)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);

   pop->objects = eina_list_remove(pop->objects, obj);
}

EAPI void
e_popup_autoclose(E_Popup *pop, Ecore_End_Cb del_cb, E_Popup_Key_Cb cb, const void *data)
{
   E_OBJECT_CHECK(pop);
   E_OBJECT_TYPE_CHECK(pop, E_POPUP_TYPE);

   pop->autoclose = 1;
   pop->del_cb = del_cb;
   pop->key_cb = cb;
   pop->cb_data = (void*)data;
   if (pop->visible) _e_popup_autoclose_setup(pop);
}
