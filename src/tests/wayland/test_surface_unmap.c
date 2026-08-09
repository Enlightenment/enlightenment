/* A surface stops taking input as soon as it is unmapped by committing a
 * null buffer.
 *
 * The failure reports what the compositor believes about the surface. A
 * surface still reported visible does not mean the unmap was skipped: E's
 * hide runs a fade-out effect, and the object stays on the canvas, taking
 * input, until it ends. */
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

#define FAIL(fmt, ...) \
  do { fprintf(stderr, "test-surface-unmap: " fmt "\n", ##__VA_ARGS__); return 1; } while (0)

static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct xdg_wm_base *wm_base;
static struct wl_seat *seat;
static struct wl_pointer *pointer;
static struct wl_test *tester;

static struct wl_surface *surface;
static int enter_count, leave_count;
static int configured, sync_done;
static int32_t info_x, info_y, info_w, info_h;
static uint32_t info_visible;
static int info_valid;

static void
_ptr_enter(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s,
           wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)p; (void)serial; (void)s; (void)x; (void)y; enter_count++; }

static void
_ptr_leave(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s)
{ (void)d; (void)p; (void)serial; (void)s; leave_count++; }

static void _ptr_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y)
{ (void)d; (void)p; (void)t; (void)x; (void)y; }
static void _ptr_button(void *d, struct wl_pointer *p, uint32_t se, uint32_t t, uint32_t b, uint32_t st)
{ (void)d; (void)p; (void)se; (void)t; (void)b; (void)st; }
static void _ptr_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v)
{ (void)d; (void)p; (void)t; (void)a; (void)v; }
static void _ptr_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void _ptr_axis_source(void *d, struct wl_pointer *p, uint32_t s) { (void)d; (void)p; (void)s; }
static void _ptr_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a)
{ (void)d; (void)p; (void)t; (void)a; }
static void _ptr_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{ (void)d; (void)p; (void)a; (void)v; }

static const struct wl_pointer_listener _ptr_listener =
{
   _ptr_enter, _ptr_leave, _ptr_motion, _ptr_button, _ptr_axis,
   _ptr_frame, _ptr_axis_source, _ptr_axis_stop, _ptr_axis_discrete
};

static void
_surface_info(void *d, struct wl_test *t, struct wl_surface *s,
              int32_t x, int32_t y, int32_t w, int32_t h, uint32_t vis, uint32_t foc)
{
   (void)d; (void)t; (void)s; (void)foc;
   info_valid = 1; info_x = x; info_y = y; info_w = w; info_h = h;
   info_visible = vis;
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
   (void)data; (void)ver;
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
     seat = wl_registry_bind(reg, id, &wl_seat_interface, 5);
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

static int
surface_info(struct wl_surface *s)
{
   info_valid = 0;
   wl_test_get_surface_info(tester, s);
   if (wl_display_roundtrip(disp) < 0) return -1;
   return 0;
}

static struct wl_buffer *
make_buffer(void)
{
   int fd, stride = W * 4, size = stride * H;
   void *map;
   struct wl_shm_pool *pool;
   struct wl_buffer *buf;

   fd = shm_open("/e-test-unmap-shm", O_RDWR | O_CREAT | O_EXCL, 0600);
   if (fd < 0) return NULL;
   shm_unlink("/e-test-unmap-shm");
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

/* Walk the pointer onto the middle of the surface in small steps, the way
 * test_pointer_enter.c does: a compositor that only re-evaluates on movement
 * gets every chance to notice. */
static int
warp_onto_surface(void)
{
   int i;

   for (i = 0; i <= 4; i++)
     {
        wl_test_pointer_warp(tester, SX + (W / 2) + i, SY + (H / 2) + i);
        if (tester_sync() < 0) return -1;
     }
   return wl_display_roundtrip(disp) < 0 ? -1 : 0;
}

static int
warp_away(void)
{
   wl_test_pointer_warp(tester, 5, 5);
   if (tester_sync() < 0) return -1;
   return wl_display_roundtrip(disp) < 0 ? -1 : 0;
}

int
main(void)
{
   struct wl_registry *reg;
   struct xdg_surface *xdg_surface;
   struct xdg_toplevel *toplevel;
   struct wl_buffer *buffer;

   disp = wl_display_connect(NULL);
   if (!disp) FAIL("cannot connect to WAYLAND_DISPLAY=%s", getenv("WAYLAND_DISPLAY") ?: "(unset)");

   reg = wl_display_get_registry(disp);
   wl_registry_add_listener(reg, &_reg_listener, NULL);
   if (wl_display_roundtrip(disp) < 0) FAIL("registry roundtrip failed");

   if (!compositor || !shm || !wm_base) FAIL("missing core globals");
   if (!seat) FAIL("no wl_seat at version 5");
   if (!tester) FAIL("no wl_test -- built without -Dtests=true?");
   wl_test_add_listener(tester, &_tester_listener, NULL);

   pointer = wl_seat_get_pointer(seat);
   if (!pointer) FAIL("seat has no pointer");
   wl_pointer_add_listener(pointer, &_ptr_listener, NULL);

   surface = wl_compositor_create_surface(compositor);
   xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
   xdg_surface_add_listener(xdg_surface, &_xdg_listener, NULL);
   toplevel = xdg_surface_get_toplevel(xdg_surface);
   xdg_toplevel_add_listener(toplevel, &_top_listener, NULL);
   xdg_toplevel_set_title(toplevel, "surface-unmap");
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

   /* Sanity: the surface takes input while it is mapped. Without this a
    * compositor that delivers no input at all would pass the real check. */
   if (warp_onto_surface() < 0) FAIL("warp failed");
   if (!enter_count)
     FAIL("no enter while the surface is still mapped -- see test-pointer-enter");
   if (warp_away() < 0) FAIL("warp away failed");
   if (!leave_count) FAIL("no leave when the pointer left the mapped surface");

   /* Unmap: a null buffer and a commit, which is the only way a client has. */
   wl_surface_attach(surface, NULL, 0, 0);
   wl_surface_commit(surface);
   if (tester_sync() < 0) FAIL("sync failed after the null attach");

   if (surface_info(surface) < 0) FAIL("surface_info roundtrip failed");

   enter_count = 0;
   if (warp_onto_surface() < 0) FAIL("warp onto the unmapped surface failed");

   if (enter_count)
     {
        if (info_valid && info_visible)
          FAIL("the surface still takes pointer input after a null buffer was "
               "committed, and is still on the canvas at (%d,%d %dx%d). Most "
               "likely the unmap ran and E's hide animation is keeping the "
               "object alive - see _e_comp_intercept_hide() - so the frame "
               "needs its events frozen, not just hiding",
               info_x, info_y, info_w, info_h);
        FAIL("the surface still takes pointer input after a null buffer was "
             "committed, and is off the canvas%s: something is delivering "
             "input to a hidden object",
             info_valid ? "" : " (E has no E_Client for it at all)");
     }

   printf("test-surface-unmap: ok (unmapped surface takes no input; "
          "compositor reports visible=%u)\n", info_valid ? info_visible : 0);
   return 0;
}
