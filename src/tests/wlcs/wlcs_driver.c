/* Drives the wlcs integration's hooks directly.
 *
 * This is not a substitute for running the conformance suite - it proves our
 * shim behaves, not that E is conformant. It is worth having anyway, because
 * it separates "our integration is correct" from "the suite is happy", a
 * failure here points at our code rather than at several hundred third-party
 * tests, and it runs where no usable wlcs runner is installed.
 *
 * What it checks, in order:
 *   - the integration exports wlcs_server_integration and can be dlopen'd,
 *     exactly as the runner loads it;
 *   - start() leaves the server able to hand out client sockets;
 *   - create_client_socket() gives a connectable fd;
 *   - every extension the descriptor declares is advertised, at least at the
 *     declared version;
 *   - position_window_absolute() actually moves a window - verified against
 *     the compositor's own view, not merely by nothing crashing;
 *   - pointer and touch input land where the runner asked, in the window's
 *     own coordinates;
 *   - stop() and destroy_server() clean up.
 */
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/input-event-codes.h>

#include <wayland-client.h>
#include <wlcs/display_server.h>
#include <wlcs/pointer.h>
#include <wlcs/touch.h>

#include "wl-test-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define W 160
#define H 120
#define WIN_X 210
#define WIN_Y 140

#define FAIL(fmt, ...) \
  do \
    { \
       fprintf(stderr, "wlcs-driver: " fmt "\n", ##__VA_ARGS__); \
       return 1; \
    } \
  while (0)

#define UNUSED __attribute__((unused))

typedef struct
{
   char name[64];
   uint32_t version;
} Seen_Global;

static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct wl_seat *seat;
static struct xdg_wm_base *wm_base;
static struct wl_test *tester;

static Seen_Global seen[64];
static int seen_count;

static int configured;
static int sync_done;
static int info_valid;
static int32_t info_x, info_y;

static struct wl_surface *pointer_surface;
static int pointer_x, pointer_y, pointer_buttons;
static struct wl_surface *touch_surface;
static int touch_x, touch_y, touch_downs, touch_ups;

static void
_registry_global(void *data UNUSED, struct wl_registry *reg, uint32_t id,
                 const char *iface, uint32_t version)
{
   if (seen_count < (int)(sizeof(seen) / sizeof(seen[0])))
     {
        snprintf(seen[seen_count].name, sizeof(seen[0].name), "%s", iface);
        seen[seen_count].version = version;
        seen_count++;
     }

   if (!strcmp(iface, "wl_compositor"))
     compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 4);
   else if (!strcmp(iface, "wl_shm"))
     shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
   else if (!strcmp(iface, "wl_seat"))
     seat = wl_registry_bind(reg, id, &wl_seat_interface, 4);
   else if (!strcmp(iface, "xdg_wm_base"))
     wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, 1);
   else if (!strcmp(iface, "wl_test"))
     tester = wl_registry_bind(reg, id, &wl_test_interface, 1);
}

static void
_registry_global_remove(void *d UNUSED, struct wl_registry *r UNUSED,
                        uint32_t id UNUSED)
{
}

static const struct wl_registry_listener _registry_listener =
{
   _registry_global, _registry_global_remove
};

static void
_tester_client_serial(void *d UNUSED, struct wl_test *t UNUSED,
                      uint32_t serial UNUSED)
{
}

static void
_tester_surface_info(void *d UNUSED, struct wl_test *t UNUSED,
                     struct wl_surface *s UNUSED, int32_t x, int32_t y,
                     int32_t w UNUSED, int32_t h UNUSED,
                     uint32_t visible UNUSED, uint32_t focused UNUSED)
{
   info_valid = 1;
   info_x = x;
   info_y = y;
}

static void
_tester_surface_unknown(void *d UNUSED, struct wl_test *t UNUSED,
                        struct wl_surface *s UNUSED)
{
   info_valid = 0;
}

static void
_tester_sync_done(void *d UNUSED, struct wl_test *t UNUSED)
{
   sync_done = 1;
}

static const struct wl_test_listener _tester_listener =
{
   _tester_client_serial, _tester_surface_info, _tester_surface_unknown,
   _tester_sync_done
};

static void
_xdg_surface_configure(void *d UNUSED, struct xdg_surface *s, uint32_t serial)
{
   xdg_surface_ack_configure(s, serial);
   configured = 1;
}

static const struct xdg_surface_listener _xdg_surface_listener =
{
   _xdg_surface_configure
};

static void
_toplevel_configure(void *d UNUSED, struct xdg_toplevel *t UNUSED,
                    int32_t w UNUSED, int32_t h UNUSED,
                    struct wl_array *st UNUSED)
{
}

static void
_toplevel_close(void *d UNUSED, struct xdg_toplevel *t UNUSED)
{
}

static const struct xdg_toplevel_listener _toplevel_listener =
{
   _toplevel_configure, _toplevel_close
};

static void
_pointer_enter(void *d UNUSED, struct wl_pointer *p UNUSED,
               uint32_t serial UNUSED, struct wl_surface *s,
               wl_fixed_t x, wl_fixed_t y)
{
   pointer_surface = s;
   pointer_x = wl_fixed_to_int(x);
   pointer_y = wl_fixed_to_int(y);
}

static void
_pointer_leave(void *d UNUSED, struct wl_pointer *p UNUSED,
               uint32_t serial UNUSED, struct wl_surface *s UNUSED)
{
   pointer_surface = NULL;
}

static void
_pointer_motion(void *d UNUSED, struct wl_pointer *p UNUSED,
                uint32_t time UNUSED, wl_fixed_t x, wl_fixed_t y)
{
   pointer_x = wl_fixed_to_int(x);
   pointer_y = wl_fixed_to_int(y);
}

static void
_pointer_button(void *d UNUSED, struct wl_pointer *p UNUSED,
                uint32_t serial UNUSED, uint32_t time UNUSED,
                uint32_t button UNUSED, uint32_t state UNUSED)
{
   pointer_buttons++;
}

static void
_pointer_axis(void *d UNUSED, struct wl_pointer *p UNUSED, uint32_t time UNUSED,
              uint32_t axis UNUSED, wl_fixed_t value UNUSED)
{
}

static const struct wl_pointer_listener _pointer_listener =
{
   _pointer_enter, _pointer_leave, _pointer_motion, _pointer_button,
   _pointer_axis, NULL, NULL, NULL, NULL, NULL, NULL
};

static void
_touch_down(void *d UNUSED, struct wl_touch *t UNUSED, uint32_t serial UNUSED,
            uint32_t time UNUSED, struct wl_surface *s, int32_t id UNUSED,
            wl_fixed_t x, wl_fixed_t y)
{
   touch_surface = s;
   touch_x = wl_fixed_to_int(x);
   touch_y = wl_fixed_to_int(y);
   touch_downs++;
}

static void
_touch_up(void *d UNUSED, struct wl_touch *t UNUSED, uint32_t serial UNUSED,
          uint32_t time UNUSED, int32_t id UNUSED)
{
   touch_ups++;
}

static void
_touch_motion(void *d UNUSED, struct wl_touch *t UNUSED, uint32_t time UNUSED,
              int32_t id UNUSED, wl_fixed_t x, wl_fixed_t y)
{
   touch_x = wl_fixed_to_int(x);
   touch_y = wl_fixed_to_int(y);
}

static void
_touch_frame(void *d UNUSED, struct wl_touch *t UNUSED)
{
}

static void
_touch_cancel(void *d UNUSED, struct wl_touch *t UNUSED)
{
}

static const struct wl_touch_listener _touch_listener =
{
   _touch_down, _touch_up, _touch_motion, _touch_frame, _touch_cancel,
   NULL, NULL
};

static struct wl_buffer *
_buffer_new(void)
{
   int fd, stride = W * 4, size = stride * H;
   void *map;
   struct wl_shm_pool *pool;
   struct wl_buffer *buf;
   char name[] = "/e-wlcs-driver-XXXXXX";

   fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
   if (fd < 0) return NULL;
   shm_unlink(name);
   if (ftruncate(fd, size) < 0)
     {
        close(fd);
        return NULL;
     }
   map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (map == MAP_FAILED)
     {
        close(fd);
        return NULL;
     }
   memset(map, 0xff, size);
   munmap(map, size);

   pool = wl_shm_create_pool(shm, fd, size);
   buf = wl_shm_pool_create_buffer(pool, 0, W, H, stride, WL_SHM_FORMAT_ARGB8888);
   wl_shm_pool_destroy(pool);
   close(fd);
   return buf;
}

static int
_tester_sync(struct wl_display *disp)
{
   sync_done = 0;
   wl_test_sync(tester);
   while (!sync_done)
     {
        if (wl_display_dispatch(disp) < 0) return -1;
     }
   return 0;
}

static int
_check_descriptor(WlcsDisplayServer *server)
{
   const WlcsIntegrationDescriptor *desc;
   size_t i;

   desc = server->get_descriptor(server);
   if (!desc) FAIL("get_descriptor returned NULL");
   for (i = 0; i < desc->num_extensions; i++)
     {
        const WlcsExtensionDescriptor *e = &desc->supported_extensions[i];
        int found = 0, j;

        for (j = 0; j < seen_count; j++)
          {
             if (strcmp(seen[j].name, e->name)) continue;
             found = 1;
             if (seen[j].version < e->version)
               FAIL("descriptor claims %s v%u but the compositor advertises "
                    "v%u - wlcs would run tests that cannot pass",
                    e->name, e->version, seen[j].version);
             break;
          }
        if (!found)
          FAIL("descriptor claims %s but the compositor does not advertise it",
               e->name);
     }
   return 0;
}

static int
_check_input(WlcsDisplayServer *server, struct wl_display *disp,
             struct wl_surface *surface)
{
   struct wl_pointer *wl_pointer;
   struct wl_touch *wl_touch;
   WlcsPointer *pointer;
   WlcsTouch *touch;

   wl_pointer = wl_seat_get_pointer(seat);
   wl_pointer_add_listener(wl_pointer, &_pointer_listener, NULL);
   wl_touch = wl_seat_get_touch(seat);
   wl_touch_add_listener(wl_touch, &_touch_listener, NULL);
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip failed binding the seat");

   pointer = server->create_pointer(server);
   if (!pointer) FAIL("create_pointer returned NULL");
   pointer->move_absolute(pointer, wl_fixed_from_int(WIN_X + 20),
                          wl_fixed_from_int(WIN_Y + 10));
   pointer->move_relative(pointer, wl_fixed_from_int(5), wl_fixed_from_int(5));
   pointer->button_down(pointer, BTN_LEFT);
   pointer->button_up(pointer, BTN_LEFT);
   pointer->destroy(pointer);

   if (_tester_sync(disp) < 0) FAIL("sync failed after pointer input");
   if (pointer_surface != surface)
     FAIL("the pointer moved onto the window but it got no wl_pointer.enter");
   if ((pointer_x != 25) || (pointer_y != 15))
     FAIL("pointer is at %d,%d in the window, expected 25,15",
          pointer_x, pointer_y);
   if (pointer_buttons != 2)
     FAIL("a click gave %d wl_pointer.button events, expected 2",
          pointer_buttons);

   /* Plain pixels, the way the runner passes them despite the wl_fixed_t in
    * <wlcs/touch.h>. */
   touch = server->create_touch(server);
   if (!touch) FAIL("create_touch returned NULL");
   touch->touch_down(touch, WIN_X + 30, WIN_Y + 40);
   touch->touch_move(touch, WIN_X + 35, WIN_Y + 45);
   touch->touch_up(touch);
   touch->destroy(touch);

   if (_tester_sync(disp) < 0) FAIL("sync failed after touch input");
   if ((touch_downs != 1) || (touch_surface != surface))
     FAIL("a touch on the window gave %d wl_touch.down events for it",
          touch_downs);
   if ((touch_x != 35) || (touch_y != 45))
     FAIL("touch ended at %d,%d in the window, expected 35,45",
          touch_x, touch_y);
   if (touch_ups != 1)
     FAIL("lifting the touch gave %d wl_touch.up events, expected 1",
          touch_ups);

   return 0;
}

/* Everything between start() and stop(), so that main() stops the compositor
 * whatever happens here. */
static int
_run(WlcsDisplayServer *server)
{
   struct wl_display *disp;
   struct wl_registry *reg;
   struct wl_surface *surface;
   struct xdg_surface *xdg_surface;
   struct xdg_toplevel *toplevel;
   struct wl_buffer *buffer;
   int fd, rc;

   fd = server->create_client_socket(server);
   if (fd < 0) FAIL("create_client_socket failed - did the compositor start?");

   disp = wl_display_connect_to_fd(fd);
   if (!disp) FAIL("wl_display_connect_to_fd on the returned socket failed");

   reg = wl_display_get_registry(disp);
   wl_registry_add_listener(reg, &_registry_listener, NULL);
   if (wl_display_roundtrip(disp) < 0) FAIL("registry roundtrip failed");

   if (!compositor) FAIL("no wl_compositor over the wlcs socket");
   if (!shm) FAIL("no wl_shm");
   if (!seat) FAIL("no wl_seat");
   if (!wm_base) FAIL("no xdg_wm_base");
   if (!tester) FAIL("no wl_test - is the wl_test module built?");
   wl_test_add_listener(tester, &_tester_listener, NULL);

   if (_check_descriptor(server)) return 1;

   surface = wl_compositor_create_surface(compositor);
   xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
   xdg_surface_add_listener(xdg_surface, &_xdg_surface_listener, NULL);
   toplevel = xdg_surface_get_toplevel(xdg_surface);
   xdg_toplevel_add_listener(toplevel, &_toplevel_listener, NULL);
   xdg_toplevel_set_title(toplevel, "wlcs-driver");
   wl_surface_commit(surface);
   while (!configured)
     {
        if (wl_display_dispatch(disp) < 0) FAIL("dispatch failed awaiting configure");
     }

   buffer = _buffer_new();
   if (!buffer) FAIL("could not create an shm buffer");
   wl_surface_attach(surface, buffer, 0, 0);
   wl_surface_damage(surface, 0, 0, W, H);
   wl_surface_commit(surface);
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip after mapping failed");

   server->position_window_absolute(server, disp, surface, WIN_X, WIN_Y);

   if (_tester_sync(disp) < 0) FAIL("dispatch failed awaiting sync");
   info_valid = 0;
   wl_test_get_surface_info(tester, surface);
   if (wl_display_roundtrip(disp) < 0) FAIL("surface_info roundtrip failed");
   if (!info_valid) FAIL("compositor has no E_Client for the mapped surface");
   if ((info_x != WIN_X) || (info_y != WIN_Y))
     FAIL("position_window_absolute(%d,%d) but the compositor believes "
          "(%d,%d)", WIN_X, WIN_Y, info_x, info_y);

   rc = _check_input(server, disp, surface);
   wl_display_disconnect(disp);
   return rc;
}

int
main(void)
{
   const char *path = getenv("E_WLCS_INTEGRATION");
   WlcsServerIntegration const *integration;
   WlcsDisplayServer *server;
   void *handle;
   int rc;

   if (!path) FAIL("E_WLCS_INTEGRATION is unset");

   /* dlopen rather than link: this is how the runner loads us, so it also
    * checks the symbol is actually exported from the shared object. */
   handle = dlopen(path, RTLD_NOW);
   if (!handle) FAIL("dlopen(%s): %s", path, dlerror());

   integration = dlsym(handle, "wlcs_server_integration");
   if (!integration) FAIL("no wlcs_server_integration symbol: %s", dlerror());
   if ((!integration->create_server) || (!integration->destroy_server))
     FAIL("integration is missing create_server/destroy_server");

   server = integration->create_server(0, NULL);
   if (!server) FAIL("create_server returned NULL");

   if (!server->start) FAIL("no start hook");
   if (!server->stop) FAIL("no stop hook");
   if (!server->create_client_socket) FAIL("no create_client_socket hook");
   if (!server->position_window_absolute) FAIL("no position_window_absolute hook");
   if (!server->create_pointer) FAIL("no create_pointer hook");
   if (!server->create_touch) FAIL("no create_touch hook");
   if (!server->get_descriptor) FAIL("no get_descriptor hook (wlcs v2)");

   server->start(server);
   rc = _run(server);
   server->stop(server);
   integration->destroy_server(server);
   dlclose(handle);
   return rc;
}
