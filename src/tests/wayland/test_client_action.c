/* wl_test.client_action: drive E through its own actions.
 *
 * Maximize and restore through window_maximized_toggle. The round trip is
 * the part worth asserting: the window must come back exactly where and as
 * big as it was. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define APP_ID "e.test.client_action"
#define TITLE  "actions under test"
#define W 320
#define H 240

#define MAXIMIZED (WL_TEST_CLIENT_STATE_MAXIMIZED_H | \
                   WL_TEST_CLIENT_STATE_MAXIMIZED_V)

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *c;
   unsigned int id;
   int x0, y0, w0, h0, cw, ch, nconf;

   tk = tk_connect("test-client-action");
   top = tk_toplevel_new(tk, APP_ID, TITLE, W, H);

   c = tk_expect(tk, APP_ID);
   id = c->id;
   x0 = c->x; y0 = c->y; w0 = c->w; h0 = c->h;

   if (c->states & MAXIMIZED)
     tk_fail(tk, "a freshly mapped window is already maximized (states=0x%x)",
             c->states);

   tk_action(tk, id, "window_maximized_toggle", NULL);

   c = tk_expect(tk, APP_ID);
   if (!(c->states & WL_TEST_CLIENT_STATE_MAXIMIZED_H))
     tk_fail(tk, "not maximized horizontally after the toggle (states=0x%x)",
             c->states);
   if (!(c->states & WL_TEST_CLIENT_STATE_MAXIMIZED_V))
     tk_fail(tk, "not maximized vertically after the toggle (states=0x%x)",
             c->states);
   tk_toplevel_configured(top, &cw, &ch, &nconf);
   if ((cw <= w0) || (ch <= h0))
     tk_fail(tk, "maximized, but the configure asked for %dx%d, no bigger "
                 "than the original %dx%d (%d configures)",
             cw, ch, w0, h0, nconf);
   if ((c->w == w0) && (c->h == h0))
     tk_fail(tk, "maximized and configured to %dx%d, but the frame is still "
                 "%dx%d (%d configures)", cw, ch, c->w, c->h, nconf);

   tk_action(tk, id, "window_maximized_toggle", NULL);

   c = tk_expect(tk, APP_ID);
   if (c->states & MAXIMIZED)
     tk_fail(tk, "still maximized after the second toggle (states=0x%x)",
             c->states);
   if ((c->x != x0) || (c->y != y0) || (c->w != w0) || (c->h != h0))
     tk_fail(tk, "restored to %dx%d+%d+%d, was %dx%d+%d+%d before maximizing",
             c->w, c->h, c->x, c->y, w0, h0, x0, y0);

   printf("test-client-action: ok (id=%u maximized and restored to "
          "%dx%d+%d+%d)\n", id, c->w, c->h, c->x, c->y);

   tk_disconnect(tk);
   return 0;
}
