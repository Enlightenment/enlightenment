#include "e_mod_main.h"

typedef struct _Edgeset Edgeset;
typedef struct _Edgehandler Edgehandler;

struct _Edgeset
{
   E_Zone *zone;
   struct {
      Evas_Object *obj;
   } l, r, t, b;
   struct {
      int button, x, y;
      Eina_Bool recognized : 1;
   } down;
};

struct _Edgehandler
{
   E_Edges_Event event;
   void (*func) (void *data, int d, double v);
   void *data;
};

static Edgehandler *_handler_find(E_Edges_Event event);
static Evas_Object *_input_obj(Edgeset *es, int x, int y, int w, int h);
static Edgeset *_edgeset_new(E_Zone *zone);
static void _edgeset_free(Edgeset *es);
static void _cb_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event);
static void _cb_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event);
static void _cb_move(void *data, Evas *e __UNUSED__, Evas_Object *obj, void *event);

static Eina_List *edges = NULL;
static Eina_List *handlers = NULL;

void
e_edges_init(void)
{
   const Eina_List *l;
   E_Zone *zone;
   
   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        Edgeset *es = _edgeset_new(zone);
        
        if (es) edges = eina_list_append(edges, es);
     }
}

void
e_edges_shutdown(void)
{
   Edgeset *es;
   Edgehandler *eh;
   
   EINA_LIST_FREE(edges, es) _edgeset_free(es);
   EINA_LIST_FREE(handlers, eh) free(eh);
}

void
e_edges_handler_set(E_Edges_Event event, void (*func) (void *data, int d, double v), void *data)
{
   Edgehandler *eh;

   eh = _handler_find(event);
   if (!eh)
     {
        eh = calloc(1, sizeof(*eh));
        if (!eh) return;
        handlers = eina_list_append(handlers, eh);
     }
   eh->event = event;
   eh->func = func;
   eh->data = data;
}

static Edgehandler *
_handler_find(E_Edges_Event event)
{
   Eina_List *l;
   Edgehandler *eh;
   
   EINA_LIST_FOREACH(handlers, l, eh)
     {
        if (eh->event == event)
          {
             handlers = eina_list_promote_list(handlers, l);
             return eh;
          }
     }
   return NULL;
}

static void
_handler_call(E_Edges_Event event, int d, double v)
{
   Edgehandler *eh = _handler_find(event);
   
   if (!eh) return;
   if (!eh->func) return;
   eh->func(eh->data, d, v);
}

static Evas_Object *
_input_obj(Edgeset *es, int x, int y, int w, int h)
{
   E_Comp *c = e_comp_get(es->zone);
   Evas_Object *o = evas_object_rectangle_add(c->evas);
   evas_object_color_set(o, 0, 0, 0, 0);
   evas_object_move(o, x, y);
   evas_object_resize(o, w, h);
   evas_object_layer_set(o, 999);
   evas_object_show(o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN, _cb_down, es);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP, _cb_up, es);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE, _cb_move, es);
   return o;
}

static Edgeset *
_edgeset_new(E_Zone *zone)
{
   Edgeset *es = calloc(1, sizeof(*es));
   
   if (!es) return NULL;
   es->zone = zone;

   es->t.obj = _input_obj(es, zone->x, zone->y, zone->w, 8);
   es->b.obj = _input_obj(es, zone->x, zone->y + zone->h - 8, zone->w, 8);
   es->l.obj = _input_obj(es, zone->x, zone->y, 8, zone->h);
   es->r.obj = _input_obj(es, zone->x + zone->w - 8, zone->y, 8, zone->h);
   return es;
}

static void
_edgeset_free(Edgeset *es)
{
   evas_object_del(es->t.obj);
   evas_object_del(es->b.obj);
   evas_object_del(es->l.obj);
   evas_object_del(es->r.obj);
   free(es);
}

static void
_cb_down(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Edgeset *es = data;
   Evas_Event_Mouse_Down *ev = event;
   if (ev->button != 1) return;
   es->down.button = ev->button;
   es->down.x = ev->canvas.x;
   es->down.y = ev->canvas.y;
   es->down.recognized = EINA_FALSE;
}

static void
_cb_up(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Edgeset *es = data;
   Evas_Event_Mouse_Up *ev = event;
   if (ev->button != 1) return;
   es->down.button = 0;
}

static void
_cb_move(void *data, Evas *e __UNUSED__, Evas_Object *obj, void *event)
{
   Edgeset *es = data;
   Evas_Event_Mouse_Move *ev = event;
   int dx, dy, d;
   double v;
   
   if (!es->down.button) return;
   dx = ev->cur.canvas.x - es->down.x;
   dy = ev->cur.canvas.y - es->down.y;
   d = 40;
   if (obj == es->l.obj)
     {
        if ((!es->down.recognized) && (dx > d) && (abs(dy) < d))
          {
             es->down.recognized = EINA_TRUE;
             _handler_call(E_EDGES_LEFT_IN_BEGIN, 0, 0);
          }
        if (es->down.recognized)
          {
             d = (dx - d);
             if (d < 0) d = 0;
             if (es->zone->w > 1) v = (double)d / (es->zone->w / 2);
             else v = 1.0;
             _handler_call(E_EDGES_LEFT_IN_SLIDE, d, v);
          }
     }
   else if (obj == es->r.obj)
     {
        if ((!es->down.recognized) && (-dx > d) && (abs(dy) < d))
          {
             es->down.recognized = EINA_TRUE;
             _handler_call(E_EDGES_RIGHT_IN_BEGIN, 0, 0);
          }
        if (es->down.recognized)
          {
             d = (-dx - d);
             if (d < 0) d = 0;
             if (es->zone->w > 1) v = (double)d / (es->zone->w / 2);
             else v = 1.0;
             _handler_call(E_EDGES_RIGHT_IN_SLIDE, d, v);
          }
     }
   else if (obj == es->t.obj)
     {
        if ((!es->down.recognized) && (dy > d) && (abs(dx) < d))
          {
             es->down.recognized = EINA_TRUE;
             _handler_call(E_EDGES_TOP_IN_BEGIN, 0, 0);
          }
        if (es->down.recognized)
          {
             d = (dy - d);
             if (d < 0) d = 0;
             if (es->zone->h > 1) v = (double)d / (es->zone->h / 2);
             else v = 1.0;
             _handler_call(E_EDGES_TOP_IN_SLIDE, d, v);
          }
     }
   else if (obj == es->b.obj)
     {
        if ((!es->down.recognized) && (-dy > d) && (abs(dx) < d))
          {
             es->down.recognized = EINA_TRUE;
             _handler_call(E_EDGES_BOTTOM_IN_BEGIN, 0, 0);
          }
        if (es->down.recognized)
          {
             d = (-dy - d);
             if (d < 0) d = 0;
             if (es->zone->h > 1) v = (double)d / (es->zone->h / 2);
             else v = 1.0;
             _handler_call(E_EDGES_BOTTOM_IN_SLIDE, d, v);
          }
     }
}
