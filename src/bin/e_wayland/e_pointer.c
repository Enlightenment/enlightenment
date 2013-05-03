#include "e.h"

/* local structures */
typedef struct _E_Pointer_Stack E_Pointer_Stack;
struct _E_Pointer_Stack
{
   void *obj;
   const char *type;
};

/* local function prototypes */
static void _e_pointer_active_handle(E_Pointer *ptr);
static void _e_pointer_canvas_add(E_Pointer *ptr);
static void _e_pointer_canvas_del(E_Pointer *ptr);
static void _e_pointer_stack_free(E_Pointer_Stack *stack);
static void _e_pointer_type_set(E_Pointer *ptr, const char *type);
static void _e_pointer_cb_free(E_Pointer *ptr);
static Eina_Bool _e_pointer_cb_idle(void *data);
static Eina_Bool _e_pointer_cb_idle_wait(void *data);
static Eina_Bool _e_pointer_cb_idle_poller(void *data);
static Eina_Bool _e_pointer_cb_mouse_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED);
static Eina_Bool _e_pointer_cb_mouse_up(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED);
static Eina_Bool _e_pointer_cb_mouse_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED);
static Eina_Bool _e_pointer_cb_mouse_wheel(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED);
static void _e_pointer_cb_hot_move(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED);

/* local variables */
static Eina_List *_ptrs = NULL;
static Eina_List *_hdlrs = NULL;

EINTERN int 
e_pointer_init(void)
{
   E_LIST_HANDLER_APPEND(_hdlrs, ECORE_EVENT_MOUSE_BUTTON_DOWN, 
                         _e_pointer_cb_mouse_down, NULL);
   E_LIST_HANDLER_APPEND(_hdlrs, ECORE_EVENT_MOUSE_BUTTON_UP, 
                         _e_pointer_cb_mouse_up, NULL);
   E_LIST_HANDLER_APPEND(_hdlrs, ECORE_EVENT_MOUSE_MOVE, 
                         _e_pointer_cb_mouse_move, NULL);
   E_LIST_HANDLER_APPEND(_hdlrs, ECORE_EVENT_MOUSE_WHEEL, 
                         _e_pointer_cb_mouse_wheel, NULL);

   return 1;
}

EINTERN int 
e_pointer_shutdown(void)
{
   E_FREE_LIST(_hdlrs, ecore_event_handler_del);
   return 1;
}

EAPI E_Pointer *
e_pointer_new(Ecore_Wl_Window *win, Eina_Bool filled)
{
   E_Pointer *ptr;

   if (!(ptr = E_OBJECT_ALLOC(E_Pointer, E_POINTER_TYPE, _e_pointer_cb_free)))
     return NULL;

   ptr->e_cursor = e_config->use_e_cursor;
   ptr->color = ptr->e_cursor;
   ptr->win = win;

   if (filled) e_pointer_type_push(ptr, ptr, "default");

   _ptrs = eina_list_append(_ptrs, ptr);

   return ptr;
}

EAPI void 
e_pointer_hide(E_Pointer *p)
{
   if (!p) return;
   if (p->evas) _e_pointer_canvas_del(p);
}

EAPI void 
e_pointer_type_push(E_Pointer *p, void *obj, const char *type)
{
   E_Pointer_Stack *stack;

   if (!p) return;
   p->e_cursor = e_config->use_e_cursor;
   _e_pointer_type_set(p, type);
   p->obj = obj;

   if ((stack = E_NEW(E_Pointer_Stack, 1)))
     {
        stack->type = eina_stringshare_add(type);
        stack->obj = obj;
        p->stack = eina_list_append(p->stack, stack);
     }
}

EAPI void 
e_pointer_type_pop(E_Pointer *p, void *obj, const char *type)
{
   Eina_List *l;
   E_Pointer_Stack *stack;

   if (!p) return;
   EINA_LIST_FOREACH(p->stack, l, stack)
     {
        if ((stack->obj == obj) && ((!type) || (!strcmp(stack->type, type))))
          {
             _e_pointer_stack_free(stack);
             p->stack = eina_list_remove_list(p->stack, l);
             if (type) break;
          }
     }

   if (!p->stack)
     {
        if (p->evas) _e_pointer_canvas_del(p);
        if (p->type) eina_stringshare_del(p->type);
        p->type = NULL;
        return;
     }

   stack = eina_list_data_get(p->stack);
   _e_pointer_type_set(p, stack->type);
   eina_stringshare_replace(&p->type, stack->type);
   p->obj = stack->obj;
   p->e_cursor = e_config->use_e_cursor;
}

EAPI void 
e_pointer_size_set(int size)
{
   Eina_List *l;
   E_Pointer *ptr;

   if (!e_config->show_cursor) return;
   EINA_LIST_FOREACH(_ptrs, l, ptr)
     {
        ptr->w = ptr->h = size;
        if (ptr->evas)
          {
             evas_output_size_set(ptr->evas, ptr->w, ptr->h);
             evas_output_viewport_set(ptr->evas, 0, 0, ptr->w, ptr->h);
             evas_object_move(ptr->o_ptr, 0, 0);
             evas_object_resize(ptr->o_ptr, ptr->w, ptr->h);
          }
        else
          {
             const char *type;

             if ((type = ptr->type))
               {
                  ptr->type = NULL;
                  _e_pointer_type_set(ptr, type);
                  eina_stringshare_del(type);
               }
          }
     }
}

EAPI void 
e_pointer_idler_before(void)
{
   Eina_List *l;
   E_Pointer *ptr;

   if (!e_config->show_cursor) return;

   EINA_LIST_FOREACH(_ptrs, l, ptr)
     {
        Eina_List *updates;

        if (!ptr->e_cursor) continue;
        if (!ptr->evas) continue;

        updates = evas_render_updates(ptr->evas);
        if ((updates) || (ptr->hot.update))
          {
             ptr->hot.update = EINA_FALSE;
          }
        if (updates) evas_render_updates_free(updates);
     }
}

/* local functions */
static void 
_e_pointer_active_handle(E_Pointer *ptr)
{
   if (!ptr) return;
   if (ptr->idle_timer) ecore_timer_del(ptr->idle_timer);
   ptr->idle_timer = NULL;
   if (ptr->idle_poller) ecore_poller_del(ptr->idle_poller);
   ptr->idle_poller = NULL;
   if (ptr->idle)
     {
        if (ptr->o_ptr) 
          edje_object_signal_emit(ptr->o_ptr, "e,state,mouse,active", "e");
        ptr->idle = EINA_FALSE;
     }
   if (e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM) return;
   ptr->idle_timer = ecore_timer_loop_add(1.0, _e_pointer_cb_idle, ptr);
}

static void 
_e_pointer_canvas_add(E_Pointer *ptr)
{
   Evas_Coord x, y;

   /* check for valid pointer */
   if (!ptr) return;

   ptr->w = e_config->cursor_size;
   ptr->h = e_config->cursor_size;

   ecore_wl_pointer_xy_get(&x, &y);

   ptr->ee = 
     e_canvas_new(0, x, y, ptr->w, ptr->h, EINA_TRUE, EINA_FALSE, NULL);
   e_canvas_add(ptr->ee);

   ptr->evas = ecore_evas_get(ptr->ee);

   ptr->o_ptr = edje_object_add(ptr->evas);
   evas_object_move(ptr->o_ptr, 0, 0);
   evas_object_resize(ptr->o_ptr, ptr->w, ptr->h);
   evas_object_show(ptr->o_ptr);

   ptr->o_hot = evas_object_rectangle_add(ptr->evas);
   evas_object_color_set(ptr->o_hot, 0, 0, 0, 0);
   evas_object_event_callback_add(ptr->o_hot, EVAS_CALLBACK_MOVE, 
                                  _e_pointer_cb_hot_move, ptr);
}

static void 
_e_pointer_canvas_del(E_Pointer *ptr)
{
   if (!ptr) return;
   if (ptr->o_ptr) evas_object_del(ptr->o_ptr);
   ptr->o_ptr = NULL;
   if (ptr->o_hot) evas_object_del(ptr->o_hot);
   ptr->o_hot = NULL;
   if (ptr->evas) evas_free(ptr->evas);
   ptr->evas = NULL;
   /* TODO: free pixels ?? */
}

static void 
_e_pointer_stack_free(E_Pointer_Stack *stack)
{
   if (stack->type) eina_stringshare_del(stack->type);
   E_FREE(stack);
}

static void 
_e_pointer_type_set(E_Pointer *ptr, const char *type)
{
   if (!ptr) return;
   if ((ptr->type) && (!strcmp(ptr->type, type))) return;

   eina_stringshare_replace(&ptr->type, type);
   if (!e_config->show_cursor) return;

   if (ptr->e_cursor)
     {
        char cursor[1024];
        Evas_Coord x, y;

        if (!ptr->evas) _e_pointer_canvas_add(ptr);
        if (ptr->color)
          snprintf(cursor, sizeof(cursor), 
                   "e/pointer/enlightenment/%s/color", type);
        else
          snprintf(cursor, sizeof(cursor), 
                   "e/pointer/enlightenment/%s/mono", type);
        if (!e_theme_edje_object_set(ptr->o_ptr, "base/theme/pointer", cursor))
          goto fallback;

        edje_object_part_swallow(ptr->o_ptr, "e.swallow.hotspot", ptr->o_hot);
        edje_object_part_geometry_get(ptr->o_ptr, "e.swallow.hotspot", 
                                      &x, &y, NULL, NULL);

        if ((ptr->hot.x != x) || (ptr->hot.y != y))
          {
             ptr->hot.x = x;
             ptr->hot.y = y;
          }
        ptr->hot.update = EINA_TRUE;
        return;
     }

fallback:
   if (ptr->evas) _e_pointer_canvas_del(ptr);

   /* TODO: How to handle ?? */
   return;
}

static void 
_e_pointer_cb_free(E_Pointer *ptr)
{
   E_Pointer_Stack *stack;

   if (!ptr) return;

   _ptrs = eina_list_remove(_ptrs, ptr);

   _e_pointer_canvas_del(ptr);

   EINA_LIST_FREE(ptr->stack, stack)
     _e_pointer_stack_free(stack);

   if (ptr->type) eina_stringshare_del(ptr->type);
   ptr->type = NULL;

   if (ptr->idle_timer) ecore_timer_del(ptr->idle_timer);
   ptr->idle_timer = NULL;

   if (ptr->idle_poller) ecore_poller_del(ptr->idle_poller);
   ptr->idle_poller = NULL;

   E_FREE(ptr);
}

static Eina_Bool 
_e_pointer_cb_idle(void *data)
{
   E_Pointer *ptr;
   Evas_Coord x, y;

   if (!(ptr = data)) return ECORE_CALLBACK_RENEW;
   ecore_wl_pointer_xy_get(&x, &y);
   ptr->x = x;
   ptr->y = y;
   ptr->idle_timer = ecore_timer_loop_add(4.0, _e_pointer_cb_idle_wait, ptr);
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool 
_e_pointer_cb_idle_wait(void *data)
{
   E_Pointer *ptr;

   if (!(ptr = data)) return ECORE_CALLBACK_RENEW;
   if ((e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM))
     {
        if (ptr->idle_poller) ecore_poller_del(ptr->idle_poller);
        ptr->idle_poller = NULL;
        ptr->idle_timer = NULL;
        return ECORE_CALLBACK_CANCEL;
     }
   if (!ptr->idle_poller)
     ptr->idle_poller = ecore_poller_add(ECORE_POLLER_CORE, 64, 
                                         _e_pointer_cb_idle_poller, ptr);
   ptr->idle_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool 
_e_pointer_cb_idle_poller(void *data)
{
   E_Pointer *ptr;
   Evas_Coord x, y;

   if (!(ptr = data)) return ECORE_CALLBACK_RENEW;
   if ((e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM))
     {
        ptr->idle_poller = NULL;
        return ECORE_CALLBACK_CANCEL;
     }

   ecore_wl_pointer_xy_get(&x, &y);
   if ((x != ptr->x) || (y != ptr->y))
     {
        ptr->x = x;
        ptr->y = y;
        if (ptr->idle)
          {
             if (ptr->o_ptr)
               edje_object_signal_emit(ptr->o_ptr, "e,state,mouse,active", "e");
             ptr->idle = EINA_FALSE;
          }
        return ECORE_CALLBACK_RENEW;
     }

   if (!ptr->idle)
     {
        if (ptr->o_ptr)
          edje_object_signal_emit(ptr->o_ptr, "e,state,mouse,idle", "e");
        ptr->idle = EINA_TRUE;
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_pointer_cb_mouse_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l;
   E_Pointer *ptr;

   EINA_LIST_FOREACH(_ptrs, l, ptr)
     {
        _e_pointer_active_handle(ptr);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_HIGH)
          {
             if (ptr->o_ptr)
               edje_object_signal_emit(ptr->o_ptr, "e,action,mouse,down", "e");
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool 
_e_pointer_cb_mouse_up(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l;
   E_Pointer *ptr;

   EINA_LIST_FOREACH(_ptrs, l, ptr)
     {
        _e_pointer_active_handle(ptr);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_HIGH)
          {
             if (ptr->o_ptr)
               edje_object_signal_emit(ptr->o_ptr, "e,action,mouse,up", "e");
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool 
_e_pointer_cb_mouse_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l;
   E_Pointer *ptr;

   EINA_LIST_FOREACH(_ptrs, l, ptr)
     {
        _e_pointer_active_handle(ptr);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_HIGH)
          {
             if (ptr->o_ptr)
               edje_object_signal_emit(ptr->o_ptr, "e,action,mouse,move", "e");
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool 
_e_pointer_cb_mouse_wheel(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l;
   E_Pointer *ptr;

   EINA_LIST_FOREACH(_ptrs, l, ptr)
     {
        _e_pointer_active_handle(ptr);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_HIGH)
          {
             if (ptr->o_ptr)
               edje_object_signal_emit(ptr->o_ptr, "e,action,mouse,wheel", "e");
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void 
_e_pointer_cb_hot_move(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Pointer *ptr;
   Evas_Coord x, y;

   /* check for valid pointer */
   if (!(ptr = data)) return;

   if (!e_config->show_cursor) return;
   if (!ptr->e_cursor) return;

   edje_object_part_geometry_get(ptr->o_ptr, "e.swallow.hotspot", 
                                 &x, &y, NULL, NULL);

   if ((ptr->hot.x != x) || (ptr->hot.y != y))
     {
        ptr->hot.x = x;
        ptr->hot.y = y;
        ptr->hot.update = EINA_TRUE;
     }
}
