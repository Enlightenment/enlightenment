/* A window that draws its own shadow comes back from maximized at the size
 * it was.
 *
 * Its surface is larger than its window geometry, so "its size" is two
 * numbers, and restoring the surface size grows the window by its shadow
 * every time. Asked both ways, as they are different code in E: the client
 * asking (xdg_toplevel.set_maximized and unset_maximized), and E's own
 * maximize action. Both the size E configures and the size it then reports
 * are checked: one can be right and the other wrong. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define PROG "test-maximize-restore"
#define APP_ID "e.test.maximize_restore"

#define SHADOW 16
#define GEOM_W 400
#define GEOM_H 300

#define MAXIMIZED (WL_TEST_CLIENT_STATE_MAXIMIZED_H | \
                   WL_TEST_CLIENT_STATE_MAXIMIZED_V)

static void
_check_maximized(Tk *tk, int on, const char *who)
{
   Tk_Client *c = tk_expect(tk, APP_ID);

   if (((c->states & MAXIMIZED) == MAXIMIZED) != !!on)
     tk_fail(tk, "%s: the window is %s maximized (states 0x%x)", who,
             on ? "not" : "still", c->states);
}

static void
_check_restored(Tk *tk, Tk_Toplevel *top, const char *who)
{
   Tk_Client *c;
   int cw, ch;

   _check_maximized(tk, 0, who);
   tk_toplevel_configured(top, &cw, &ch, NULL);
   if ((cw != GEOM_W) || (ch != GEOM_H))
     tk_fail(tk, "%s: restored with a configure of %dx%d, the window was "
                 "%dx%d in a %dx%d surface", who, cw, ch, GEOM_W, GEOM_H,
             GEOM_W + (2 * SHADOW), GEOM_H + (2 * SHADOW));
   c = tk_expect(tk, APP_ID);
   if ((c->w != GEOM_W) || (c->h != GEOM_H))
     tk_fail(tk, "%s: configured %dx%d, but the window is reported %dx%d",
             who, cw, ch, c->w, c->h);
}

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *c;
   unsigned int id;

   tk = tk_connect(PROG);
   top = tk_toplevel_new_shadowed(tk, APP_ID, PROG, GEOM_W, GEOM_H, SHADOW);
   c = tk_expect(tk, APP_ID);
   id = c->id;
   if ((c->w != GEOM_W) || (c->h != GEOM_H))
     tk_fail(tk, "a %dx%d window in a %dx%d surface is reported %dx%d",
             GEOM_W, GEOM_H, GEOM_W + (2 * SHADOW), GEOM_H + (2 * SHADOW),
             c->w, c->h);

   tk_toplevel_maximize(top, 1);
   _check_maximized(tk, 1, "set_maximized");
   tk_toplevel_maximize(top, 0);
   _check_restored(tk, top, "unset_maximized");

   tk_action(tk, id, "window_maximized_toggle", NULL);
   _check_maximized(tk, 1, "E's maximize action");
   tk_action(tk, id, "window_maximized_toggle", NULL);
   _check_restored(tk, top, "E's maximize action, again");

   tk_toplevel_del(top);
   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
