/* wl_subsurface.set_position takes effect when the parent's state is
 * applied, and with a synchronized parent that is the parent's own commit
 * followed by its parent's.
 *
 * toplevel <- A (synchronized) <- B. B's new position is scheduled on A's
 * state, so a commit of the toplevel alone must not move B; once A has
 * committed too, the toplevel's commit applies it. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define PROG "test-subsurface-position"

static void
_pos(Tk *tk, struct wl_surface *s, int *x, int *y)
{
   tk_surface_geometry(tk, s, x, y, NULL, NULL);
}

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   struct wl_subcompositor *subcomp;
   struct wl_surface *ts, *a, *b;
   struct wl_subsurface *sa, *sb;
   int tx, ty, bx, by, x, y;

   tk = tk_connect(PROG);
   subcomp = tk_bind(tk, &wl_subcompositor_interface, 1);
   if (!subcomp) tk_fail(tk, "no wl_subcompositor");

   top = tk_toplevel_new(tk, "e.test.subsurface_position", PROG, 300, 200);
   ts = tk_toplevel_surface(top);

   a = tk_surface_new(tk);
   sa = wl_subcompositor_get_subsurface(subcomp, a, ts);
   wl_subsurface_set_sync(sa);
   wl_subsurface_set_position(sa, 20, 20);
   b = tk_surface_new(tk);
   sb = wl_subcompositor_get_subsurface(subcomp, b, a);
   wl_subsurface_set_position(sb, 10, 10);
   tk_surface_paint(tk, b, 40, 40);
   tk_surface_paint(tk, a, 100, 100);
   wl_surface_commit(ts);
   tk_settle(tk);

   _pos(tk, ts, &tx, &ty);
   _pos(tk, b, &bx, &by);
   if ((bx != tx + 30) || (by != ty + 30))
     tk_fail(tk, "B is at %d,%d, expected %d,%d", bx, by, tx + 30, ty + 30);

   wl_subsurface_set_position(sb, 50, 50);
   wl_surface_commit(ts);
   tk_settle(tk);
   _pos(tk, b, &x, &y);
   if ((x != bx) || (y != by))
     tk_fail(tk, "a commit of the toplevel alone moved B from %d,%d to "
                 "%d,%d, though A never committed its new position",
             bx, by, x, y);

   wl_surface_commit(a);
   wl_surface_commit(ts);
   tk_settle(tk);
   _pos(tk, b, &x, &y);
   if ((x != tx + 70) || (y != ty + 70))
     tk_fail(tk, "after A and the toplevel committed, B is at %d,%d, "
                 "expected %d,%d", x, y, tx + 70, ty + 70);

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
