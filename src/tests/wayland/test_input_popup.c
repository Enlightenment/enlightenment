/* zwp_input_popup_surface_v2, the input method's candidate list: shown if
 * and only if the input method is active, below the text cursor of the
 * focused window, and told where that cursor is in its own coordinates. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/input-event-codes.h>

#include "e_wl_testkit.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "input-method-unstable-v2-client-protocol.h"

#define PROG "test-input-popup"
#define APP_ID "e.test.input-popup"
#define FOCUS WL_TEST_CLIENT_STATE_FOCUSED
#define PW 80
#define PH 40

typedef struct
{
   int x, y, w, h;
   int rects;
} Rect;

static Tk *app, *ime;
static struct wl_seat *ime_seat;
static struct zwp_input_method_manager_v2 *im_manager;
static struct zwp_text_input_v3 *ti;
static int origin_x, origin_y, origin_known, zone_w, zone_h;
static struct wl_surface *ime_pointer_surface;

static void
_rect(void *data, struct zwp_input_popup_surface_v2 *popup __attribute__((unused)),
      int32_t x, int32_t y, int32_t w, int32_t h)
{
   Rect *r = data;

   r->x = x;
   r->y = y;
   r->w = w;
   r->h = h;
   r->rects++;
}

static const struct zwp_input_popup_surface_v2_listener _popup_listener = { _rect };

static void
_im_event(void *data __attribute__((unused)), struct zwp_input_method_v2 *im __attribute__((unused)))
{
}

static void
_im_text(void *data __attribute__((unused)), struct zwp_input_method_v2 *im __attribute__((unused)),
         const char *text __attribute__((unused)), uint32_t c __attribute__((unused)),
         uint32_t a __attribute__((unused)))
{
}

static void
_im_uint(void *data __attribute__((unused)), struct zwp_input_method_v2 *im __attribute__((unused)),
         uint32_t v __attribute__((unused)))
{
}

static void
_im_uint2(void *data __attribute__((unused)), struct zwp_input_method_v2 *im __attribute__((unused)),
          uint32_t a __attribute__((unused)), uint32_t b __attribute__((unused)))
{
}

static const struct zwp_input_method_v2_listener _im_listener =
{
   _im_event, _im_event, _im_text, _im_uint, _im_uint2, _im_event, _im_event
};

static void
_ptr_enter(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
           uint32_t serial __attribute__((unused)), struct wl_surface *s __attribute__((unused)),
           wl_fixed_t x, wl_fixed_t y)
{
   if (origin_known) return;
   origin_x = 150 - wl_fixed_to_int(x);
   origin_y = 150 - wl_fixed_to_int(y);
   origin_known = 1;
}

static void
_ptr_leave(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
           uint32_t serial __attribute__((unused)), struct wl_surface *s __attribute__((unused)))
{
}

static void
_ptr_motion(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
            uint32_t time __attribute__((unused)), wl_fixed_t x __attribute__((unused)),
            wl_fixed_t y __attribute__((unused)))
{
}

static void
_ptr_button(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
            uint32_t serial __attribute__((unused)), uint32_t time __attribute__((unused)),
            uint32_t button __attribute__((unused)), uint32_t state __attribute__((unused)))
{
}

static void
_ptr_axis(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
          uint32_t time __attribute__((unused)), uint32_t axis __attribute__((unused)),
          wl_fixed_t value __attribute__((unused)))
{
}

static void
_ime_ptr_enter(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
               uint32_t serial __attribute__((unused)), struct wl_surface *s,
               wl_fixed_t x __attribute__((unused)), wl_fixed_t y __attribute__((unused)))
{
   ime_pointer_surface = s;
}

static void
_ime_ptr_leave(void *data __attribute__((unused)), struct wl_pointer *p __attribute__((unused)),
               uint32_t serial __attribute__((unused)), struct wl_surface *s __attribute__((unused)))
{
   ime_pointer_surface = NULL;
}

static const struct wl_pointer_listener _ime_ptr_listener =
{
   _ime_ptr_enter, _ime_ptr_leave, _ptr_motion, _ptr_button, _ptr_axis,
   NULL, NULL, NULL, NULL, NULL, NULL
};

static const struct wl_pointer_listener _ptr_listener =
{
   _ptr_enter, _ptr_leave, _ptr_motion, _ptr_button, _ptr_axis,
   NULL, NULL, NULL, NULL, NULL, NULL
};

static void
_output_geometry(void *data __attribute__((unused)), struct wl_output *o __attribute__((unused)),
                 int32_t x __attribute__((unused)), int32_t y __attribute__((unused)),
                 int32_t pw __attribute__((unused)), int32_t ph __attribute__((unused)),
                 int32_t subpixel __attribute__((unused)), const char *make __attribute__((unused)),
                 const char *model __attribute__((unused)), int32_t transform __attribute__((unused)))
{
}

static void
_output_mode(void *data __attribute__((unused)), struct wl_output *o __attribute__((unused)),
             uint32_t flags __attribute__((unused)), int32_t w, int32_t h,
             int32_t refresh __attribute__((unused)))
{
   zone_w = w;
   zone_h = h;
}

static const struct wl_output_listener _output_listener =
{
   _output_geometry, _output_mode, NULL, NULL, NULL, NULL
};

static void
_settle(void)
{
   tk_sync(app);
   tk_sync(ime);
   tk_sync(app);
}

static void
_cursor(int enable, int x, int y)
{
   if (enable) zwp_text_input_v3_enable(ti);
   zwp_text_input_v3_set_cursor_rectangle(ti, x, y, 5, 15);
   zwp_text_input_v3_commit(ti);
   _settle();
}

static void
_disable(void)
{
   zwp_text_input_v3_disable(ti);
   zwp_text_input_v3_commit(ti);
   _settle();
}

static struct zwp_input_popup_surface_v2 *
_popup_new(struct zwp_input_method_v2 *im, struct wl_surface **surface, Rect *r, int buffer_first)
{
   struct zwp_input_popup_surface_v2 *popup;

   *surface = tk_surface_new(ime);
   if (buffer_first) tk_surface_paint(ime, *surface, PW, PH);
   popup = zwp_input_method_v2_get_input_popup_surface(im, *surface);
   zwp_input_popup_surface_v2_add_listener(popup, &_popup_listener, r);
   if (!buffer_first) tk_surface_paint(ime, *surface, PW, PH);
   _settle();
   return popup;
}

/* A hide may run the theme's hide animation first. */
static void
_expect_hidden(struct wl_surface *surface, const char *what)
{
   time_t deadline = time(NULL) + 5;

   while (tk_surface_visible(ime, surface))
     {
        if (time(NULL) > deadline) tk_fail(ime, "%s: the popup is shown", what);
        tk_sync(ime);
     }
}

static void
_expect_at(struct wl_surface *surface, Rect *r, int x, int y, int rx, int ry, const char *what)
{
   int px, py;

   if (!tk_surface_visible(ime, surface)) tk_fail(ime, "%s: the popup is hidden", what);
   tk_surface_geometry(ime, surface, &px, &py, NULL, NULL);
   if ((px != x) || (py != y))
     tk_fail(ime, "%s: the popup is at %d,%d, not %d,%d", what, px, py, x, y);
   if ((!r->rects) || (r->x != rx) || (r->y != ry) || (r->w != 5) || (r->h != 15))
     tk_fail(ime, "%s: text_input_rectangle %d,%d %dx%d (%d sent), not %d,%d 5x15",
             what, r->x, r->y, r->w, r->h, r->rects, rx, ry);
}

static void
_test_role_error(void)
{
   struct zwp_input_method_manager_v2 *manager;
   struct zwp_input_method_v2 *im;
   const struct wl_interface *iface;
   struct wl_surface *surface;
   struct wl_seat *seat;
   Tk *tk;

   tk = tk_connect(PROG);
   seat = tk_bind(tk, &wl_seat_interface, 1);
   manager = tk_bind(tk, &zwp_input_method_manager_v2_interface, 1);
   if ((!seat) || (!manager)) tk_fail(tk, "missing global");
   im = zwp_input_method_manager_v2_get_input_method(manager, seat);
   surface = tk_surface_new(tk);
   zwp_input_method_v2_get_input_popup_surface(im, surface);
   zwp_input_method_v2_get_input_popup_surface(im, surface);
   if ((tk_protocol_error(tk, &iface) != ZWP_INPUT_METHOD_V2_ERROR_ROLE) ||
       (iface != &zwp_input_method_v2_interface))
     tk_fail(tk, "a second role is not a role error");
   tk_disconnect(tk);
}

int
main(void)
{
   struct zwp_text_input_manager_v3 *ti_manager;
   struct zwp_input_popup_surface_v2 *popup, *early, *other;
   struct zwp_input_method_v2 *im, *im2;
   struct wl_surface *surface, *early_surface, *other_surface;
   struct wl_test *tester;
   struct wl_seat *app_seat;
   struct wl_output *output;
   Tk_Toplevel *top;
   Rect r = { 0 }, early_r = { 0 }, other_r = { 0 };

   tk_privileges_file("test-input-popup", NULL, "input_method;");
   app = tk_connect(PROG);
   ime = tk_connect(PROG);
   tester = tk_bind(app, &wl_test_interface, 1);
   app_seat = tk_bind(app, &wl_seat_interface, 1);
   output = tk_bind(app, &wl_output_interface, 1);
   ime_seat = tk_bind(ime, &wl_seat_interface, 1);
   if (ime_seat)
     wl_pointer_add_listener(wl_seat_get_pointer(ime_seat), &_ime_ptr_listener, NULL);
   ti_manager = tk_bind(app, &zwp_text_input_manager_v3_interface, 1);
   im_manager = tk_bind(ime, &zwp_input_method_manager_v2_interface, 1);
   if ((!tester) || (!app_seat) || (!output) || (!ime_seat) || (!ti_manager) || (!im_manager))
     tk_fail(app, "missing global");
   wl_output_add_listener(output, &_output_listener, NULL);
   wl_pointer_add_listener(wl_seat_get_pointer(app_seat), &_ptr_listener, NULL);

   ti = zwp_text_input_manager_v3_get_text_input(ti_manager, app_seat);
   top = tk_toplevel_new_shadowed(app, APP_ID, APP_ID, 200, 150, 10);
   tk_wait_states(app, APP_ID, FOCUS, FOCUS, "a new window has the focus");
   wl_test_move_surface(tester, tk_toplevel_surface(top), 100, 100);
   tk_settle(app);
   /* The window's surface origin, from where the pointer is in it. */
   tk_pointer_warp(app, 150, 150);
   _settle();
   if ((!origin_known) || (!zone_w) || (!zone_h)) tk_fail(app, "no pointer enter or no output mode");

   im = zwp_input_method_manager_v2_get_input_method(im_manager, ime_seat);
   zwp_input_method_v2_add_listener(im, &_im_listener, NULL);
   popup = _popup_new(im, &surface, &r, 0);
   early = _popup_new(im, &early_surface, &early_r, 1);
   _expect_hidden(surface, "no field enabled");
   _expect_hidden(early_surface, "no field enabled, buffer before the role");

   _cursor(1, 10, 20);
   _expect_at(surface, &r, origin_x + 10, origin_y + 35, 0, -15, "enabled");
   _expect_at(early_surface, &early_r, origin_x + 10, origin_y + 35, 0, -15,
              "enabled, buffer before the role");
   if (tk_surface_visible(app, tk_toplevel_surface(top)) != 1)
     tk_fail(app, "the window is hidden");
   tk_wait_states(app, APP_ID, FOCUS, FOCUS, "the window keeps the focus");

   _cursor(0, 30, 40);
   _expect_at(surface, &r, origin_x + 30, origin_y + 55, 0, -15, "the cursor moved");

   /* No room below: above the cursor. */
   _cursor(0, 30, zone_h - origin_y - 20);
   _expect_at(surface, &r, origin_x + 30, zone_h - 20 - PH, 0, PH, "no room below");

   /* No room above either: against the bottom edge. */
   _cursor(0, 30, zone_h - origin_y + 100);
   _expect_at(surface, &r, origin_x + 30, zone_h - PH, 0, 100 + PH, "no room at all");

   /* No room on the right: against the right edge. */
   _cursor(0, zone_w - origin_x - 10, 40);
   _expect_at(surface, &r, zone_w - PW, origin_y + 55, PW - 10, -15, "no room on the right");

   /* A resized popup is placed with its new size. */
   tk_surface_paint(ime, surface, PW + 20, PH);
   _settle();
   _expect_at(surface, &r, zone_w - PW - 20, origin_y + 55, PW + 10, -15, "resized");
   tk_surface_paint(ime, surface, PW, PH);
   _cursor(0, 10, 20);

   /* Above the window, even raised, and a click does not take the focus. */
   tk_action(app, tk_expect(app, APP_ID)->id, "window_raise", NULL);
   tk_pointer_warp(app, origin_x + 10 + PW / 2, origin_y + 35 + PH / 2);
   _settle();
   if (ime_pointer_surface != surface) tk_fail(ime, "the pointer is not on the popup");
   tk_pointer_button(app, BTN_LEFT, 1);
   tk_pointer_button(app, BTN_LEFT, 0);
   _settle();
   tk_wait_states(app, APP_ID, FOCUS, FOCUS, "the window keeps the focus after a click on the popup");
   _expect_at(surface, &r, origin_x + 10, origin_y + 35, 0, -15, "clicked");

   /* Unmapped by a NULL buffer, and mapped again. */
   wl_surface_attach(surface, NULL, 0, 0);
   wl_surface_commit(surface);
   _settle();
   _expect_hidden(surface, "unmapped");
   tk_surface_paint(ime, surface, PW, PH);
   _settle();
   _expect_at(surface, &r, origin_x + 10, origin_y + 35, 0, -15, "mapped again");

   /* A second input method is unavailable, and so is its popup. */
   im2 = zwp_input_method_manager_v2_get_input_method(im_manager, ime_seat);
   zwp_input_method_v2_add_listener(im2, &_im_listener, NULL);
   other = _popup_new(im2, &other_surface, &other_r, 0);
   _expect_hidden(other_surface, "the popup of an unavailable input method");
   _disable();
   _expect_hidden(surface, "disabled");
   _cursor(1, 10, 20);
   _expect_hidden(other_surface, "the popup of an unavailable input method, enabled again");
   if (other_r.rects) tk_fail(ime, "the popup of an unavailable input method got a rectangle");
   zwp_input_popup_surface_v2_destroy(other);
   zwp_input_method_v2_destroy(im2);

   /* The input method goes first. */
   zwp_input_method_v2_destroy(im);
   _settle();
   _expect_hidden(surface, "the input method is gone");
   _expect_hidden(early_surface, "the input method is gone, buffer before the role");
   zwp_input_popup_surface_v2_destroy(popup);
   zwp_input_popup_surface_v2_destroy(early);
   _settle();

   _test_role_error();

   tk_disconnect(ime);
   tk_disconnect(app);
   printf(PROG ": ok\n");
   return 0;
}
