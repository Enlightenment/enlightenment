/* Helpers shared by the X11 tests: each test is a single program, so these
 * are static and live in the header.
 *
 * The window under test is an elementary window with its own decorations,
 * which EFL draws on X11 when EFL_WIN_FRAME_MODE=on. A press on them hands
 * the drag to the window manager with _NET_WM_MOVERESIZE, as GTK does, so
 * the request E gets is the one a real CSD client sends.
 *
 * The pointer is moved and pressed with ecore_x_test_fake_mouse_*(): the
 * press, the motion and the release have to come from the server for E to
 * see them under its grab. */
#ifndef X11_TESTKIT_H
#define X11_TESTKIT_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <Elementary.h>
#include <Ecore_X.h>

static const char *xtk_name = "x11-test";

/* What the next press in the content starts, nothing when 0. */
static Elm_Win_Move_Resize_Mode xtk_press_mode = 0;
static Eina_Bool xtk_press_started = EINA_FALSE;

static inline void
xtk_fail(const char *fmt, ...)
{
   va_list ap;

   fprintf(stderr, "%s: FAIL: ", xtk_name);
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fputc('\n', stderr);
   exit(1);
}

/* Runs the main loop for a while: E and the window both have to handle
 * what was just sent, and X has no round trip that waits for the window
 * manager. */
static inline void
xtk_settle(double seconds)
{
   double end = ecore_time_get() + seconds;

   ecore_x_sync();
   while (ecore_time_get() < end)
     {
        ecore_main_loop_iterate();
        usleep(10 * 1000);
     }
}

static inline void
xtk_init(int argc, char **argv, const char *name)
{
   xtk_name = name;
   /* Decorations drawn by the client on X11 as well, not only on Wayland. */
   setenv("EFL_WIN_FRAME_MODE", "on", 1);
   setenv("ELM_ENGINE", "software_x11", 1);
   elm_init(argc, argv);
   /* elementary opens the display with its first window, too late here. */
   if (!ecore_x_init(NULL)) xtk_fail("cannot open the display");
}

/* Where the X window is on the root window: E reparents it into its own
 * frame, so the position is the sum down from the root. */
static inline void
xtk_geometry_get(Evas_Object *win, int *x, int *y, int *w, int *h)
{
   Ecore_X_Window xwin = elm_win_xwindow_get(win), root, p;
   int px, py;

   ecore_x_sync();
   root = ecore_x_window_root_get(xwin);
   ecore_x_window_geometry_get(xwin, x, y, w, h);
   for (p = ecore_x_window_parent_get(xwin); p && (p != root);
        p = ecore_x_window_parent_get(p))
     {
        ecore_x_window_geometry_get(p, &px, &py, NULL, NULL);
        *x += px;
        *y += py;
     }
}

static inline void
xtk_pointer_move_to(int x, int y)
{
   if (!ecore_x_test_fake_mouse_move(x, y))
     xtk_fail("cannot move the pointer: XTest missing from ecore_x or X");
   xtk_settle(0.05);
}

/* Moves the pointer by dx,dy in steps, as a hand would. */
static inline void
xtk_pointer_drag(int x, int y, int dx, int dy)
{
   int i;

   for (i = 1; i <= 10; i++)
     xtk_pointer_move_to(x + (i * dx / 10), y + (i * dy / 10));
}

/* Where a press lands in the window's content, x and y in from its
 * top-left corner, or from its bottom-right one when negative: the
 * decorations the window draws itself surround the content. */
static inline void
xtk_content_point(Evas_Object *win, int x, int y, int *px, int *py)
{
   Evas_Object *bg = evas_object_data_get(win, "xtk_bg");
   int wx, wy, fx, fy, cx, cy, cw, ch;

   xtk_geometry_get(win, &wx, &wy, NULL, NULL);
   /* Canvas coordinates start inside the decorations. */
   evas_output_framespace_get(evas_object_evas_get(win), &fx, &fy, NULL, NULL);
   evas_object_geometry_get(bg, &cx, &cy, &cw, &ch);
   cx += fx;
   cy += fy;
   *px = wx + (x >= 0 ? cx + x : cx + cw + x);
   *py = wy + (y >= 0 ? cy + y : cy + ch + y);
}

/* The middle of the title bar the window draws above its content. */
static inline void
xtk_title_point(Evas_Object *win, int *px, int *py)
{
   Evas *e = evas_object_evas_get(win);
   int wx, wy, fy, ww;

   xtk_geometry_get(win, &wx, &wy, &ww, NULL);
   /* The title bar is the top of the space around the canvas, below the
    * shadow: lower than its middle rather than higher. */
   evas_output_framespace_get(e, NULL, &fy, NULL, NULL);
   *px = wx + (ww / 2);
   *py = wy + (fy / 2) + 4;
}

static inline void
_xtk_press_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   if (xtk_press_mode)
     xtk_press_started = elm_win_move_resize_start(data, xtk_press_mode);
}

/* A window that draws its own decorations, at x,y with content w by h,
 * mapped where it asked to be. */
static inline Evas_Object *
xtk_csd_window_new(int x, int y, int w, int h, const char *title)
{
   Evas_Object *win, *bg;
   Ecore_X_Window xwin;

   win = elm_win_add(NULL, title, ELM_WIN_BASIC);
   elm_win_title_set(win, title);
   elm_win_autodel_set(win, EINA_FALSE);

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_event_callback_add(bg, EVAS_CALLBACK_MOUSE_DOWN,
                                  _xtk_press_cb, win);
   evas_object_data_set(win, "xtk_bg", bg);
   evas_object_show(bg);

   evas_object_resize(win, w, h);
   evas_object_move(win, x, y);

   /* The position is the user's, as a session restore would give it: E
    * places it there instead of choosing one. */
   xwin = elm_win_xwindow_get(win);
   ecore_x_icccm_size_pos_hints_set(xwin, EINA_TRUE,
                                    ECORE_X_GRAVITY_NW, 0, 0, 0, 0,
                                    0, 0, 1, 1, 0.0, 0.0);
   evas_object_show(win);
   xtk_settle(1.0);
   if (!ecore_x_window_visible_get(xwin))
     xtk_fail("the window was never mapped");
   return win;
}

static inline void
xtk_button_press(void)
{
   ecore_x_test_fake_mouse_down(1);
   xtk_settle(0.3);
}

/* A press on the title bar: the window's own decorations hand the move to
 * the window manager, as a header bar does. The pointer must already be on
 * the title bar. */
static inline void
xtk_csd_move_begin(void)
{
   xtk_press_mode = 0;
   xtk_button_press();
}

/* A press in the content that the window turns into a resize from the
 * given edges, with the call its own decorations make from their border.
 * The pointer must already be over the content. */
static inline void
xtk_csd_resize_begin(Elm_Win_Move_Resize_Mode mode)
{
   xtk_press_mode = mode;
   xtk_press_started = EINA_FALSE;
   xtk_button_press();
   if (!xtk_press_started)
     xtk_fail("the press did not start a resize");
}

/* E moves and resizes only the composited frame during a drag; the X
 * window follows when the button is released. */
static inline void
xtk_csd_drag_end(void)
{
   ecore_x_test_fake_mouse_up(1);
   xtk_settle(0.3);
   /* The window never sees that release, E took it: it learns the drag is
    * over when the pointer comes back in, and refuses the next one until
    * then. */
   xtk_pointer_move_to(0, 0);
   xtk_settle(0.1);
}

static inline void
xtk_shutdown(Evas_Object *win)
{
   evas_object_del(win);
   ecore_x_shutdown();
   elm_shutdown();
}

#endif
