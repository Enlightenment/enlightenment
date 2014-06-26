#include "e.h"

typedef struct _E_Pointer_Stack E_Pointer_Stack;
struct _E_Pointer_Stack
{
   void *obj;
   const char *type;
};

/* local variables */
static Eina_List *_hdlrs = NULL;
static Eina_List *_ptrs = NULL;

static void 
_e_pointer_active(E_Pointer *ptr)
{
   if (!ptr->idle) return;
   if (ptr->o_ptr) 
     edje_object_signal_emit(ptr->o_ptr, "e,state,mouse,active", "e");
   ptr->idle = EINA_FALSE;
}

static void 
_e_pointer_idle(E_Pointer *ptr)
{
   if (ptr->idle) return;
   if (ptr->o_ptr)
     edje_object_signal_emit(ptr->o_ptr, "e,state,mouse,idle", "e");
   ptr->idle = EINA_TRUE;
}

static Eina_Bool 
_e_pointer_cb_idle_poll(void *data)
{
   E_Pointer *ptr;
   int x = 0, y = 0;

   if (!(ptr = data)) return ECORE_CALLBACK_RENEW;
   if ((e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM) || 
       (!e_config->idle_cursor))
     {
        ptr->idle_poll = NULL;
        return ECORE_CALLBACK_CANCEL;
     }

   ecore_evas_pointer_xy_get(ptr->ee, &x, &y);
   if ((ptr->x != x) || (ptr->y != y))
     {
        ptr->x = x;
        ptr->y = y;
        if (ptr->idle) _e_pointer_active(ptr);
        return ECORE_CALLBACK_RENEW;
     }

   if (!ptr->idle) _e_pointer_idle(ptr);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_pointer_cb_idle_wait(void *data)
{
   E_Pointer *ptr;

   if (!(ptr = data)) return ECORE_CALLBACK_RENEW;
   if ((e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM) || 
       (!e_config->idle_cursor))
     {
        E_FREE_FUNC(ptr->idle_poll, ecore_poller_del);
        ptr->idle_tmr = NULL;
        return ECORE_CALLBACK_CANCEL;
     }

   if (!ptr->idle_poll)
     {
        ptr->idle_poll = 
          ecore_poller_add(ECORE_POLLER_CORE, 64, 
                           _e_pointer_cb_idle_poll, ptr);
     }

   ptr->idle_tmr = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool 
_e_pointer_cb_idle_pre(void *data)
{
   E_Pointer *ptr;

   if (!(ptr = data)) return ECORE_CALLBACK_RENEW;
   ecore_evas_pointer_xy_get(ptr->ee, &ptr->x, &ptr->y);
   ptr->idle_tmr = ecore_timer_loop_add(4.0, _e_pointer_cb_idle_wait, ptr);
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool 
_e_pointer_cb_idle(void *data)
{
   E_Pointer *ptr;

   if (!(ptr = data)) return ECORE_CALLBACK_RENEW;
   if (e_config->idle_cursor) _e_pointer_idle(ptr);

   return EINA_TRUE;
}

static void 
_e_pointer_active_handle(E_Pointer *ptr)
{
   if (!ptr) return;
   _e_pointer_active(ptr);
   if (ptr->idle_tmr)
     ecore_timer_reset(ptr->idle_tmr);
   else
     {
        E_FREE_FUNC(ptr->idle_tmr, ecore_timer_del);
        E_FREE_FUNC(ptr->idle_poll, ecore_poller_del);
        if (e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM) return;
        if (!e_config->idle_cursor) return;
        ptr->idle_tmr = ecore_timer_loop_add(1.0, _e_pointer_cb_idle_pre, ptr);
     }
}

static void 
_e_pointer_hot_update(E_Pointer *ptr, Evas_Coord x, Evas_Coord y)
{
   if ((ptr->hot.x != x) || (ptr->hot.y != y))
     {
        ptr->hot.x = x;
        ptr->hot.y = y;
        ptr->hot.update = EINA_TRUE;
     }
}

static Eina_Bool 
_e_pointer_cb_mouse_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l;
   E_Pointer *ptr;

   EINA_LIST_FOREACH(_ptrs, l, ptr)
     {
        _e_pointer_active_handle(ptr);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_EXTREME)
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
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_EXTREME)
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
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_EXTREME)
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
   Evas_Coord x = 0, y = 0;

   if (!(ptr = data)) return;
   if (!e_config->show_cursor) return;
   if (!ptr->e_cursor) return;
   if (!evas_object_visible_get(ptr->o_ptr)) return;
   edje_object_part_geometry_get(ptr->o_ptr, "e.swallow.hotspot", 
                                 &x, &y, NULL, NULL);
   _e_pointer_hot_update(ptr, x, y);
}

static void 
_e_pointer_cb_hot_show(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Pointer *ptr;
   Evas_Coord x = 0, y = 0;

   if (!(ptr = data)) return;
   edje_object_part_geometry_get(obj, "e.swallow.hotspot", 
                                 &x, &y, NULL, NULL);
   _e_pointer_hot_update(ptr, x, y);
}

static void 
_e_pointer_stack_free(E_Pointer_Stack *stack)
{
   if (stack->type) eina_stringshare_del(stack->type);
   free(stack);
}

static void 
_e_pointer_cb_free(E_Pointer *ptr)
{
   _ptrs = eina_list_remove(_ptrs, ptr);

   E_FREE_FUNC(ptr->o_ptr, evas_object_del);
   E_FREE_FUNC(ptr->o_hot, evas_object_del);

   E_FREE_LIST(ptr->stack, _e_pointer_stack_free);

   if (ptr->type) eina_stringshare_del(ptr->type);

   E_FREE_FUNC(ptr->idle_tmr, ecore_timer_del);
   E_FREE_FUNC(ptr->idle_poll, ecore_poller_del);

   E_FREE(ptr);
}

static void 
_e_pointer_type_set(E_Pointer *ptr, const char *type)
{
   if (!ptr) return;

   if (!e_util_strcmp(ptr->type, type)) return;

   eina_stringshare_replace(&ptr->type, type);

   if (!e_config->show_cursor) 
     {
        e_pointer_hide(ptr);
        return;
     }

   if (ptr->e_cursor)
     {
        char cursor[1024];
        Evas_Coord x, y;

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

        _e_pointer_hot_update(ptr, x, y);

        evas_object_show(ptr->o_ptr);

//        if (ptr->hot.update)
          {
             /* Layer Max - 32 snarfed from Elm */
             ecore_evas_object_cursor_set(ptr->ee, ptr->o_ptr, EVAS_LAYER_MAX, 
                                          ptr->hot.x, ptr->hot.y);
             /* ptr->hot.update = EINA_FALSE; */
          }

        return;
     }

fallback:
   WRN("FALLBACK POINTER !!!");
     {
#ifndef HAVE_WAYLAND_ONLY
        Ecore_X_Cursor cursor = 0;

        if (!strcmp(type, "move"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_FLEUR);
# if 0
        else if (!strcmp(type, "resize"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_SIZING);
# endif
        else if (!strcmp(type, "resize_tl"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_TOP_LEFT_CORNER);
        else if (!strcmp(type, "resize_t"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_TOP_SIDE);
        else if (!strcmp(type, "resize_tr"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_TOP_RIGHT_CORNER);
        else if (!strcmp(type, "resize_r"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_RIGHT_SIDE);
        else if (!strcmp(type, "resize_br"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_BOTTOM_RIGHT_CORNER);
        else if (!strcmp(type, "resize_b"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_BOTTOM_SIDE);
        else if (!strcmp(type, "resize_bl"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_BOTTOM_LEFT_CORNER);
        else if (!strcmp(type, "resize_l"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_LEFT_SIDE);
        else if (!strcmp(type, "entry"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_XTERM);
        else if (!strcmp(type, "default"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_LEFT_PTR);
        else if (!strcmp(type, "plus"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_PLUS);
        else if (!strcmp(type, "hand"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_HAND1);
        else if (!strcmp(type, "rotate"))
          cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_EXCHANGE);
        else
          {
             WRN("Unknown pointer type: %s\n", type);
             cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_ARROW);
          }
        if (!cursor) WRN("X Cursor for %s is missing\n", type);
        ecore_x_window_cursor_set(ptr->win, cursor);
        if (cursor) ecore_x_cursor_free(cursor);
#endif
     }
   return;
}

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

#ifndef HAVE_WAYLAND_ONLY
   ecore_x_cursor_size_set(e_config->cursor_size * 3 / 4);
#endif

   return 1;
}

EINTERN int 
e_pointer_shutdown(void)
{
   E_FREE_LIST(_hdlrs, ecore_event_handler_del);

   return 1;
}

EAPI E_Pointer *
e_pointer_window_new(Ecore_Window win, Eina_Bool filled)
{
   WRN("E_Pointer Window New Called !!!");
   return NULL;

   E_Pointer *ptr;
   E_Comp *comp;

   EINA_SAFETY_ON_FALSE_RETURN_VAL(win, NULL);

   /* allocate new e_pointer object */
   if (!(ptr = E_OBJECT_ALLOC(E_Pointer, E_POINTER_TYPE, _e_pointer_cb_free)))
     return NULL;

   /* set some pointer properties */
   ptr->e_cursor = e_config->use_e_cursor;
   ptr->color = EINA_FALSE;
   ptr->win = win;

   if ((comp = e_comp_get(NULL)))
     {
        if (comp->pointer)
          ptr->color = comp->pointer->color;
     }

   if (filled) e_pointer_type_push(ptr, ptr, "default");

   /* append this pointer to the list */
   _ptrs = eina_list_append(_ptrs, ptr);

   return ptr;
}

EAPI E_Pointer *
e_pointer_canvas_new(Ecore_Evas *ee, Eina_Bool filled)
{
   E_Pointer *ptr;
   E_Comp *comp;

   EINA_SAFETY_ON_FALSE_RETURN_VAL(ee, NULL);

   /* allocate new e_pointer object */
   if (!(ptr = E_OBJECT_ALLOC(E_Pointer, E_POINTER_TYPE, _e_pointer_cb_free)))
     return NULL;

   /* set some pointer properties */
   ptr->ee = ee;
   ptr->e_cursor = e_config->use_e_cursor;
   ptr->w = ptr->h = e_config->cursor_size;
   ptr->canvas = EINA_TRUE;
   ptr->win = ecore_evas_window_get(ee);

   ptr->color = EINA_TRUE;
   if ((comp = e_comp_get(NULL)))
     {
        if (comp->pointer)
          ptr->color = comp->pointer->color;
     }

   ptr->evas = ecore_evas_get(ee);
   ptr->o_ptr = edje_object_add(ptr->evas);

   ptr->o_hot = evas_object_rectangle_add(ptr->evas);
   evas_object_color_set(ptr->o_hot, 0, 0, 0, 0);
   evas_object_event_callback_add(ptr->o_hot, EVAS_CALLBACK_MOVE, 
                                  _e_pointer_cb_hot_move, ptr);
   evas_object_event_callback_add(ptr->o_hot, EVAS_CALLBACK_SHOW, 
                                  _e_pointer_cb_hot_show, ptr);

   evas_object_layer_set(ptr->o_ptr, EVAS_LAYER_MAX);
   evas_object_move(ptr->o_ptr, 0, 0);
   evas_object_resize(ptr->o_ptr, ptr->w, ptr->h);
   evas_object_show(ptr->o_ptr);

   if (filled) e_pointer_type_push(ptr, ptr, "default");

   ptr->idle_tmr = ecore_timer_loop_add(4.0, _e_pointer_cb_idle, ptr);

   /* append this pointer to the list */
   _ptrs = eina_list_append(_ptrs, ptr);

   return ptr;
}

EAPI void 
e_pointers_size_set(int size)
{
   Eina_List *l;
   E_Pointer *ptr;

   if (!e_config->show_cursor) return;

   EINA_LIST_FOREACH(_ptrs, l, ptr)
     {
        if ((ptr->w == size) && (ptr->h == size)) continue;
        ptr->w = size;
        ptr->h = size;
        evas_object_resize(ptr->o_ptr, ptr->w, ptr->h);
     }
}

EAPI void 
e_pointer_hide(E_Pointer *ptr)
{
   if (!ptr) return;
#ifndef HAVE_WAYLAND_ONLY
   ecore_x_window_cursor_set(ptr->win, 0);
#endif
   evas_object_hide(ptr->o_ptr);
}

EAPI void 
e_pointer_type_push(E_Pointer *ptr, void *obj, const char *type)
{
   E_Pointer_Stack *stack;

   if (!ptr) return;

   _e_pointer_type_set(ptr, type);

   /* ptr->obj = obj; */

   if (!(stack = E_NEW(E_Pointer_Stack, 1))) return;
   stack->type = eina_stringshare_ref(ptr->type);
   stack->obj = obj;
   ptr->stack = eina_list_prepend(ptr->stack, stack);
}

EAPI void 
e_pointer_type_pop(E_Pointer *ptr, void *obj, const char *type)
{
   Eina_List *l, *ll;
   E_Pointer_Stack *stack;

   if (!ptr) return;

   EINA_LIST_FOREACH_SAFE(ptr->stack, l, ll, stack)
     {
        if ((stack->obj == obj) && 
            ((!type) || (!e_util_strcmp(stack->type, type))))
          {
             _e_pointer_stack_free(stack);
             ptr->stack = eina_list_remove_list(ptr->stack, l);
             if (type) break;
          }
     }

   if (!ptr->stack)
     {
//        e_pointer_hide(ptr);
        eina_stringshare_replace(&ptr->type, NULL);
        return;
     }

   if (!(stack = eina_list_data_get(ptr->stack))) return;

   _e_pointer_type_set(ptr, stack->type);

   eina_stringshare_refplace(&ptr->type, stack->type);
   /* ptr->obj = stack->obj; */
}

EAPI void 
e_pointer_mode_push(void *obj, E_Pointer_Mode mode)
{
   switch (mode)
     {
      case E_POINTER_RESIZE_TL:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "resize_tl");
        break;

      case E_POINTER_RESIZE_T:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "resize_t");
        break;

      case E_POINTER_RESIZE_TR:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "resize_tr");
        break;

      case E_POINTER_RESIZE_R:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "resize_r");
        break;

      case E_POINTER_RESIZE_BR:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "resize_br");
        break;

      case E_POINTER_RESIZE_B:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "resize_b");
        break;

      case E_POINTER_RESIZE_BL:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "resize_bl");
        break;

      case E_POINTER_RESIZE_L:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "resize_l");
        break;

      case E_POINTER_MOVE:
        e_pointer_type_push(e_comp_get(obj)->pointer, obj, "move");
        break;

      default: break;
     }
}

EAPI void 
e_pointer_mode_pop(void *obj, E_Pointer_Mode mode)
{
   switch (mode)
     {
      case E_POINTER_RESIZE_TL:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "resize_tl");
        break;

      case E_POINTER_RESIZE_T:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "resize_t");
        break;

      case E_POINTER_RESIZE_TR:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "resize_tr");
        break;

      case E_POINTER_RESIZE_R:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "resize_r");
        break;

      case E_POINTER_RESIZE_BR:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "resize_br");
        break;

      case E_POINTER_RESIZE_B:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "resize_b");
        break;

      case E_POINTER_RESIZE_BL:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "resize_bl");
        break;

      case E_POINTER_RESIZE_L:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "resize_l");
        break;

      case E_POINTER_MOVE:
        e_pointer_type_pop(e_comp_get(obj)->pointer, obj, "move");
        break;

      default: break;
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
        if ((!ptr->e_cursor) || (!ptr->evas)) continue;
        if (ptr->hot.update)
          _e_pointer_type_set(ptr, ptr->type);
        ptr->hot.update = EINA_FALSE;
     }
}
