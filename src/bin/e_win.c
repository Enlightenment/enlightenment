#include "e.h"
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
# include "e_comp_wl.h"
#endif


/* local subsystem functions */
static void _e_win_free(E_Win *win);
static void _e_win_prop_update(E_Win *win);
static void _e_win_state_update(E_Win *win);
static void _e_win_cb_move(Ecore_Evas *ee);
static void _e_win_cb_resize(Ecore_Evas *ee);
static void _e_win_cb_delete(Ecore_Evas *ee);
static void _e_win_cb_destroy(Ecore_Evas *ee);
static void _e_win_cb_state(Ecore_Evas *ee);

/* local subsystem globals */
static Eina_List *wins = NULL;

/* intercept elm_win operations so we talk directly to e_client */

#include <Elementary.h>

typedef struct _Elm_Win_Trap_Ctx
{
   E_Client      *client;
   Ecore_Window xwin;
   Eina_Bool      centered : 1;
   Eina_Bool      placed : 1;
} Elm_Win_Trap_Ctx;

static void
_elm_win_prop_update(Elm_Win_Trap_Ctx *ctx)
{
   EC_CHANGED(ctx->client);
   ctx->client->changes.internal_props = 1;
}

static void *
_elm_win_trap_add(Evas_Object *o __UNUSED__)
{
   Elm_Win_Trap_Ctx *ctx = calloc(1, sizeof(Elm_Win_Trap_Ctx));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   return ctx;
}

static void
_elm_win_trap_del(void *data, Evas_Object *o)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN(ctx);
   if (ctx->client)
     {
        evas_object_hide(ctx->client->frame);
        e_object_del(E_OBJECT(ctx->client));
        evas_object_data_set(o, "E_Client", NULL);
        ctx->client->internal_ecore_evas = NULL;
     }
   free(ctx);
}

static Eina_Bool
_elm_win_trap_hide(void *data, Evas_Object *o __UNUSED__)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   if (!ctx->client) return EINA_TRUE;
   evas_object_hide(ctx->client->frame);
   return EINA_FALSE;
}

static Eina_Bool
_elm_win_trap_show(void *data, Evas_Object *o)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   if (!ctx->client)
     {
        E_Client *ec;
        Ecore_Window win;

#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        win = elm_win_window_id_get(o);
        ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, win);
#else
        win = elm_win_xwindow_get(o);
        ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, win);
#endif

        Evas *e = evas_object_evas_get(o);
        Ecore_Evas *ee = ecore_evas_ecore_evas_get(e);

        ctx->xwin = win;

        if (ec)
          ctx->client = ec;
        else
          {
             E_Pixmap *cp;
             E_Comp *c = NULL;

#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
             cp = e_pixmap_new(E_PIXMAP_TYPE_WL, win);
#else
             cp = e_pixmap_new(E_PIXMAP_TYPE_X, win);
#endif
             EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_TRUE);

             /* if (eina_list_count(e_comp_list()) > 1) */
             /*   { */
/* #ifndef HAVE_WAYLAND_ONLY */
             /*      c = e_comp_find_by_window(ecore_x_window_root_get(win)); */
/* #else */
             /*      c = ; */
/* #endif */
             /*   } */

             if (!c)
               c = e_comp_get(NULL);
             ctx->client = e_client_new(c, cp, 0, 1);
             EINA_SAFETY_ON_NULL_RETURN_VAL(ctx->client, EINA_TRUE);
          }
        ctx->client->placed = ctx->placed;
        ctx->client->internal_ecore_evas = ee;
        evas_object_data_set(o, "E_Client", ctx->client);
     }
//#endif
   if (ctx->centered) e_comp_object_util_center(ctx->client->frame);
   evas_object_show(ctx->client->frame);
   return EINA_TRUE;
}

static Eina_Bool
_elm_win_trap_move(void *data, Evas_Object *o __UNUSED__, int x, int y)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   ctx->centered = EINA_FALSE;
   ctx->placed = EINA_TRUE;
   if (!ctx->client) return EINA_TRUE;
   e_client_util_move_without_frame(ctx->client, x, y);
   return EINA_FALSE;
}

static Eina_Bool
_elm_win_trap_resize(void *data, Evas_Object *o __UNUSED__, int w, int h)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   ctx->centered = EINA_FALSE;
   if (!ctx->client) return EINA_TRUE;
   e_client_resize_limit(ctx->client, &w, &h);
   e_client_util_resize_without_frame(ctx->client, w, h);
   return EINA_FALSE;
}

static Eina_Bool
_elm_win_trap_center(void *data, Evas_Object *o __UNUSED__)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   ctx->centered = EINA_TRUE;
   if (!ctx->client) return EINA_TRUE;
   if (ctx->centered) e_comp_object_util_center(ctx->client->frame);
   return EINA_FALSE;
}

static Eina_Bool
_elm_win_trap_lower(void *data, Evas_Object *o __UNUSED__)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   if (!ctx->client) return EINA_TRUE;
   evas_object_lower(ctx->client->frame);
   return EINA_FALSE;
}

static Eina_Bool
_elm_win_trap_raise(void *data, Evas_Object *o __UNUSED__)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   if (!ctx->client) return EINA_TRUE;
   evas_object_raise(ctx->client->frame);
   return EINA_FALSE;
}

static Eina_Bool
_elm_win_trap_activate(void *data, Evas_Object *o __UNUSED__)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   if (!ctx->client) return EINA_TRUE;
   evas_object_focus_set(ctx->client->frame, 1);
   if (!ctx->client->lock_user_stacking)
     evas_object_raise(ctx->client->frame);
   return EINA_FALSE;
}

static Eina_Bool
_elm_win_trap_size_base_set(void *data, Evas_Object *o __UNUSED__, int w, int h)
{
   Elm_Win_Trap_Ctx *ctx = data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_TRUE);
   if (!ctx->client) return EINA_TRUE;
   ctx->client->icccm.base_w = w;
   ctx->client->icccm.base_h = h;
   _elm_win_prop_update(ctx);

   return EINA_TRUE;
}

static const Elm_Win_Trap _elm_win_trap = {
   ELM_WIN_TRAP_VERSION,
   _elm_win_trap_add,
   _elm_win_trap_del,
   _elm_win_trap_hide,
   _elm_win_trap_show,
   _elm_win_trap_move,
   _elm_win_trap_resize,
   _elm_win_trap_center,
   _elm_win_trap_lower,
   _elm_win_trap_raise,
   _elm_win_trap_activate,
   /* alpha_set */ NULL,
   /* aspect_set */ NULL,
   /* avoid_damage_set */ NULL,
   /* borderless_set */ NULL,
   /* demand_attention_set */ NULL,
   /* focus_skip_set */ NULL,
   /* fullscreen_set */ NULL,
   /* iconified_set */ NULL,
   /* layer_set */ NULL,
   /* manual_render_set */ NULL,
   /* maximized_set */ NULL,
   /* modal_set */ NULL,
   /* name_class_set */ NULL,
   /* object_cursor_set */ NULL,
   /* override_set */ NULL,
   /* rotation_set */ NULL,
   /* rotation_with_resize_set */ NULL,
   /* shaped_set */ NULL,
   _elm_win_trap_size_base_set,
   /* size_step_set */ NULL,
   /* size_min_set */ NULL,
   /* size_max_set */ NULL,
   /* sticky_set */ NULL,
   /* title_set */ NULL,
   /* urgent_set */ NULL,
   /* withdrawn_set */ NULL
};

static void
_e_win_hide(void *obj)
{
   E_Win *win = obj;
   E_Win_Cb cb;

   if (!win->client) return;
   e_object_ref(E_OBJECT(win));
   cb = win->cb_delete;
   win->cb_delete = NULL;
   if (cb) cb(win);
   if (!e_object_unref(E_OBJECT(win)))
     return;
   if ((win->pointer) && (win->comp->comp_type != E_PIXMAP_TYPE_WL))
     E_FREE_FUNC(win->pointer, e_object_del);
   e_canvas_del(win->ecore_evas);
   ecore_evas_callback_move_set(win->ecore_evas, NULL);
   ecore_evas_callback_resize_set(win->ecore_evas, NULL);
   ecore_evas_callback_delete_request_set(win->ecore_evas, NULL);
   ecore_evas_callback_state_change_set(win->ecore_evas, NULL);
   ecore_evas_callback_destroy_set(win->ecore_evas, NULL);
   /* prevent any more rendering at this point */
   ecore_evas_manual_render_set(win->ecore_evas, 1);
   if (win->client->visible) evas_object_hide(win->client->frame);
   e_object_del(E_OBJECT(win->client));
}

/* externally accessible functions */
EINTERN int
e_win_init(void)
{
   if (!elm_win_trap_set(&_elm_win_trap)) return 0;
   return 1;
}

EINTERN int
e_win_shutdown(void)
{
/*
   while (wins)
     {
        e_object_del(E_OBJECT(wins->data));
     }
 */
   return 1;
}

E_API Eina_Bool
e_win_elm_available(void)
{
   return EINA_TRUE;
}

E_API E_Win *
e_win_new(E_Comp *c)
{
   E_Win *win;
   Evas_Object *obj;

   win = E_OBJECT_ALLOC(E_Win, E_WIN_TYPE, _e_win_free);
   if (!win) return NULL;
   e_object_delay_del_set(E_OBJECT(win), _e_win_hide);
   win->comp = c;
   win->ecore_evas = e_canvas_new(c->man->root,
                                  0, 0, 1, 1, 1, 0,
                                  &win->evas_win);
   e_canvas_add(win->ecore_evas);
   ecore_evas_data_set(win->ecore_evas, "E_Win", win);
   ecore_evas_callback_move_set(win->ecore_evas, _e_win_cb_move);
   ecore_evas_callback_resize_set(win->ecore_evas, _e_win_cb_resize);
   ecore_evas_callback_delete_request_set(win->ecore_evas, _e_win_cb_delete);
   ecore_evas_callback_destroy_set(win->ecore_evas, _e_win_cb_destroy);
   ecore_evas_callback_state_change_set(win->ecore_evas, _e_win_cb_state);
   win->evas = ecore_evas_get(win->ecore_evas);
   ecore_evas_name_class_set(win->ecore_evas, "E", "_e_internal_window");
   ecore_evas_title_set(win->ecore_evas, "E");
   obj = evas_object_rectangle_add(win->evas);
   evas_object_name_set(obj, "E_Win");
   evas_object_data_set(obj, "E_Win", win);
   win->x = 0;
   win->y = 0;
   win->w = 1;
   win->h = 1;
   win->placed = 0;
   win->min_w = 0;
   win->min_h = 0;
   win->max_w = 9999;
   win->max_h = 9999;
   win->base_w = 0;
   win->base_h = 0;
   win->step_x = 1;
   win->step_y = 1;
   win->min_aspect = 0.0;
   win->max_aspect = 0.0;
   wins = eina_list_append(wins, win);

   if (c->comp_type == E_PIXMAP_TYPE_X)
     win->pointer = e_pointer_window_new(win->evas_win, EINA_TRUE);
   else if (c->comp_type == E_PIXMAP_TYPE_WL)
     win->pointer = c->pointer;

   return win;
}

E_API void
e_win_show(E_Win *win)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   ecore_evas_show(win->ecore_evas);
   if (!win->client)
     {
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        if (!strncmp(ecore_evas_engine_name_get(win->ecore_evas), "wayland", 7))
          {
             Ecore_Wl_Window *wl_win;
             E_Pixmap *ep;
             uint64_t id;

             wl_win = ecore_evas_wayland_window_get(win->ecore_evas);
             id = e_comp_wl_id_get(getpid(), ecore_wl_window_surface_id_get(wl_win));
             if (!(ep = e_pixmap_find(E_PIXMAP_TYPE_WL, id)))
               ep = e_pixmap_new(E_PIXMAP_TYPE_WL, id);
             win->client = e_client_new(win->comp, ep, 1, 1);
          }
        else
#endif
          win->client = e_client_new(win->comp, e_pixmap_new(E_PIXMAP_TYPE_X, win->evas_win), 0, 1);
        EINA_SAFETY_ON_NULL_RETURN(win->client);
        if (win->ecore_evas)
          win->client->internal_ecore_evas = win->ecore_evas;
        if (win->state.no_remember) win->client->internal_no_remember = 1;
        win->client->internal_no_reopen = win->state.no_reopen;
        win->client->client.w = win->client->w = win->w;
        win->client->client.h = win->client->h = win->h;
        win->client->take_focus = win->client->changes.size = win->client->changes.pos = 1;
        EC_CHANGED(win->client);
     }
#ifndef HAVE_WAYLAND_ONLY
   if (e_pixmap_is_x(win->client->pixmap))
     {
        if (win->state.dialog)
          ecore_x_icccm_transient_for_set(ecore_evas_window_get(win->ecore_evas), win->client->comp->man->root);
        else
          ecore_x_icccm_transient_for_unset(ecore_evas_window_get(win->ecore_evas));
     }
#endif
   _e_win_prop_update(win);
   if (win->state.centered)
     e_comp_object_util_center(win->client->frame);
}

E_API void
e_win_hide(E_Win *win)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   if (win->client) evas_object_hide(win->client->frame);
}

/**
 * This will move window to position, automatically accounts client decorations.
 *
 * @parm x horizontal position to place window.
 * @parm y vertical position to place window.
 */
E_API void
e_win_move(E_Win *win, int x, int y)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   if (win->client)
     {
        e_client_util_move_without_frame(win->client, x, y);
        win->client->changes.pos = 1;
        EC_CHANGED(win->client);
     }
   else
     ecore_evas_move(win->ecore_evas, x, y);
}

/**
 * This will resize window, automatically accounts client decorations.
 *
 * @parm w horizontal window size.
 * @parm h vertical window size.
 */
E_API void
e_win_resize(E_Win *win, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   if (win->client)
     {
        e_client_util_resize_without_frame(win->client, w, h);
        win->client->changes.size = 1;
        EC_CHANGED(win->client);
     }
   else
     ecore_evas_resize(win->ecore_evas, w, h);
}

/**
 * This will move and resize window to position, automatically
 * accounts client decorations.
 *
 * @parm x horizontal position to place window.
 * @parm y vertical position to place window.
 * @parm w horizontal window size.
 * @parm h vertical window size.
 */
E_API void
e_win_move_resize(E_Win *win, int x, int y, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   if (win->client)
     {
        e_client_util_move_resize_without_frame(win->client, x, y, w, h);
        win->client->changes.pos = win->client->changes.size = 1;
        EC_CHANGED(win->client);
     }
   else
     ecore_evas_move_resize(win->ecore_evas, x, y, w, h);
}

E_API void
e_win_raise(E_Win *win)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   if (win->client)
     evas_object_raise(win->client->frame);
}

E_API void
e_win_lower(E_Win *win)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   if (win->client)
     evas_object_lower(win->client->frame);
}

E_API void
e_win_placed_set(E_Win *win, int placed)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   win->placed = placed;
   if (win->client)
     _e_win_prop_update(win);
}

E_API Evas *
e_win_evas_get(E_Win *win)
{
   E_OBJECT_CHECK_RETURN(win, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(win, E_WIN_TYPE, NULL);
   return win->evas;
}

E_API void
e_win_move_callback_set(E_Win *win, void (*func)(E_Win *win))
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   win->cb_move = func;
}

E_API void
e_win_resize_callback_set(E_Win *win, void (*func)(E_Win *win))
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   win->cb_resize = func;
}

E_API void
e_win_delete_callback_set(E_Win *win, void (*func)(E_Win *win))
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   win->cb_delete = func;
}

E_API void
e_win_shaped_set(E_Win *win, int shaped)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   ecore_evas_shaped_set(win->ecore_evas, shaped);
}

E_API void
e_win_avoid_damage_set(E_Win *win, int avoid)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   ecore_evas_avoid_damage_set(win->ecore_evas, avoid);
}

E_API void
e_win_borderless_set(E_Win *win, int borderless)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   ecore_evas_borderless_set(win->ecore_evas, borderless);
}

E_API void
e_win_layer_set(E_Win *win, E_Win_Layer layer)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   ecore_evas_layer_set(win->ecore_evas, layer);
}

E_API void
e_win_sticky_set(E_Win *win, int sticky)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   ecore_evas_sticky_set(win->ecore_evas, sticky);
}

E_API void
e_win_size_min_set(E_Win *win, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   w = MIN(w, 32768);
   h = MIN(h, 32768);
   win->min_w = w;
   win->min_h = h;
   if (!win->client) return;
   _e_win_prop_update(win);
}

E_API void
e_win_size_max_set(E_Win *win, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   w = MIN(w, 32768);
   h = MIN(h, 32768);
   win->max_w = w;
   win->max_h = h;
   if (!win->client) return;
   _e_win_prop_update(win);
}

E_API void
e_win_size_base_set(E_Win *win, int w, int h)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   w = MIN(w, 32768);
   h = MIN(h, 32768);
   win->base_w = w;
   win->base_h = h;
   if (!win->client) return;
   _e_win_prop_update(win);
}

E_API void
e_win_step_set(E_Win *win, int x, int y)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   win->step_x = x;
   win->step_y = y;
   if (!win->client) return;
   _e_win_prop_update(win);
}

E_API void
e_win_name_class_set(E_Win *win, const char *name, const char *class)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   ecore_evas_name_class_set(win->ecore_evas, name, class);
}

E_API void
e_win_title_set(E_Win *win, const char *title)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   ecore_evas_title_set(win->ecore_evas, title);
}

E_API void
e_win_centered_set(E_Win *win, int centered)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   if ((win->state.centered) && (!centered))
     {
        win->state.centered = 0;
        _e_win_state_update(win);
     }
   else if ((!win->state.centered) && (centered))
     {
        win->state.centered = 1;
        _e_win_state_update(win);
     }
}

E_API void
e_win_dialog_set(E_Win *win, int dialog)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   if ((win->state.dialog) && (!dialog))
     {
        win->state.dialog = 0;
        _e_win_prop_update(win);
     }
   else if ((!win->state.dialog) && (dialog))
     {
        win->state.dialog = 1;
        _e_win_prop_update(win);
     }
}

E_API void
e_win_no_remember_set(E_Win *win, int no_remember)
{
   E_OBJECT_CHECK(win);
   E_OBJECT_TYPE_CHECK(win, E_WIN_TYPE);
   win->state.no_remember = no_remember;
}

E_API E_Win *
e_win_evas_object_win_get(Evas_Object *obj)
{
   Evas *evas;
   Evas_Object *wobj;
   E_Win *win;

   if (!obj) return NULL;
   evas = evas_object_evas_get(obj);
   wobj = evas_object_name_find(evas, "E_Win");
   if (!wobj) return NULL;
   win = evas_object_data_get(wobj, "E_Win");
   return win;
}

E_API void
e_win_client_icon_set(E_Win *win, const char *icon)
{
   if (win->client)
     eina_stringshare_replace(&win->client->internal_icon, icon);
}

E_API void
e_win_client_icon_key_set(E_Win *win, const char *key)
{
   if (win->client)
     eina_stringshare_replace(&win->client->internal_icon_key, key);
}

/* local subsystem functions */
static void
_e_win_free(E_Win *win)
{
   if ((win->pointer) && (win->comp->comp_type != E_PIXMAP_TYPE_WL))
     e_object_del(E_OBJECT(win->pointer));

   if (win->ecore_evas)
     {
        ecore_evas_callback_move_set(win->ecore_evas, NULL);
        ecore_evas_callback_resize_set(win->ecore_evas, NULL);
        ecore_evas_callback_state_change_set(win->ecore_evas, NULL);
        ecore_evas_callback_destroy_set(win->ecore_evas, NULL);
     }
   wins = eina_list_remove(wins, win);
   free(win);
}

static void
_e_win_prop_update(E_Win *win)
{
   if (!win->client) return;
   EC_CHANGED(win->client);
   win->client->icccm.min_w = win->min_w;
   win->client->icccm.min_h = win->min_h;
   win->client->icccm.max_w = win->max_w;
   win->client->icccm.max_h = win->max_h;
   win->client->icccm.base_w = win->base_w;
   win->client->icccm.base_h = win->base_h;
   win->client->icccm.step_w = win->step_x;
   win->client->icccm.step_h = win->step_y;
   win->client->icccm.min_aspect = win->min_aspect;
   win->client->icccm.max_aspect = win->max_aspect;
   win->client->changes.internal_props = 1;
}

static void
_e_win_state_update(E_Win *win)
{
   if (!win->client) return;
   EC_CHANGED(win->client);
   win->client->changes.internal_state = 1;
   if (win->state.centered)
     e_comp_object_util_center(win->client->frame);
}

static void
_e_win_cb_move(Ecore_Evas *ee)
{
   E_Win *win;

   win = ecore_evas_data_get(ee, "E_Win");
   if (!win) return;
   if (win->client)
     win->x = win->client->x, win->y = win->client->y;
   else
     ecore_evas_geometry_get(win->ecore_evas, &win->x, &win->y, NULL, NULL);
   if (win->cb_move) win->cb_move(win);
}

static void
_e_win_cb_resize(Ecore_Evas *ee)
{
   E_Win *win;

   win = ecore_evas_data_get(ee, "E_Win");
   if (!win) return;
   ecore_evas_geometry_get(win->ecore_evas, NULL, NULL, &win->w, &win->h);
   if (win->cb_resize) win->cb_resize(win);
}

static void
_e_win_cb_destroy(Ecore_Evas *ee)
{
   E_Win *win;

   win = ecore_evas_data_get(ee, "E_Win");
   if (!win) return;
   e_object_ref(E_OBJECT(win));
   if (win->cb_delete) win->cb_delete(win);
   win->cb_delete = NULL;
   e_object_unref(E_OBJECT(win));
   /* E_FREE_FUNC(win->pointer, e_object_del); */
   if (!win->client) return;
   win->client->internal_ecore_evas = NULL;
   e_canvas_del(ee);
   e_pixmap_usable_set(win->client->pixmap, 0);
   if (win->client->visible) evas_object_hide(win->client->frame);
   win->ecore_evas = NULL;
   e_object_del(E_OBJECT(win->client));
}

static void
_e_win_cb_delete(Ecore_Evas *ee)
{
   E_Win *win;
   E_Win_Cb cb;

   win = ecore_evas_data_get(ee, "E_Win");
   if (!win) return;
   e_object_ref(E_OBJECT(win));
   cb = win->cb_delete;
   win->cb_delete = NULL;
   if (cb) cb(win);
   e_object_unref(E_OBJECT(win));
}

static void
_e_win_cb_state(Ecore_Evas *ee)
{
   E_Win *win;

   win = ecore_evas_data_get(ee, "E_Win");
   if (!win) return;
   if (!win->client) return;
   EC_CHANGED(win->client);
   win->client->changes.size = 1;
}
