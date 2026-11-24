/* The primary selection between two clients: it follows the keyboard focus,
 * and a client without the focus cannot read it. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "e_wl_testkit.h"
#include "primary-selection-unstable-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"

#define PROG "test-primary-selection"
#define APP_A "e.test.primary.a"
#define APP_B "e.test.primary.b"
#define APP_C "e.test.primary.c"
#define MIME "text/plain"
#define FOCUS WL_TEST_CLIENT_STATE_FOCUSED

typedef struct
{
   struct zwp_primary_selection_source_v1 *obj;
   const char *text;
   int sends, cancels;
} Source;

typedef struct
{
   Tk *tk;
   Tk_Toplevel *top;
   const char *app_id;
   struct xdg_activation_v1 *activation;
   struct wl_seat *seat;
   struct zwp_primary_selection_device_manager_v1 *manager;
   struct zwp_primary_selection_device_v1 *device;
   struct zwp_primary_selection_offer_v1 *offer;
   int selections, mimes;
} Peer;

static void
_source_send(void *data, struct zwp_primary_selection_source_v1 *o __attribute__((unused)),
             const char *mime __attribute__((unused)), int32_t fd)
{
   Source *s = data;

   s->sends++;
   if (write(fd, s->text, strlen(s->text)) < 0) perror("write");
   close(fd);
}

static void
_source_cancelled(void *data, struct zwp_primary_selection_source_v1 *o __attribute__((unused)))
{
   Source *s = data;

   s->cancels++;
}

static const struct zwp_primary_selection_source_v1_listener _source_listener =
{
   _source_send,
   _source_cancelled
};

static void
_offer_mime(void *data, struct zwp_primary_selection_offer_v1 *o __attribute__((unused)),
            const char *mime)
{
   Peer *p = data;

   if (!strcmp(mime, MIME)) p->mimes++;
}

static const struct zwp_primary_selection_offer_v1_listener _offer_listener =
{
   _offer_mime
};

static void
_device_data_offer(void *data, struct zwp_primary_selection_device_v1 *o __attribute__((unused)),
                   struct zwp_primary_selection_offer_v1 *offer)
{
   Peer *p = data;

   p->mimes = 0;
   zwp_primary_selection_offer_v1_add_listener(offer, &_offer_listener, p);
}

static void
_device_selection(void *data, struct zwp_primary_selection_device_v1 *o __attribute__((unused)),
                  struct zwp_primary_selection_offer_v1 *offer)
{
   Peer *p = data;

   p->selections++;
   p->offer = offer;
}

static const struct zwp_primary_selection_device_v1_listener _device_listener =
{
   _device_data_offer,
   _device_selection
};

static void
_peer_new(Peer *p, const char *app_id)
{
   memset(p, 0, sizeof(*p));
   p->tk = tk_connect(PROG);
   p->app_id = app_id;
   p->top = tk_toplevel_new(p->tk, app_id, app_id, 200, 150);
   p->activation = tk_bind(p->tk, &xdg_activation_v1_interface, 1);
   p->manager = tk_bind(p->tk, &zwp_primary_selection_device_manager_v1_interface, 1);
   if ((!p->activation) || (!p->manager)) tk_fail(p->tk, "missing global");
   p->seat = tk_bind(p->tk, &wl_seat_interface, 1);
   /* E only tells a client it has the focus through its wl_keyboard. */
   wl_seat_get_keyboard(p->seat);
   p->device = zwp_primary_selection_device_manager_v1_get_device(p->manager, p->seat);
   zwp_primary_selection_device_v1_add_listener(p->device, &_device_listener, p);
   tk_sync(p->tk);
}

static void
_token_done(void *data, struct xdg_activation_token_v1 *o __attribute__((unused)),
            const char *token)
{
   snprintf(data, 64, "%s", token);
}

static const struct xdg_activation_token_v1_listener _token_listener =
{
   _token_done
};

/* Hand the focus from one client to the other, and let both hear of it. */
static void
_focus(Peer *from, Peer *to)
{
   struct xdg_activation_token_v1 *obj;
   char token[64] = "";

   obj = xdg_activation_v1_get_activation_token(from->activation);
   xdg_activation_token_v1_add_listener(obj, &_token_listener, token);
   xdg_activation_token_v1_commit(obj);
   tk_sync(from->tk);
   xdg_activation_token_v1_destroy(obj);
   xdg_activation_v1_activate(to->activation, token, tk_toplevel_surface(to->top));
   tk_settle(to->tk);
   tk_wait_states(to->tk, to->app_id, FOCUS, FOCUS, "the focus moved");
   tk_wait_states(from->tk, from->app_id, FOCUS, 0, "the focus left");
   tk_sync(from->tk);
   tk_sync(to->tk);
}

static void
_source_set(Peer *p, Source *s, const char *text, int with_mime)
{
   memset(s, 0, sizeof(*s));
   s->text = text;
   s->obj = zwp_primary_selection_device_manager_v1_create_source(p->manager);
   zwp_primary_selection_source_v1_add_listener(s->obj, &_source_listener, s);
   if (with_mime) zwp_primary_selection_source_v1_offer(s->obj, MIME);
   zwp_primary_selection_device_v1_set_selection(p->device, s->obj, 0);
   tk_sync(p->tk);
}

/* Ask for the data through this offer and return what came back. */
static const char *
_read(Peer *reader, struct zwp_primary_selection_offer_v1 *offer, Peer *owner)
{
   static char buf[64];
   int fds[2];
   ssize_t n;

   if (pipe(fds)) tk_fail(reader->tk, "pipe");
   zwp_primary_selection_offer_v1_receive(offer, MIME, fds[1]);
   tk_sync(reader->tk);
   close(fds[1]);
   tk_sync(owner->tk);
   n = read(fds[0], buf, sizeof(buf) - 1);
   close(fds[0]);
   buf[(n > 0) ? n : 0] = 0;
   return buf;
}

int
main(void)
{
   struct zwp_primary_selection_offer_v1 *stale;
   Source s, s2, s3;
   Peer a, b;
   Tk *c;
   int count;

   _peer_new(&a, APP_A);
   _peer_new(&b, APP_B);
   tk_wait_states(b.tk, APP_B, FOCUS, FOCUS, "the newest window has the focus");

   /* No selection yet: nothing to say to either client, focused or not. */
   _focus(&b, &a);
   _focus(&a, &b);
   if (a.selections || b.selections)
     tk_fail(a.tk, "an empty selection was announced (%d, %d)", a.selections, b.selections);

   /* The focused client owns it, and only it hears about it. */
   _source_set(&b, &s, "from b", 1);
   tk_sync(a.tk);
   if ((b.selections != 1) || (!b.offer)) tk_fail(b.tk, "the owner got no offer");
   if (a.selections) tk_fail(a.tk, "a client without the focus heard of the selection");

   /* The focus moves it over, with its types, and the old client is told. */
   stale = b.offer;
   count = b.selections;
   _focus(&b, &a);
   if ((a.selections != 1) || (!a.offer) || (a.mimes != 1))
     tk_fail(a.tk, "the focused client got no usable offer");
   if ((b.selections != count + 1) || b.offer)
     tk_fail(b.tk, "the client losing the focus was not told");
   if (strcmp(_read(&a, a.offer, &b), "from b") || (s.sends != 1))
     tk_fail(a.tk, "the focused client could not read the selection");

   /* An offer kept past the focus reads nothing. */
   if (_read(&b, stale, &b)[0] || (s.sends != 1))
     tk_fail(b.tk, "a client without the focus read the selection");

   /* A new selection cancels the old owner. */
   _source_set(&a, &s2, "from a", 1);
   tk_sync(b.tk);
   if ((s.cancels != 1) || s2.cancels) tk_fail(a.tk, "replacing did not cancel the old owner");
   zwp_primary_selection_source_v1_destroy(s.obj);

   /* The owner can clear its own, even without the focus. */
   _focus(&a, &b);
   if (!b.offer) tk_fail(b.tk, "no offer after the focus came back");
   count = b.selections;
   zwp_primary_selection_device_v1_set_selection(a.device, NULL, 0);
   tk_sync(a.tk);
   tk_sync(b.tk);
   if ((s2.cancels != 1) || (b.selections != count + 1) || b.offer)
     tk_fail(b.tk, "the owner could not clear its selection");
   zwp_primary_selection_source_v1_destroy(s2.obj);

   /* A source with no types still moves with the focus, and when its owner
    * destroys it the focused client is told it is gone. */
   _source_set(&b, &s3, "empty", 0);
   _focus(&b, &a);
   if ((!a.offer) || a.mimes) tk_fail(a.tk, "a source with no types was not offered");

   /* The focus going to a client with no wl_keyboard still ends the offer. */
   c = tk_connect(PROG);
   tk_toplevel_new(c, APP_C, APP_C, 200, 150);
   tk_wait_states(c, APP_C, FOCUS, FOCUS, "the new window has the focus");
   tk_sync(a.tk);
   if (a.offer) tk_fail(a.tk, "focus went to a client with no keyboard, the offer lives on");
   tk_disconnect(c);
   tk_wait_states(a.tk, APP_A, FOCUS, FOCUS, "the focus came back");
   tk_sync(a.tk);
   if (!a.offer) tk_fail(a.tk, "no offer after the focus came back");
   count = a.selections;
   zwp_primary_selection_source_v1_destroy(s3.obj);
   tk_sync(b.tk);
   tk_sync(a.tk);
   if ((a.selections != count + 1) || a.offer)
     tk_fail(a.tk, "the end of the source was not announced");

   tk_disconnect(b.tk);
   tk_disconnect(a.tk);
   printf(PROG ": ok\n");
   return 0;
}
