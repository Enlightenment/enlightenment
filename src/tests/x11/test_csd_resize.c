/* Resize a client-side-decorated window with _NET_WM_MOVERESIZE, the way
 * GTK starts a resize from the shadow around its window, from the
 * bottom-right corner and from the top-left one. The dragged corner must
 * follow the pointer and the opposite corner must stay where it was. */
#include "x11_testkit.h"

#define W 300
#define H 200
#define DX 60
#define DY 40

static void
corner_drag(Display *dpy, Window win, int direction, const char *corner)
{
   int x0, y0, w0, h0, x1, y1, w1, h1, px, py, ex, ey, ew, eh;

   xtk_geometry_get(dpy, win, &x0, &y0, &w0, &h0);
   if (direction == NET_WM_MOVERESIZE_SIZE_TOPLEFT)
     {
        px = x0 + 2;
        py = y0 + 2;
        ex = x0 + DX;
        ey = y0 + DY;
        ew = w0 - DX;
        eh = h0 - DY;
     }
   else
     {
        px = x0 + w0 - 2;
        py = y0 + h0 - 2;
        ex = x0;
        ey = y0;
        ew = w0 + DX;
        eh = h0 + DY;
     }

   xtk_pointer_move_to(dpy, px, py);
   xtk_settle(dpy);
   xtk_csd_drag_begin(dpy, win, px, py, direction);
   xtk_pointer_drag(dpy, px, py, DX, DY);
   xtk_settle(dpy);
   xtk_csd_drag_end(dpy);
   xtk_geometry_get(dpy, win, &x1, &y1, &w1, &h1);

   if ((x1 != ex) || (y1 != ey) || (w1 != ew) || (h1 != eh))
     xtk_fail("a %d,%d drag of the %s corner made %dx%d+%d+%d into "
              "%dx%d+%d+%d, expected %dx%d+%d+%d", DX, DY, corner,
              w0, h0, x0, y0, w1, h1, x1, y1, ew, eh, ex, ey);
   printf("test-x11-csd-resize: %s ok (%dx%d+%d+%d -> %dx%d+%d+%d)\n",
          corner, w0, h0, x0, y0, w1, h1, x1, y1);
}

int
main(void)
{
   Display *dpy;
   Window win;

   dpy = xtk_connect("test-x11-csd-resize");

   /* Bottom right, which is the right-hand screen with two heads: a drag
    * that loses its starting point falls back to 0,0 and has to show. */
   win = xtk_csd_window_new(dpy,
                            DisplayWidth(dpy, DefaultScreen(dpy)) - W - 200,
                            DisplayHeight(dpy, DefaultScreen(dpy)) - H - 200,
                            W, H, "csd resize under test");

   corner_drag(dpy, win, NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT, "bottom-right");
   corner_drag(dpy, win, NET_WM_MOVERESIZE_SIZE_TOPLEFT, "top-left");

   XCloseDisplay(dpy);
   return 0;
}
