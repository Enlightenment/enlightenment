/* A grabbing popup is dismissed when a new toplevel turns up, and as soon as
 * the role is taken: the new window may sit on its first configure, and the
 * menu must not keep the keyboard in the meantime. */
#include <stdio.h>
#include <string.h>

#include "e_wl_testkit.h"

#define PROG "test-popup-dismiss"
#define APP_ID "e.test.popup_dismiss"

typedef struct
{
   Tk *tk;
   struct wl_surface *surface;
   int configured, done;
} Popup;

static void
_popup_surface_configure(void *data, struct xdg_surface *s, uint32_t serial)
{
   Popup *p = data;

   xdg_surface_ack_configure(s, serial);
   if (!p->configured) tk_surface_paint(p->tk, p->surface, 60, 40);
   p->configured++;
}

static const struct xdg_surface_listener _popup_surface_listener =
{
   _popup_surface_configure
};

static void
_popup_configure(void *d, struct xdg_popup *x, int32_t px, int32_t py,
                 int32_t w, int32_t h)
{
   (void)d; (void)x; (void)px; (void)py; (void)w; (void)h;
}

static void
_popup_done(void *data, struct xdg_popup *x)
{
   Popup *p = data;

   (void)x;
   p->done++;
}

static void
_popup_repositioned(void *d, struct xdg_popup *x, uint32_t token)
{
   (void)d; (void)x; (void)token;
}

static const struct xdg_popup_listener _popup_listener =
{
   _popup_configure, _popup_done, _popup_repositioned
};

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Popup p;
   struct wl_seat *seat;
   struct xdg_positioner *pos;
   struct xdg_surface *xs, *xs2;
   struct xdg_popup *popup;
   struct xdg_toplevel *second;
   struct wl_surface *s2;

   memset(&p, 0, sizeof(p));
   tk = tk_connect(PROG);
   seat = tk_bind(tk, &wl_seat_interface, 1);
   if (!seat) tk_fail(tk, "no wl_seat");

   top = tk_toplevel_new(tk, APP_ID, PROG, 300, 200);

   pos = xdg_wm_base_create_positioner(tk_wm_base(tk));
   xdg_positioner_set_size(pos, 60, 40);
   xdg_positioner_set_anchor_rect(pos, 10, 10, 1, 1);

   p.tk = tk;
   p.surface = tk_surface_new(tk);
   xs = xdg_wm_base_get_xdg_surface(tk_wm_base(tk), p.surface);
   xdg_surface_add_listener(xs, &_popup_surface_listener, &p);
   popup = xdg_surface_get_popup(xs, tk_toplevel_xdg_surface(top), pos);
   xdg_popup_add_listener(popup, &_popup_listener, &p);
   xdg_popup_grab(popup, seat, 0);
   wl_surface_commit(p.surface);
   tk_settle(tk);
   if (!p.configured) tk_fail(tk, "the popup was never configured");
   if (p.done) tk_fail(tk, "the popup was dismissed before anything happened");

   /* The role, and nothing else: no commit, so no configure and no map. */
   s2 = tk_surface_new(tk);
   xs2 = xdg_wm_base_get_xdg_surface(tk_wm_base(tk), s2);
   second = xdg_surface_get_toplevel(xs2);
   tk_sync(tk);
   if (p.done != 1)
     tk_fail(tk, "a new toplevel was created and the grabbing popup got %d "
                 "popup_done", p.done);

   xdg_toplevel_destroy(second);
   xdg_surface_destroy(xs2);
   wl_surface_destroy(s2);
   xdg_popup_destroy(popup);
   xdg_surface_destroy(xs);
   wl_surface_destroy(p.surface);
   xdg_positioner_destroy(pos);
   tk_toplevel_del(top);

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
