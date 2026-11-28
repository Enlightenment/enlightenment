/* fcitx5 with its pinyin engine and a real browser, both started by the
 * test's E_TEST_APP: "nihao" typed in the page's text field shows
 * fcitx5's candidate window, and a space commits Chinese text to the
 * field. The page reports its field in its title, as
 * e-ti:<focused|blur>:<the field's value>|, which the browser may decorate.
 *
 *   argv[1]  part of the browser's app_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <linux/input-event-codes.h>

#include "e_wl_testkit.h"

#define PROG "test-browser-pinyin"

typedef struct
{
   Tk *tk;
   struct wl_test *tester;
   int browser_pid, e_pid;
} Ctx;

static void
_type(Ctx *ctx, const char *name)
{
   wl_test_key(ctx->tester, name, 1);
   wl_test_key(ctx->tester, name, 0);
   tk_settle(ctx->tk);
}

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
   Ctx *ctx = data;
   time_t now = time(NULL);

   if (strstr(c->title, "e-ti:focused:")) return 1;
   if (now == last) return 0;
   last = now;
   tk_pointer_warp(ctx->tk, c->x + (c->w / 2), c->y + ((c->h * 3) / 4));
   tk_pointer_button(ctx->tk, BTN_LEFT, 1);
   tk_pointer_button(ctx->tk, BTN_LEFT, 0);
   return 0;
}

/* A window shown by neither the browser nor E is fcitx5's candidate
 * window. */
static int
_candidates_shown(Ctx *ctx)
{
   static Tk_Client all[TK_MAX_CLIENTS];
   int i, n;

   n = tk_clients(ctx->tk, all, TK_MAX_CLIENTS);
   for (i = 0; i < n; i++)
     if ((all[i].states & WL_TEST_CLIENT_STATE_VISIBLE) &&
         (all[i].pid != ctx->browser_pid) && (all[i].pid != ctx->e_pid))
       return 1;
   return 0;
}

/* The browser enables its text input when the field takes the focus, and
 * fcitx5 grabs the keyboard when it is told: a key typed in between goes to
 * the field as it is, and is taken back. A field shows the text being
 * composed too, so only the candidate window, which can come later, tells
 * them apart. */
static int
_composing(const Tk_Client *c, void *data)
{
   static int typed;
   static time_t filled;
   Ctx *ctx = data;
   time_t now = time(NULL);

   if (_candidates_shown(ctx)) return 1;
   if (strstr(c->title, "e-ti:focused:|"))
     {
        filled = 0;
        if (!typed) _type(ctx, "n");
        typed = 1;
        return 0;
     }
   if (!filled) filled = now;
   if (now - filled < 2) return 0;
   _type(ctx, "BackSpace");
   filled = 0;
   typed = 0;
   return 0;
}

static int
_candidates(const Tk_Client *c __attribute__((unused)), void *data)
{
   return _candidates_shown(data);
}

/* Which characters depends on fcitx5's dictionary: they are not ASCII,
 * which the letters being composed are. */
static int
_committed(const Tk_Client *c, void *data __attribute__((unused)))
{
   const char *s = strstr(c->title, "e-ti:focused:");

   if ((!s) || (s[13] == '|')) return 0;
   for (s += 13; *s != '|'; s++)
     if (!(*s & 0x80)) return 0;
   return 1;
}

int
main(int argc, char **argv)
{
   Ctx ctx = { 0 };
   char app_id[256];
   Tk_Client *c;

   if ((argc < 2) || (!getenv("E_TEST_PID"))) return 2;
   ctx.e_pid = atoi(getenv("E_TEST_PID"));
   ctx.tk = tk_connect(PROG);
   ctx.tester = tk_bind(ctx.tk, &wl_test_interface, 1);
   if (!ctx.tester) tk_fail(ctx.tk, "missing global");

   c = tk_wait_window(ctx.tk, argv[1]);
   snprintf(app_id, sizeof(app_id), "%s", c->app_id);
   ctx.browser_pid = c->pid;
   tk_wait(ctx.tk, app_id, _titled, "e-ti:", "the browser to show the page");
   tk_wait(ctx.tk, app_id, _focused_by_click, &ctx, "a click to focus the field");

   tk_wait(ctx.tk, app_id, _composing, &ctx, "fcitx5 to compose \"n\"");
   _type(&ctx, "i");
   _type(&ctx, "h");
   _type(&ctx, "a");
   _type(&ctx, "o");
   tk_wait(ctx.tk, app_id, _candidates, &ctx, "the candidate window");
   _type(&ctx, " ");
   c = tk_wait(ctx.tk, app_id, _committed, NULL, "Chinese text to reach the field");

   printf(PROG ": ok, '%s' shows '%s'\n", app_id, c->title);
   tk_disconnect(ctx.tk);
   return 0;
}
