/* A toplevel with a window geometry offset stays where it is across an
 * unmap and a remap: the offset between its buffer and its window is
 * taken off once, when it is first placed. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define PROG "test-window-geometry"
#define W 300
#define H 200
#define OX 12
#define OY 5

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   int x0, y0, x, y, i;

   tk = tk_connect(PROG);
   top = tk_toplevel_new(tk, "e.test.window_geometry", PROG, W, H);
   xdg_surface_set_window_geometry(tk_toplevel_xdg_surface(top),
                                   OX, OY, W - 2 * OX, H - 2 * OY);
   wl_surface_commit(tk_toplevel_surface(top));
   tk_settle(tk);
   tk_surface_geometry(tk, tk_toplevel_surface(top), &x0, &y0, NULL, NULL);

   for (i = 1; i <= 3; i++)
     {
        tk_toplevel_remap(top);
        tk_surface_geometry(tk, tk_toplevel_surface(top), &x, &y, NULL, NULL);
        if ((x != x0) || (y != y0))
          tk_fail(tk, "after remap %d the window is at %d,%d, was %d,%d",
                  i, x, y, x0, y0);
     }

   printf(PROG ": ok (stays at %d,%d)\n", x0, y0);
   tk_disconnect(tk);
   return 0;
}
