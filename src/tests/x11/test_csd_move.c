/* Move a client-side-decorated window with _NET_WM_MOVERESIZE, the way GTK
 * starts a drag from its header bar. E records where the drag starts from
 * the message itself: the window must follow the pointer by how far the
 * pointer travelled. */
#include "x11_testkit.h"

#define W 300
#define H 200
#define DX 60
#define DY 40

int
main(void)
{
   Display *dpy;
   Window win;
   int x0, y0, x1, y1, px, py;

   dpy = xtk_connect("test-x11-csd-move");

   /* Bottom right, which is the right-hand screen with two heads: a drag
    * that loses its starting point falls back to 0,0 and has to show. */
   win = xtk_csd_window_new(dpy,
                            DisplayWidth(dpy, DefaultScreen(dpy)) - W - 150,
                            DisplayHeight(dpy, DefaultScreen(dpy)) - H - 150,
                            W, H, "csd move under test");
   xtk_geometry_get(dpy, win, &x0, &y0, NULL, NULL);

   /* On the header bar. */
   px = x0 + (W / 2);
   py = y0 + 10;
   xtk_pointer_move_to(dpy, px, py);
   xtk_settle(dpy);
   xtk_csd_drag_begin(dpy, win, px, py, NET_WM_MOVERESIZE_MOVE);
   xtk_pointer_drag(dpy, px, py, DX, DY);
   xtk_settle(dpy);
   xtk_csd_drag_end(dpy);
   xtk_geometry_get(dpy, win, &x1, &y1, NULL, NULL);

   if ((x1 != x0 + DX) || (y1 != y0 + DY))
     xtk_fail("a %d,%d drag from the header bar moved the window from "
              "+%d+%d to +%d+%d, expected +%d+%d", DX, DY, x0, y0, x1, y1,
              x0 + DX, y0 + DY);

   printf("test-x11-csd-move: ok (+%d+%d -> +%d+%d)\n", x0, y0, x1, y1);
   XCloseDisplay(dpy);
   return 0;
}
