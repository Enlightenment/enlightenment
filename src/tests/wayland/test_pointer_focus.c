/* Pointer focus: what wl_pointer says when the window under it changes
 * without the pointer moving.
 *
 * xdg_toplevel.move and .resize: the surface "will lose the focus of the
 * device (wl_pointer, wl_touch, etc) used for the move", so a granted grab
 * sends wl_pointer.leave. E gives the focus back when the grab ends, also
 * when the pointer has not moved since, and the enter that does so carries
 * the pointer's real surface-local position.
 *
 * A window unmapped or destroyed under the pointer is left, so the window
 * that comes under it next is entered.
 *
 * Throughout: no motion without an enter before it, and no second enter
 * without a leave between. */
#include <stdio.h>
#include <string.h>
#include <linux/input-event-codes.h>

#include "e_wl_testkit.h"

#define PROG "test-pointer-focus"
#define APP_ID "e.test.pointer_focus"
#define W 200
#define H 150
#define PX 60
#define PY 40

typedef struct
{
   Tk *tk;
   int in;
   struct wl_surface *entered;
   int enters, leaves, motions;
   double ex, ey;
   uint32_t button_serial;
} Ptr_Watch;

static void
_enter(void *data, struct wl_pointer *p, uint32_t serial,
       struct wl_surface *s, wl_fixed_t x, wl_fixed_t y)
{
   Ptr_Watch *w = data;

   (void)p; (void)serial;
   if (w->in) tk_fail(w->tk, "a second wl_pointer.enter with no leave between");
   w->in = 1;
   w->entered = s;
   w->enters++;
   w->ex = wl_fixed_to_double(x);
   w->ey = wl_fixed_to_double(y);
}

static void
_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s)
{
   Ptr_Watch *w = data;

   (void)p; (void)serial; (void)s;
   if (!w->in) tk_fail(w->tk, "wl_pointer.leave for a surface never entered");
   w->in = 0;
   w->leaves++;
}

static void
_motion(void *data, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y)
{
   Ptr_Watch *w = data;

   (void)p; (void)t; (void)x; (void)y;
   if (!w->in) tk_fail(w->tk, "wl_pointer.motion with no enter before it");
   w->motions++;
}

static void
_button(void *data, struct wl_pointer *p, uint32_t serial, uint32_t t,
        uint32_t button, uint32_t state)
{
   Ptr_Watch *w = data;

   (void)p; (void)t; (void)button;
   if (state == WL_POINTER_BUTTON_STATE_PRESSED) w->button_serial = serial;
}

static void
_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v)
{
   (void)d; (void)p; (void)t; (void)a; (void)v;
}

static void
_frame(void *d, struct wl_pointer *p)
{
   (void)d; (void)p;
}

static void
_axis_source(void *d, struct wl_pointer *p, uint32_t s)
{
   (void)d; (void)p; (void)s;
}

static void
_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a)
{
   (void)d; (void)p; (void)t; (void)a;
}

static void
_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v)
{
   (void)d; (void)p; (void)a; (void)v;
}

static const struct wl_pointer_listener _pointer_listener =
{
   _enter, _leave, _motion, _button, _axis,
   _frame, _axis_source, _axis_stop, _axis_discrete
};

static void
_grab(Tk *tk, Ptr_Watch *w, struct wl_seat *seat, Tk_Toplevel *top,
      int resize, const char *what)
{
   int leaves = w->leaves, enters = w->enters;

   tk_pointer_button(tk, BTN_LEFT, 1);
   if (!w->button_serial) tk_fail(tk, "%s: no button press reached the window", what);
   if (resize)
     xdg_toplevel_resize(tk_toplevel_xdg(top), seat, w->button_serial,
                         XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT);
   else
     xdg_toplevel_move(tk_toplevel_xdg(top), seat, w->button_serial);
   tk_settle(tk);
   if (w->leaves != leaves + 1)
     tk_fail(tk, "%s: the grab did not take the pointer focus (%d leaves)",
             what, w->leaves - leaves);

   tk_pointer_button(tk, BTN_LEFT, 0);
   tk_settle(tk);
   if (w->enters != enters + 1)
     tk_fail(tk, "%s: the focus did not come back when the grab ended, with "
                 "the pointer still (%d enters)", what, w->enters - enters);
   if (((int)w->ex != PX) || ((int)w->ey != PY))
     tk_fail(tk, "%s: the enter after the grab says %g,%g, the pointer is at "
                 "%d,%d", what, w->ex, w->ey, PX, PY);

   tk_pointer_move(tk, 1, 0);
   tk_pointer_move(tk, -1, 0);
   w->button_serial = 0;
}

/* Put the pointer on a window, take the window away by unmapping or
 * destroying it, and put the pointer on another window of the same
 * client. The first must be left and the second entered. */
static void
_replace(Tk *tk, Ptr_Watch *w, const char *app_id, int unmap,
         const char *what)
{
   Tk_Toplevel *top, *next;
   Tk_Client *c;
   char next_id[64];
   int leaves;

   top = tk_toplevel_new(tk, app_id, what, W, H);
   c = tk_expect(tk, app_id);
   tk_pointer_warp(tk, c->x + PX, c->y + PY);
   if (!w->in) tk_fail(tk, "%s: the window was not entered", what);
   leaves = w->leaves;

   if (unmap)
     {
        wl_surface_attach(tk_toplevel_surface(top), NULL, 0, 0);
        wl_surface_commit(tk_toplevel_surface(top));
        tk_settle(tk);
     }
   else
     tk_toplevel_del(top);
   if (w->leaves != leaves + 1)
     tk_fail(tk, "%s: the window under the pointer went with no leave", what);

   snprintf(next_id, sizeof(next_id), "%s.next", app_id);
   next = tk_toplevel_new(tk, next_id, what, W, H);
   c = tk_expect(tk, next_id);
   tk_pointer_warp(tk, c->x + PX, c->y + PY);
   if ((!w->in) || (w->entered != tk_toplevel_surface(next)))
     tk_fail(tk, "%s: the next window under the pointer was not entered",
             what);
   tk_toplevel_del(next);
}

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *top;
   Tk_Client *c;
   Ptr_Watch w;
   struct wl_seat *seat;
   struct wl_pointer *pointer;

   memset(&w, 0, sizeof(w));
   tk = tk_connect(PROG);
   w.tk = tk;

   if (tk_global_version(tk, "wl_seat") < 5)
     tk_fail(tk, "wl_seat below version 5");
   seat = tk_bind(tk, &wl_seat_interface, 5);
   pointer = wl_seat_get_pointer(seat);
   wl_pointer_add_listener(pointer, &_pointer_listener, &w);

   top = tk_toplevel_new(tk, APP_ID, "pointer focus", W, H);
   c = tk_expect(tk, APP_ID);

   tk_pointer_warp(tk, c->x + PX, c->y + PY);
   if (!w.in) tk_fail(tk, "no enter when the pointer was put on the window");
   if (((int)w.ex != PX) || ((int)w.ey != PY))
     tk_fail(tk, "the first enter says %g,%g, expected %d,%d",
             w.ex, w.ey, PX, PY);

   _grab(tk, &w, seat, top, 0, "move");
   _grab(tk, &w, seat, top, 1, "resize");

   tk_toplevel_del(top);
   _replace(tk, &w, APP_ID ".destroyed", 0, "destroyed");
   _replace(tk, &w, APP_ID ".unmapped", 1, "unmapped");

   printf(PROG ": ok (%d enters, %d leaves, %d motions)\n",
          w.enters, w.leaves, w.motions);

   tk_disconnect(tk);
   return 0;
}
