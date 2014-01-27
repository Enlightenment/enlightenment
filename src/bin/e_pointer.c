#include "e.h"

/*
 * TODO
 * - Make fallback user controlable.
 * - Define the allowed signals?
 */

typedef struct _E_Pointer_Stack E_Pointer_Stack;

struct _E_Pointer_Stack
{
   void       *obj;
   const char *type;
};

static Eina_List *_e_pointers = NULL;
static Eina_List *handlers = NULL;

static void      _e_pointer_canvas_del(E_Pointer *p);
static void      _e_pointer_free(E_Pointer *p);
static void      _e_pointer_stack_free(E_Pointer_Stack *elem);
static void      _e_pointer_type_set(E_Pointer *p,
                                     const char *type);
static void      _e_pointer_active_handle(E_Pointer *p);

static Eina_Bool _e_pointer_cb_idle_timer_pre(void *data);
static Eina_Bool _e_pointer_cb_idle_timer_wait(void *data);
static Eina_Bool _e_pointer_cb_idle_poller(void *data);


static void
_e_pointer_hot_update(E_Pointer *p, int x, int y)
{
   if ((p->hot.x != x) || (p->hot.y != y))
     {
        p->hot.x = x;
        p->hot.y = y;
        p->hot.update = 1;
     }
}

static void
_e_pointer_active(E_Pointer *p)
{
   if (!p->idle) return;
   if (p->pointer_object)
     edje_object_signal_emit(p->pointer_object,
                             "e,state,mouse,active", "e");
   p->idle = 0;
}

static void
_e_pointer_idle(E_Pointer *p)
{
   if (p->idle) return;
   if (p->pointer_object)
     edje_object_signal_emit(p->pointer_object, "e,state,mouse,idle", "e");
   p->idle = 1;
}

static void
_e_pointer_cb_show(void *data,
                   Evas *e __UNUSED__,
                   Evas_Object *obj,
                   void *event_info __UNUSED__)
{
   int x, y;
   edje_object_part_geometry_get(obj, "e.swallow.hotspot",
                                 &x, &y, NULL, NULL);
   _e_pointer_hot_update(data, x, y);
}

static void
_e_pointer_cb_move(void *data,
                   Evas *e __UNUSED__,
                   Evas_Object *obj __UNUSED__,
                   void *event_info __UNUSED__)
{
   E_Pointer *p;
   Evas_Coord x, y;

   if (!e_config->show_cursor) return;
   if (!(p = data)) return;
   if (!p->e_cursor) return;
   if (!evas_object_visible_get(p->pointer_object)) return;
   edje_object_part_geometry_get(p->pointer_object, "e.swallow.hotspot",
                                 &x, &y, NULL, NULL);
   _e_pointer_hot_update(p, x, y);
}

static void
_e_pointer_canvas_resize(E_Pointer *p, int w, int h)
{
   Evas_Engine_Info_Buffer *einfo;

   if ((p->w == w) && (p->h == h)) return;
   p->w = w, p->h = h;
   evas_output_size_set(p->evas, p->w, p->h);
   evas_output_viewport_set(p->evas, 0, 0, p->w, p->h);

   p->pixels = realloc(p->pixels, p->w * p->h * sizeof(int));

   einfo = (Evas_Engine_Info_Buffer *)evas_engine_info_get(p->evas);

   EINA_SAFETY_ON_NULL_RETURN(einfo);
   einfo->info.dest_buffer = p->pixels;
   einfo->info.dest_buffer_row_bytes = p->w * sizeof(int);
   evas_engine_info_set(p->evas, (Evas_Engine_Info *)einfo);

   evas_object_move(p->pointer_object, 0, 0);
   evas_object_resize(p->pointer_object, p->w, p->h);
   if (p->pointer_image) evas_object_resize(p->pointer_image, p->w, p->h);
}

/* local subsystem functions */
static void
_e_pointer_canvas_add(E_Pointer *p)
{
   Evas_Engine_Info_Buffer *einfo;
   Evas_Object *o;
   int rmethod;

   if (!p) return;
   p->w = e_config->cursor_size;
   p->h = e_config->cursor_size;

   /* create evas */
   p->evas = evas_new();
   if (!p->evas)
     {
        e_object_del(E_OBJECT(p));
        return;
     }
   rmethod = evas_render_method_lookup("buffer");
   evas_output_method_set(p->evas, rmethod);
   evas_output_size_set(p->evas, p->w, p->h);
   evas_output_viewport_set(p->evas, 0, 0, p->w, p->h);

   p->pixels = malloc(p->w * p->h * sizeof(int));
   if (!p->pixels)
     {
        _e_pointer_canvas_del(p);
        return;
     }
   einfo = (Evas_Engine_Info_Buffer *)evas_engine_info_get(p->evas);
   if (!einfo)
     {
        _e_pointer_canvas_del(p);
        return;
     }
   einfo->info.depth_type = EVAS_ENGINE_BUFFER_DEPTH_ARGB32;
   einfo->info.dest_buffer = p->pixels;
   einfo->info.dest_buffer_row_bytes = p->w * sizeof(int);
   einfo->info.use_color_key = 0;
   einfo->info.alpha_threshold = 0;
   einfo->info.func.new_update_region = NULL;
   einfo->info.func.free_update_region = NULL;
   evas_engine_info_set(p->evas, (Evas_Engine_Info *)einfo);

   /* set the pointer edje */
   o = edje_object_add(p->evas);
   p->pointer_object = o;
   /* Create the hotspot object */
   o = evas_object_rectangle_add(p->evas);
   evas_object_color_set(o, 0, 0, 0, 0);
   p->hot_object = o;
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE,
                                  _e_pointer_cb_move, p);
   evas_object_event_callback_add(o, EVAS_CALLBACK_SHOW,
                                  _e_pointer_cb_show, p);
   /* init edje */
   evas_object_move(p->pointer_object, 0, 0);
   evas_object_resize(p->pointer_object, p->w, p->h);
   evas_object_show(p->pointer_object);
}

static void
_e_pointer_canvas_del(E_Pointer *p)
{
   if (!p) return;
   E_FREE_FUNC(p->pointer_object, evas_object_del);
   E_FREE_FUNC(p->hot_object, evas_object_del);
   if (p->evas && (!p->canvas)) evas_free(p->evas);
   p->evas = NULL;
   E_FREE(p->pixels);
}

static void
_e_pointer_free(E_Pointer *p)
{
   if (!p) return;
   _e_pointers = eina_list_remove(_e_pointers, p);

   _e_pointer_canvas_del(p);
   E_FREE_FUNC(p->pointer_image, evas_object_del);

   E_FREE_LIST(p->stack, _e_pointer_stack_free);

   if (p->type) eina_stringshare_del(p->type);
   if (p->idle_timer) ecore_timer_del(p->idle_timer);
   if (p->idle_poller) ecore_poller_del(p->idle_poller);

   p->type = NULL;
   p->idle_timer = NULL;
   p->idle_poller = NULL;
   E_FREE(p);
}

static void
_e_pointer_stack_free(E_Pointer_Stack *elem)
{
   if (elem->type) eina_stringshare_del(elem->type);
   free(elem);
}

static void
_e_pointer_type_set(E_Pointer *p,
                    const char *type)
{
   if (!p) return;

   /* Check if this pointer is already set */
   if (!e_util_strcmp(p->type, type)) return;

   eina_stringshare_replace(&p->type, type);

   /* Do not set type if in "hidden mode" */
   if (!e_config->show_cursor)
     {
#ifdef WAYLAND_ONLY
        evas_object_hide(p->pointer_image);
#else
        if (!p->canvas)
          ecore_x_window_cursor_set(p->win, 0);
#endif
        return;
     }

   if (p->e_cursor)
     {
        char cursor[1024];
        Evas_Coord x, y;

        if (!p->evas) _e_pointer_canvas_add(p);
        if (p->color)
          snprintf(cursor, sizeof(cursor),
                   "e/pointer/enlightenment/%s/color", type);
        else
          snprintf(cursor, sizeof(cursor),
                   "e/pointer/enlightenment/%s/mono", type);
        if (!e_theme_edje_object_set(p->pointer_object,
                                     "base/theme/pointer", cursor))
          goto fallback;
        edje_object_part_swallow(p->pointer_object, "e.swallow.hotspot",
                                 p->hot_object);
        edje_object_part_geometry_get(p->pointer_object, "e.swallow.hotspot",
                                      &x, &y, NULL, NULL);
        if ((p->hot.x != x) || (p->hot.y != y))
          {
             p->hot.x = x;
             p->hot.y = y;
          }
        p->hot.update = 1;
        evas_object_show(p->pointer_object);
        return;
     }
fallback:
#ifndef WAYLAND_ONLY
   if (p->canvas) return;
   {
      Ecore_X_Cursor cursor = 0;

      E_FREE_FUNC(p->pointer_image, evas_object_del);
      if (p->evas) _e_pointer_canvas_del(p);
      if (!strcmp(type, "move"))
        cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_FLEUR);
#if 0
      else if (!strcmp(type, "resize"))
        cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_SIZING);
#endif
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
           printf("Unknown pointer type: %s\n", type);
           cursor = ecore_x_cursor_shape_get(ECORE_X_CURSOR_ARROW);
        }
      if (!cursor) printf("X Cursor for %s is missing\n", type);
      ecore_x_window_cursor_set(p->win, cursor);
      if (cursor) ecore_x_cursor_free(cursor);
   }
#endif
}

static void
_e_pointer_active_handle(E_Pointer *p)
{
   if (!p) return;
   _e_pointer_active(p);
   if (p->canvas)
     {
        ecore_timer_reset(p->idle_timer);
        return;
     }

   /* we got some mouse event - if there was an idle timer emit an active
    * signal as we WERE idle, NOW we are active */
   E_FREE_FUNC(p->idle_timer, ecore_timer_del);
   E_FREE_FUNC(p->idle_poller, ecore_poller_del);
   if (e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM) return;
   /* and schedule a pre-idle check in 1 second if no more events happen */
   if (!e_config->idle_cursor) return;
   p->idle_timer = ecore_timer_loop_add(1.0, _e_pointer_cb_idle_timer_pre, p);
}

static Eina_Bool
_e_pointer_cb_mouse_down(void *data __UNUSED__,
                         int type __UNUSED__,
                         void *event __UNUSED__)
{
   Eina_List *l;
   E_Pointer *p;

   EINA_LIST_FOREACH(_e_pointers, l, p)
     {
        _e_pointer_active_handle(p);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_EXTREME)
          {
             if (p->pointer_object)
               edje_object_signal_emit(p->pointer_object,
                                       "e,action,mouse,down", "e");
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_pointer_cb_mouse_up(void *data __UNUSED__,
                       int type __UNUSED__,
                       void *event __UNUSED__)
{
   Eina_List *l;
   E_Pointer *p;

   EINA_LIST_FOREACH(_e_pointers, l, p)
     {
        _e_pointer_active_handle(p);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_EXTREME)
          {
             if (p->pointer_object)
               edje_object_signal_emit(p->pointer_object,
                                       "e,action,mouse,up", "e");
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_pointer_cb_mouse_move(void *data __UNUSED__,
                         int type __UNUSED__,
                         Ecore_Event_Mouse_Move *ev)
{
   Eina_List *l;
   E_Pointer *p;

   EINA_LIST_FOREACH(_e_pointers, l, p)
     {
        _e_pointer_active_handle(p);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_HIGH)
          {
             if (p->pointer_object)
               edje_object_signal_emit(p->pointer_object,
                                       "e,action,mouse,move", "e");
          }
        if (p->canvas)
          {
             evas_object_move(p->pointer_image,
               e_comp_canvas_x_root_adjust(e_comp_get(NULL), ev->root.x),
               e_comp_canvas_y_root_adjust(e_comp_get(NULL), ev->root.y)
              );
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_pointer_cb_mouse_wheel(void *data __UNUSED__,
                          int type __UNUSED__,
                          void *event __UNUSED__)
{
   Eina_List *l;
   E_Pointer *p;

   EINA_LIST_FOREACH(_e_pointers, l, p)
     {
        _e_pointer_active_handle(p);
        if (e_powersave_mode_get() < E_POWERSAVE_MODE_EXTREME)
          {
             if (p->pointer_object)
               edje_object_signal_emit(p->pointer_object,
                                       "e,action,mouse,wheel", "e");
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_pointer_idle_timer(void *data)
{
   E_Pointer *p = data;

   if (e_config->idle_cursor)
     _e_pointer_idle(p);

   return EINA_TRUE;
}

#ifndef WAYLAND_ONLY
static Eina_Bool
_e_pointer_cb_idle_timer_pre(void *data)
{
   E_Pointer *p;
   int x, y;

   if (!(p = data)) return ECORE_CALLBACK_RENEW;
   ecore_x_pointer_xy_get(p->win, &x, &y);
   p->x = x;
   p->y = y;
   if (p->canvas)
     evas_object_move(p->pointer_image,
       e_comp_canvas_x_root_adjust(e_comp_get(NULL), x),
       e_comp_canvas_y_root_adjust(e_comp_get(NULL), y)
      );
   p->idle_timer = ecore_timer_loop_add(4.0, _e_pointer_cb_idle_timer_wait, p);
   return ECORE_CALLBACK_CANCEL;
}


static Eina_Bool
_e_pointer_cb_idle_timer_wait(void *data)
{
   E_Pointer *p;

   if (!(p = data)) return ECORE_CALLBACK_RENEW;
   if ((e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM) ||
       (!e_config->idle_cursor))
     {
        if (p->idle_poller) ecore_poller_del(p->idle_poller);
        p->idle_poller = NULL;
        p->idle_timer = NULL;
        return ECORE_CALLBACK_CANCEL;
     }
   if (!p->idle_poller)
     p->idle_poller = ecore_poller_add(ECORE_POLLER_CORE, 64,
                                       _e_pointer_cb_idle_poller, p);
   p->idle_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_pointer_cb_idle_poller(void *data)
{
   E_Pointer *p;
   int x, y;

   if (!(p = data)) return ECORE_CALLBACK_RENEW;
   if ((e_powersave_mode_get() >= E_POWERSAVE_MODE_MEDIUM) ||
       (!e_config->idle_cursor))
     {
        p->idle_poller = NULL;
        return ECORE_CALLBACK_CANCEL;
     }
   /* check if pointer actually moved since the 1 second post-mouse move idle
    * pre-timer that fetches the position */
   ecore_x_pointer_xy_get(p->win, &x, &y);
   if ((x != p->x) || (y != p->y))
     {
        /* it moved - so we are not idle yet - record position and wait
         * 4 secons more */
        p->x = x;
        p->y = y;
        if (p->idle)
          _e_pointer_active(p);

        /* use poller to check from now on */
        return ECORE_CALLBACK_RENEW;
     }
   /* we are idle - report it if not idle before */
   if (!p->idle)
     _e_pointer_idle(p);
   return ECORE_CALLBACK_RENEW;
}
#endif


/* externally accessible functions */
EINTERN int
e_pointer_init(void)
{
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_DOWN,
                         _e_pointer_cb_mouse_down, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_UP,
                         _e_pointer_cb_mouse_up, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_MOVE,
                         _e_pointer_cb_mouse_move, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_WHEEL,
                         _e_pointer_cb_mouse_wheel, NULL);
   return 1;
}

EINTERN int
e_pointer_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
   return 1;
}

EAPI E_Pointer *
e_pointer_window_new(Ecore_Window win,
                     int filled)
{
   E_Pointer *p = NULL;

   EINA_SAFETY_ON_FALSE_RETURN_VAL(win, NULL);
   p = E_OBJECT_ALLOC(E_Pointer, E_POINTER_TYPE, _e_pointer_free);
   if (!p) return NULL;

   p->e_cursor = e_config->use_e_cursor;
   p->win = win;
   p->color = 0;
   {
      E_Comp *c;

      c = e_comp_get(NULL);
      if (c->pointer)
        p->color = c->pointer->color;
   }

#ifndef WAYLAND_ONLY
   ecore_x_cursor_size_set(e_config->cursor_size * 3 / 4);
#endif
   if (filled) e_pointer_type_push(p, p, "default");
   _e_pointers = eina_list_append(_e_pointers, p);
   return p;
}

EAPI E_Pointer *
e_pointer_canvas_new(Evas *e, int filled)
{
   E_Pointer *p = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e, NULL);
   p = E_OBJECT_ALLOC(E_Pointer, E_POINTER_TYPE, _e_pointer_free);
   if (!p) return NULL;

   p->e_cursor = p->canvas = 1;
   p->color = 1;
   p->w = p->h = e_config->cursor_size;
   p->idle_timer = ecore_timer_loop_add(4.0, _e_pointer_idle_timer, p);
   p->pointer_image = evas_object_image_filled_add(e);
   evas_object_pass_events_set(p->pointer_image, 1);
   evas_object_layer_set(p->pointer_image, EVAS_LAYER_MAX);
   evas_object_image_alpha_set(p->pointer_image, 1);
   evas_object_show(p->pointer_image);

   if (filled) e_pointer_type_push(p, p, "default");
   _e_pointers = eina_list_append(_e_pointers, p);
   return p;
}

EAPI void
e_pointer_image_set(E_Pointer *p, E_Pixmap *cp, int w, int h, int hot_x, int hot_y)
{
   void *img = NULL;

   EINA_SAFETY_ON_NULL_RETURN(p);

   p->pixmap = cp;
   if (cp)
     {
        e_pixmap_refresh(cp);
        e_pixmap_image_refresh(cp);
        img = e_pixmap_image_data_get(cp);
     }
   else
     {
        Eina_Stringshare *type;

        type = p->type;
        if (type)
          {
             p->type = NULL;
             _e_pointer_type_set(p, type);
             eina_stringshare_del(type);
          }
        evas_object_image_size_set(p->pointer_image, 1, 1);
        evas_object_image_data_set(p->pointer_image, NULL);
        E_FREE_FUNC(p->pointer_image, evas_object_del);
        p->blocks = 0;
        _e_pointer_canvas_resize(p, e_config->cursor_size, e_config->cursor_size);
        return;
     }
   _e_pointer_hot_update(p, hot_x, hot_y);
          
   if (p->evas)
     {
        /* e cursor: edje or image */
        if (!p->canvas)
          _e_pointer_canvas_resize(p, w, h);
        if (!p->pointer_image)
          {
             if (!p->e_cursor)
               p->blocks = 1;
             p->pointer_image = evas_object_image_filled_add(p->evas);
             evas_object_image_alpha_set(p->pointer_image, 1);
          }
        evas_object_image_size_set(p->pointer_image, w, h);
        evas_object_image_data_set(p->pointer_image, img);
        evas_object_resize(p->pointer_image, w, h);
        if (p->e_cursor)
          {
             evas_object_hide(p->pointer_image);
             evas_object_show(p->pointer_object);
          }
        else
          {
             evas_object_hide(p->pointer_object);
             evas_object_show(p->pointer_image);
          }
     }
#ifndef WAYLAND_ONLY
   else if (!p->e_cursor)
     {
         Ecore_X_Cursor cur;

         if (cp)
           cur = ecore_x_cursor_new(p->win, img, w, h,
                                    p->hot.x, p->hot.y);
         ecore_x_window_cursor_set(p->win, cp ? cur : 0);
         if (cp)
           ecore_x_cursor_free(cur);
     }
#endif
}

EAPI void
e_pointers_size_set(int size)
{
   Eina_List *l;
   E_Pointer *p;

   if (!e_config->show_cursor) return;
   EINA_LIST_FOREACH(_e_pointers, l, p)
     {
        Eina_Stringshare *type;

        p->e_cursor = e_config->use_e_cursor;
        if (p->evas)
          {
             if (!p->canvas)
               _e_pointer_canvas_resize(p, size, size);
          }
#ifndef WAYLAND_ONLY
        else
          ecore_x_cursor_size_set(e_config->cursor_size * 3 / 4);
#endif
        type = p->type;
        if (type)
          {
             p->type = NULL;
             _e_pointer_type_set(p, type);
             eina_stringshare_del(type);
          }
     }
}

EAPI void
e_pointer_hide(E_Pointer *p)
{
   if (!p) return;
#ifdef WAYLAND_ONLY
#else
   if (p->win) ecore_x_window_cursor_set(p->win, 0);
#endif
   if (p->canvas)
     evas_object_hide(p->pointer_image);
   else
     E_FREE_FUNC(p->pointer_image, evas_object_del);
   if (p->evas) _e_pointer_canvas_del(p);
}

EAPI void
e_pointer_type_push(E_Pointer *p,
                    void *obj,
                    const char *type)
{
   E_Pointer_Stack *stack;

   if (!p) return;
   p->e_cursor = e_config->use_e_cursor;

   if (!p->canvas)
     {
        evas_object_hide(p->pointer_image);
#ifndef WAYLAND_ONLY
        if (p->blocks)
          {
             ecore_x_cursor_size_set(e_config->cursor_size * 3 / 4);
             _e_pointer_canvas_resize(p, e_config->cursor_size, e_config->cursor_size);
          }
#endif
     }
   _e_pointer_type_set(p, type);

   p->obj = obj;

   stack = E_NEW(E_Pointer_Stack, 1);
   stack->type = eina_stringshare_ref(p->type);
   stack->obj = p->obj;
   p->stack = eina_list_prepend(p->stack, stack);
}

EAPI void
e_pointer_type_pop(E_Pointer *p,
                   void *obj,
                   const char *type)
{
   Eina_List *l, *l_next;
   E_Pointer_Stack *stack;

   if (!p) return;
   EINA_LIST_FOREACH_SAFE(p->stack, l, l_next, stack)
     {
        if ((stack->obj == obj) && ((!type) || (!e_util_strcmp(stack->type, type))))
          {
             _e_pointer_stack_free(stack);
             p->stack = eina_list_remove_list(p->stack, l);
             if (type) break;
          }
     }

   if (!p->stack)
     {
        e_pointer_hide(p);
        eina_stringshare_replace(&p->type, NULL);
        return;
     }

   stack = eina_list_data_get(p->stack);
   _e_pointer_type_set(p, stack->type);
   if ((p->blocks) && (stack->obj == p))
     {
        evas_object_hide(p->pointer_object);
        if (p->pointer_image)
          {
             evas_object_show(p->pointer_image);
#ifndef WAYLAND_ONLY
             if (!p->canvas)
               {
                  int w, h;

                  e_pixmap_size_get(p->pixmap, &w, &h);
                  _e_pointer_canvas_resize(p, w, h);
          }
#endif
          }
     }

   eina_stringshare_refplace(&p->type, stack->type);
   p->obj = stack->obj;

   /* try the default cursor next time */
   p->e_cursor = e_config->use_e_cursor;
}

EAPI void
e_pointer_idler_before(void)
{
#ifndef WAYLAND_ONLY
   Eina_List *l;
   E_Pointer *p;

   if (!e_config->show_cursor) return;
   EINA_LIST_FOREACH(_e_pointers, l, p)
     {
        Eina_List *updates;

        if (!p->e_cursor) continue;
        if (!p->evas) continue;

        updates = evas_render_updates(p->evas);
        if ((updates) || (p->hot.update))
          {
             if (p->canvas)
               {
                  if (!p->blocks)
                    {
                       evas_object_resize(p->pointer_image, p->w, p->h);
                       evas_object_image_size_set(p->pointer_image, p->w, p->h);
                       evas_object_image_data_set(p->pointer_image, p->pixels);
                    }
               }
             else
               {
                  Ecore_X_Cursor cur;

                  cur = ecore_x_cursor_new(p->win, p->pixels, p->w, p->h,
                                           p->hot.x, p->hot.y);
                  ecore_x_window_cursor_set(p->win, cur);
                  ecore_x_cursor_free(cur);
               }
             evas_render_updates_free(updates);
          }
        p->hot.update = 0;
     }
#endif
}

/**
 * mode_{push,pop} functions are ONLY for use with compositor objects.
 * these functions should NEVER be used for setting internal window pointers!!!!
 */
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
