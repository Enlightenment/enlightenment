/* Minimise a window, as a client asks with ICCCM's WM_CHANGE_STATE: it
 * must end up Iconic and gone from the screen, whether it is asked once
 * the window is shown or while E is still showing it. What is on the
 * screen is read back from the root window, where E composites, with each
 * window filled with a colour nothing else on the screen has.
 *
 * Getting the window back is not tested here: elementary asks for it with
 * _NET_ACTIVE_WINDOW, which the test profile's activation policy ignores,
 * and E keeps an iconified window mapped, so mapping it again does not ask
 * either. */
#include "x11_testkit.h"

#define W 300
#define H 200
#define MARK 0xff00ff

static unsigned int
_screen_pixel(int x, int y)
{
   Ecore_X_Display *disp = ecore_x_display_get();
   Ecore_X_Screen *screen = ecore_x_default_screen_get();
   Ecore_X_Image *im;
   unsigned int *data, pixel;
   int bpl, rows, bpp;

   ecore_x_sync();
   im = ecore_x_image_new(1, 1, ecore_x_default_visual_get(disp, screen),
                          ecore_x_default_depth_get(disp, screen));
   if (!im) xtk_fail("cannot make an image to read the screen into");
   if (!ecore_x_image_get(im, ecore_x_window_root_first_get(), x, y,
                          0, 0, 1, 1))
     xtk_fail("cannot read the screen at %d,%d", x, y);
   data = ecore_x_image_data_get(im, &bpl, &rows, &bpp);
   pixel = data[0] & 0xffffff;
   ecore_x_image_free(im);
   return pixel;
}

/* A window filled with MARK at x,y, not shown yet. */
static Evas_Object *
_marked_window_add(int x, int y, const char *title)
{
   Evas_Object *win, *mark;

   win = elm_win_add(NULL, title, ELM_WIN_BASIC);
   elm_win_title_set(win, title);
   elm_win_autodel_set(win, EINA_FALSE);
   mark = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_color_set(mark, 255, 0, 255, 255);
   evas_object_size_hint_weight_set(mark, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, mark);
   evas_object_data_set(win, "xtk_bg", mark);
   evas_object_show(mark);
   evas_object_resize(win, W, H);
   evas_object_move(win, x, y);
   /* Placed where it asks, as a session restore would have it. */
   ecore_x_icccm_size_pos_hints_set(elm_win_xwindow_get(win), EINA_TRUE,
                                    ECORE_X_GRAVITY_NW, 0, 0, 0, 0,
                                    0, 0, 1, 1, 0.0, 0.0);
   return win;
}

/* Shows a new window and minimises it as soon as E starts drawing it,
 * while the theme's show animation runs: meson stretches the theme's
 * animations for this test, so that the request lands in it. x,y is
 * where to look for it on the screen. */
static Evas_Object *
_iconify_while_showing(int *x, int *y)
{
   Evas_Object *win = _marked_window_add(450, 50, "iconify while showing");
   unsigned int before;

   evas_object_show(win);
   while (!ecore_x_window_visible_get(elm_win_xwindow_get(win)))
     ecore_main_loop_iterate();
   xtk_content_point(win, W / 2, H / 2, x, y);
   before = _screen_pixel(*x, *y);
   while (_screen_pixel(*x, *y) == before)
     {
        if (before == MARK)
          xtk_fail("the window was shown before it could be minimised");
        ecore_main_loop_iterate();
     }
   elm_win_iconified_set(win, EINA_TRUE);
   return win;
}

static void
_expect_iconic(Evas_Object *win, int x, int y, const char *what)
{
   int state = ecore_x_icccm_state_get(elm_win_xwindow_get(win));

   if (state != ECORE_X_WINDOW_STATE_HINT_ICONIC)
     xtk_fail("a window minimised %s has WM_STATE %d, not Iconic", what,
              state);
   if (_screen_pixel(x, y) == MARK)
     xtk_fail("a window minimised %s is still on the screen", what);
}

int
main(int argc, char **argv)
{
   Evas_Object *shown, *showing;
   int px, py, qx, qy;

   xtk_init(argc, argv, "test-x11-iconify");
   shown = _marked_window_add(50, 50, "iconify once shown");
   evas_object_show(shown);
   xtk_settle(1.0);
   xtk_content_point(shown, W / 2, H / 2, &px, &py);
   if (_screen_pixel(px, py) != MARK)
     xtk_fail("the window is not on the screen before minimising: 0x%06x",
              _screen_pixel(px, py));

   elm_win_iconified_set(shown, EINA_TRUE);
   showing = _iconify_while_showing(&qx, &qy);
   xtk_settle(3.0);

   _expect_iconic(shown, px, py, "once shown");
   _expect_iconic(showing, qx, qy, "while being shown");

   printf("test-x11-iconify: ok\n");
   evas_object_del(showing);
   xtk_shutdown(shown);
   return 0;
}
