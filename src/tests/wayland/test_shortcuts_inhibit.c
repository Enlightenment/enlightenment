/* zwp_keyboard_shortcuts_inhibitor_v1: while it is in force, the keys of
 * E's bindings go to the client, except the shortcuts_inhibit_toggle
 * binding, which takes them back. The test profile binds Super+Escape to
 * that toggle and Super+i to window_maximized_toggle. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include "e_wl_testkit.h"
#include "keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h"

#define PROG "test-shortcuts-inhibit"
#define APP_A "e.test.shortcuts.a"
#define APP_B "e.test.shortcuts.b"
#define APP_C "e.test.shortcuts.c"
#define FOCUS WL_TEST_CLIENT_STATE_FOCUSED
#define MAX (WL_TEST_CLIENT_STATE_MAXIMIZED_H | WL_TEST_CLIENT_STATE_MAXIMIZED_V)

typedef struct
{
   Tk *tk;
   Tk_Toplevel *top;
   const char *app_id;
   struct wl_seat *seat;
   struct zwp_keyboard_shortcuts_inhibit_manager_v1 *manager;
   int presses[KEY_MAX], releases[KEY_MAX];
} Peer;

static Tk *tester_tk;
static struct wl_test *tester;
static int actives, inactives;

static void
_keymap(void *d __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
        uint32_t format __attribute__((unused)), int32_t fd, uint32_t size __attribute__((unused)))
{
   close(fd);
}

static void
_enter(void *d __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
       uint32_t serial __attribute__((unused)), struct wl_surface *s __attribute__((unused)),
       struct wl_array *keys __attribute__((unused)))
{
}

static void
_leave(void *d __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
       uint32_t serial __attribute__((unused)), struct wl_surface *s __attribute__((unused)))
{
}

static void
_key(void *data, struct wl_keyboard *k __attribute__((unused)),
     uint32_t serial __attribute__((unused)), uint32_t time __attribute__((unused)),
     uint32_t key, uint32_t state)
{
   Peer *p = data;

   if (key >= KEY_MAX) return;
   if (state == WL_KEYBOARD_KEY_STATE_PRESSED) p->presses[key]++;
   else p->releases[key]++;
}

static void
_modifiers(void *d __attribute__((unused)), struct wl_keyboard *k __attribute__((unused)),
           uint32_t serial __attribute__((unused)), uint32_t dep __attribute__((unused)),
           uint32_t lat __attribute__((unused)), uint32_t lock __attribute__((unused)),
           uint32_t group __attribute__((unused)))
{
}

static const struct wl_keyboard_listener _keyboard_listener =
{
   _keymap, _enter, _leave, _key, _modifiers
};

static void
_active(void *d __attribute__((unused)), struct zwp_keyboard_shortcuts_inhibitor_v1 *i __attribute__((unused)))
{
   actives++;
}

static void
_inactive(void *d __attribute__((unused)), struct zwp_keyboard_shortcuts_inhibitor_v1 *i __attribute__((unused)))
{
   inactives++;
}

static const struct zwp_keyboard_shortcuts_inhibitor_v1_listener _inhibitor_listener =
{
   _active, _inactive
};

static void
_peer_new(Peer *p, const char *app_id)
{
   memset(p, 0, sizeof(*p));
   p->tk = tk_connect(PROG);
   p->app_id = app_id;
   p->seat = tk_bind(p->tk, &wl_seat_interface, 1);
   p->manager = tk_bind(p->tk, &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1);
   if ((!p->seat) || (!p->manager)) tk_fail(p->tk, "missing global");
   wl_keyboard_add_listener(wl_seat_get_keyboard(p->seat), &_keyboard_listener, p);
   p->top = tk_toplevel_new(p->tk, app_id, app_id, 200, 150);
   tk_wait_states(p->tk, app_id, FOCUS, FOCUS, "a new window has the focus");
}

static struct zwp_keyboard_shortcuts_inhibitor_v1 *
_inhibit(Peer *p)
{
   struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;

   inhibitor = zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts
     (p->manager, tk_toplevel_surface(p->top), p->seat);
   zwp_keyboard_shortcuts_inhibitor_v1_add_listener(inhibitor, &_inhibitor_listener, NULL);
   tk_sync(p->tk);
   return inhibitor;
}

/* Super+key, pressed and released, as a user would. */
static void
_super(Peer *p, const char *key)
{
   wl_test_key_modified(tester, key, WL_TEST_MODIFIER_WIN, 1);
   wl_test_key_modified(tester, key, WL_TEST_MODIFIER_WIN, 0);
   tk_sync(tester_tk);
   tk_settle(p->tk);
}

static void
_expect_binding(Peer *p, const char *what)
{
   int presses = p->presses[KEY_I];

   _super(p, "i");
   tk_wait_states(p->tk, p->app_id, MAX, MAX, what);
   _super(p, "i");
   tk_wait_states(p->tk, p->app_id, MAX, 0, what);
   if (p->presses[KEY_I] != presses) tk_fail(p->tk, "%s: the client got the key", what);
}

static void
_expect_client(Peer *p, const char *what)
{
   int presses = p->presses[KEY_I];

   _super(p, "i");
   if ((p->presses[KEY_I] != presses + 1) || (p->releases[KEY_I] != presses + 1))
     tk_fail(p->tk, "%s: the client did not get the key", what);
   if (tk_expect(p->tk, p->app_id)->states & MAX) tk_fail(p->tk, "%s: the binding ran", what);
}

static void
_test_already_inhibited(void)
{
   const struct wl_interface *iface = NULL;
   Peer c;

   _peer_new(&c, APP_C);
   zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(c.manager, tk_toplevel_surface(c.top), c.seat);
   zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(c.manager, tk_toplevel_surface(c.top), c.seat);
   if ((tk_protocol_error(c.tk, &iface) != ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ALREADY_INHIBITED) ||
       (iface != &zwp_keyboard_shortcuts_inhibit_manager_v1_interface))
     tk_fail(c.tk, "a second inhibitor is not an already_inhibited error");
   tk_disconnect(c.tk);
}

int
main(void)
{
   struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;
   Peer a, b;

   _peer_new(&a, APP_A);
   tester_tk = a.tk;
   tester = tk_bind(a.tk, &wl_test_interface, 8);
   if (!tester) tk_fail(a.tk, "no wl_test version 8");

   _expect_binding(&a, "no inhibitor");

   inhibitor = _inhibit(&a);
   if ((actives != 1) || inactives) tk_fail(a.tk, "no active for a focused surface");
   _expect_client(&a, "inhibited");

   _super(&a, "Escape");
   if ((inactives != 1) || a.presses[KEY_ESC]) tk_fail(a.tk, "the toggle did not take the keys back");
   _expect_binding(&a, "turned off by the user");

   _super(&a, "Escape");
   if ((actives != 2) || a.presses[KEY_ESC]) tk_fail(a.tk, "the toggle did not give the keys back");
   _expect_client(&a, "turned on again");

   /* Another window takes the focus: no inactive, and its keys are E's. */
   _peer_new(&b, APP_B);
   tk_sync(a.tk);
   if (inactives != 1) tk_fail(a.tk, "inactive sent on a focus change");
   _expect_binding(&b, "another window has the focus");
   tk_disconnect(b.tk);
   tk_wait_states(a.tk, APP_A, FOCUS, FOCUS, "the focus came back");
   tk_sync(a.tk);
   if (actives != 3) tk_fail(a.tk, "no active when the focus came back");
   _expect_client(&a, "the focus came back");

   zwp_keyboard_shortcuts_inhibitor_v1_destroy(inhibitor);
   tk_sync(a.tk);
   _expect_binding(&a, "the inhibitor is gone");

   _test_already_inhibited();

   tk_disconnect(a.tk);
   printf(PROG ": ok\n");
   return 0;
}
