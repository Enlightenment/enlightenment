/* xdg_toplevel events from versions 4 to 6: configure_bounds,
 * wm_capabilities, and the suspended state of an iconic window. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define PROG "test-xdg-toplevel-v6"
#define APP_ID "e.test.xdg_toplevel_v6"

#define CAP(x) (1u << XDG_TOPLEVEL_WM_CAPABILITIES_##x)
#define SUSPENDED (1u << XDG_TOPLEVEL_STATE_SUSPENDED)

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *c;
   unsigned int id, caps, want;
   int bw, bh, mw, mh;

   tk = tk_connect(PROG);
   if (tk_global_version(tk, "xdg_wm_base") < 6)
     tk_fail(tk, "xdg_wm_base is advertised at version %u, not 6",
             tk_global_version(tk, "xdg_wm_base"));

   top = tk_toplevel_new(tk, APP_ID, PROG, 300, 200);
   c = tk_expect(tk, APP_ID);
   id = c->id;

   want = CAP(WINDOW_MENU) | CAP(MAXIMIZE) | CAP(FULLSCREEN) | CAP(MINIMIZE);
   caps = tk_toplevel_wm_capabilities(top);
   if (caps != want)
     tk_fail(tk, "wm_capabilities 0x%x, expected 0x%x", caps, want);

   tk_toplevel_bounds(top, &bw, &bh);
   if ((bw <= 0) || (bh <= 0))
     tk_fail(tk, "no configure_bounds came with the configure (%dx%d)",
             bw, bh);
   tk_action(tk, id, "window_maximized_toggle", "default both");
   tk_toplevel_configured(top, &mw, &mh, NULL);
   if ((mw != bw) || (mh != bh))
     tk_fail(tk, "configure_bounds said %dx%d, but a maximized window is "
                 "%dx%d", bw, bh, mw, mh);
   tk_action(tk, id, "window_maximized_toggle", "default both");

   if (tk_toplevel_states(top) & SUSPENDED)
     tk_fail(tk, "a window on screen is told it is suspended");
   tk_action(tk, id, "window_iconic_toggle", NULL);
   if (!(tk_toplevel_states(top) & SUSPENDED))
     tk_fail(tk, "an iconic window is not told it is suspended "
                 "(states 0x%x)", tk_toplevel_states(top));
   tk_action(tk, id, "window_iconic_toggle", NULL);
   if (tk_toplevel_states(top) & SUSPENDED)
     tk_fail(tk, "a window brought back from iconic is still suspended");

   tk_toplevel_del(top);
   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
