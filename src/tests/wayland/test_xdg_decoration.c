/* zxdg_decoration_manager_v1: who draws a toplevel's titlebar.
 *
 * Unasked, E draws it. A client that asks to draw its own gets a window
 * with no border from E, and gets E's back when it unsets the mode. Each
 * protocol error ends a connection, so each has one of its own. */
#include <stdio.h>
#include <string.h>

#include "e_wl_testkit.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#define PROG "test-xdg-decoration"
#define APP_ID "e.test.xdg_decoration"
#define W 300
#define H 200

#define CSD ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
#define SSD ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE

typedef struct
{
   Tk *tk;
   struct wl_surface *surface;
   struct xdg_surface *xdg_surface;
   struct xdg_toplevel *toplevel;
   int configures;
   int mode;
} Win;

static void
_surface_configure(void *data, struct xdg_surface *s, uint32_t serial)
{
   Win *w = data;

   xdg_surface_ack_configure(s, serial);
   tk_surface_paint(w->tk, w->surface, W, H);
   w->configures++;
}

static const struct xdg_surface_listener _surface_listener =
{
   _surface_configure
};

static void
_configure(void *d, struct xdg_toplevel *t, int32_t w, int32_t h,
           struct wl_array *s)
{
   (void)d; (void)t; (void)w; (void)h; (void)s;
}

static void
_close(void *d, struct xdg_toplevel *t)
{
   (void)d; (void)t;
}

static void
_bounds(void *d, struct xdg_toplevel *t, int32_t w, int32_t h)
{
   (void)d; (void)t; (void)w; (void)h;
}

static void
_caps(void *d, struct xdg_toplevel *t, struct wl_array *c)
{
   (void)d; (void)t; (void)c;
}

static const struct xdg_toplevel_listener _toplevel_listener =
{
   _configure, _close, _bounds, _caps
};

static void
_deco_configure(void *data, struct zxdg_toplevel_decoration_v1 *d,
                uint32_t mode)
{
   Win *w = data;

   (void)d;
   w->mode = mode;
}

static const struct zxdg_toplevel_decoration_v1_listener _deco_listener =
{
   _deco_configure
};

/* A toplevel with its role and nothing committed yet. */
static void
_win_new(Tk *tk, Win *w)
{
   memset(w, 0, sizeof(*w));
   w->tk = tk;
   w->mode = -1;
   w->surface = tk_surface_new(tk);
   w->xdg_surface = xdg_wm_base_get_xdg_surface(tk_wm_base(tk), w->surface);
   xdg_surface_add_listener(w->xdg_surface, &_surface_listener, w);
   w->toplevel = xdg_surface_get_toplevel(w->xdg_surface);
   xdg_toplevel_add_listener(w->toplevel, &_toplevel_listener, w);
   xdg_toplevel_set_app_id(w->toplevel, APP_ID);
}

static struct zxdg_decoration_manager_v1 *
_manager(Tk *tk)
{
   struct zxdg_decoration_manager_v1 *m;

   m = tk_bind(tk, &zxdg_decoration_manager_v1_interface, 1);
   if (!m) tk_fail(tk, "no zxdg_decoration_manager_v1");
   return m;
}

static void
_expect_error(Tk *tk, int want, const char *what)
{
   const struct wl_interface *iface;
   int code = tk_protocol_error(tk, &iface);

   if (code != want)
     tk_fail(tk, "%s: protocol error %d, expected %d", what, code, want);
   if (iface != &zxdg_decoration_manager_v1_interface)
     tk_fail(tk, "%s: the error was posted on %s, not on the manager", what,
             iface ? iface->name : "nothing");
   tk_disconnect(tk);
}

static void
_expect_frame(Tk *tk, int bordered, const char *what)
{
   Tk_Client *c = tk_expect(tk, APP_ID);

   if (bordered && (c->w <= W) && (c->h <= H))
     tk_fail(tk, "%s: E draws the titlebar, but the %dx%d window has a %dx%d "
                 "frame", what, W, H, c->w, c->h);
   if ((!bordered) && ((c->w != W) || (c->h != H)))
     tk_fail(tk, "%s: the client draws its titlebar, but E framed the "
                 "%dx%d window as %dx%d", what, W, H, c->w, c->h);
}

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Win w;
   struct zxdg_decoration_manager_v1 *m;
   struct zxdg_toplevel_decoration_v1 *d, *d2;

   tk = tk_connect(PROG);
   m = _manager(tk);
   _win_new(tk, &w);
   d = zxdg_decoration_manager_v1_get_toplevel_decoration(m, w.toplevel);
   zxdg_toplevel_decoration_v1_add_listener(d, &_deco_listener, &w);
   tk_sync(tk);
   if (w.mode != SSD)
     tk_fail(tk, "a new decoration was configured with mode %d, not "
                 "server side", w.mode);
   wl_surface_commit(w.surface);
   tk_settle(tk);
   if (!w.configures) tk_fail(tk, "the toplevel was never configured");
   _expect_frame(tk, 1, "unasked");

   zxdg_toplevel_decoration_v1_set_mode(d, CSD);
   tk_settle(tk);
   if (w.mode != CSD)
     tk_fail(tk, "asked for client side and was configured with mode %d",
             w.mode);
   _expect_frame(tk, 0, "client side");

   zxdg_toplevel_decoration_v1_unset_mode(d);
   tk_settle(tk);
   if (w.mode != SSD)
     tk_fail(tk, "unset_mode was configured with mode %d, not server side",
             w.mode);
   _expect_frame(tk, 1, "unset_mode");

   zxdg_toplevel_decoration_v1_destroy(d);
   xdg_toplevel_destroy(w.toplevel);
   xdg_surface_destroy(w.xdg_surface);
   wl_surface_destroy(w.surface);
   zxdg_decoration_manager_v1_destroy(m);
   tk_sync(tk);
   tk_disconnect(tk);

   tk = tk_connect(PROG);
   m = _manager(tk);
   _win_new(tk, &w);
   d = zxdg_decoration_manager_v1_get_toplevel_decoration(m, w.toplevel);
   d2 = zxdg_decoration_manager_v1_get_toplevel_decoration(m, w.toplevel);
   (void)d; (void)d2;
   _expect_error(tk, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED,
                 "a second decoration for one toplevel");

   tk = tk_connect(PROG);
   m = _manager(tk);
   _win_new(tk, &w);
   d = zxdg_decoration_manager_v1_get_toplevel_decoration(m, w.toplevel);
   xdg_toplevel_destroy(w.toplevel);
   _expect_error(tk, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED,
                 "a toplevel destroyed before its decoration");

   tk = tk_connect(PROG);
   m = _manager(tk);
   top = tk_toplevel_new(tk, APP_ID, PROG, W, H);
   d = zxdg_decoration_manager_v1_get_toplevel_decoration(m,
                                                          tk_toplevel_xdg(top));
   _expect_error(tk, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_UNCONFIGURED_BUFFER,
                 "a decoration for a toplevel with a buffer");

   printf(PROG ": ok\n");
   return 0;
}
