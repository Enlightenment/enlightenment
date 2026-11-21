/* Minimise a window through E's own action while the theme's show
 * animation still runs for it, then get it back. The minimised window has
 * to leave the screen once the animations are over, and the restored one
 * to come back. meson stretches the theme's animations for this test, so
 * that the action is sure to land in the show animation. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define PROG "test-client-iconify"
#define APP_ID "e.test.client_iconify"
#define W 320
#define H 240

#define MASK (WL_TEST_CLIENT_STATE_ICONIFIED | WL_TEST_CLIENT_STATE_VISIBLE)

int
main(void)
{
   Tk *tk;
   unsigned int id;
   int i;

   tk = tk_connect(PROG);
   tk_toplevel_new(tk, APP_ID, "iconify under test", W, H);
   id = tk_expect(tk, APP_ID)->id;

   /* Twice: restoring runs the show animation again. */
   for (i = 0; i < 2; i++)
     {
        tk_action(tk, id, "window_iconic_toggle", NULL);
        tk_wait_states(tk, APP_ID, MASK, WL_TEST_CLIENT_STATE_ICONIFIED,
                       "minimised while being shown");
        tk_action(tk, id, "window_iconic_toggle", NULL);
        tk_wait_states(tk, APP_ID, MASK, WL_TEST_CLIENT_STATE_VISIBLE,
                       "restored");
     }

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
