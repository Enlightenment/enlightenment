/* The input method's keyboard grab takes the keys while a text field is
 * enabled, and gives back those it does not use through
 * zwp_virtual_keyboard_v1. The test profile binds Super+i to
 * window_maximized_toggle. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include "e_wl_testkit.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "input-method-unstable-v2-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

#define PROG "test-input-method-keyboard"
#define APP_ID "e.test.input-method-keyboard"
#define FOCUS WL_TEST_CLIENT_STATE_FOCUSED
#define MAX (WL_TEST_CLIENT_STATE_MAXIMIZED_H | WL_TEST_CLIENT_STATE_MAXIMIZED_V)

/* "+30" for a press of key 30, "-30" for its release, "m" for modifiers */
typedef struct
{
   char log[256];
   uint32_t mods;
   int keymap_fd;
   int repeat_info;
} Keys;

static Tk *app, *ime;
static struct wl_test *tester;
static Keys app_keys, grab_keys;

static void
_log_key(Keys *k, uint32_t key, uint32_t state)
{
   size_t n = strlen(k->log);

   snprintf(k->log + n, sizeof(k->log) - n, "%c%u",
            state == WL_KEYBOARD_KEY_STATE_PRESSED ? '+' : '-', key);
}

static void
_expect(Tk *tk, Keys *k, const char *expected, const char *what)
{
   if (strcmp(k->log, expected))
     tk_fail(tk, "%s: got keys \"%s\", expected \"%s\"", what, k->log, expected);
   k->log[0] = 0;
}

static void
_kbd_keymap(void *data __attribute__((unused)), struct wl_keyboard *kbd __attribute__((unused)),
            uint32_t format __attribute__((unused)), int32_t fd, uint32_t size __attribute__((unused)))
{
   close(fd);
}

static void
_kbd_enter(void *data __attribute__((unused)), struct wl_keyboard *kbd __attribute__((unused)),
           uint32_t serial __attribute__((unused)), struct wl_surface *surface __attribute__((unused)),
           struct wl_array *keys __attribute__((unused)))
{
}

static void
_kbd_leave(void *data __attribute__((unused)), struct wl_keyboard *kbd __attribute__((unused)),
           uint32_t serial __attribute__((unused)), struct wl_surface *surface __attribute__((unused)))
{
}

static void
_kbd_key(void *data, struct wl_keyboard *kbd __attribute__((unused)), uint32_t serial __attribute__((unused)),
         uint32_t time __attribute__((unused)), uint32_t key, uint32_t state)
{
   _log_key(data, key, state);
}

static void
_kbd_modifiers(void *data, struct wl_keyboard *kbd __attribute__((unused)), uint32_t serial __attribute__((unused)),
               uint32_t depressed, uint32_t latched __attribute__((unused)),
               uint32_t locked __attribute__((unused)), uint32_t group __attribute__((unused)))
{
   ((Keys *)data)->mods = depressed;
}

static void
_kbd_repeat_info(void *data __attribute__((unused)), struct wl_keyboard *kbd __attribute__((unused)),
                 int32_t rate __attribute__((unused)), int32_t delay __attribute__((unused)))
{
}

static const struct wl_keyboard_listener _kbd_listener =
{
   _kbd_keymap, _kbd_enter, _kbd_leave, _kbd_key, _kbd_modifiers, _kbd_repeat_info
};

static void
_grab_keymap(void *data, struct zwp_input_method_keyboard_grab_v2 *grab __attribute__((unused)),
             uint32_t format __attribute__((unused)), int32_t fd, uint32_t size __attribute__((unused)))
{
   Keys *k = data;

   if (k->keymap_fd >= 0) close(k->keymap_fd);
   k->keymap_fd = fd;
}

static void
_grab_key(void *data, struct zwp_input_method_keyboard_grab_v2 *grab __attribute__((unused)),
          uint32_t serial __attribute__((unused)), uint32_t time __attribute__((unused)),
          uint32_t key, uint32_t state)
{
   _log_key(data, key, state);
}

static void
_grab_modifiers(void *data, struct zwp_input_method_keyboard_grab_v2 *grab __attribute__((unused)),
                uint32_t serial __attribute__((unused)), uint32_t depressed,
                uint32_t latched __attribute__((unused)), uint32_t locked __attribute__((unused)),
                uint32_t group __attribute__((unused)))
{
   ((Keys *)data)->mods = depressed;
}

static void
_grab_repeat_info(void *data, struct zwp_input_method_keyboard_grab_v2 *grab __attribute__((unused)),
                  int32_t rate __attribute__((unused)), int32_t delay __attribute__((unused)))
{
   ((Keys *)data)->repeat_info++;
}

static const struct zwp_input_method_keyboard_grab_v2_listener _grab_listener =
{
   _grab_keymap, _grab_key, _grab_modifiers, _grab_repeat_info
};

static void
_settle(void)
{
   tk_sync(app);
   tk_sync(ime);
   tk_sync(app);
}

static void
_key(const char *name, int pressed)
{
   wl_test_key(tester, name, pressed);
   wl_display_flush(tk_display(app));
   _settle();
}

static void
_type(const char *name)
{
   _key(name, 1);
   _key(name, 0);
}

static void
_enable(struct zwp_text_input_v3 *ti, int enable)
{
   if (enable) zwp_text_input_v3_enable(ti);
   else zwp_text_input_v3_disable(ti);
   zwp_text_input_v3_commit(ti);
   _settle();
}

static struct zwp_input_method_keyboard_grab_v2 *
_grab_new(struct zwp_input_method_v2 *im)
{
   struct zwp_input_method_keyboard_grab_v2 *grab;

   grab = zwp_input_method_v2_grab_keyboard(im);
   zwp_input_method_keyboard_grab_v2_add_listener(grab, &_grab_listener, &grab_keys);
   _settle();
   return grab;
}

static void
_vk_error(int set_keymap, uint32_t format, int expected, const char *what)
{
   struct zwp_virtual_keyboard_manager_v1 *manager;
   struct zwp_virtual_keyboard_v1 *vk;
   const struct wl_interface *iface;
   struct wl_seat *seat;
   Tk *tk;

   tk = tk_connect(PROG);
   seat = tk_bind(tk, &wl_seat_interface, 1);
   manager = tk_bind(tk, &zwp_virtual_keyboard_manager_v1_interface, 1);
   if ((!seat) || (!manager)) tk_fail(tk, "missing global");
   vk = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(manager, seat);
   if (set_keymap)
     zwp_virtual_keyboard_v1_keymap(vk, format, grab_keys.keymap_fd, 1);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_A, WL_KEYBOARD_KEY_STATE_PRESSED);
   if ((tk_protocol_error(tk, &iface) != expected) ||
       (iface != &zwp_virtual_keyboard_v1_interface))
     tk_fail(tk, "%s is not the expected protocol error", what);
   tk_disconnect(tk);
}

static Tk *_ime_seat_tk;
static struct wl_seat *_ime_seat;
static struct zwp_input_method_manager_v2 *_im_manager;

static void
_inert_grab(void)
{
   struct zwp_input_method_keyboard_grab_v2 *grab;
   struct zwp_input_method_v2 *im;
   Keys keys = { .keymap_fd = -1 };

   im = zwp_input_method_manager_v2_get_input_method(_im_manager, _ime_seat);
   grab = zwp_input_method_v2_grab_keyboard(im);
   zwp_input_method_keyboard_grab_v2_add_listener(grab, &_grab_listener, &keys);
   _settle();
   if ((keys.keymap_fd >= 0) || keys.repeat_info)
     tk_fail(ime, "the grab of an unavailable input method got events");
   _type("a");
   _expect(_ime_seat_tk, &keys, "", "the grab of an unavailable input method");
   _expect(ime, &grab_keys, "+30-30", "the grab, with an unavailable input method");
   zwp_input_method_keyboard_grab_v2_release(grab);
   zwp_input_method_v2_destroy(im);
   if (keys.keymap_fd >= 0) close(keys.keymap_fd);
}

int
main(void)
{
   struct zwp_text_input_manager_v3 *ti_manager;
   struct zwp_input_method_manager_v2 *im_manager;
   struct zwp_virtual_keyboard_manager_v1 *vk_manager;
   struct zwp_input_method_keyboard_grab_v2 *grab, *old;
   struct zwp_input_method_v2 *im;
   struct zwp_virtual_keyboard_v1 *vk;
   struct zwp_text_input_v3 *ti;
   struct wl_seat *app_seat, *ime_seat;
   unsigned int id;

   grab_keys.keymap_fd = -1;
   tk_privileges_file("test-input-method-keyboard", NULL, "input_method;virtual_keyboard;");
   app = tk_connect(PROG);
   ime = tk_connect(PROG);
   tester = tk_bind(app, &wl_test_interface, 8);
   app_seat = tk_bind(app, &wl_seat_interface, 1);
   ime_seat = tk_bind(ime, &wl_seat_interface, 1);
   ti_manager = tk_bind(app, &zwp_text_input_manager_v3_interface, 1);
   im_manager = tk_bind(ime, &zwp_input_method_manager_v2_interface, 1);
   vk_manager = tk_bind(ime, &zwp_virtual_keyboard_manager_v1_interface, 1);
   if ((!tester) || (!app_seat) || (!ime_seat) || (!ti_manager) || (!im_manager) || (!vk_manager))
     tk_fail(app, "missing global");

   _ime_seat_tk = ime;
   _ime_seat = ime_seat;
   _im_manager = im_manager;
   wl_keyboard_add_listener(wl_seat_get_keyboard(app_seat), &_kbd_listener, &app_keys);
   ti = zwp_text_input_manager_v3_get_text_input(ti_manager, app_seat);
   tk_toplevel_new(app, APP_ID, APP_ID, 200, 200);
   tk_wait_states(app, APP_ID, FOCUS, FOCUS, "a new window has the focus");
   id = tk_expect(app, APP_ID)->id;

   im = zwp_input_method_manager_v2_get_input_method(im_manager, ime_seat);
   grab = _grab_new(im);
   if ((grab_keys.keymap_fd < 0) || (grab_keys.repeat_info != 1))
     tk_fail(ime, "the grab got no keymap or no repeat info");

   _type("a");
   _expect(app, &app_keys, "+30-30", "no field enabled, the window");
   _expect(ime, &grab_keys, "", "no field enabled, the grab");

   _enable(ti, 1);
   _type("a");
   _expect(app, &app_keys, "", "a field enabled, the window");
   _expect(ime, &grab_keys, "+30-30", "a field enabled, the grab");

   _key("a", 1);
   _key("a", 1);
   _key("a", 0);
   _expect(ime, &grab_keys, "+30-30", "a repeated key");

   _key("Shift_L", 1);
   _expect(ime, &grab_keys, "+42", "shift");
   if (!grab_keys.mods) tk_fail(ime, "the grab got no modifiers for shift");
   _key("Shift_L", 0);
   _expect(ime, &grab_keys, "-42", "shift released");
   if (grab_keys.mods) tk_fail(ime, "the grab still has shift");

   /* The release goes where its press went. */
   _key("a", 1);
   _enable(ti, 0);
   _key("a", 0);
   _expect(app, &app_keys, "", "a release after the field was disabled, the window");
   _expect(ime, &grab_keys, "+30-30", "a release after the field was disabled, the grab");
   _enable(ti, 1);

   /* Key bindings run before the grab. */
   wl_test_key_modified(tester, "i", WL_TEST_MODIFIER_WIN, 1);
   wl_test_key_modified(tester, "i", WL_TEST_MODIFIER_WIN, 0);
   tk_wait_states(app, APP_ID, MAX, MAX, "Super+i maximized with the grab");
   _settle();
   _expect(ime, &grab_keys, "+125-125", "Super+i, the grab");
   tk_action(app, id, "window_maximized_toggle", NULL);
   tk_wait_states(app, APP_ID, MAX, 0, "unmaximized");

   /* The way back reaches the window, not the grab, nor the bindings. */
   vk = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(vk_manager, ime_seat);
   zwp_virtual_keyboard_v1_keymap(vk, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, grab_keys.keymap_fd, 1);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_A, WL_KEYBOARD_KEY_STATE_PRESSED);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_A, WL_KEYBOARD_KEY_STATE_RELEASED);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_PRESSED);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_I, WL_KEYBOARD_KEY_STATE_PRESSED);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_I, WL_KEYBOARD_KEY_STATE_RELEASED);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_RELEASED);
   _settle();
   _expect(app, &app_keys, "+30-30+125+23-23-125", "keys from the virtual keyboard, the window");
   _expect(ime, &grab_keys, "", "keys from the virtual keyboard, the grab");
   tk_wait_states(app, APP_ID, MAX, 0, "Super+i from the virtual keyboard");

   /* An input method gives a key back while it is still held. */
   _key("a", 1);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_A, WL_KEYBOARD_KEY_STATE_PRESSED);
   zwp_virtual_keyboard_v1_key(vk, 0, KEY_A, WL_KEYBOARD_KEY_STATE_RELEASED);
   _settle();
   _key("a", 0);
   _expect(app, &app_keys, "+30-30", "a key given back while held, the window");
   _expect(ime, &grab_keys, "+30-30", "a key given back while held, the grab");

   /* A second input method's grab gets nothing. */
   _inert_grab();

   /* E's own keyboard move gets the keys. */
   tk_action(app, id, "window_move", NULL);
   _type("a");
   _expect(ime, &grab_keys, "", "a key during a keyboard move");
   tk_action_end(app, id, "window_move", NULL);
   tk_wait_states(app, APP_ID, FOCUS, FOCUS, "the focus after the keyboard move");
   _enable(ti, 1);
   _type("a");
   _expect(ime, &grab_keys, "+30-30", "a key after the keyboard move");
   app_keys.log[0] = 0;

   /* A second grab replaces the first. */
   old = grab;
   grab = _grab_new(im);
   _type("a");
   _expect(ime, &grab_keys, "+30-30", "the second grab");
   zwp_input_method_keyboard_grab_v2_release(old);

   /* A grab released with a key down: the release goes nowhere. */
   _key("a", 1);
   zwp_input_method_keyboard_grab_v2_release(grab);
   _settle();
   _key("a", 0);
   _expect(app, &app_keys, "", "a release after the grab was gone, the window");
   _expect(ime, &grab_keys, "+30", "a release after the grab was gone, the grab");
   _type("a");
   _expect(app, &app_keys, "+30-30", "no grab, the window");

   /* The input method goes first, then its grab. */
   grab = _grab_new(im);
   zwp_input_method_v2_destroy(im);
   _settle();
   _type("a");
   _expect(app, &app_keys, "+30-30", "no input method, the window");
   zwp_input_method_keyboard_grab_v2_release(grab);
   _settle();

   _vk_error(0, 0, ZWP_VIRTUAL_KEYBOARD_V1_ERROR_NO_KEYMAP, "a key before the keymap");
   _vk_error(1, 0, ZWP_VIRTUAL_KEYBOARD_V1_ERROR_INVALID_KEYMAP_FORMAT,
             "a keymap that is not xkb");

   tk_disconnect(ime);
   tk_disconnect(app);
   printf(PROG ": ok\n");
   return 0;
}
