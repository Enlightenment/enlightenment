/* A real browser, as browser-run.sh started it: its window has to survive
 * what a user does to it. Browsers draw their own decorations with a shadow
 * outside the window, so geometry is checked against where the window
 * started, not against the screen.
 *
 *   argv[1]  part of the browser's app_id
 *   argv[2]  the title of the page it was given */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e_wl_testkit.h"

#define PROG "test-browser"
#define MAX (WL_TEST_CLIENT_STATE_MAXIMIZED_H | WL_TEST_CLIENT_STATE_MAXIMIZED_V)
#define SHOWN (WL_TEST_CLIENT_STATE_ICONIFIED | WL_TEST_CLIENT_STATE_VISIBLE)

typedef struct
{
   int x, y, w, h;
   unsigned int output;
} Geometry;

static int
_at(const Tk_Client *c, void *data)
{
   const Geometry *g = data;

   return (c->x == g->x) && (c->y == g->y) && (c->w == g->w) && (c->h == g->h) &&
          (c->output == g->output);
}

static int
_bigger(const Tk_Client *c, void *data)
{
   const Geometry *g = data;

   return (c->states & MAX) && ((c->w > g->w) || (c->h > g->h));
}

static int
_titled(const Tk_Client *c, void *data)
{
   return !!strstr(c->title, data);
}

int
main(int argc, char **argv)
{
   char app_id[256];
   Geometry start, g;
   unsigned int id;
   Tk_Client *c;
   Tk *tk;

   if (argc < 3) return 2;
   tk = tk_connect(PROG);
   c = tk_wait_window(tk, argv[1]);
   id = c->id;
   snprintf(app_id, sizeof(app_id), "%s", c->app_id);
   tk_wait(tk, app_id, _titled, argv[2], "the browser to show the page");

   /* Firefox opens maximized, Chromium does not. */
   if (tk_expect(tk, app_id)->states & MAX)
     {
        tk_action(tk, id, "window_maximized_toggle", NULL);
        tk_wait_states(tk, app_id, MAX, 0, "unmaximized to start");
     }
   c = tk_expect(tk, app_id);
   start = (Geometry){ c->x, c->y, c->w, c->h, c->output };
   printf(PROG ": '%s' at %dx%d+%d+%d\n", app_id, start.w, start.h, start.x, start.y);

   tk_action(tk, id, "window_maximized_toggle", NULL);
   tk_wait(tk, app_id, _bigger, &start, "maximized");
   tk_action(tk, id, "window_maximized_toggle", NULL);
   tk_wait(tk, app_id, _at, &start, "unmaximized back where it was");

   g = start;
   g.x += 50;
   g.y += 40;
   tk_action(tk, id, "window_move_by", "50 40");
   tk_wait(tk, app_id, _at, &g, "moved by 50,40 at the same size");
   tk_action(tk, id, "window_move_by", "-50 -40");
   tk_wait(tk, app_id, _at, &start, "moved back");

   g = start;
   g.w += 120;
   g.h += 90;
   tk_action(tk, id, "window_resize_by", "120 90");
   tk_wait(tk, app_id, _at, &g, "resized by 120,90");
   tk_action(tk, id, "window_resize_by", "-120 -90");
   tk_wait(tk, app_id, _at, &start, "resized back");

   tk_action(tk, id, "window_iconic_toggle", NULL);
   tk_wait_states(tk, app_id, SHOWN, WL_TEST_CLIENT_STATE_ICONIFIED, "iconified");
   tk_action(tk, id, "window_iconic_toggle", NULL);
   tk_wait_states(tk, app_id, SHOWN, WL_TEST_CLIENT_STATE_VISIBLE, "restored");
   tk_wait(tk, app_id, _at, &start, "restored where it was");

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
