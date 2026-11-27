/* A real browser, as browser-run.sh started it, with this test as its input
 * method: clicking in the page's text field activates the input method, and
 * text the input method commits ends up in the field. The page reports its
 * field in its title, as e-ti:<focused|blur>:<value>|, which the browser
 * may decorate.
 *
 *   argv[1]  part of the browser's app_id */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <linux/input-event-codes.h>

#include "e_wl_testkit.h"
#include "input-method-unstable-v2-client-protocol.h"

#define PROG "test-browser-text-input"
#define TEXT "eime"

typedef struct
{
   struct zwp_input_method_v2 *im;
   int activates, dones;
} Ime;

static void
_activate(void *data, struct zwp_input_method_v2 *im __attribute__((unused)))
{
   ((Ime *)data)->activates++;
}

static void
_ignore(void *data __attribute__((unused)), struct zwp_input_method_v2 *im __attribute__((unused)))
{
}

static void
_text(void *data __attribute__((unused)), struct zwp_input_method_v2 *im __attribute__((unused)),
      const char *text __attribute__((unused)), uint32_t cursor __attribute__((unused)),
      uint32_t anchor __attribute__((unused)))
{
}

static void
_uint(void *data __attribute__((unused)), struct zwp_input_method_v2 *im __attribute__((unused)),
      uint32_t v __attribute__((unused)))
{
}

static void
_uint2(void *data __attribute__((unused)), struct zwp_input_method_v2 *im __attribute__((unused)),
       uint32_t a __attribute__((unused)), uint32_t b __attribute__((unused)))
{
}

static void
_done(void *data, struct zwp_input_method_v2 *im __attribute__((unused)))
{
   ((Ime *)data)->dones++;
}

static const struct zwp_input_method_v2_listener _im_listener =
{
   _activate, _ignore, _text, _uint, _uint2, _done, _ignore
};

static int
_titled(const Tk_Client *c, void *data)
{
   return !!strstr(c->title, data);
}

/* Firefox drops a click that comes before its first page is ready to take
 * input, with nothing on the wire to tell when it is: click again, once a
 * second, until the field has the focus. */
static int
_focused_by_click(const Tk_Client *c, void *data)
{
   static time_t last;
   Tk *tk = data;
   time_t now = time(NULL);

   if (strstr(c->title, "e-ti:focused:")) return 1;
   if (now == last) return 0;
   last = now;
   tk_pointer_warp(tk, c->x + (c->w / 2), c->y + ((c->h * 3) / 4));
   tk_pointer_button(tk, BTN_LEFT, 1);
   tk_pointer_button(tk, BTN_LEFT, 0);
   return 0;
}

static int
_activated(const Tk_Client *c __attribute__((unused)), void *data)
{
   return ((Ime *)data)->activates > 0;
}

int
main(int argc, char **argv)
{
   struct zwp_input_method_manager_v2 *manager;
   struct wl_seat *seat;
   Ime ime = { 0 };
   char app_id[256];
   Tk_Client *c;
   Tk *tk;

   if (argc < 2) return 2;
   tk_privileges_file(PROG, NULL, "input_method;");
   tk = tk_connect(PROG);
   seat = tk_bind(tk, &wl_seat_interface, 1);
   manager = tk_bind(tk, &zwp_input_method_manager_v2_interface, 1);
   if ((!seat) || (!manager)) tk_fail(tk, "missing global");
   ime.im = zwp_input_method_manager_v2_get_input_method(manager, seat);
   zwp_input_method_v2_add_listener(ime.im, &_im_listener, &ime);

   c = tk_wait_window(tk, argv[1]);
   snprintf(app_id, sizeof(app_id), "%s", c->app_id);
   c = tk_wait(tk, app_id, _titled, "e-ti:", "the browser to show the page");

   /* A browser starts with its address bar focused. */
   tk_wait(tk, app_id, _focused_by_click, tk, "a click to focus the field");
   tk_wait(tk, app_id, _activated, &ime, "the input method to be activated");
   zwp_input_method_v2_commit_string(ime.im, TEXT);
   zwp_input_method_v2_commit(ime.im, ime.dones);
   c = tk_wait(tk, app_id, _titled, "e-ti:focused:" TEXT "|", "the text to reach the field");

   printf(PROG ": ok, '%s' shows '%s'\n", app_id, c->title);
   tk_disconnect(tk);
   return 0;
}
