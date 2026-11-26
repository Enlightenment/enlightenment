/* zwp_text_input_v3 and zwp_input_method_v2: one connection plays the
 * application with a text field, another plays the input method, and every
 * check is that a request on one side reached the other. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e_wl_testkit.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "input-method-unstable-v2-client-protocol.h"

#define PROG "test-text-input"
#define APP_A "e.test.text-input.a"
#define APP_B "e.test.text-input.b"
#define APP_C "e.test.text-input.c"
#define FOCUS WL_TEST_CLIENT_STATE_FOCUSED
#define ICONIC WL_TEST_CLIENT_STATE_ICONIFIED

#define TEXT "some text"
#define HINT (ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION | \
              ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA)
#define PURPOSE ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME

typedef struct
{
   struct wl_surface *entered;
   /* one letter per event: Enter, Leave, Preedit, Commit, Delete, done */
   char log[64];
   char preedit[64], commit[64];
   int preedit_begin, preedit_end;
   uint32_t del_before, del_after, done_serial;
   uint32_t commits;
} Ti_Watch;

typedef struct
{
   /* Activate, Deactivate, Surrounding, cause (C), Type, done, Unavailable */
   char log[64];
   char surrounding[64];
   uint32_t cursor, anchor, cause, hint, purpose;
} Im_Watch;

static void
_log(char *log, char c)
{
   size_t n = strlen(log);

   if (n >= 63) return;
   log[n] = c;
   log[n + 1] = 0;
}

static void
_ti_enter(void *data, struct zwp_text_input_v3 *ti __attribute__((unused)), struct wl_surface *surface)
{
   Ti_Watch *w = data;

   w->entered = surface;
   _log(w->log, 'E');
}

static void
_ti_leave(void *data, struct zwp_text_input_v3 *ti __attribute__((unused)), struct wl_surface *surface __attribute__((unused)))
{
   Ti_Watch *w = data;

   w->entered = NULL;
   _log(w->log, 'L');
}

static void
_ti_preedit(void *data, struct zwp_text_input_v3 *ti __attribute__((unused)), const char *text, int32_t begin, int32_t end)
{
   Ti_Watch *w = data;

   snprintf(w->preedit, sizeof(w->preedit), "%s", text ? text : "");
   w->preedit_begin = begin;
   w->preedit_end = end;
   _log(w->log, 'P');
}

static void
_ti_commit_string(void *data, struct zwp_text_input_v3 *ti __attribute__((unused)), const char *text)
{
   Ti_Watch *w = data;

   snprintf(w->commit, sizeof(w->commit), "%s", text ? text : "");
   _log(w->log, 'C');
}

static void
_ti_delete(void *data, struct zwp_text_input_v3 *ti __attribute__((unused)), uint32_t before, uint32_t after)
{
   Ti_Watch *w = data;

   w->del_before = before;
   w->del_after = after;
   _log(w->log, 'D');
}

static void
_ti_done(void *data, struct zwp_text_input_v3 *ti __attribute__((unused)), uint32_t serial)
{
   Ti_Watch *w = data;

   w->done_serial = serial;
   _log(w->log, 'd');
}

static const struct zwp_text_input_v3_listener _ti_listener =
{
   _ti_enter, _ti_leave, _ti_preedit, _ti_commit_string, _ti_delete, _ti_done
};

static void
_im_activate(void *data, struct zwp_input_method_v2 *im __attribute__((unused)))
{
   _log(((Im_Watch *)data)->log, 'A');
}

static void
_im_deactivate(void *data, struct zwp_input_method_v2 *im __attribute__((unused)))
{
   _log(((Im_Watch *)data)->log, 'D');
}

static void
_im_surrounding(void *data, struct zwp_input_method_v2 *im __attribute__((unused)), const char *text, uint32_t cursor, uint32_t anchor)
{
   Im_Watch *w = data;

   snprintf(w->surrounding, sizeof(w->surrounding), "%s", text ? text : "");
   w->cursor = cursor;
   w->anchor = anchor;
   _log(w->log, 'S');
}

static void
_im_cause(void *data, struct zwp_input_method_v2 *im __attribute__((unused)), uint32_t cause)
{
   Im_Watch *w = data;

   w->cause = cause;
   _log(w->log, 'C');
}

static void
_im_content_type(void *data, struct zwp_input_method_v2 *im __attribute__((unused)), uint32_t hint, uint32_t purpose)
{
   Im_Watch *w = data;

   w->hint = hint;
   w->purpose = purpose;
   _log(w->log, 'T');
}

static void
_im_done(void *data, struct zwp_input_method_v2 *im __attribute__((unused)))
{
   _log(((Im_Watch *)data)->log, 'd');
}

static void
_im_unavailable(void *data, struct zwp_input_method_v2 *im __attribute__((unused)))
{
   _log(((Im_Watch *)data)->log, 'U');
}

static const struct zwp_input_method_v2_listener _im_listener =
{
   _im_activate, _im_deactivate, _im_surrounding, _im_cause, _im_content_type,
   _im_done, _im_unavailable
};

static Tk *app, *ime;
static struct wl_seat *app_seat, *ime_seat;
static struct zwp_text_input_manager_v3 *ti_manager;
static struct zwp_input_method_manager_v2 *im_manager;

/* A request on one connection is an event on the other only once the
 * compositor has run. */
static void
_settle(void)
{
   tk_sync(app);
   tk_sync(ime);
   tk_sync(app);
}

static void
_expect(Tk *tk, char *log, const char *expected, const char *what)
{
   if (strcmp(log, expected))
     tk_fail(tk, "%s: got events \"%s\", expected \"%s\"", what, log, expected);
   log[0] = 0;
}

static struct zwp_text_input_v3 *
_text_input_new(Ti_Watch *w)
{
   struct zwp_text_input_v3 *ti;

   ti = zwp_text_input_manager_v3_get_text_input(ti_manager, app_seat);
   zwp_text_input_v3_add_listener(ti, &_ti_listener, w);
   return ti;
}

static struct zwp_input_method_v2 *
_input_method_new(Im_Watch *w)
{
   struct zwp_input_method_v2 *im;

   im = zwp_input_method_manager_v2_get_input_method(im_manager, ime_seat);
   zwp_input_method_v2_add_listener(im, &_im_listener, w);
   return im;
}

static void
_commit(struct zwp_text_input_v3 *ti, Ti_Watch *w)
{
   zwp_text_input_v3_commit(ti);
   w->commits++;
   _settle();
}

static void
_enable(struct zwp_text_input_v3 *ti, Ti_Watch *w)
{
   zwp_text_input_v3_enable(ti);
   _commit(ti, w);
}

static void
_disable(struct zwp_text_input_v3 *ti, Ti_Watch *w)
{
   zwp_text_input_v3_disable(ti);
   _commit(ti, w);
}

static void
_test_relay(struct zwp_text_input_v3 *ti, Ti_Watch *tw, struct zwp_input_method_v2 *im, Im_Watch *iw)
{
   zwp_text_input_v3_enable(ti);
   zwp_text_input_v3_set_surrounding_text(ti, TEXT, 2, 1);
   zwp_text_input_v3_set_text_change_cause(ti, ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
   zwp_text_input_v3_set_content_type(ti, HINT, PURPOSE);
   _commit(ti, tw);
   _expect(ime, iw->log, "ASCTd", "enabled with state");
   if (strcmp(iw->surrounding, TEXT) || (iw->cursor != 2) || (iw->anchor != 1))
     tk_fail(ime, "surrounding text \"%s\" %u/%u", iw->surrounding, iw->cursor, iw->anchor);
   if (iw->cause != ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER)
     tk_fail(ime, "change cause %u", iw->cause);
   if ((iw->hint != HINT) || (iw->purpose != PURPOSE))
     tk_fail(ime, "content type 0x%x/%u", iw->hint, iw->purpose);

   /* The cause is for one change, the rest stays until the next enable. */
   _commit(ti, tw);
   _expect(ime, iw->log, "STd", "a commit with nothing new");

   zwp_input_method_v2_set_preedit_string(im, "pre", 1, 2);
   zwp_input_method_v2_commit_string(im, "text");
   zwp_input_method_v2_delete_surrounding_text(im, 3, 4);
   zwp_input_method_v2_commit(im, 2);
   _settle();
   _expect(app, tw->log, "PDCd", "input method commit");
   if (strcmp(tw->preedit, "pre") || (tw->preedit_begin != 1) || (tw->preedit_end != 2))
     tk_fail(app, "preedit \"%s\" %d..%d", tw->preedit, tw->preedit_begin, tw->preedit_end);
   if (strcmp(tw->commit, "text")) tk_fail(app, "commit string \"%s\"", tw->commit);
   if ((tw->del_before != 3) || (tw->del_after != 4))
     tk_fail(app, "delete %u/%u", tw->del_before, tw->del_after);
   if (tw->done_serial != tw->commits)
     tk_fail(app, "done serial %u, not the %u commits made", tw->done_serial, tw->commits);

   /* An enable resets what an earlier one set. */
   _enable(ti, tw);
   _expect(ime, iw->log, "d", "enabled again with no state");

   _disable(ti, tw);
   _expect(ime, iw->log, "Dd", "disabled");

   zwp_input_method_v2_commit_string(im, "lost");
   zwp_input_method_v2_commit(im, 4);
   _settle();
   _expect(app, tw->log, "", "input method commit with no field enabled");
}

/* One text input enabled at a time, and destroying it disables it. */
static void
_test_one_enabled(Im_Watch *iw)
{
   struct zwp_text_input_v3 *first, *second;
   Ti_Watch fw = { 0 }, sw = { 0 };

   first = _text_input_new(&fw);
   second = _text_input_new(&sw);
   _settle();
   _expect(app, fw.log, "E", "text input created with the focus");
   _expect(app, sw.log, "E", "text input created with the focus");

   _enable(first, &fw);
   _expect(ime, iw->log, "Ad", "first enabled");
   _enable(second, &sw);
   _expect(ime, iw->log, "", "second enabled while the first is");
   zwp_text_input_v3_destroy(first);
   _settle();
   _expect(ime, iw->log, "Dd", "the enabled text input is destroyed");
   _enable(second, &sw);
   _expect(ime, iw->log, "Ad", "the second enabled");
   zwp_text_input_v3_destroy(second);
   _settle();
   _expect(ime, iw->log, "Dd", "the second is destroyed");
}

/* Another client's window takes the focus: leave and deactivate. Requests
 * are then ignored but still counted. */
static void
_test_focus_change(struct zwp_text_input_v3 *ti, Ti_Watch *tw, struct zwp_input_method_v2 *im, Im_Watch *iw)
{
   Tk *other;

   _enable(ti, tw);
   _expect(ime, iw->log, "Ad", "enabled");

   other = tk_connect(PROG);
   tk_toplevel_new(other, APP_B, APP_B, 100, 100);
   tk_wait_states(other, APP_B, FOCUS, FOCUS, "a new window has the focus");
   _settle();
   _expect(app, tw->log, "L", "another window got the focus");
   _expect(ime, iw->log, "Dd", "another window got the focus");

   _enable(ti, tw);
   _expect(ime, iw->log, "", "enabled without the focus");

   tk_disconnect(other);
   tk_wait_states(app, APP_A, FOCUS, FOCUS, "the focus came back");
   _settle();
   _expect(app, tw->log, "E", "the focus came back");
   _expect(ime, iw->log, "", "the focus came back");
   _commit(ti, tw);
   _expect(ime, iw->log, "", "a commit with the enable from before the leave");

   _enable(ti, tw);
   _expect(ime, iw->log, "Ad", "enabled after the focus came back");
   zwp_input_method_v2_commit_string(im, "x");
   zwp_input_method_v2_commit(im, 0);
   _settle();
   _expect(app, tw->log, "Cd", "commit after the focus came back");
   if (tw->done_serial != tw->commits)
     tk_fail(app, "done serial %u, not the %u commits made, including those "
                  "made without the focus", tw->done_serial, tw->commits);
   _disable(ti, tw);
   _expect(ime, iw->log, "Dd", "disabled");
}

/* The focus goes to no window at all. */
static void
_test_focus_lost(struct zwp_text_input_v3 *ti, Ti_Watch *tw, Im_Watch *iw)
{
   unsigned int id = tk_expect(app, APP_A)->id;

   _enable(ti, tw);
   _expect(ime, iw->log, "Ad", "enabled");
   tk_action(app, id, "window_iconic_toggle", NULL);
   tk_wait_states(app, APP_A, ICONIC | FOCUS, ICONIC, "iconified");
   _settle();
   _expect(app, tw->log, "L", "the window was iconified");
   _expect(ime, iw->log, "Dd", "the window was iconified");
   tk_action(app, id, "window_iconic_toggle", NULL);
   tk_wait_states(app, APP_A, ICONIC | FOCUS, FOCUS, "restored");
   _settle();
   _expect(app, tw->log, "E", "the window was restored");
}

/* Closing the focused window: the leave for its surface comes before the
 * enter for the window under it. */
static void
_test_focused_window_closed(Tk_Toplevel *top, Ti_Watch *tw)
{
   Tk_Toplevel *child;

   child = tk_toplevel_new(app, APP_C, APP_C, 100, 100);
   tk_wait_states(app, APP_C, FOCUS, FOCUS, "a second window has the focus");
   _settle();
   _expect(app, tw->log, "LE", "the second window got the focus");
   if (tw->entered != tk_toplevel_surface(child))
     tk_fail(app, "enter is not for the second window");

   tk_toplevel_del(child);
   tk_wait_states(app, APP_A, FOCUS, FOCUS, "the first window has the focus again");
   _settle();
   _expect(app, tw->log, "LE", "the focused window was closed");
   if (tw->entered != tk_toplevel_surface(top))
     tk_fail(app, "enter is not for the first window");
}

/* Only one input method per seat: a second one is unavailable, and one that
 * comes after the field was enabled is activated. */
static void
_test_input_methods(struct zwp_text_input_v3 *ti, Ti_Watch *tw, struct zwp_input_method_v2 *im)
{
   Im_Watch second = { 0 }, third = { 0 };
   struct zwp_input_method_v2 *im2, *im3;

   im2 = _input_method_new(&second);
   _settle();
   _expect(ime, second.log, "U", "a second input method");
   _enable(ti, tw);
   _expect(ime, second.log, "", "enabled with an unavailable input method");
   zwp_input_method_v2_commit_string(im2, "x");
   zwp_input_method_v2_commit(im2, 0);
   _settle();
   _expect(app, tw->log, "", "a commit from an unavailable input method");

   zwp_input_method_v2_destroy(im);
   zwp_input_method_v2_destroy(im2);
   im3 = _input_method_new(&third);
   _settle();
   _expect(ime, third.log, "Ad", "an input method created after the enable");
   zwp_input_method_v2_destroy(im3);
}

int
main(void)
{
   struct zwp_text_input_v3 *ti;
   struct zwp_input_method_v2 *im;
   Tk_Toplevel *top;
   Ti_Watch tw = { 0 };
   Im_Watch iw = { 0 };

   tk_privileges_file("test-text-input", NULL, "input_method");
   app = tk_connect(PROG);
   ime = tk_connect(PROG);
   app_seat = tk_bind(app, &wl_seat_interface, 1);
   ime_seat = tk_bind(ime, &wl_seat_interface, 1);
   ti_manager = tk_bind(app, &zwp_text_input_manager_v3_interface, 1);
   im_manager = tk_bind(ime, &zwp_input_method_manager_v2_interface, 1);
   if ((!app_seat) || (!ime_seat) || (!ti_manager) || (!im_manager))
     tk_fail(app, "missing global");

   im = _input_method_new(&iw);
   ti = _text_input_new(&tw);
   _settle();
   _expect(app, tw.log, "", "no window");

   top = tk_toplevel_new(app, APP_A, APP_A, 200, 200);
   tk_wait_states(app, APP_A, FOCUS, FOCUS, "a new window has the focus");
   _settle();
   _expect(app, tw.log, "E", "the window got the focus");
   if (tw.entered != tk_toplevel_surface(top))
     tk_fail(app, "enter is not for the window");

   _test_relay(ti, &tw, im, &iw);
   _test_one_enabled(&iw);
   _test_focus_change(ti, &tw, im, &iw);
   _test_focus_lost(ti, &tw, &iw);
   _test_focused_window_closed(top, &tw);
   _test_input_methods(ti, &tw, im);

   tk_disconnect(ime);
   tk_disconnect(app);
   printf(PROG ": ok\n");
   return 0;
}
