/* X clients of E's own Xwayland. One scenario per run, named by the only
 * argument:
 *
 *   close      The client closes its X connection, as an application does
 *              when it quits. E has to find the window it manages from the
 *              X window id in the DestroyNotify, and delete it.
 *   restore    The window is minimised and restored. E unmaps the frame of
 *              a minimised window, and Xwayland gives it a new wl_surface
 *              when it is mapped again: E has to move the window to it.
 *   early-map  A window mapped before E manages the display. E adopts it
 *              and reparents it into a frame, and Xwayland then gives the
 *              frame a wl_surface of its own, after the one it gave the
 *              window: E has to move the window to the newer surface.
 *              Skipped when E manages the display before the test can map
 *              the window.
 *
 * Every scenario ends with the client closing, so each also checks that E
 * still finds the window from its X window id.
 *
 * The X display is the one E opened for its Xwayland: the lock file of an
 * X display holds the pid of the server, and E writes its own pid there. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <Ecore_X.h>

#include "e_wl_testkit.h"

#define PROG   "test-xwayland"
#define APP_ID "E.test.xwayland"
#define TITLE  "xwayland under test"
#define W 160
#define H 120
#define MAX_DISPLAYS 64

static double
_now(void)
{
   struct timespec ts;

   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + (ts.tv_nsec / 1e9);
}

static int
_lock_owner(int disp)
{
   char path[64], buf[16];
   FILE *f;
   int pid = -1;

   snprintf(path, sizeof(path), "/tmp/.X%d-lock", disp);
   f = fopen(path, "r");
   if (!f) return -1;
   if (fgets(buf, sizeof(buf), f)) pid = atoi(buf);
   fclose(f);
   return pid;
}

/* The lock is taken when E loads the xwayland module, which may be after
 * its Wayland socket is up. E locks a display before it binds its sockets
 * and drops the lock again when another server has them, so a lock only
 * counts once E has gone back to its main loop and still holds it. */
static int
_xwayland_display_find(Tk *tk, int e_pid)
{
   double deadline = _now() + TK_WAIT_SECONDS;
   int i;

   for (;;)
     {
        for (i = 0; i < MAX_DISPLAYS; i++)
          {
             if (_lock_owner(i) != e_pid) continue;
             tk_sync(tk);
             if (_lock_owner(i) == e_pid) return i;
          }
        if (_now() > deadline) break;
        tk_sync(tk);
     }
   tk_fail(tk, "no X display locked by the compositor (pid %d) after %ds",
           e_pid, TK_WAIT_SECONDS);
}

/* E sets _NET_SUPPORTING_WM_CHECK once it redirects the root's children. */
static Eina_Bool
_x_window_managed(void)
{
   Ecore_X_Window check;

   return ecore_x_window_prop_window_get(ecore_x_window_root_first_get(),
                                         ECORE_X_ATOM_NET_SUPPORTING_WM_CHECK,
                                         &check, 1) == 1;
}

static Ecore_X_Window
_x_window_show(void)
{
   Ecore_X_Window win;

   win = ecore_x_window_new(0, 0, 0, W, H);
   ecore_x_icccm_name_class_set(win, "xwayland", APP_ID);
   ecore_x_icccm_title_set(win, TITLE);
   ecore_x_window_background_color_set(win, 0xffff, 0x8000, 0);
   ecore_x_window_show(win);
   ecore_x_sync();
   return win;
}

static void
_x_window_wait_shown(Tk *tk)
{
   double deadline = _now() + TK_WAIT_SECONDS;

   /* Before Xwayland names the window's wl_surface, the X window is not a
    * window E shows: visible means E took it over through that surface. */
   while (!tk_find(tk, APP_ID))
     {
        if (_now() > deadline)
          tk_fail(tk, "the X window never became a window of the compositor");
        tk_sync(tk);
     }
   tk_wait_states(tk, APP_ID, WL_TEST_CLIENT_STATE_VISIBLE,
                  WL_TEST_CLIENT_STATE_VISIBLE, "X window shown");
}

static void
_x_window_manager_wait(Tk *tk)
{
   double deadline = _now() + TK_WAIT_SECONDS;

   while (!_x_window_managed())
     {
        if (_now() > deadline)
          tk_fail(tk, "the compositor did not manage its Xwayland display "
                      "after %ds", TK_WAIT_SECONDS);
        tk_sync(tk);
     }
}

static void
_x_close_wait_gone(Tk *tk)
{
   double deadline;

   ecore_x_shutdown();
   deadline = _now() + TK_WAIT_SECONDS;
   while (tk_find(tk, APP_ID))
     {
        if (_now() > deadline)
          tk_fail(tk, "the window is still listed %ds after its X client "
                      "closed its connection", TK_WAIT_SECONDS);
        tk_sync(tk);
     }
}

static void
_x_window_map_managed(Tk *tk)
{
   _x_window_manager_wait(tk);
   _x_window_show();
   _x_window_wait_shown(tk);
}

static void
_restore(Tk *tk)
{
   const unsigned int mask = WL_TEST_CLIENT_STATE_VISIBLE |
                             WL_TEST_CLIENT_STATE_ICONIFIED;
   Tk_Client *c;
   int i;

   _x_window_map_managed(tk);
   /* Twice: the second time, the window is one E already moved. */
   for (i = 0; i < 2; i++)
     {
        tk_action(tk, tk_expect(tk, APP_ID)->id, "window_iconic_toggle", NULL);
        tk_wait_states(tk, APP_ID, mask, WL_TEST_CLIENT_STATE_ICONIFIED,
                       "minimised");
        tk_action(tk, tk_expect(tk, APP_ID)->id, "window_iconic_toggle", NULL);
        c = tk_wait_states(tk, APP_ID, mask, WL_TEST_CLIENT_STATE_VISIBLE,
                           "restored");
        if (strcmp(c->title, TITLE))
          tk_fail(tk, "restored window has title '%s', expected '%s'",
                  c->title, TITLE);
     }
}

static void
_early_map(Tk *tk)
{
   Ecore_X_Window win, frame;
   double deadline;

   /* Xwayland was started by this connection, and E becomes its window
    * manager only once it is up. The grab keeps E from doing so before
    * the window is mapped. */
   ecore_x_grab();
   if (_x_window_managed())
     {
        printf(PROG ": early-map skipped: the compositor manages its Xwayland "
                    "display already\n");
        ecore_x_shutdown();
        tk_disconnect(tk);
        exit(77);
     }
   win = _x_window_show();
   ecore_x_ungrab();
   ecore_x_sync();

   /* Xwayland names the frame's surface when E maps the frame. */
   deadline = _now() + TK_WAIT_SECONDS;
   for (;;)
     {
        frame = ecore_x_window_parent_get(win);
        if ((frame != ecore_x_window_root_first_get()) &&
            ecore_x_window_visible_get(frame))
          break;
        if (_now() > deadline)
          tk_fail(tk, "the compositor did not map a frame around the window "
                      "after %ds", TK_WAIT_SECONDS);
        tk_sync(tk);
     }
   tk_settle(tk);
   _x_window_wait_shown(tk);
}

int
main(int argc, char **argv)
{
   Tk *tk;
   const char *e_pid;
   char disp[16];

   if (argc != 2)
     {
        fprintf(stderr, "usage: %s close|restore|early-map\n", argv[0]);
        return 2;
     }

   tk = tk_connect(PROG);

   e_pid = getenv("E_TEST_PID");
   if (!e_pid) tk_fail(tk, "E_TEST_PID is not set; run this under run-nested.sh");
   snprintf(disp, sizeof(disp), ":%d", _xwayland_display_find(tk, atoi(e_pid)));
   if (!ecore_x_init(disp))
     tk_fail(tk, "cannot connect to the compositor's Xwayland on %s", disp);

   if (!strcmp(argv[1], "close"))
     _x_window_map_managed(tk);
   else if (!strcmp(argv[1], "restore"))
     _restore(tk);
   else if (!strcmp(argv[1], "early-map"))
     _early_map(tk);
   else
     tk_fail(tk, "unknown scenario '%s'", argv[1]);
   _x_close_wait_gone(tk);

   printf(PROG ": %s ok (display %s)\n", argv[1], disp);
   tk_disconnect(tk);
   return 0;
}
