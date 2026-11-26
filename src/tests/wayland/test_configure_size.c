/* A configure that leaves the size to the client, as a focus change sends,
 * keeps the size E asked for until the client has acked it and drawn it:
 * the latest configure wins, so 0x0 would let the client keep its old
 * size. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define PROG "test-configure-size"
#define APP_ID "e.test.configure-size"
#define W 200
#define H 150
#define ICONIC WL_TEST_CLIENT_STATE_ICONIFIED
#define FOCUS WL_TEST_CLIENT_STATE_FOCUSED

static uint32_t last_serial;
static int last_w, last_h, configures;

static void
_xdg_configure(void *data __attribute__((unused)), struct xdg_surface *xs __attribute__((unused)),
               uint32_t serial)
{
   last_serial = serial;
   configures++;
}

static const struct xdg_surface_listener _xdg_listener = { _xdg_configure };

static void
_toplevel_configure(void *data __attribute__((unused)), struct xdg_toplevel *top __attribute__((unused)),
                    int32_t w, int32_t h, struct wl_array *states __attribute__((unused)))
{
   last_w = w;
   last_h = h;
}

static void
_toplevel_close(void *data __attribute__((unused)), struct xdg_toplevel *top __attribute__((unused)))
{
}

static void
_toplevel_bounds(void *data __attribute__((unused)), struct xdg_toplevel *top __attribute__((unused)),
                 int32_t w __attribute__((unused)), int32_t h __attribute__((unused)))
{
}

static void
_toplevel_caps(void *data __attribute__((unused)), struct xdg_toplevel *top __attribute__((unused)),
               struct wl_array *caps __attribute__((unused)))
{
}

static const struct xdg_toplevel_listener _toplevel_listener =
{
   _toplevel_configure, _toplevel_close, _toplevel_bounds, _toplevel_caps
};

static void
_expect(Tk *tk, int count, int w, int h, const char *what)
{
   if (configures == count) tk_fail(tk, "%s: no configure", what);
   if ((last_w != w) || (last_h != h))
     tk_fail(tk, "%s: configured %dx%d, not %dx%d", what, last_w, last_h, w, h);
}

int
main(void)
{
   struct xdg_surface *xs;
   struct xdg_toplevel *top;
   struct wl_surface *surface;
   unsigned int id;
   int count;
   Tk *tk;

   tk = tk_connect(PROG);
   surface = tk_surface_new(tk);
   xs = xdg_wm_base_get_xdg_surface(tk_wm_base(tk), surface);
   xdg_surface_add_listener(xs, &_xdg_listener, NULL);
   top = xdg_surface_get_toplevel(xs);
   xdg_toplevel_add_listener(top, &_toplevel_listener, NULL);
   xdg_toplevel_set_app_id(top, APP_ID);
   wl_surface_commit(surface);
   tk_sync(tk);
   xdg_surface_ack_configure(xs, last_serial);
   tk_surface_paint(tk, surface, W, H);
   tk_wait_states(tk, APP_ID, FOCUS, FOCUS, "a new window has the focus");
   id = tk_expect(tk, APP_ID)->id;
   xdg_surface_ack_configure(xs, last_serial);
   wl_surface_commit(surface);
   tk_sync(tk);

   count = configures;
   tk_action(tk, id, "window_resize_by", "120 90");
   _expect(tk, count, W + 120, H + 90, "resized");

   count = configures;
   tk_action(tk, id, "window_iconic_toggle", NULL);
   tk_wait_states(tk, APP_ID, ICONIC, ICONIC, "iconified");
   _expect(tk, count, W + 120, H + 90, "iconified before the ack");

   xdg_surface_ack_configure(xs, last_serial);
   count = configures;
   tk_action(tk, id, "window_iconic_toggle", NULL);
   tk_wait_states(tk, APP_ID, ICONIC | FOCUS, FOCUS, "restored");
   _expect(tk, count, W + 120, H + 90, "restored after the ack, before the commit");

   xdg_surface_ack_configure(xs, last_serial);
   tk_surface_paint(tk, surface, W + 120, H + 90);
   count = configures;
   tk_action(tk, id, "window_iconic_toggle", NULL);
   tk_wait_states(tk, APP_ID, ICONIC, ICONIC, "iconified again");
   _expect(tk, count, 0, 0, "iconified after the commit");

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
