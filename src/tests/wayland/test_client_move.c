/* Move and resize a window, both through actions and through the grab a
 * user's drag takes. A move must not resize, and a resize must reach the
 * client: the size E reports and the size the client painted must agree. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define APP_ID "e.test.client_move"
#define TITLE  "moves under test"
#define W 320
#define H 240

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *c;
   unsigned int id;
   int x0, y0, w0, h0, cw, ch;

   tk = tk_connect("test-client-move");
   top = tk_toplevel_new(tk, APP_ID, TITLE, W, H);

   c = tk_expect(tk, APP_ID);
   id = c->id;
   x0 = c->x; y0 = c->y; w0 = c->w; h0 = c->h;


   tk_action(tk, id, "window_move_by", "40 30");

   c = tk_expect(tk, APP_ID);
   if ((c->x != x0 + 40) || (c->y != y0 + 30))
     tk_fail(tk, "window_move_by 40 30 put the window at +%d+%d, expected "
                 "+%d+%d", c->x, c->y, x0 + 40, y0 + 30);
   if ((c->w != w0) || (c->h != h0))
     tk_fail(tk, "moving resized the window to %dx%d, was %dx%d",
             c->w, c->h, w0, h0);

   tk_action(tk, id, "window_move_by", "-40 -30");
   c = tk_expect(tk, APP_ID);
   if ((c->x != x0) || (c->y != y0))
     tk_fail(tk, "moving back put the window at +%d+%d, expected +%d+%d",
             c->x, c->y, x0, y0);


   /* The pointer has to be over the window before the grab starts: a move
    * grab drags by the delta from where the pointer was when it began. */
   tk_pointer_warp(tk, c->x + (c->w / 2), c->y + (c->h / 2));
   tk_action(tk, id, "window_move", NULL);
   tk_pointer_move(tk, 60, 45);
   tk_action_end(tk, id, "window_move", NULL);

   c = tk_expect(tk, APP_ID);
   if ((c->x != x0 + 60) || (c->y != y0 + 45))
     tk_fail(tk, "a 60,45 drag put the window at +%d+%d, expected +%d+%d",
             c->x, c->y, x0 + 60, y0 + 45);
   if ((c->w != w0) || (c->h != h0))
     tk_fail(tk, "dragging resized the window to %dx%d, was %dx%d",
             c->w, c->h, w0, h0);

   tk_action(tk, id, "window_move_by", "-60 -45");
   c = tk_expect(tk, APP_ID);
   if ((c->x != x0) || (c->y != y0))
     tk_fail(tk, "moving back after the drag put the window at +%d+%d, "
                 "expected +%d+%d", c->x, c->y, x0, y0);


   tk_action(tk, id, "window_resize_by", "100 80");

   c = tk_expect(tk, APP_ID);
   if ((c->w != w0 + 100) || (c->h != h0 + 80))
     tk_fail(tk, "window_resize_by 100 80 made the frame %dx%d, expected "
                 "%dx%d", c->w, c->h, w0 + 100, h0 + 80);

   tk_toplevel_painted(top, &cw, &ch);
   if ((cw != c->w) || (ch != c->h))
     tk_fail(tk, "resized: E reports %dx%d and the client is painting %dx%d - "
                 "the compositor and the client disagree about the size",
             c->w, c->h, cw, ch);


   /* Bottom-right corner, so e_client_act_resize_begin picks RESIZE_BR and
    * the drag grows the window rather than moving its top-left edge. */
   c = tk_expect(tk, APP_ID);
   x0 = c->x; y0 = c->y; w0 = c->w; h0 = c->h;
   tk_pointer_warp(tk, c->x + c->w - 4, c->y + c->h - 4);
   tk_action(tk, id, "window_resize", NULL);
   tk_pointer_move(tk, 70, 50);
   tk_action_end(tk, id, "window_resize", NULL);

   c = tk_expect(tk, APP_ID);
   if ((c->x != x0) || (c->y != y0))
     tk_fail(tk, "a bottom-right resize moved the window to +%d+%d, was "
                 "+%d+%d - only the far corner should have moved",
             c->x, c->y, x0, y0);
   if ((c->w != w0 + 70) || (c->h != h0 + 50))
     tk_fail(tk, "a 70,50 corner drag made the frame %dx%d, expected %dx%d",
             c->w, c->h, w0 + 70, h0 + 50);

   /* The painted size, not the last configure: E ends an interactive resize
    * with a 0x0 configure meaning "keep what you have". */
   tk_toplevel_painted(top, &cw, &ch);
   if ((cw != c->w) || (ch != c->h))
     tk_fail(tk, "after the drag E reports %dx%d and the client is painting "
                 "%dx%d", c->w, c->h, cw, ch);

   printf("test-client-move: ok (id=%u dragged, moved and resized to "
          "%dx%d+%d+%d)\n", id, c->w, c->h, c->x, c->y);

   tk_disconnect(tk);
   return 0;
}
