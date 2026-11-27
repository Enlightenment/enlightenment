/* A subsurface takes the pointer only within its input region: elsewhere
 * the pointer is over the surface under it. Firefox gives the subsurface of
 * its page an empty input region before its first buffer, and reads the
 * pointer from its toplevel. */
#include <stdio.h>

#include "e_wl_testkit.h"

#define PROG "test-subsurface-input"

static struct wl_surface *entered;

static void
_enter(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
       uint32_t serial __attribute__((unused)), struct wl_surface *s,
       wl_fixed_t x __attribute__((unused)), wl_fixed_t y __attribute__((unused)))
{
   entered = s;
}

static void
_leave(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
       uint32_t serial __attribute__((unused)), struct wl_surface *s __attribute__((unused)))
{
   entered = NULL;
}

static void
_motion(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
        uint32_t time __attribute__((unused)), wl_fixed_t x __attribute__((unused)),
        wl_fixed_t y __attribute__((unused)))
{
}

static void
_button(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
        uint32_t serial __attribute__((unused)), uint32_t time __attribute__((unused)),
        uint32_t button __attribute__((unused)), uint32_t state __attribute__((unused)))
{
}

static void
_axis(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
      uint32_t time __attribute__((unused)), uint32_t axis __attribute__((unused)),
      wl_fixed_t value __attribute__((unused)))
{
}

static const struct wl_pointer_listener _pointer_listener =
{
   _enter, _leave, _motion, _button, _axis, NULL, NULL, NULL, NULL, NULL, NULL
};

static Tk *tk;
static struct wl_surface *ts, *sub;
static int sx, sy;

static void
_frame_done(void *data, struct wl_callback *cb, uint32_t t __attribute__((unused)))
{
   int *done = data;

   wl_callback_destroy(cb);
   *done = 1;
}

static const struct wl_callback_listener _frame_listener = { _frame_done };

/* E lays out a window it shows during its next render, and only then can
 * the pointer find it. A frame done can come from a render already under
 * way at the commit, so wait for two. */
static void
_drawn(void)
{
   int frames, done;

   for (frames = 0; frames < 2; frames++)
     {
        done = 0;
        wl_surface_damage(sub, 0, 0, 100, 100);
        wl_callback_add_listener(wl_surface_frame(sub), &_frame_listener, &done);
        wl_surface_commit(sub);
        while (!done)
          if (wl_display_dispatch(tk_display(tk)) < 0)
            tk_fail(tk, "disconnected waiting for a frame");
     }
}

static void
_expect(int x, int y, struct wl_surface *want, const char *what)
{
   tk_pointer_warp(tk, sx + x, sy + y);
   tk_sync(tk);
   if (entered != want)
     tk_fail(tk, "%s: the pointer is on %s, not on the %s", what,
             (entered == ts) ? "the toplevel" : (entered == sub) ? "the subsurface" : "no surface",
             (want == ts) ? "toplevel" : "subsurface");
}

static void
_region_set(struct wl_compositor *compositor, int w)
{
   struct wl_region *region = NULL;

   /* Off the window while the region changes. */
   tk_pointer_warp(tk, sx + 500, sy + 500);
   if (w >= 0)
     {
        region = wl_compositor_create_region(compositor);
        if (w) wl_region_add(region, 0, 0, w, 100);
     }
   wl_surface_set_input_region(sub, region);
   if (region) wl_region_destroy(region);
   wl_surface_commit(sub);
   wl_surface_commit(ts);
   tk_settle(tk);
}

int
main(void)
{
   struct wl_subcompositor *subcompositor;
   struct wl_compositor *compositor;
   struct wl_subsurface *subsurface;
   struct wl_region *region;
   struct wl_seat *seat;
   Tk_Toplevel *top;

   tk = tk_connect(PROG);
   compositor = tk_bind(tk, &wl_compositor_interface, 4);
   subcompositor = tk_bind(tk, &wl_subcompositor_interface, 1);
   seat = tk_bind(tk, &wl_seat_interface, 1);
   if ((!compositor) || (!subcompositor) || (!seat)) tk_fail(tk, "missing global");
   wl_pointer_add_listener(wl_seat_get_pointer(seat), &_pointer_listener, NULL);

   top = tk_toplevel_new(tk, "e.test.subsurface_input", PROG, 300, 200);
   ts = tk_toplevel_surface(top);
   /* As Firefox does: the empty region is committed before the buffer. */
   sub = tk_surface_new(tk);
   subsurface = wl_subcompositor_get_subsurface(subcompositor, sub, ts);
   wl_subsurface_set_desync(subsurface);
   wl_subsurface_set_position(subsurface, 20, 20);
   region = wl_compositor_create_region(compositor);
   wl_surface_set_input_region(sub, region);
   wl_region_destroy(region);
   wl_surface_commit(sub);
   tk_settle(tk);
   tk_surface_paint(tk, sub, 100, 100);
   wl_surface_commit(ts);
   _drawn();
   tk_surface_geometry(tk, sub, &sx, &sy, NULL, NULL);

   _expect(50, 50, ts, "an empty input region from the start");
   _region_set(compositor, -1);
   _expect(50, 50, sub, "no input region");
   _region_set(compositor, 0);
   _expect(50, 50, ts, "an empty input region");
   _region_set(compositor, 40);
   _expect(20, 50, sub, "inside a region of the left part");
   _expect(70, 50, ts, "outside a region of the left part");
   _region_set(compositor, -1);
   _expect(70, 50, sub, "the input region reset");

   printf(PROG ": ok\n");
   tk_disconnect(tk);
   return 0;
}
