/* wl_seat.capabilities is sent when the seat's devices change, so a second
 * client binding the seat must not resend it to the first: that client
 * would create a second wl_pointer and get every event twice. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e_wl_testkit.h"

#define PROG "test-seat-capabilities"

typedef struct
{
   struct wl_seat *seat;
   int events;
   uint32_t caps;
} Seat_Watch;

static void
_caps(void *data, struct wl_seat *seat, uint32_t caps)
{
   Seat_Watch *w = data;

   (void)seat;
   w->events++;
   w->caps = caps;
}

static void
_name(void *d, struct wl_seat *s, const char *n)
{ (void)d; (void)s; (void)n; }

static const struct wl_seat_listener _seat_listener = { _caps, _name };

static void
_seat_watch(Tk *tk, Seat_Watch *w)
{
   uint32_t version = tk_global_version(tk, "wl_seat");

   if (!version) tk_fail(tk, "no wl_seat");
   w->seat = tk_bind(tk, &wl_seat_interface, version);
   if (!w->seat) tk_fail(tk, "wl_seat advertised but would not bind");
   wl_seat_add_listener(w->seat, &_seat_listener, w);
   tk_sync(tk);
}

int
main(void)
{
   Tk *a, *b;
   Seat_Watch wa = { 0 }, wb = { 0 };
   uint32_t caps_after_first;

   a = tk_connect(PROG "-a");
   _seat_watch(a, &wa);

   if (wa.events != 1)
     tk_fail(a, "first client saw %d capabilities events on binding the seat, "
                "expected exactly 1", wa.events);
   caps_after_first = wa.caps;

   /* A second client arrives. This is the whole test: nothing about the seat
    * has changed, so the first client must hear nothing. */
   b = tk_connect(PROG "-b");
   _seat_watch(b, &wb);

   if (wb.events != 1)
     tk_fail(b, "second client saw %d capabilities events on binding the seat, "
                "expected exactly 1", wb.events);

   tk_sync(a);
   tk_sync(a);

   if (wa.events != 1)
     tk_fail(a, "first client saw %d capabilities events after a second client "
                "bound the seat, expected 1 - a client that creates a "
                "wl_pointer for each one now has two, and gets every pointer "
                "event twice", wa.events);

   if (wa.caps != caps_after_first)
     tk_fail(a, "capabilities changed from 0x%x to 0x%x without any device "
                "being added or removed", caps_after_first, wa.caps);

   /* Both clients must agree about what the seat has; they are looking at the
    * same seat. A compositor that computes the value per bind rather than from
    * one place can get this wrong without ever sending a spurious event. */
   if (wa.caps != wb.caps)
     tk_fail(a, "the two clients were told different capabilities for the same "
                "seat: 0x%x and 0x%x", wa.caps, wb.caps);

   printf(PROG ": ok - one capabilities event each, caps 0x%x\n", wa.caps);

   tk_disconnect(b);
   tk_disconnect(a);
   return 0;
}
