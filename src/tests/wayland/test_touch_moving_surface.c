/* Touch points and the surface under them.
 *
 * A finger that stays still while the surface under it moves is now over
 * a different part of the surface, and only the compositor knows: it has
 * to send a wl_touch.motion with the new surface-local position.
 *
 * A finger still down on a surface the client destroys has to be lifted:
 * nothing else will ever end that touch. */
#include <stdio.h>
#include <string.h>

#include "e_wl_testkit.h"

#define PROG "test-touch-moving-surface"
#define APP_ID "e.test.touch_moving_surface"
#define TX 50
#define TY 40
#define DX 30
#define DY 20

typedef struct
{
   int downs, ups, motions;
   double x, y;
} Touch_Watch;

static void
_down(void *data, struct wl_touch *t, uint32_t serial, uint32_t time,
      struct wl_surface *s, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
   Touch_Watch *w = data;

   (void)t; (void)serial; (void)time; (void)s; (void)id;
   w->downs++;
   w->x = wl_fixed_to_double(x);
   w->y = wl_fixed_to_double(y);
}

static void
_up(void *data, struct wl_touch *t, uint32_t serial, uint32_t time, int32_t id)
{
   Touch_Watch *w = data;

   (void)t; (void)serial; (void)time; (void)id;
   w->ups++;
}

static void
_motion(void *data, struct wl_touch *t, uint32_t time, int32_t id,
        wl_fixed_t x, wl_fixed_t y)
{
   Touch_Watch *w = data;

   (void)t; (void)time; (void)id;
   w->motions++;
   w->x = wl_fixed_to_double(x);
   w->y = wl_fixed_to_double(y);
}

static void
_frame(void *d, struct wl_touch *t)
{
   (void)d; (void)t;
}

static void
_cancel(void *d, struct wl_touch *t)
{
   (void)d; (void)t;
}

static void
_shape(void *d, struct wl_touch *t, int32_t id, wl_fixed_t ma, wl_fixed_t mi)
{
   (void)d; (void)t; (void)id; (void)ma; (void)mi;
}

static void
_orientation(void *d, struct wl_touch *t, int32_t id, wl_fixed_t o)
{
   (void)d; (void)t; (void)id; (void)o;
}

static const struct wl_touch_listener _touch_listener =
{
   _down, _up, _motion, _frame, _cancel, _shape, _orientation
};

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *c;
   Touch_Watch w;
   struct wl_seat *seat;
   struct wl_touch *touch;
   char by[32];

   memset(&w, 0, sizeof(w));
   tk = tk_connect(PROG);
   seat = tk_bind(tk, &wl_seat_interface, 5);
   if (!seat) tk_fail(tk, "no wl_seat");
   touch = wl_seat_get_touch(seat);
   wl_touch_add_listener(touch, &_touch_listener, &w);

   top = tk_toplevel_new(tk, APP_ID, PROG, 300, 200);
   c = tk_expect(tk, APP_ID);

   tk_touch_down(tk, 1, c->x + TX, c->y + TY);
   if (w.downs != 1) tk_fail(tk, "the touch down did not reach the window");

   snprintf(by, sizeof(by), "%d %d", DX, DY);
   tk_action(tk, c->id, "window_move_by", by);
   if (!w.motions)
     tk_fail(tk, "the window moved under a still finger and no "
                 "wl_touch.motion said so");
   if (((int)w.x != TX - DX) || ((int)w.y != TY - DY))
     tk_fail(tk, "the finger is reported at %g,%g, it is at %d,%d of the "
                 "moved window", w.x, w.y, TX - DX, TY - DY);

   tk_touch_up(tk, 1);
   if (w.ups != 1) tk_fail(tk, "lifting the finger sent %d ups", w.ups);

   c = tk_expect(tk, APP_ID);
   tk_touch_down(tk, 2, c->x + TX, c->y + TY);
   if (w.downs != 2) tk_fail(tk, "the second touch down did not arrive");
   tk_toplevel_del(top);
   if (w.ups != 2)
     tk_fail(tk, "the surface under a finger was destroyed and the touch was "
                 "never ended");
   tk_touch_up(tk, 2);

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
