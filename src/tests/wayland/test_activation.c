/* xdg_activation_v1 between two clients: who may hand the focus over, and
 * how long and how often a token is good for. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "e_wl_testkit.h"
#include "xdg-activation-v1-client-protocol.h"

#define PROG "test-activation"
#define APP_A "e.test.activation.a"
#define APP_B "e.test.activation.b"
#define TOKENS_MAX 32
#define FOCUS WL_TEST_CLIENT_STATE_FOCUSED
#define URGENT WL_TEST_CLIENT_STATE_URGENT

typedef struct
{
   Tk *tk;
   struct xdg_activation_v1 *activation;
   Tk_Toplevel *top;
   const char *app_id;
} Peer;

typedef struct
{
   char str[64];
   int dones;
} Token;

static void
_token_done(void *data, struct xdg_activation_token_v1 *o __attribute__((unused)),
            const char *token)
{
   Token *t = data;

   snprintf(t->str, sizeof(t->str), "%s", token);
   t->dones++;
}

static const struct xdg_activation_token_v1_listener _token_listener =
{
   _token_done
};

static struct xdg_activation_token_v1 *
_token_commit(Peer *p, Token *t)
{
   struct xdg_activation_token_v1 *obj;

   memset(t, 0, sizeof(*t));
   obj = xdg_activation_v1_get_activation_token(p->activation);
   xdg_activation_token_v1_add_listener(obj, &_token_listener, t);
   xdg_activation_token_v1_commit(obj);
   tk_sync(p->tk);
   if (t->dones != 1) tk_fail(p->tk, "%d done events for a token", t->dones);
   if (!t->str[0]) tk_fail(p->tk, "empty token");
   return obj;
}

static void
_token_new(Peer *p, Token *t)
{
   xdg_activation_token_v1_destroy(_token_commit(p, t));
   tk_sync(p->tk);
}

static void
_activate(Peer *p, const char *token)
{
   xdg_activation_v1_activate(p->activation, token, tk_toplevel_surface(p->top));
   tk_settle(p->tk);
}

static void
_peer_new(Peer *p, const char *app_id)
{
   p->tk = tk_connect(PROG);
   p->app_id = app_id;
   p->top = tk_toplevel_new(p->tk, app_id, app_id, 200, 150);
   p->activation = tk_bind(p->tk, &xdg_activation_v1_interface, 1);
   if (!p->activation) tk_fail(p->tk, "no xdg_activation_v1");
}

static void
_expect_focus(Peer *p, Peer *other, const char *what)
{
   tk_wait_states(p->tk, p->app_id, FOCUS, FOCUS, what);
   tk_wait_states(other->tk, other->app_id, FOCUS, 0, what);
}

static void
_expect_refused(Peer *focused, Peer *p, unsigned int urgent, const char *what)
{
   tk_sync(p->tk);
   tk_wait_states(focused->tk, focused->app_id, FOCUS, FOCUS, what);
   tk_wait_states(p->tk, p->app_id, FOCUS | URGENT, urgent, what);
}

static void
_test_double_commit(void)
{
   const struct wl_interface *iface = NULL;
   struct xdg_activation_v1 *activation;
   struct xdg_activation_token_v1 *obj;
   Token t = { "", 0 };
   Tk *tk;

   tk = tk_connect(PROG);
   activation = tk_bind(tk, &xdg_activation_v1_interface, 1);
   obj = xdg_activation_v1_get_activation_token(activation);
   xdg_activation_token_v1_add_listener(obj, &_token_listener, &t);
   xdg_activation_token_v1_commit(obj);
   xdg_activation_token_v1_commit(obj);
   if ((tk_protocol_error(tk, &iface) != XDG_ACTIVATION_TOKEN_V1_ERROR_ALREADY_USED) ||
       (iface != &xdg_activation_token_v1_interface))
     tk_fail(tk, "a second commit is not an already_used error");
   tk_disconnect(tk);
}

static void
_test_set_after_commit(void)
{
   const struct wl_interface *iface = NULL;
   struct xdg_activation_v1 *activation;
   struct xdg_activation_token_v1 *obj;
   Token t = { "", 0 };
   Tk *tk;

   tk = tk_connect(PROG);
   activation = tk_bind(tk, &xdg_activation_v1_interface, 1);
   obj = xdg_activation_v1_get_activation_token(activation);
   xdg_activation_token_v1_add_listener(obj, &_token_listener, &t);
   xdg_activation_token_v1_commit(obj);
   xdg_activation_token_v1_set_app_id(obj, "too.late");
   if ((tk_protocol_error(tk, &iface) != XDG_ACTIVATION_TOKEN_V1_ERROR_ALREADY_USED) ||
       (iface != &xdg_activation_token_v1_interface))
     tk_fail(tk, "set_app_id after commit is not an already_used error");
   tk_disconnect(tk);
}

static void
_test_expiry(Peer *a, Peer *b)
{
   struct timespec wait = { 1, 200000000 };
   Token t;

   _token_new(b, &t);
   nanosleep(&wait, NULL);
   tk_sync(a->tk);
   _activate(a, t.str);
   _expect_refused(b, a, 0, "an expired token");
}

int
main(void)
{
   struct xdg_activation_token_v1 *obj;
   Peer a, b;
   Token t, t2, first;
   int i;

   _peer_new(&a, APP_A);
   _peer_new(&b, APP_B);
   _expect_focus(&b, &a, "the newest window has the focus");

   if (getenv("E_TEST_ACTIVATION_TOKEN_LIFETIME"))
     {
        _test_expiry(&a, &b);
        printf(PROG ": ok (expiry)\n");
        tk_disconnect(b.tk);
        tk_disconnect(a.tk);
        return 0;
     }

   /* The focused client hands the focus over, from a token object it
    * already destroyed. */
   _token_new(&b, &t);
   _activate(&a, t.str);
   _expect_focus(&a, &b, "a token from the focused client");

   /* Back, and the first token is spent. */
   _token_new(&a, &t2);
   if (!strcmp(t.str, t2.str)) tk_fail(a.tk, "two tokens are identical");
   _activate(&b, t2.str);
   _expect_focus(&b, &a, "a second token");
   _activate(&a, t.str);
   _expect_refused(&b, &a, 0, "a token used twice");

   _activate(&a, "never-issued");
   _expect_refused(&b, &a, 0, "an unknown token");

   /* A client without the focus cannot give it to itself. */
   _token_new(&a, &t);
   _activate(&a, t.str);
   _expect_refused(&b, &a, URGENT, "a token from an unfocused client");

   /* The maker may destroy its object after the token was used. */
   obj = _token_commit(&b, &t);
   _activate(&a, t.str);
   _expect_focus(&a, &b, "a token used before its object is gone");
   tk_wait_states(a.tk, APP_A, URGENT, 0, "focus ended the urgency");
   xdg_activation_token_v1_destroy(obj);
   tk_sync(b.tk);

   /* The table is bounded: the oldest token goes first. */
   _token_new(&a, &first);
   for (i = 0; i < TOKENS_MAX; i++) _token_new(&a, &t);
   _activate(&b, first.str);
   _expect_refused(&a, &b, 0, "a token pushed out of the table");
   _activate(&b, t.str);
   _expect_focus(&b, &a, "the newest token");

   _test_double_commit();
   _test_set_after_commit();

   printf(PROG ": ok\n");
   tk_disconnect(b.tk);
   tk_disconnect(a.tk);
   return 0;
}
