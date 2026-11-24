/* zwp_idle_inhibitor_v1 keeps the screensaver off only while its surface
 * is visible. */
#include <stdio.h>
#include <time.h>

#include "e_wl_testkit.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

#define PROG "test-idle-inhibit"
#define APP_ID "e.test.idle-inhibit"
#define TIMEOUT 1
#define MASK (WL_TEST_CLIENT_STATE_ICONIFIED | WL_TEST_CLIENT_STATE_VISIBLE)

typedef struct
{
   Tk *tk;
   struct wl_test *tester;
   int on;
} Saver;

static void
_screensaver(void *data, struct wl_test *t __attribute__((unused)), uint32_t on)
{
   Saver *saver = data;

   saver->on = !!on;
}

/* Only the requests this test sends are answered on its own wl_test. */
static const struct wl_test_listener _tester_listener =
{
   .screensaver = _screensaver
};

static int
_saver_get(Saver *saver)
{
   saver->on = -1;
   wl_test_get_screensaver(saver->tester);
   tk_sync(saver->tk);
   if (saver->on < 0) tk_fail(saver->tk, "no answer to get_screensaver");
   return saver->on;
}

static void
_saver_wait(Saver *saver, int on, const char *what)
{
   struct timespec step = { 0, 50000000 };
   int i;

   for (i = 0; i < TK_WAIT_SECONDS * 20; i++)
     {
        if (_saver_get(saver) == on) return;
        nanosleep(&step, NULL);
     }
   tk_fail(saver->tk, "screensaver still %s: %s", on ? "off" : "on", what);
}

/* Nothing to wait for when the screensaver must stay off: wait well past
 * its timeout and look. */
static void
_saver_stays_off(Saver *saver, const char *what)
{
   struct timespec wait = { (TIMEOUT * 2) + 1, 0 };

   nanosleep(&wait, NULL);
   if (_saver_get(saver)) tk_fail(saver->tk, "screensaver came on: %s", what);
}

static void
_wake(Saver *saver)
{
   tk_pointer_move(saver->tk, 1, 0);
   _saver_wait(saver, 0, "input wakes the screen");
}

int
main(void)
{
   struct zwp_idle_inhibit_manager_v1 *manager;
   struct zwp_idle_inhibitor_v1 *inhibitor;
   struct wl_surface *bare;
   Tk_Toplevel *top;
   Saver saver;
   unsigned int id;

   saver.tk = tk_connect(PROG);
   saver.tester = tk_bind(saver.tk, &wl_test_interface, 7);
   manager = tk_bind(saver.tk, &zwp_idle_inhibit_manager_v1_interface, 1);
   if ((!saver.tester) || (!manager)) tk_fail(saver.tk, "missing global");
   wl_test_add_listener(saver.tester, &_tester_listener, &saver);
   top = tk_toplevel_new(saver.tk, APP_ID, APP_ID, 200, 150);
   id = tk_expect(saver.tk, APP_ID)->id;

   wl_test_screensaver_configure(saver.tester, TIMEOUT);
   _saver_wait(&saver, 1, "no inhibitor");
   _wake(&saver);

   inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(manager, tk_toplevel_surface(top));
   tk_sync(saver.tk);
   _saver_stays_off(&saver, "with an inhibitor on a visible window");

   tk_action(saver.tk, id, "window_iconic_toggle", NULL);
   tk_wait_states(saver.tk, APP_ID, MASK, WL_TEST_CLIENT_STATE_ICONIFIED, "iconified");
   _saver_wait(&saver, 1, "the inhibiting window is iconified");

   tk_action(saver.tk, id, "window_iconic_toggle", NULL);
   tk_wait_states(saver.tk, APP_ID, MASK, WL_TEST_CLIENT_STATE_VISIBLE, "restored");
   _wake(&saver);
   _saver_stays_off(&saver, "the inhibiting window is shown again");

   zwp_idle_inhibitor_v1_destroy(inhibitor);
   _saver_wait(&saver, 1, "the inhibitor is destroyed");
   _wake(&saver);

   /* An unmapped surface inhibits nothing, and its inhibitor outlives it. */
   bare = tk_surface_new(saver.tk);
   inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(manager, bare);
   tk_sync(saver.tk);
   _saver_wait(&saver, 1, "the inhibiting surface is not mapped");
   wl_surface_destroy(bare);
   _wake(&saver);
   _saver_wait(&saver, 1, "the inhibiting surface is destroyed");
   zwp_idle_inhibitor_v1_destroy(inhibitor);
   tk_sync(saver.tk);

   tk_disconnect(saver.tk);
   printf(PROG ": ok\n");
   return 0;
}
