/* Every group of pointer and touch events ends with a frame.
 *
 * wayland.xml lets a client buffer events until the frame, and wlcs, GTK
 * and Chromium do, so a group left open is never applied. The test records
 * events in order and checks that each group is closed. It binds wl_seat
 * at 5, the first version with wl_pointer.frame. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include "wl-test-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define W 200
#define H 150
#define SX 120
#define SY 90

#define FAIL(fmt, ...) \
  do { fprintf(stderr, "test-pointer-frame: " fmt "\n", ##__VA_ARGS__); return 1; } while (0)

/* One slot per event we care about, in arrival order. Small fixed buffer:
 * a test that overruns it has gone wrong in a way a bigger buffer would only
 * hide. */
typedef enum
{
   EV_ENTER, EV_LEAVE, EV_MOTION, EV_BUTTON, EV_AXIS,
   EV_AXIS_SOURCE, EV_AXIS_DISCRETE, EV_FRAME,
   EV_TOUCH_DOWN, EV_TOUCH_UP, EV_TOUCH_MOTION, EV_TOUCH_FRAME
} Ev;

static const char *_ev_name[] =
{
   "enter", "leave", "motion", "button", "axis",
   "axis_source", "axis_discrete", "frame",
   "touch.down", "touch.up", "touch.motion", "touch.frame"
};

#define MAX_EV 256
static Ev _log[MAX_EV];
static int _log_n;

static void
_rec(Ev e)
{
   if (_log_n < MAX_EV) _log[_log_n++] = e;
}

static void
_log_reset(void)
{
   _log_n = 0;
}

static void
_log_dump(void)
{
   int i;

   fprintf(stderr, "  events seen:");
   if (!_log_n) fprintf(stderr, " (none)");
   for (i = 0; i < _log_n; i++) fprintf(stderr, " %s", _ev_name[_log[i]]);
   fprintf(stderr, "\n");
}

static int
_seen(Ev e)
{
   int i;

   for (i = 0; i < _log_n; i++) if (_log[i] == e) return 1;
   return 0;
}

/* Is every event of kind `open` closed by a later `closer`, with no `open`
 * left dangling at the end? That is the whole contract. */
static int
_closed_by(Ev open, Ev closer)
{
   int i, pending = 0;

   for (i = 0; i < _log_n; i++)
     {
        if (_log[i] == open) pending = 1;
        else if (_log[i] == closer) pending = 0;
     }
   return !pending;
}

/* Index of the first occurrence, or -1. */
static int
_first(Ev e)
{
   int i;

   for (i = 0; i < _log_n; i++) if (_log[i] == e) return i;
   return -1;
}

static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct xdg_wm_base *wm_base;
static struct wl_seat *seat;
static struct wl_pointer *pointer;
static struct wl_touch *touch;
static struct wl_test *tester;
static uint32_t seat_advertised_version;

static struct wl_surface *surface;
static int configured, sync_done;
static int32_t info_x, info_y;
static int info_valid;
static int32_t axis_discrete_value;
static uint32_t axis_source_value;

static void
_ptr_enter(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s,
           wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)p; (void)serial; (void)s; (void)x; (void)y; _rec(EV_ENTER); }

static void
_ptr_leave(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s)
{ (void)d; (void)p; (void)serial; (void)s; _rec(EV_LEAVE); }

static void
_ptr_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)p; (void)t; (void)x; (void)y; _rec(EV_MOTION); }

static void
_ptr_button(void *d, struct wl_pointer *p, uint32_t se, uint32_t t, uint32_t b, uint32_t st)
{ (void)d; (void)p; (void)se; (void)t; (void)b; (void)st; _rec(EV_BUTTON); }

static void
_ptr_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v)
{ (void)d; (void)p; (void)t; (void)a; (void)v; _rec(EV_AXIS); }

static void
_ptr_frame(void *d, struct wl_pointer *p)
{ (void)d; (void)p; _rec(EV_FRAME); }

static void
_ptr_axis_source(void *d, struct wl_pointer *p, uint32_t s)
{ (void)d; (void)p; axis_source_value = s; _rec(EV_AXIS_SOURCE); }

static void
_ptr_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a)
{ (void)d; (void)p; (void)t; (void)a; }

static void
_ptr_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{ (void)d; (void)p; (void)a; axis_discrete_value = v; _rec(EV_AXIS_DISCRETE); }

static const struct wl_pointer_listener _ptr_listener =
{
   _ptr_enter, _ptr_leave, _ptr_motion, _ptr_button, _ptr_axis,
   _ptr_frame, _ptr_axis_source, _ptr_axis_stop, _ptr_axis_discrete
};

static void
_touch_down(void *d, struct wl_touch *t, uint32_t se, uint32_t ti,
            struct wl_surface *s, int32_t id, wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)t; (void)se; (void)ti; (void)s; (void)id; (void)x; (void)y; _rec(EV_TOUCH_DOWN); }

static void
_touch_up(void *d, struct wl_touch *t, uint32_t se, uint32_t ti, int32_t id)
{ (void)d; (void)t; (void)se; (void)ti; (void)id; _rec(EV_TOUCH_UP); }

static void
_touch_motion(void *d, struct wl_touch *t, uint32_t ti, int32_t id, wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)t; (void)ti; (void)id; (void)x; (void)y; _rec(EV_TOUCH_MOTION); }

static void
_touch_frame(void *d, struct wl_touch *t)
{ (void)d; (void)t; _rec(EV_TOUCH_FRAME); }

static void
_touch_cancel(void *d, struct wl_touch *t)
{ (void)d; (void)t; }

static void
_touch_shape(void *d, struct wl_touch *t, int32_t id, wl_fixed_t maj, wl_fixed_t min)
{ (void)d; (void)t; (void)id; (void)maj; (void)min; }

static void
_touch_orientation(void *d, struct wl_touch *t, int32_t id, wl_fixed_t o)
{ (void)d; (void)t; (void)id; (void)o; }

static const struct wl_touch_listener _touch_listener =
{
   _touch_down, _touch_up, _touch_motion, _touch_frame, _touch_cancel,
   _touch_shape, _touch_orientation
};

static void
_surface_info(void *d, struct wl_test *t, struct wl_surface *s,
              int32_t x, int32_t y, int32_t w, int32_t h, uint32_t vis, uint32_t foc)
{
   (void)d; (void)t; (void)s; (void)w; (void)h; (void)vis; (void)foc;
   info_valid = 1; info_x = x; info_y = y;
}
static void _surface_unknown(void *d, struct wl_test *t, struct wl_surface *s)
{ (void)d; (void)t; (void)s; info_valid = 0; }
static void _client_serial(void *d, struct wl_test *t, uint32_t s) { (void)d; (void)t; (void)s; }
static void _sync_done(void *d, struct wl_test *t) { (void)d; (void)t; sync_done = 1; }

static const struct wl_test_listener _tester_listener =
{ _client_serial, _surface_info, _surface_unknown, _sync_done };

static void _wm_ping(void *d, struct xdg_wm_base *b, uint32_t serial)
{ (void)d; xdg_wm_base_pong(b, serial); }
static const struct xdg_wm_base_listener _wm_listener = { _wm_ping };

static void _xdg_conf(void *d, struct xdg_surface *s, uint32_t serial)
{ (void)d; xdg_surface_ack_configure(s, serial); configured = 1; }
static const struct xdg_surface_listener _xdg_listener = { _xdg_conf };

static void _top_conf(void *d, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *st)
{ (void)d; (void)t; (void)w; (void)h; (void)st; }
static void _top_close(void *d, struct xdg_toplevel *t) { (void)d; (void)t; }
static const struct xdg_toplevel_listener _top_listener = { _top_conf, _top_close };

static void
_global(void *data, struct wl_registry *reg, uint32_t id, const char *iface, uint32_t ver)
{
   (void)data;
   if (!strcmp(iface, "wl_compositor"))
     compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 4);
   else if (!strcmp(iface, "wl_shm"))
     shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
   else if (!strcmp(iface, "xdg_wm_base"))
     {
        wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wm_base, &_wm_listener, NULL);
     }
   else if (!strcmp(iface, "wl_seat"))
     {
        /* Bind at 5 exactly rather than at whatever is offered: this test is
         * about what a v5 client sees, and binding higher later would change
         * the answer without changing the test. */
        seat_advertised_version = ver;
        if (ver >= 5)
          seat = wl_registry_bind(reg, id, &wl_seat_interface, 5);
     }
   else if (!strcmp(iface, "wl_test"))
     tester = wl_registry_bind(reg, id, &wl_test_interface, 1);
}

static void _global_rm(void *d, struct wl_registry *r, uint32_t id) { (void)d; (void)r; (void)id; }
static const struct wl_registry_listener _reg_listener = { _global, _global_rm };

static struct wl_display *disp;

static int
tester_sync(void)
{
   sync_done = 0;
   wl_test_sync(tester);
   while (!sync_done)
     if (wl_display_dispatch(disp) < 0) return -1;
   return 0;
}

static struct wl_buffer *
make_buffer(void)
{
   int fd, stride = W * 4, size = stride * H;
   void *map;
   struct wl_shm_pool *pool;
   struct wl_buffer *buf;

   fd = shm_open("/e-test-frame-shm", O_RDWR | O_CREAT | O_EXCL, 0600);
   if (fd < 0) return NULL;
   shm_unlink("/e-test-frame-shm");
   if (ftruncate(fd, size) < 0) { close(fd); return NULL; }
   map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (map == MAP_FAILED) { close(fd); return NULL; }
   memset(map, 0xff, size);
   pool = wl_shm_create_pool(shm, fd, size);
   buf = wl_shm_pool_create_buffer(pool, 0, W, H, stride, WL_SHM_FORMAT_ARGB8888);
   wl_shm_pool_destroy(pool);
   close(fd);
   return buf;
}

int
main(void)
{
   struct wl_registry *reg;
   struct xdg_surface *xdg_surface;
   struct xdg_toplevel *toplevel;
   struct wl_buffer *buffer;
   int i;

   disp = wl_display_connect(NULL);
   if (!disp) FAIL("cannot connect to WAYLAND_DISPLAY=%s", getenv("WAYLAND_DISPLAY") ?: "(unset)");

   reg = wl_display_get_registry(disp);
   wl_registry_add_listener(reg, &_reg_listener, NULL);
   if (wl_display_roundtrip(disp) < 0) FAIL("registry roundtrip failed");

   if (!compositor || !shm || !wm_base) FAIL("missing core globals");
   if (!tester) FAIL("no wl_test -- built without -Dtests=true?");
   if (!seat)
     FAIL("wl_seat is advertised at version %u; wl_pointer.frame needs 5. "
          "Every v5 client buffers pointer input until the frame arrives, so "
          "at 4 they get nothing at all",
          seat_advertised_version);
   wl_test_add_listener(tester, &_tester_listener, NULL);

   pointer = wl_seat_get_pointer(seat);
   if (!pointer) FAIL("seat has no pointer");
   wl_pointer_add_listener(pointer, &_ptr_listener, NULL);

   touch = wl_seat_get_touch(seat);
   if (!touch) FAIL("seat has no touch");
   wl_touch_add_listener(touch, &_touch_listener, NULL);

   surface = wl_compositor_create_surface(compositor);
   xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
   xdg_surface_add_listener(xdg_surface, &_xdg_listener, NULL);
   toplevel = xdg_surface_get_toplevel(xdg_surface);
   xdg_toplevel_add_listener(toplevel, &_top_listener, NULL);
   xdg_toplevel_set_title(toplevel, "pointer-frame");
   wl_surface_commit(surface);
   while (!configured)
     if (wl_display_dispatch(disp) < 0) FAIL("dispatch failed awaiting configure");

   buffer = make_buffer();
   if (!buffer) FAIL("could not create an shm buffer");
   wl_surface_attach(surface, buffer, 0, 0);
   wl_surface_damage(surface, 0, 0, W, H);
   wl_surface_commit(surface);
   if (tester_sync() < 0) FAIL("sync failed after mapping");

   wl_test_move_surface(tester, surface, SX, SY);
   if (tester_sync() < 0) FAIL("sync failed after move");
   wl_test_get_surface_info(tester, surface);
   if (wl_display_roundtrip(disp) < 0) FAIL("surface_info roundtrip failed");
   if (!info_valid) FAIL("compositor has no E_Client for the mapped surface");
   if ((info_x != SX) || (info_y != SY))
     FAIL("surface should be at (%d,%d), compositor says (%d,%d)",
          SX, SY, info_x, info_y);

   /* Pointer enter and motion. */
   _log_reset();
   for (i = 0; i <= 4; i++)
     {
        wl_test_pointer_warp(tester, SX + (W / 2) + i, SY + (H / 2) + i);
        if (tester_sync() < 0) FAIL("sync failed during warp");
     }
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after warp failed");

   if (!_seen(EV_ENTER)) { _log_dump(); FAIL("no wl_pointer.enter -- see test-pointer-enter first"); }
   if (!_seen(EV_FRAME))
     {
        _log_dump();
        FAIL("wl_pointer.enter arrived but no wl_pointer.frame ever did. A v5 "
             "client holds the enter in a pending slot and commits it in the "
             "frame handler, so as far as it is concerned the pointer never "
             "arrived");
     }
   if (!_closed_by(EV_ENTER, EV_FRAME))
     { _log_dump(); FAIL("an enter was left unterminated by a frame"); }
   if (!_closed_by(EV_MOTION, EV_FRAME))
     { _log_dump(); FAIL("a motion was left unterminated by a frame"); }
   printf("test-pointer-frame: enter and motion are framed\n");

   /* Buttons. */
   _log_reset();
   wl_test_pointer_button(tester, BTN_LEFT, 1);
   if (tester_sync() < 0) FAIL("sync failed after button down");
   wl_test_pointer_button(tester, BTN_LEFT, 0);
   if (tester_sync() < 0) FAIL("sync failed after button up");
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after button failed");

   if (!_seen(EV_BUTTON)) { _log_dump(); FAIL("no wl_pointer.button delivered"); }
   if (!_closed_by(EV_BUTTON, EV_FRAME))
     {
        _log_dump();
        FAIL("a button was left unterminated by a frame. This is what makes "
             "every interactive move and resize test time out: the client is "
             "waiting for a press it has already been sent");
     }
   printf("test-pointer-frame: buttons are framed\n");

   /* Scroll. A v5 client that gets a bare axis assumes a touchpad and scrolls
    * by the raw value, so the wheel has to identify itself. */
   _log_reset();
   axis_discrete_value = 0;
   axis_source_value = 0xffffffff;
   wl_test_pointer_axis(tester, 0 /* vertical */, 1);
   if (tester_sync() < 0) FAIL("sync failed after axis");
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after axis failed");

   if (!_seen(EV_AXIS)) { _log_dump(); FAIL("no wl_pointer.axis delivered"); }
   if (!_closed_by(EV_AXIS, EV_FRAME))
     { _log_dump(); FAIL("an axis was left unterminated by a frame"); }
   if (!_seen(EV_AXIS_SOURCE))
     { _log_dump(); FAIL("wl_pointer.axis with no axis_source: the client cannot tell a wheel from a touchpad"); }
   if (axis_source_value != WL_POINTER_AXIS_SOURCE_WHEEL)
     FAIL("axis_source is %u, expected wheel (%u)",
          axis_source_value, WL_POINTER_AXIS_SOURCE_WHEEL);
   if (!_seen(EV_AXIS_DISCRETE))
     { _log_dump(); FAIL("a wheel click with no axis_discrete"); }
   if (axis_discrete_value != 1)
     FAIL("one wheel click reported as %d discrete steps", axis_discrete_value);
   if (_first(EV_AXIS_SOURCE) > _first(EV_AXIS))
     { _log_dump(); FAIL("axis_source came after the axis it describes"); }
   if (_first(EV_AXIS_DISCRETE) > _first(EV_AXIS))
     { _log_dump(); FAIL("axis_discrete came after the axis it describes"); }
   printf("test-pointer-frame: axis is framed, sourced and discrete\n");

   /* Touch. wl_touch.frame is not version-gated -- it has been part of the
    * protocol since the beginning.
    *
    * Touch id 1, never 0: Evas reserves multi-touch device 0 for the mouse
    * pointer and drops MULTI_DOWN/MOVE/UP for it silently. Passing 0 here
    * produces no wl_touch events whatever the compositor does, which is
    * indistinguishable from a compositor bug. See src/protocol/wl-test.xml. */
   _log_reset();
   wl_test_touch_down(tester, 1, SX + (W / 2), SY + (H / 2));
   if (tester_sync() < 0) FAIL("sync failed after touch down");
   wl_test_touch_move(tester, 1, SX + (W / 2) + 10, SY + (H / 2) + 10);
   if (tester_sync() < 0) FAIL("sync failed after touch move");
   wl_test_touch_up(tester, 1);
   if (tester_sync() < 0) FAIL("sync failed after touch up");
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after touch failed");

   if (!_seen(EV_TOUCH_DOWN)) { _log_dump(); FAIL("no wl_touch.down delivered"); }
   if (!_closed_by(EV_TOUCH_DOWN, EV_TOUCH_FRAME))
     { _log_dump(); FAIL("a touch down was left unterminated by a wl_touch.frame"); }
   if (!_closed_by(EV_TOUCH_UP, EV_TOUCH_FRAME))
     { _log_dump(); FAIL("a touch up was left unterminated by a wl_touch.frame"); }
   if (_seen(EV_TOUCH_MOTION) && (!_closed_by(EV_TOUCH_MOTION, EV_TOUCH_FRAME)))
     { _log_dump(); FAIL("a touch motion was left unterminated by a wl_touch.frame"); }
   printf("test-pointer-frame: touch is framed\n");

   /* Leave. The pointer is moved right off the surface. */
   _log_reset();
   wl_test_pointer_warp(tester, 5, 5);
   if (tester_sync() < 0) FAIL("sync failed moving pointer away");
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after leaving failed");

   if (!_seen(EV_LEAVE)) { _log_dump(); FAIL("no wl_pointer.leave after moving off the surface"); }
   if (!_closed_by(EV_LEAVE, EV_FRAME))
     { _log_dump(); FAIL("a leave was left unterminated by a frame"); }
   printf("test-pointer-frame: leave is framed\n");

   /* Touch with the pointer elsewhere: the leave above parked it at 5,5. */
   _log_reset();
   wl_test_touch_down(tester, 1, SX + (W / 2), SY + (H / 2));
   if (tester_sync() < 0) FAIL("sync failed after off-pointer touch down");
   wl_test_touch_up(tester, 1);
   if (tester_sync() < 0) FAIL("sync failed after off-pointer touch up");
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after off-pointer touch failed");

   if (!_seen(EV_TOUCH_DOWN))
     {
        _log_dump();
        FAIL("no wl_touch.down for a touch at (%d,%d), inside the surface at "
             "(%d,%d) %dx%d, while the pointer sits at (5,5). Touch is not the "
             "mouse: this must be delivered",
             SX + W / 2, SY + H / 2, SX, SY, W, H);
     }
   if (!_closed_by(EV_TOUCH_DOWN, EV_TOUCH_FRAME))
     { _log_dump(); FAIL("off-pointer touch down not framed"); }
   if (!_seen(EV_TOUCH_UP))
     { _log_dump(); FAIL("no wl_touch.up for the off-pointer touch"); }
   printf("test-pointer-frame: touch works with the pointer elsewhere\n");

   return 0;
}
