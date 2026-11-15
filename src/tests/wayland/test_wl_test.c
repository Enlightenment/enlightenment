/* Exercises the test module itself.
 *
 * If this passes, these things are true that nothing else in the suite could
 * establish:
 *
 *   - a module can be pulled into the compositor with E_MODULE_FORCE_LOAD
 *     and serve a private protocol;
 *   - a test can ask the compositor what it *believes* about a surface, and
 *     compare that against what it told the client;
 *   - input injected through the module reaches a client as wl_pointer,
 *     wl_keyboard and wl_touch events, with the right values;
 *   - a second zone can be conjured at runtime, so multi-output behaviour is
 *     testable on a headless backend that has exactly one.
 */
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
#define WIN_X 120
#define WIN_Y 90

#define FAIL(fmt, ...) \
  do \
    { \
       fprintf(stderr, "test-wl-test: " fmt "\n", ##__VA_ARGS__); \
       return 1; \
    } \
  while (0)

#define UNUSED __attribute__((unused))

typedef struct
{
   struct wl_surface *surface;
   wl_fixed_t x, y;
   uint32_t button, button_state;
   uint32_t axis;
   wl_fixed_t axis_value;
   int enters, buttons, axes;
} Pointer_Log;

typedef struct
{
   struct wl_surface *surface;
   uint32_t key, key_state;
   int enters, keys;
} Keyboard_Log;

typedef struct
{
   struct wl_surface *surface;
   int32_t down_id, up_id;
   wl_fixed_t x, y;
   int downs, ups;
} Touch_Log;

static struct wl_display *disp;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct wl_seat *seat;
static struct xdg_wm_base *wm_base;
static struct wl_test *tester;
static int output_count;
static unsigned int client_serial;
static int sync_done;
static int info_valid;
static int32_t info_x, info_y, info_w, info_h;
static uint32_t info_visible, info_focused;
static int configured;
static Pointer_Log ptr;
static Keyboard_Log kbd;
static Touch_Log tch;

static void
_client_serial(void *d UNUSED, struct wl_test *t UNUSED, uint32_t serial)
{
   client_serial = serial;
}

static void
_surface_info(void *d UNUSED, struct wl_test *t UNUSED, struct wl_surface *s UNUSED,
              int32_t x, int32_t y, int32_t w, int32_t h,
              uint32_t visible, uint32_t focused)
{
   info_valid = 1;
   info_x = x;
   info_y = y;
   info_w = w;
   info_h = h;
   info_visible = visible;
   info_focused = focused;
}

static void
_surface_unknown(void *d UNUSED, struct wl_test *t UNUSED, struct wl_surface *s UNUSED)
{
   info_valid = 0;
}

static void
_sync_done(void *d UNUSED, struct wl_test *t UNUSED)
{
   sync_done = 1;
}

static const struct wl_test_listener _tester_listener =
{
   _client_serial, _surface_info, _surface_unknown, _sync_done
};

static void
_wm_base_ping(void *d UNUSED, struct xdg_wm_base *b, uint32_t serial)
{
   xdg_wm_base_pong(b, serial);
}

static const struct xdg_wm_base_listener _wm_base_listener = { _wm_base_ping };

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
                    struct wl_array *states UNUSED)
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
_pointer_enter(void *d UNUSED, struct wl_pointer *p UNUSED, uint32_t serial UNUSED,
               struct wl_surface *s, wl_fixed_t x, wl_fixed_t y)
{
   ptr.surface = s;
   ptr.x = x;
   ptr.y = y;
   ptr.enters++;
}

static void
_pointer_leave(void *d UNUSED, struct wl_pointer *p UNUSED, uint32_t serial UNUSED,
               struct wl_surface *s UNUSED)
{
   ptr.surface = NULL;
}

static void
_pointer_motion(void *d UNUSED, struct wl_pointer *p UNUSED, uint32_t time UNUSED,
                wl_fixed_t x, wl_fixed_t y)
{
   ptr.x = x;
   ptr.y = y;
}

static void
_pointer_button(void *d UNUSED, struct wl_pointer *p UNUSED, uint32_t serial UNUSED,
                uint32_t time UNUSED, uint32_t button, uint32_t state)
{
   ptr.button = button;
   ptr.button_state = state;
   ptr.buttons++;
}

static void
_pointer_axis(void *d UNUSED, struct wl_pointer *p UNUSED, uint32_t time UNUSED,
              uint32_t axis, wl_fixed_t value)
{
   ptr.axis = axis;
   ptr.axis_value = value;
   ptr.axes++;
}

static const struct wl_pointer_listener _pointer_listener =
{
   _pointer_enter, _pointer_leave, _pointer_motion, _pointer_button,
   _pointer_axis, NULL, NULL, NULL, NULL, NULL, NULL
};

static void
_keyboard_keymap(void *d UNUSED, struct wl_keyboard *k UNUSED,
                 uint32_t format UNUSED, int32_t fd, uint32_t size UNUSED)
{
   close(fd);
}

static void
_keyboard_enter(void *d UNUSED, struct wl_keyboard *k UNUSED, uint32_t serial UNUSED,
                struct wl_surface *s, struct wl_array *keys UNUSED)
{
   kbd.surface = s;
   kbd.enters++;
}

static void
_keyboard_leave(void *d UNUSED, struct wl_keyboard *k UNUSED, uint32_t serial UNUSED,
                struct wl_surface *s UNUSED)
{
   kbd.surface = NULL;
}

static void
_keyboard_key(void *d UNUSED, struct wl_keyboard *k UNUSED, uint32_t serial UNUSED,
              uint32_t time UNUSED, uint32_t key, uint32_t state)
{
   kbd.key = key;
   kbd.key_state = state;
   kbd.keys++;
}

static void
_keyboard_modifiers(void *d UNUSED, struct wl_keyboard *k UNUSED, uint32_t serial UNUSED,
                    uint32_t depressed UNUSED, uint32_t latched UNUSED,
                    uint32_t locked UNUSED, uint32_t group UNUSED)
{
}

static void
_keyboard_repeat_info(void *d UNUSED, struct wl_keyboard *k UNUSED,
                      int32_t rate UNUSED, int32_t delay UNUSED)
{
}

static const struct wl_keyboard_listener _keyboard_listener =
{
   _keyboard_keymap, _keyboard_enter, _keyboard_leave, _keyboard_key,
   _keyboard_modifiers, _keyboard_repeat_info
};

static void
_touch_down(void *d UNUSED, struct wl_touch *t UNUSED, uint32_t serial UNUSED,
            uint32_t time UNUSED, struct wl_surface *s, int32_t id,
            wl_fixed_t x, wl_fixed_t y)
{
   tch.surface = s;
   tch.down_id = id;
   tch.x = x;
   tch.y = y;
   tch.downs++;
}

static void
_touch_up(void *d UNUSED, struct wl_touch *t UNUSED, uint32_t serial UNUSED,
          uint32_t time UNUSED, int32_t id)
{
   tch.up_id = id;
   tch.ups++;
}

static void
_touch_motion(void *d UNUSED, struct wl_touch *t UNUSED, uint32_t time UNUSED,
              int32_t id UNUSED, wl_fixed_t x UNUSED, wl_fixed_t y UNUSED)
{
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

static void
_global_add(void *d UNUSED, struct wl_registry *reg, uint32_t id,
            const char *iface, uint32_t version UNUSED)
{
   if (!strcmp(iface, "wl_compositor"))
     compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 4);
   else if (!strcmp(iface, "wl_shm"))
     shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
   else if (!strcmp(iface, "wl_seat"))
     seat = wl_registry_bind(reg, id, &wl_seat_interface, 4);
   else if (!strcmp(iface, "xdg_wm_base"))
     {
        wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wm_base, &_wm_base_listener, NULL);
     }
   else if (!strcmp(iface, "wl_test"))
     tester = wl_registry_bind(reg, id, &wl_test_interface, 1);
   else if (!strcmp(iface, "wl_output"))
     output_count++;
}

static void
_global_remove(void *d UNUSED, struct wl_registry *r UNUSED, uint32_t id UNUSED)
{
}

static const struct wl_registry_listener _registry_listener =
{
   _global_add, _global_remove
};

/* Round-trip through the compositor's main loop, not just its request queue:
 * injected input is dispatched by that loop, and E applies much of what it
 * does from jobs and idlers. */
static int
_tester_sync(void)
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
_surface_info_get(struct wl_surface *surface)
{
   info_valid = 0;
   wl_test_get_surface_info(tester, surface);
   return wl_display_roundtrip(disp);
}

static struct wl_buffer *
_buffer_new(void)
{
   int fd, stride = W * 4, size = stride * H;
   void *map;
   struct wl_shm_pool *pool;
   struct wl_buffer *buf;
   char name[] = "/e-test-shm-XXXXXX";

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
_test_pointer(struct wl_surface *surface)
{
   struct wl_pointer *pointer;

   pointer = wl_seat_get_pointer(seat);
   wl_pointer_add_listener(pointer, &_pointer_listener, NULL);
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip failed binding the pointer");

   wl_test_pointer_warp(tester, WIN_X + 30, WIN_Y + 20);
   if (_tester_sync() < 0) FAIL("sync failed after pointer_warp");
   if ((!ptr.enters) || (ptr.surface != surface))
     FAIL("pointer_warp onto the window sent no wl_pointer.enter for it");
   if ((wl_fixed_to_int(ptr.x) != 30) || (wl_fixed_to_int(ptr.y) != 20))
     FAIL("pointer is at %d,%d in the window, expected 30,20",
          wl_fixed_to_int(ptr.x), wl_fixed_to_int(ptr.y));

   wl_test_pointer_button(tester, BTN_RIGHT, 1);
   if (_tester_sync() < 0) FAIL("sync failed after pointer_button");
   if ((ptr.buttons != 1) || (ptr.button != BTN_RIGHT) ||
       (ptr.button_state != WL_POINTER_BUTTON_STATE_PRESSED))
     FAIL("pressing BTN_RIGHT gave %d button events, last %#x state %u",
          ptr.buttons, ptr.button, ptr.button_state);

   wl_test_pointer_button(tester, BTN_RIGHT, 0);
   if (_tester_sync() < 0) FAIL("sync failed after pointer_button");
   if ((ptr.buttons != 2) || (ptr.button != BTN_RIGHT) ||
       (ptr.button_state != WL_POINTER_BUTTON_STATE_RELEASED))
     FAIL("releasing BTN_RIGHT gave %d button events, last %#x state %u",
          ptr.buttons, ptr.button, ptr.button_state);

   wl_test_pointer_axis(tester, 0, 1);
   if (_tester_sync() < 0) FAIL("sync failed after pointer_axis");
   if ((!ptr.axes) || (ptr.axis != WL_POINTER_AXIS_VERTICAL_SCROLL) ||
       (ptr.axis_value <= 0))
     FAIL("scrolling down one step gave %d axis events, last axis %u value %d",
          ptr.axes, ptr.axis, ptr.axis_value);

   return 0;
}

static int
_test_keyboard(struct wl_surface *surface)
{
   struct wl_keyboard *keyboard;

   keyboard = wl_seat_get_keyboard(seat);
   wl_keyboard_add_listener(keyboard, &_keyboard_listener, NULL);

   /* A click gives the window the focus, which is what decides where keys
    * go. */
   wl_test_pointer_button(tester, BTN_LEFT, 1);
   wl_test_pointer_button(tester, BTN_LEFT, 0);
   if (_tester_sync() < 0) FAIL("sync failed after clicking the window");
   if (kbd.surface != surface)
     FAIL("clicking the window did not give it the keyboard focus");

   /* The evdev code, which is what wl_keyboard.key carries: "a" is 30. The
    * xkb keycode is 8 more, and that off-by-eight is only visible from a
    * client. */
   wl_test_key(tester, "a", 1);
   if (_tester_sync() < 0) FAIL("sync failed after key");
   if ((kbd.keys != 1) || (kbd.key != KEY_A) ||
       (kbd.key_state != WL_KEYBOARD_KEY_STATE_PRESSED))
     FAIL("pressing \"a\" gave %d key events, last key %u state %u",
          kbd.keys, kbd.key, kbd.key_state);

   wl_test_key(tester, "a", 0);
   if (_tester_sync() < 0) FAIL("sync failed after key");
   if ((kbd.keys != 2) || (kbd.key != KEY_A) ||
       (kbd.key_state != WL_KEYBOARD_KEY_STATE_RELEASED))
     FAIL("releasing \"a\" gave %d key events, last key %u state %u",
          kbd.keys, kbd.key, kbd.key_state);

   return 0;
}

static int
_test_touch(struct wl_surface *surface)
{
   struct wl_touch *touch;

   touch = wl_seat_get_touch(seat);
   wl_touch_add_listener(touch, &_touch_listener, NULL);
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip failed binding touch");

   wl_test_touch_down(tester, 3, WIN_X + 40, WIN_Y + 50);
   if (_tester_sync() < 0) FAIL("sync failed after touch_down");
   if ((tch.downs != 1) || (tch.surface != surface) || (tch.down_id != 3))
     FAIL("touch_down id 3 on the window gave %d down events, last id %d",
          tch.downs, tch.down_id);
   if ((wl_fixed_to_int(tch.x) != 40) || (wl_fixed_to_int(tch.y) != 50))
     FAIL("touch is at %d,%d in the window, expected 40,50",
          wl_fixed_to_int(tch.x), wl_fixed_to_int(tch.y));

   wl_test_touch_up(tester, 3);
   if (_tester_sync() < 0) FAIL("sync failed after touch_up");
   if ((tch.ups != 1) || (tch.up_id != 3))
     FAIL("touch_up id 3 gave %d up events, last id %d", tch.ups, tch.up_id);

   return 0;
}

int
main(void)
{
   struct wl_registry *reg;
   struct wl_surface *surface;
   struct xdg_surface *xdg_surface;
   struct xdg_toplevel *toplevel;
   struct wl_buffer *buffer;
   int outputs_before;

   disp = wl_display_connect(NULL);
   if (!disp) FAIL("cannot connect to the compositor");

   reg = wl_display_get_registry(disp);
   wl_registry_add_listener(reg, &_registry_listener, NULL);
   if (wl_display_roundtrip(disp) < 0) FAIL("registry roundtrip failed");

   if (!tester)
     FAIL("no wl_test global - is E_MODULE_FORCE_LOAD=wl_test set?");
   if (!compositor) FAIL("no wl_compositor");
   if (!shm) FAIL("no wl_shm");
   if (!seat) FAIL("no wl_seat");
   if (!wm_base) FAIL("no xdg_wm_base");

   wl_test_add_listener(tester, &_tester_listener, NULL);

   /* Client serials are assigned server-side, in connection order. */
   wl_test_get_client_serial(tester);
   if (wl_display_roundtrip(disp) < 0) FAIL("client_serial roundtrip failed");
   if (client_serial < 1)
     FAIL("client serial is %u, expected >= 1", client_serial);

   /* E creates the E_Client in wl_compositor.create_surface, not at the
    * first commit the way wlroots and Weston do, so a surface is known to the
    * compositor before it has any content. */
   surface = wl_compositor_create_surface(compositor);
   if (_surface_info_get(surface) < 0) FAIL("surface_info roundtrip failed");
   if (!info_valid)
     FAIL("no E_Client for a freshly created surface");

   xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
   xdg_surface_add_listener(xdg_surface, &_xdg_surface_listener, NULL);
   toplevel = xdg_surface_get_toplevel(xdg_surface);
   xdg_toplevel_add_listener(toplevel, &_toplevel_listener, NULL);
   xdg_toplevel_set_title(toplevel, "e-test");
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
   if (_tester_sync() < 0) FAIL("sync failed after mapping");

   if (_surface_info_get(surface) < 0) FAIL("surface_info roundtrip failed");
   if (!info_valid)
     FAIL("compositor reports no E_Client after a mapped commit");
   if ((info_w != W) || (info_h != H))
     FAIL("compositor believes the surface is %dx%d, client committed %dx%d",
          info_w, info_h, W, H);

   /* move_surface, and the compositor's own view agrees afterwards. The wire
    * never carries this. */
   wl_test_move_surface(tester, surface, WIN_X, WIN_Y);
   if (_tester_sync() < 0) FAIL("sync failed after move");
   if (_surface_info_get(surface) < 0) FAIL("surface_info roundtrip failed");
   if (!info_valid) FAIL("no surface_info after move");
   if ((info_x != WIN_X) || (info_y != WIN_Y))
     FAIL("asked for (%d,%d), compositor believes (%d,%d)",
          WIN_X, WIN_Y, info_x, info_y);

   if (_test_pointer(surface)) return 1;
   if (_test_keyboard(surface)) return 1;
   if (_test_touch(surface)) return 1;

   /* A second zone, and with it a second wl_output. This is also what used
    * to walk _e_comp_wl_output_get() into strcmp(NULL), since the first
    * output of the buffer backend has no id. */
   outputs_before = output_count;
   wl_test_zone_add(tester, 1024, 0, 800, 600);
   if (_tester_sync() < 0) FAIL("sync failed after zone_add");
   if (wl_display_roundtrip(disp) < 0) FAIL("roundtrip failed after zone_add");
   if (output_count <= outputs_before)
     FAIL("zone_add produced no new wl_output (had %d, still %d)",
          outputs_before, output_count);

   /* Last, since it costs the connection: touch id 0 is the pointer in evas
    * and cannot be told apart from it, so the module refuses it. */
   wl_test_touch_down(tester, 0, WIN_X + 40, WIN_Y + 50);
   if (wl_display_roundtrip(disp) >= 0)
     FAIL("touch_down with id 0 was accepted");
   {
      const struct wl_interface *iface = NULL;
      uint32_t code = wl_display_get_protocol_error(disp, &iface, NULL);

      if ((iface != &wl_test_interface) ||
          (code != WL_TEST_ERROR_INVALID_TOUCH_ID))
        FAIL("touch_down with id 0 raised error %u on %s, expected "
             "invalid_touch_id on wl_test", code, iface ? iface->name : "none");
   }

   wl_display_disconnect(disp);
   return 0;
}
