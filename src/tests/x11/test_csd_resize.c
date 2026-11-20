/* Resize a window that draws its own decorations, as it does from the
 * border of its frame, from the bottom-right corner and from the top-left
 * one. The dragged corner must follow the pointer and the opposite corner
 * must stay where it was. */
#include "x11_testkit.h"

#define W 300
#define H 200
#define DX 60
#define DY 40

static void
corner_drag(Evas_Object *win, Elm_Win_Move_Resize_Mode mode,
            const char *corner)
{
   int x0, y0, w0, h0, x1, y1, w1, h1, px, py, ex, ey, ew, eh;

   xtk_geometry_get(win, &x0, &y0, &w0, &h0);
   if (mode == (ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_LEFT))
     {
        xtk_content_point(win, 2, 2, &px, &py);
        ex = x0 + DX;
        ey = y0 + DY;
        ew = w0 - DX;
        eh = h0 - DY;
     }
   else
     {
        xtk_content_point(win, -2, -2, &px, &py);
        ex = x0;
        ey = y0;
        ew = w0 + DX;
        eh = h0 + DY;
     }

   xtk_pointer_move_to(px, py);
   xtk_csd_resize_begin(mode);
   xtk_pointer_drag(px, py, DX, DY);
   xtk_settle(0.3);
   xtk_csd_drag_end();
   xtk_geometry_get(win, &x1, &y1, &w1, &h1);

   if ((x1 != ex) || (y1 != ey) || (w1 != ew) || (h1 != eh))
     xtk_fail("a %d,%d drag of the %s corner made %dx%d+%d+%d into "
              "%dx%d+%d+%d, expected %dx%d+%d+%d", DX, DY, corner,
              w0, h0, x0, y0, w1, h1, x1, y1, ew, eh, ex, ey);
   printf("test-x11-csd-resize: %s ok (%dx%d+%d+%d -> %dx%d+%d+%d)\n",
          corner, w0, h0, x0, y0, w1, h1, x1, y1);
}

int
main(int argc, char **argv)
{
   Evas_Object *win;
   int sw, sh;

   xtk_init(argc, argv, "test-x11-csd-resize");

   /* Bottom right, which is the right-hand screen with two heads: a drag
    * that loses its starting point falls back to 0,0 and has to show. */
   ecore_x_screen_size_get(ecore_x_default_screen_get(), &sw, &sh);
   win = xtk_csd_window_new(sw - W - 250, sh - H - 250, W, H,
                            "csd resize under test");

   corner_drag(win, ELM_WIN_MOVE_RESIZE_BOTTOM | ELM_WIN_MOVE_RESIZE_RIGHT,
               "bottom-right");
   corner_drag(win, ELM_WIN_MOVE_RESIZE_TOP | ELM_WIN_MOVE_RESIZE_LEFT,
               "top-left");

   xtk_shutdown(win);
   return 0;
}
