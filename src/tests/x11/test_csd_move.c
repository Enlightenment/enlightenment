/* Move a window that draws its own decorations from its title bar: the
 * press hands the drag to E with _NET_WM_MOVERESIZE, as a header bar does.
 * E records where the drag starts from the message itself: the window must
 * follow the pointer by how far the pointer travelled. */
#include "x11_testkit.h"

#define W 300
#define H 200
#define DX 60
#define DY 40

int
main(int argc, char **argv)
{
   Evas_Object *win;
   int x0, y0, x1, y1, px, py, sw, sh;

   xtk_init(argc, argv, "test-x11-csd-move");

   /* Bottom right, which is the right-hand screen with two heads: a drag
    * that loses its starting point falls back to 0,0 and has to show. */
   ecore_x_screen_size_get(ecore_x_default_screen_get(), &sw, &sh);
   win = xtk_csd_window_new(sw - W - 200, sh - H - 200, W, H,
                            "csd move under test");
   xtk_geometry_get(win, &x0, &y0, NULL, NULL);

   xtk_title_point(win, &px, &py);
   xtk_pointer_move_to(px, py);
   xtk_csd_move_begin();
   xtk_pointer_drag(px, py, DX, DY);
   xtk_settle(0.3);
   xtk_csd_drag_end();
   xtk_geometry_get(win, &x1, &y1, NULL, NULL);

   if ((x1 != x0 + DX) || (y1 != y0 + DY))
     xtk_fail("a %d,%d drag moved the window from +%d+%d to +%d+%d, "
              "expected +%d+%d", DX, DY, x0, y0, x1, y1, x0 + DX, y0 + DY);

   printf("test-x11-csd-move: ok (+%d+%d -> +%d+%d)\n", x0, y0, x1, y1);
   xtk_shutdown(win);
   return 0;
}
