/* fcitx5, as fcitx5-run.sh started it, is the input method of a window
 * of the test with a text field. Its keyboard layout engine takes each key
 * from E's keyboard grab and hands it back through zwp_virtual_keyboard_v1,
 * so a key the window gets once fcitx5 holds the grab went both ways. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include "e_wl_testkit.h"
#include "text-input-unstable-v3-client-protocol.h"

#define PROG "test-fcitx5"
#define APP_ID "e.test.fcitx5"

/* fcitx5 hands back text and editing keys alike. */
static const struct
{
   const char *name;
   uint32_t code;
} _keys[] =
{
   { "a", KEY_A },
   { "BackSpace", KEY_BACKSPACE },
};

static struct wl_test *tester;
static uint32_t last_key;
static int keys, dones;

static void
_keymap(void *data __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
        uint32_t format __attribute__((unused)), int32_t fd, uint32_t size __attribute__((unused)))
{
   close(fd);
}

static void
_enter(void *data __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
       uint32_t serial __attribute__((unused)), struct wl_surface *s __attribute__((unused)),
       struct wl_array *held __attribute__((unused)))
{
}

static void
_leave(void *data __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
       uint32_t serial __attribute__((unused)), struct wl_surface *s __attribute__((unused)))
{
}

static void
_key(void *data __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
     uint32_t serial __attribute__((unused)), uint32_t time __attribute__((unused)),
     uint32_t key, uint32_t state __attribute__((unused)))
{
   last_key = key;
   keys++;
}

static void
_modifiers(void *data __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
           uint32_t serial __attribute__((unused)), uint32_t depressed __attribute__((unused)),
           uint32_t latched __attribute__((unused)), uint32_t locked __attribute__((unused)),
           uint32_t group __attribute__((unused)))
{
}

static void
_repeat_info(void *data __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
             int32_t rate __attribute__((unused)), int32_t delay __attribute__((unused)))
{
}

static const struct wl_keyboard_listener _keyboard_listener =
{
   _keymap, _enter, _leave, _key, _modifiers, _repeat_info
};

static void
_ti_surface(void *data __attribute__((unused)), struct zwp_text_input_v3 *ti __attribute__((unused)),
            struct wl_surface *s __attribute__((unused)))
{
}

static void
_ti_preedit(void *data __attribute__((unused)), struct zwp_text_input_v3 *ti __attribute__((unused)),
            const char *text __attribute__((unused)), int32_t begin __attribute__((unused)),
            int32_t end __attribute__((unused)))
{
}

static void
_ti_commit(void *data __attribute__((unused)), struct zwp_text_input_v3 *ti __attribute__((unused)),
           const char *text __attribute__((unused)))
{
}

static void
_ti_delete(void *data __attribute__((unused)), struct zwp_text_input_v3 *ti __attribute__((unused)),
           uint32_t before __attribute__((unused)), uint32_t after __attribute__((unused)))
{
}

static void
_ti_done(void *data __attribute__((unused)), struct zwp_text_input_v3 *ti __attribute__((unused)),
         uint32_t serial __attribute__((unused)))
{
   dones++;
}

static const struct zwp_text_input_v3_listener _ti_listener =
{
   _ti_surface, _ti_surface, _ti_preedit, _ti_commit, _ti_delete, _ti_done
};

/* Activated, fcitx5 grabs the keyboard and then commits, which E passes
 * on to the field as a done. */
static int
_fcitx5_grabbed(const Tk_Client *c __attribute__((unused)), void *data __attribute__((unused)))
{
   return dones > 0;
}

static void
_type(Tk *tk, const char *name)
{
   wl_test_key(tester, name, 1);
   wl_test_key(tester, name, 0);
   tk_settle(tk);
}

static int
_keys_back(const Tk_Client *c __attribute__((unused)), void *data)
{
   return keys >= *(int *)data;
}

int
main(void)
{
   struct zwp_text_input_manager_v3 *ti_manager;
   struct zwp_text_input_v3 *ti;
   struct wl_seat *seat;
   Tk *tk;
   unsigned int i;
   int want;

   tk = tk_connect(PROG);
   tester = tk_bind(tk, &wl_test_interface, 1);
   seat = tk_bind(tk, &wl_seat_interface, 1);
   ti_manager = tk_bind(tk, &zwp_text_input_manager_v3_interface, 1);
   if ((!tester) || (!seat) || (!ti_manager)) tk_fail(tk, "missing global");
   wl_keyboard_add_listener(wl_seat_get_keyboard(seat), &_keyboard_listener, NULL);

   tk_toplevel_new(tk, APP_ID, PROG, 300, 200);
   tk_wait_states(tk, APP_ID, WL_TEST_CLIENT_STATE_FOCUSED,
                  WL_TEST_CLIENT_STATE_FOCUSED, "the window to take the focus");

   /* fcitx5 may come after: E activates it then. */
   ti = zwp_text_input_manager_v3_get_text_input(ti_manager, seat);
   zwp_text_input_v3_add_listener(ti, &_ti_listener, NULL);
   zwp_text_input_v3_enable(ti);
   zwp_text_input_v3_commit(ti);
   tk_wait(tk, APP_ID, _fcitx5_grabbed, NULL, "fcitx5 to grab the keyboard");

   for (i = 0; i < sizeof(_keys) / sizeof(_keys[0]); i++)
     {
        want = keys + 2;
        _type(tk, _keys[i].name);
        tk_wait(tk, APP_ID, _keys_back, &want, "the key to come back from fcitx5");
        if (last_key != _keys[i].code)
          tk_fail(tk, "%s came back as %u, not %u", _keys[i].name, last_key, _keys[i].code);
     }

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
