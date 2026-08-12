/* Which xdg_toplevel states a maximized window is told.
 *
 * A window snapped to half the screen, or stretched along one axis, is not
 * maximized: from version 2 it hears which edges are tiled instead. A client
 * bound at version 1 has no word for that and still hears maximized. */
#include <stdio.h>
#include <string.h>

#include "e_wl_testkit.h"

#define PROG "test-xdg-tiled"
#define APP_ID "e.test.xdg_tiled"
#define APP_ID_V1 "e.test.xdg_tiled.v1"

#define S(x) (1u << XDG_TOPLEVEL_STATE_##x)
#define SHAPE (S(MAXIMIZED) | S(TILED_LEFT) | S(TILED_RIGHT) | \
               S(TILED_TOP) | S(TILED_BOTTOM))

typedef struct
{
   Tk *tk;
   struct wl_surface *surface;
   unsigned int pending, states;
   int w, h, configures;
} V1;

static void
_v1_ping(void *d, struct xdg_wm_base *b, uint32_t serial)
{
   (void)d;
   xdg_wm_base_pong(b, serial);
}

static const struct xdg_wm_base_listener _v1_wm_base_listener = { _v1_ping };

static void
_v1_surface_configure(void *data, struct xdg_surface *s, uint32_t serial)
{
   V1 *v = data;

   xdg_surface_ack_configure(s, serial);
   v->states = v->pending;
   tk_surface_paint(v->tk, v->surface, v->w, v->h);
   v->configures++;
}

static const struct xdg_surface_listener _v1_surface_listener =
{
   _v1_surface_configure
};

static void
_v1_configure(void *data, struct xdg_toplevel *t, int32_t w, int32_t h,
              struct wl_array *states)
{
   V1 *v = data;
   uint32_t *st;

   (void)t;
   if (w > 0) v->w = w;
   if (h > 0) v->h = h;
   v->pending = 0;
   wl_array_for_each(st, states)
     if (*st < 32) v->pending |= 1u << *st;
}

static void
_v1_close(void *d, struct xdg_toplevel *t)
{
   (void)d; (void)t;
}

static const struct xdg_toplevel_listener _v1_listener =
{
   _v1_configure, _v1_close
};

static void
_expect(Tk *tk, const char *params, unsigned int states, unsigned int want,
        const char *what)
{
   if ((states & SHAPE) != want)
     tk_fail(tk, "%s (window_maximized_toggle \"%s\"): states 0x%x, "
                 "expected 0x%x", what, params, states & SHAPE, want);
}

static void
_toggle(Tk *tk, unsigned int id, const char *params)
{
   tk_action(tk, id, "window_maximized_toggle", params);
   tk_settle(tk);
}

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *c;
   V1 v1;
   struct xdg_wm_base *wm1;
   struct xdg_surface *xs1;
   struct xdg_toplevel *t1;
   unsigned int id;

   tk = tk_connect(PROG);
   if (tk_global_version(tk, "xdg_wm_base") < 2)
     tk_fail(tk, "xdg_wm_base is advertised at version %u, not 2",
             tk_global_version(tk, "xdg_wm_base"));

   top = tk_toplevel_new(tk, APP_ID, PROG, 300, 200);
   c = tk_expect(tk, APP_ID);
   id = c->id;

   _toggle(tk, id, "default left");
   _expect(tk, "default left", tk_toplevel_states(top),
           S(TILED_LEFT) | S(TILED_TOP) | S(TILED_BOTTOM),
           "a window snapped to the left half");
   _toggle(tk, id, "default left");
   _expect(tk, "default left", tk_toplevel_states(top), 0,
           "the snap undone");

   _toggle(tk, id, "default right");
   _expect(tk, "default right", tk_toplevel_states(top),
           S(TILED_RIGHT) | S(TILED_TOP) | S(TILED_BOTTOM),
           "a window snapped to the right half");
   _toggle(tk, id, "default right");

   _toggle(tk, id, "default horizontal");
   _expect(tk, "default horizontal", tk_toplevel_states(top),
           S(TILED_LEFT) | S(TILED_RIGHT),
           "a window maximized horizontally");
   _toggle(tk, id, "default horizontal");

   _toggle(tk, id, "default both");
   if (!(tk_toplevel_states(top) & S(MAXIMIZED)))
     tk_fail(tk, "a window maximized both ways is not told maximized "
                 "(states 0x%x)", tk_toplevel_states(top));
   _toggle(tk, id, "default both");
   tk_toplevel_del(top);

   memset(&v1, 0, sizeof(v1));
   v1.tk = tk;
   v1.w = 300;
   v1.h = 200;
   wm1 = tk_bind(tk, &xdg_wm_base_interface, 1);
   xdg_wm_base_add_listener(wm1, &_v1_wm_base_listener, NULL);
   v1.surface = tk_surface_new(tk);
   xs1 = xdg_wm_base_get_xdg_surface(wm1, v1.surface);
   xdg_surface_add_listener(xs1, &_v1_surface_listener, &v1);
   t1 = xdg_surface_get_toplevel(xs1);
   xdg_toplevel_add_listener(t1, &_v1_listener, &v1);
   xdg_toplevel_set_app_id(t1, APP_ID_V1);
   wl_surface_commit(v1.surface);
   tk_settle(tk);
   if (!v1.configures) tk_fail(tk, "the version 1 toplevel was never configured");
   c = tk_expect(tk, APP_ID_V1);
   id = c->id;

   _toggle(tk, id, "default left");
   _expect(tk, "default left", v1.states, S(MAXIMIZED),
           "a version 1 client snapped to the left half");
   _toggle(tk, id, "default left");

   xdg_toplevel_destroy(t1);
   xdg_surface_destroy(xs1);
   wl_surface_destroy(v1.surface);
   xdg_wm_base_destroy(wm1);

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
