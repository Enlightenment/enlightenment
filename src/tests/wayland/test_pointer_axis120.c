/* wl_pointer v8 replaced axis_discrete with axis_value120: the old event
 * is not sent to a v8 client, and the new one not to an older one. The
 * test binds the seat at 8 and at 5 from one connection and checks what
 * each sees for one wheel click. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include "wl-test-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define W 200
#define H 150
#define SX 120
#define SY 90

/* One wheel click, and what wayland.xml says it weighs. */
#define CLICKS 1
#define VALUE120_PER_CLICK 120

#define FAIL(fmt, ...) \
  do { fprintf(stderr, "test-pointer-axis120: " fmt "\n", ##__VA_ARGS__); return 1; } while (0)

/* What one wl_pointer saw. Both pointers are fed by the same wheel event, so
 * everything interesting is the difference between the two of these. */
typedef struct
{
   const char *name;
   int axis;
   int axis_source;
   int axis_discrete;
   int axis_value120;
   int frames;
   int32_t discrete_value;
   int32_t value120_value;
   uint32_t source_value;
   /* Did the step event arrive before the wl_pointer.axis it describes?
    * wayland.xml requires the pairing within one frame, and a client that
    * applies the axis on sight would miss a step that came after it. */
   int step_after_axis;
} Seen;

static Seen v8 = { "v8", 0, 0, 0, 0, 0, 0, 0, 0xffffffff, 0 };
static Seen v5 = { "v5", 0, 0, 0, 0, 0, 0, 0, 0xffffffff, 0 };

static void
_ptr_enter(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *sf, wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)p; (void)s; (void)sf; (void)x; (void)y; }
static void
_ptr_leave(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *sf)
{ (void)d; (void)p; (void)s; (void)sf; }
static void
_ptr_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)p; (void)t; (void)x; (void)y; }
static void
_ptr_button(void *d, struct wl_pointer *p, uint32_t s, uint32_t t, uint32_t b, uint32_t st)
{ (void)d; (void)p; (void)s; (void)t; (void)b; (void)st; }

static void
_ptr_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v)
{
   Seen *s = d;

   (void)p; (void)t; (void)a; (void)v;
   s->axis++;
}

static void
_ptr_frame(void *d, struct wl_pointer *p)
{
   Seen *s = d;

   (void)p;
   s->frames++;
}

static void
_ptr_axis_source(void *d, struct wl_pointer *p, uint32_t src)
{
   Seen *s = d;

   (void)p;
   s->axis_source++;
   s->source_value = src;
}

static void
_ptr_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a)
{ (void)d; (void)p; (void)t; (void)a; }

static void
_ptr_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{
   Seen *s = d;

   (void)p; (void)a;
   s->axis_discrete++;
   s->discrete_value = v;
   if (s->axis) s->step_after_axis = 1;
}

static void
_ptr_axis_value120(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{
   Seen *s = d;

   (void)p; (void)a;
   s->axis_value120++;
   s->value120_value = v;
   if (s->axis) s->step_after_axis = 1;
}

static const struct wl_pointer_listener _ptr_listener =
{
   _ptr_enter, _ptr_leave, _ptr_motion, _ptr_button, _ptr_axis,
   _ptr_frame, _ptr_axis_source, _ptr_axis_stop, _ptr_axis_discrete,
   _ptr_axis_value120
};

static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct xdg_wm_base *wm_base;
static struct wl_test *tester;
static struct wl_surface *surface;
static uint32_t seat_name, seat_advertised_version;
static struct wl_registry *reg;
static int sync_done, configured;

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

static void _sync_done(void *d, struct wl_test *t) { (void)d; (void)t; sync_done = 1; }
static void _client_serial(void *d, struct wl_test *t, uint32_t s) { (void)d; (void)t; (void)s; }
static void _surface_info(void *d, struct wl_test *t, struct wl_surface *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t visible, uint32_t focused)
{ (void)d; (void)t; (void)s; (void)x; (void)y; (void)w; (void)h; (void)visible; (void)focused; }
static void _surface_unknown(void *d, struct wl_test *t, struct wl_surface *s)
{ (void)d; (void)t; (void)s; }
static const struct wl_test_listener _tester_listener =
{ _client_serial, _surface_info, _surface_unknown, _sync_done };

static void
_global(void *data, struct wl_registry *r, uint32_t id, const char *iface, uint32_t ver)
{
   (void)data; (void)r;
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
        /* Remembered rather than bound here: this test needs the same global
         * bound twice, which is easiest once the registry has settled. */
        seat_name = id;
        seat_advertised_version = ver;
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

   fd = shm_open("/e-test-axis120-shm", O_RDWR | O_CREAT | O_EXCL, 0600);
   if (fd < 0) return NULL;
   shm_unlink("/e-test-axis120-shm");
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

static struct wl_pointer *
pointer_at_version(uint32_t version, Seen *seen)
{
   struct wl_seat *s;
   struct wl_pointer *p;

   s = wl_registry_bind(reg, seat_name, &wl_seat_interface, version);
   if (!s) return NULL;
   p = wl_seat_get_pointer(s);
   if (!p) return NULL;
   wl_pointer_add_listener(p, &_ptr_listener, seen);
   return p;
}

int
main(void)
{
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
   if (!seat_name)
     FAIL("no wl_seat advertised at all");
   if (seat_advertised_version < 8)
     FAIL("wl_seat is advertised at version %u; axis_value120 needs 8. Both "
          "browsers read the wheel through value120, and at %u they get "
          "axis_discrete instead -- which they do not read",
          seat_advertised_version, seat_advertised_version);
   wl_test_add_listener(tester, &_tester_listener, NULL);

   if (!pointer_at_version(8, &v8)) FAIL("could not get a v8 pointer");
   if (!pointer_at_version(5, &v5)) FAIL("could not get a v5 pointer");

   surface = wl_compositor_create_surface(compositor);
   xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
   xdg_surface_add_listener(xdg_surface, &_xdg_listener, NULL);
   toplevel = xdg_surface_get_toplevel(xdg_surface);
   xdg_toplevel_add_listener(toplevel, &_top_listener, NULL);
   xdg_toplevel_set_title(toplevel, "pointer-axis120");
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

   /* The wheel only goes to a surface the pointer is inside. */
   for (i = 0; i <= 4; i++)
     {
        wl_test_pointer_warp(tester, SX + (W / 2) + i, SY + (H / 2) + i);
        if (tester_sync() < 0) FAIL("sync failed during warp");
     }
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after warp failed");

   /* One click down. Both pointers are on the same surface, so both are fed
    * from the one event and any difference between them is the version gate
    * doing its job. */
   wl_test_pointer_axis(tester, 0 /* vertical */, CLICKS);
   if (tester_sync() < 0) FAIL("sync failed after axis");
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after axis failed");

   if (!v8.axis) FAIL("v8 pointer got no wl_pointer.axis at all");
   if (!v5.axis) FAIL("v5 pointer got no wl_pointer.axis at all");
   if (!v8.frames) FAIL("v8 pointer got an axis with no frame to close it");
   if (!v5.frames) FAIL("v5 pointer got an axis with no frame to close it");

   /* The v8 half. */
   if (!v8.axis_value120)
     FAIL("v8 pointer got no axis_value120 for a wheel click. This is the "
          "event Firefox and Chromium read the wheel through");
   if (v8.value120_value != CLICKS * VALUE120_PER_CLICK)
     FAIL("v8 pointer: %d wheel click(s) reported as value120 %d, expected %d",
          CLICKS, v8.value120_value, CLICKS * VALUE120_PER_CLICK);
   if (v8.axis_discrete)
     FAIL("v8 pointer got axis_discrete as well as axis_value120. wayland.xml "
          "says the deprecated event is not sent to a client on 8 or later, "
          "and a client that honours both counts the detent twice");
   if (!v8.axis_source)
     FAIL("v8 pointer got no axis_source, so it cannot tell a wheel from a "
          "touchpad");
   if (v8.source_value != WL_POINTER_AXIS_SOURCE_WHEEL)
     FAIL("v8 pointer: axis_source is %u, expected wheel (%u)",
          v8.source_value, WL_POINTER_AXIS_SOURCE_WHEEL);
   if (v8.step_after_axis)
     FAIL("v8 pointer: axis_value120 arrived after the wl_pointer.axis it "
          "describes");

   /* The v5 half, which must not have changed. */
   if (!v5.axis_discrete)
     FAIL("v5 pointer lost axis_discrete. Raising the seat version must not "
          "take the old event away from clients that only understand it");
   if (v5.discrete_value != CLICKS)
     FAIL("v5 pointer: %d wheel click(s) reported as %d discrete steps",
          CLICKS, v5.discrete_value);
   if (v5.axis_value120)
     FAIL("v5 pointer got axis_value120, which did not exist at version 5");

   printf("test-pointer-axis120: v8 sees value120 %d and no discrete; "
          "v5 sees discrete %d and no value120\n",
          v8.value120_value, v5.discrete_value);

   /* Direction has to survive the multiplication by 120 with its sign. */
   v8.axis_value120 = v8.value120_value = 0;
   v5.axis_discrete = v5.discrete_value = 0;
   wl_test_pointer_axis(tester, 0 /* vertical */, -2);
   if (tester_sync() < 0) FAIL("sync failed after reverse axis");
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after reverse axis failed");

   if (v8.value120_value != -2 * VALUE120_PER_CLICK)
     FAIL("v8 pointer: two clicks back reported as value120 %d, expected %d",
          v8.value120_value, -2 * VALUE120_PER_CLICK);
   if (v5.discrete_value != -2)
     FAIL("v5 pointer: two clicks back reported as %d discrete steps",
          v5.discrete_value);
   printf("test-pointer-axis120: scrolling back keeps its sign\n");

   printf("test-pointer-axis120: ok\n");
   return 0;
}
