/* A toplevel with a parent is a dialog, not a popup: wl_keyboard.enter must
 * name the dialog, not the parent. The surface in the enter is what every
 * toolkit routes keys on. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "e_wl_testkit.h"

#define PROG "test-dialog-focus"

typedef struct
{
   struct wl_surface *entered;   /* last surface named in enter */
   int enters, leaves;
} Kbd_Watch;

static void
_kbd_keymap(void *d, struct wl_keyboard *k, uint32_t f, int32_t fd, uint32_t s)
{ (void)d; (void)k; (void)f; (void)s; if (fd >= 0) close(fd); }

static void
_kbd_enter(void *data, struct wl_keyboard *k, uint32_t serial,
           struct wl_surface *surface, struct wl_array *keys)
{
   Kbd_Watch *w = data;

   (void)k; (void)serial; (void)keys;
   w->entered = surface;
   w->enters++;
}

static void
_kbd_leave(void *data, struct wl_keyboard *k, uint32_t serial,
           struct wl_surface *surface)
{
   Kbd_Watch *w = data;

   (void)k; (void)serial;
   if (w->entered == surface) w->entered = NULL;
   w->leaves++;
}

static void
_kbd_key(void *d, struct wl_keyboard *k, uint32_t se, uint32_t t, uint32_t ke, uint32_t st)
{ (void)d; (void)k; (void)se; (void)t; (void)ke; (void)st; }

static void
_kbd_mods(void *d, struct wl_keyboard *k, uint32_t se, uint32_t a, uint32_t b, uint32_t c, uint32_t g)
{ (void)d; (void)k; (void)se; (void)a; (void)b; (void)c; (void)g; }

static void
_kbd_repeat(void *d, struct wl_keyboard *k, int32_t r, int32_t delay)
{ (void)d; (void)k; (void)r; (void)delay; }

static const struct wl_keyboard_listener _kbd_listener =
{
   _kbd_keymap, _kbd_enter, _kbd_leave, _kbd_key, _kbd_mods, _kbd_repeat
};

int
main(void)
{
   Tk *tk;
   Tk_Toplevel *parent, *dialog;
   struct wl_seat *seat;
   struct wl_keyboard *kbd;
   Kbd_Watch w = { 0 };
   uint32_t seat_version;

   tk = tk_connect(PROG);

   seat_version = tk_global_version(tk, "wl_seat");
   if (!seat_version) tk_fail(tk, "no wl_seat");
   seat = tk_bind(tk, &wl_seat_interface, seat_version);
   if (!seat) tk_fail(tk, "wl_seat advertised but would not bind");
   kbd = wl_seat_get_keyboard(seat);
   if (!kbd) tk_fail(tk, "seat has no keyboard");
   wl_keyboard_add_listener(kbd, &_kbd_listener, &w);
   tk_sync(tk);

   parent = tk_toplevel_new(tk, "dialog-focus-parent", "parent", 200, 200);
   tk_settle(tk);
   tk_sync(tk);

   if (w.entered != tk_toplevel_surface(parent))
     tk_fail(tk, "the parent toplevel did not get the keyboard when it mapped "
                 "(%d enters, %d leaves) - nothing after this means anything",
             w.enters, w.leaves);

   /* The dialog. Its parent is set before it is mapped, which is what a
    * toolkit does. */
   dialog = tk_toplevel_new_child(tk, "dialog-focus-dialog", "dialog",
                                  120, 80, parent);
   tk_settle(tk);
   tk_sync(tk);

   if (!w.entered)
     tk_fail(tk, "after the dialog mapped nothing holds the keyboard "
                 "(%d enters, %d leaves)", w.enters, w.leaves);

   if (w.entered == tk_toplevel_surface(parent))
     tk_fail(tk, "the keyboard stayed on the parent when a child toplevel "
                 "mapped - a dialog can never be typed into. set_parent says "
                 "what a window belongs to, not that it declines input; that "
                 "is what an xdg_popup without a grab does");

   if (w.entered != tk_toplevel_surface(dialog))
     tk_fail(tk, "the keyboard went to neither the parent nor the dialog "
                 "(%d enters, %d leaves)", w.enters, w.leaves);

   printf(PROG ": ok - the dialog holds the keyboard (%d enters, %d leaves)\n",
          w.enters, w.leaves);

   tk_disconnect(tk);
   return 0;
}
