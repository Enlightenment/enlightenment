/* xdg_toplevel_drag_v1: a window attached to a drag follows the pointer at
 * the offset the client gave, relative to its window geometry. It is not a
 * drop target while it is carried, it stays where the drop leaves it, and
 * it is detached when it is unmapped. */
#include <stdio.h>
#include <string.h>
#include <linux/input-event-codes.h>

#include "e_wl_testkit.h"
#include "xdg-toplevel-drag-v1-client-protocol.h"

#define PROG "test-toplevel-drag"
#define SHADOW 10

typedef struct
{
   Tk *tk;
   struct xdg_toplevel_drag_manager_v1 *manager;
   struct wl_data_device_manager *ddm;
   struct wl_data_device *device;
   struct wl_seat *seat;
   struct wl_surface *drag_target, *pointer_focus;
} Session;

static void
_dd_data_offer(void *d __attribute__((unused)), struct wl_data_device *dd __attribute__((unused)),
               struct wl_data_offer *o __attribute__((unused)))
{
}

static void
_dd_enter(void *data, struct wl_data_device *dd __attribute__((unused)),
          uint32_t serial __attribute__((unused)), struct wl_surface *surface,
          wl_fixed_t x __attribute__((unused)), wl_fixed_t y __attribute__((unused)),
          struct wl_data_offer *o __attribute__((unused)))
{
   Session *s = data;

   s->drag_target = surface;
}

static void
_dd_leave(void *data, struct wl_data_device *dd __attribute__((unused)))
{
   Session *s = data;

   s->drag_target = NULL;
}

static void
_dd_motion(void *d __attribute__((unused)), struct wl_data_device *dd __attribute__((unused)),
           uint32_t t __attribute__((unused)), wl_fixed_t x __attribute__((unused)),
           wl_fixed_t y __attribute__((unused)))
{
}

static void
_dd_drop(void *d __attribute__((unused)), struct wl_data_device *dd __attribute__((unused)))
{
}

static void
_dd_selection(void *d __attribute__((unused)), struct wl_data_device *dd __attribute__((unused)),
              struct wl_data_offer *o __attribute__((unused)))
{
}

static const struct wl_data_device_listener _device_listener =
{
   _dd_data_offer, _dd_enter, _dd_leave, _dd_motion, _dd_drop, _dd_selection
};

static void
_ptr_enter(void *data, struct wl_pointer *p __attribute__((unused)),
           uint32_t serial __attribute__((unused)), struct wl_surface *surface,
           wl_fixed_t x __attribute__((unused)), wl_fixed_t y __attribute__((unused)))
{
   Session *s = data;

   s->pointer_focus = surface;
}

static void
_ptr_leave(void *data, struct wl_pointer *p __attribute__((unused)),
           uint32_t serial __attribute__((unused)), struct wl_surface *surface __attribute__((unused)))
{
   Session *s = data;

   s->pointer_focus = NULL;
}

static void
_ptr_motion(void *d __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
            uint32_t t __attribute__((unused)), wl_fixed_t x __attribute__((unused)),
            wl_fixed_t y __attribute__((unused)))
{
}

static void
_ptr_button(void *d __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
            uint32_t serial __attribute__((unused)), uint32_t t __attribute__((unused)),
            uint32_t b __attribute__((unused)), uint32_t st __attribute__((unused)))
{
}

static void
_ptr_axis(void *d __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
          uint32_t t __attribute__((unused)), uint32_t a __attribute__((unused)),
          wl_fixed_t v __attribute__((unused)))
{
}

static const struct wl_pointer_listener _pointer_listener =
{
   _ptr_enter, _ptr_leave, _ptr_motion, _ptr_button, _ptr_axis
};

static void
_session_new(Session *s)
{
   memset(s, 0, sizeof(*s));
   s->tk = tk_connect(PROG);
   s->manager = tk_bind(s->tk, &xdg_toplevel_drag_manager_v1_interface, 1);
   s->ddm = tk_bind(s->tk, &wl_data_device_manager_interface, 3);
   s->seat = tk_bind(s->tk, &wl_seat_interface, 1);
   if ((!s->manager) || (!s->ddm) || (!s->seat)) tk_fail(s->tk, "missing global");
   s->device = wl_data_device_manager_get_data_device(s->ddm, s->seat);
   wl_data_device_add_listener(s->device, &_device_listener, s);
   wl_pointer_add_listener(wl_seat_get_pointer(s->seat), &_pointer_listener, s);
}

static struct wl_data_source *
_source_new(Session *s)
{
   struct wl_data_source *src = wl_data_device_manager_create_data_source(s->ddm);

   wl_data_source_offer(src, "text/plain");
   wl_data_source_set_actions(src, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
   return src;
}

/* A press in the origin window, which has the keyboard focus, and a drag
 * started from it. */
static void
_drag_start(Session *s, struct wl_data_source *src, Tk_Toplevel *origin, int x, int y)
{
   tk_pointer_warp(s->tk, x, y);
   tk_pointer_button(s->tk, BTN_LEFT, 1);
   tk_sync(s->tk);
   wl_data_device_start_drag(s->device, src, tk_toplevel_surface(origin), NULL, 0);
   tk_sync(s->tk);
}

/* Where the window geometry is: for a window with a shadow, E's geometry
 * is the window geometry, inside the surface. */
static void
_expect_at(Session *s, Tk_Toplevel *top, int x, int y, const char *what)
{
   int wx, wy;

   tk_surface_geometry(s->tk, tk_toplevel_surface(top), &wx, &wy, NULL, NULL);
   if ((wx != x) || (wy != y))
     tk_fail(s->tk, "%s: the window is at %d,%d, not %d,%d", what, wx, wy, x, y);
}

static void
_expect_error(Session *s, const struct wl_interface *want, int code, const char *what)
{
   const struct wl_interface *iface = NULL;
   int got;

   got = tk_protocol_error(s->tk, &iface);
   if ((got != code) || (iface != want))
     tk_fail(s->tk, "%s is not the protocol's error: %d on %s", what, got,
             iface ? iface->name : "nothing");
   tk_disconnect(s->tk);
}

/* Carried at an offset, over a window of the same client that must be the
 * drop target although the carried window is under the pointer; left where
 * the drop puts it, and back to taking pointer events. */
static void
_test_carry(void)
{
   struct xdg_toplevel_drag_v1 *drag;
   struct wl_data_source *src;
   Tk_Toplevel *dragged, *origin;
   Session s;
   int ox, oy;

   _session_new(&s);
   dragged = tk_toplevel_new(s.tk, "e.test.drag.dragged", "dragged", 100, 80);
   origin = tk_toplevel_new(s.tk, "e.test.drag.origin", "origin", 400, 300);
   tk_surface_geometry(s.tk, tk_toplevel_surface(origin), &ox, &oy, NULL, NULL);

   src = _source_new(&s);
   drag = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(s.manager, src);
   _drag_start(&s, src, origin, ox + 20, oy + 20);
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(dragged), 10, 20);
   tk_settle(s.tk);
   _expect_at(&s, dragged, ox + 10, oy, "attached");

   tk_pointer_warp(s.tk, ox + 200, oy + 150);
   tk_settle(s.tk);
   _expect_at(&s, dragged, ox + 190, oy + 130, "first move");
   if (s.drag_target != tk_toplevel_surface(origin))
     tk_fail(s.tk, "the carried window, not the one under it, is the drop target");

   tk_pointer_warp(s.tk, ox + 100, oy + 60);
   tk_settle(s.tk);
   _expect_at(&s, dragged, ox + 90, oy + 40, "second move");
   if (s.drag_target != tk_toplevel_surface(origin))
     tk_fail(s.tk, "the drop target changed to the carried window");

   tk_pointer_button(s.tk, BTN_LEFT, 0);
   tk_settle(s.tk);
   tk_pointer_warp(s.tk, ox + 300, oy + 250);
   tk_settle(s.tk);
   _expect_at(&s, dragged, ox + 90, oy + 40, "after the drop");
   tk_pointer_warp(s.tk, ox + 120, oy + 70);
   tk_settle(s.tk);
   if (s.pointer_focus != tk_toplevel_surface(dragged))
     tk_fail(s.tk, "after the drop the window does not get the pointer");

   xdg_toplevel_drag_v1_destroy(drag);
   wl_data_source_destroy(src);
   tk_sync(s.tk);
   tk_disconnect(s.tk);
}

/* The offset is relative to the window geometry, not to the surface,
 * which is SHADOW larger on every side. */
static void
_test_window_geometry(void)
{
   struct xdg_toplevel_drag_v1 *drag;
   struct wl_data_source *src;
   Tk_Toplevel *dragged, *origin;
   Session s;
   int ox, oy;

   _session_new(&s);
   dragged = tk_toplevel_new_shadowed(s.tk, "e.test.drag.shadowed", "shadowed", 100, 80, SHADOW);
   origin = tk_toplevel_new(s.tk, "e.test.drag.origin", "origin", 400, 300);
   tk_surface_geometry(s.tk, tk_toplevel_surface(origin), &ox, &oy, NULL, NULL);

   /* Attached before the drag starts, as for a window that already
    * exists: it goes to the pointer when the drag starts. */
   src = _source_new(&s);
   drag = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(s.manager, src);
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(dragged), 5, 5);
   _drag_start(&s, src, origin, ox + 20, oy + 20);
   tk_settle(s.tk);
   _expect_at(&s, dragged, ox + 15, oy + 15, "when the drag starts");
   tk_pointer_warp(s.tk, ox + 200, oy + 150);
   tk_settle(s.tk);
   _expect_at(&s, dragged, ox + 195, oy + 145, "with a shadow");

   tk_pointer_button(s.tk, BTN_LEFT, 0);
   tk_settle(s.tk);
   tk_disconnect(s.tk);
}

/* Unmapped while carried, a window is detached: it stays where it is,
 * and another one can be attached in its place, before or after the
 * pointer moves again. */
static void
_test_unmap(void)
{
   struct xdg_toplevel_drag_v1 *drag;
   struct wl_data_source *src;
   Tk_Toplevel *first, *second, *third, *origin;
   Session s;
   int ox, oy;

   _session_new(&s);
   first = tk_toplevel_new(s.tk, "e.test.drag.first", "first", 100, 80);
   second = tk_toplevel_new(s.tk, "e.test.drag.second", "second", 100, 80);
   third = tk_toplevel_new(s.tk, "e.test.drag.third", "third", 100, 80);
   origin = tk_toplevel_new(s.tk, "e.test.drag.origin", "origin", 400, 300);
   tk_surface_geometry(s.tk, tk_toplevel_surface(origin), &ox, &oy, NULL, NULL);

   src = _source_new(&s);
   drag = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(s.manager, src);
   _drag_start(&s, src, origin, ox + 20, oy + 20);
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(first), 0, 0);
   tk_pointer_warp(s.tk, ox + 200, oy + 150);
   tk_settle(s.tk);
   _expect_at(&s, first, ox + 200, oy + 150, "the first window");

   wl_surface_attach(tk_toplevel_surface(first), NULL, 0, 0);
   wl_surface_commit(tk_toplevel_surface(first));
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(second), 0, 0);
   tk_pointer_warp(s.tk, ox + 100, oy + 60);
   tk_settle(s.tk);
   _expect_at(&s, second, ox + 100, oy + 60, "attached right after an unmap");
   _expect_at(&s, first, ox + 200, oy + 150, "the first window, unmapped");

   wl_surface_attach(tk_toplevel_surface(second), NULL, 0, 0);
   wl_surface_commit(tk_toplevel_surface(second));
   tk_settle(s.tk);
   tk_pointer_warp(s.tk, ox + 150, oy + 100);
   tk_settle(s.tk);
   _expect_at(&s, second, ox + 100, oy + 60, "the second window, unmapped");
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(third), 0, 0);
   tk_pointer_warp(s.tk, ox + 120, oy + 80);
   tk_settle(s.tk);
   _expect_at(&s, third, ox + 120, oy + 80, "attached after a move");

   tk_pointer_button(s.tk, BTN_LEFT, 0);
   tk_settle(s.tk);
   tk_disconnect(s.tk);
}

/* The window or the source going away mid-drag leaves nothing dangling. */
static void
_test_gone_mid_drag(void)
{
   struct xdg_toplevel_drag_v1 *drag;
   struct wl_data_source *src;
   Tk_Toplevel *dragged, *origin;
   Session s;
   int ox, oy;

   _session_new(&s);
   dragged = tk_toplevel_new(s.tk, "e.test.drag.dragged", "dragged", 100, 80);
   origin = tk_toplevel_new(s.tk, "e.test.drag.origin", "origin", 400, 300);
   tk_surface_geometry(s.tk, tk_toplevel_surface(origin), &ox, &oy, NULL, NULL);

   src = _source_new(&s);
   drag = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(s.manager, src);
   _drag_start(&s, src, origin, ox + 20, oy + 20);
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(dragged), 0, 0);
   tk_settle(s.tk);
   tk_toplevel_del(dragged);
   tk_pointer_warp(s.tk, ox + 200, oy + 150);
   tk_settle(s.tk);
   wl_data_source_destroy(src);
   tk_pointer_warp(s.tk, ox + 100, oy + 60);
   tk_settle(s.tk);
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(origin), 0, 0);
   xdg_toplevel_drag_v1_destroy(drag);
   tk_pointer_button(s.tk, BTN_LEFT, 0);
   tk_settle(s.tk);
   tk_expect(s.tk, "e.test.drag.origin");
   tk_disconnect(s.tk);
}

static void
_drag_start_in(Session *s, struct wl_data_source *src, Tk_Toplevel *origin)
{
   int ox, oy;

   tk_surface_geometry(s->tk, tk_toplevel_surface(origin), &ox, &oy, NULL, NULL);
   _drag_start(s, src, origin, ox + 20, oy + 20);
}

static void
_test_errors(void)
{
   struct xdg_toplevel_drag_v1 *drag;
   struct wl_data_source *src;
   Tk_Toplevel *origin, *a, *b;
   Session s;

   _session_new(&s);
   origin = tk_toplevel_new(s.tk, "e.test.drag.origin", "origin", 400, 300);
   src = _source_new(&s);
   drag = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(s.manager, src);
   _drag_start_in(&s, src, origin);
   /* The proxy is gone once destroy is sent, so the error names no
    * interface. */
   xdg_toplevel_drag_v1_destroy(drag);
   _expect_error(&s, NULL, XDG_TOPLEVEL_DRAG_V1_ERROR_ONGOING_DRAG,
                 "destroying during the drag");

   _session_new(&s);
   src = wl_data_device_manager_create_data_source(s.ddm);
   wl_data_source_offer(src, "text/plain");
   xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(s.manager, src);
   wl_data_device_set_selection(s.device, src, 0);
   _expect_error(&s, &wl_data_source_interface,
                 WL_DATA_SOURCE_ERROR_INVALID_SOURCE, "a toplevel drag source as selection");

   _session_new(&s);
   origin = tk_toplevel_new(s.tk, "e.test.drag.origin", "origin", 400, 300);
   src = _source_new(&s);
   _drag_start_in(&s, src, origin);
   xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(s.manager, src);
   _expect_error(&s, &xdg_toplevel_drag_manager_v1_interface,
                 XDG_TOPLEVEL_DRAG_MANAGER_V1_ERROR_INVALID_SOURCE, "a toplevel drag after start_drag");

   _session_new(&s);
   a = tk_toplevel_new(s.tk, "e.test.drag.a", "a", 100, 80);
   b = tk_toplevel_new(s.tk, "e.test.drag.b", "b", 100, 80);
   drag = xdg_toplevel_drag_manager_v1_get_xdg_toplevel_drag(s.manager, _source_new(&s));
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(a), 0, 0);
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(a), 5, 5);
   tk_sync(s.tk);
   xdg_toplevel_drag_v1_attach(drag, tk_toplevel_xdg(b), 0, 0);
   _expect_error(&s, &xdg_toplevel_drag_v1_interface,
                 XDG_TOPLEVEL_DRAG_V1_ERROR_TOPLEVEL_ATTACHED, "attaching a second mapped toplevel");
}

int
main(void)
{
   _test_carry();
   _test_window_geometry();
   _test_unmap();
   _test_gone_mid_drag();
   _test_errors();
   printf(PROG ": ok\n");
   return 0;
}
