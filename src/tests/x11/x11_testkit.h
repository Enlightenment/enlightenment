/* Helpers shared by the X11 tests: each test is a single program, so these
 * are static and live in the header. */
#ifndef X11_TESTKIT_H
#define X11_TESTKIT_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

/* _NET_WM_MOVERESIZE directions and source, from the EWMH spec */
#define NET_WM_MOVERESIZE_SIZE_TOPLEFT     0
#define NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define NET_WM_MOVERESIZE_MOVE             8
#define NET_WM_SOURCE_APPLICATION          1

static const char *xtk_name = "x11-test";

static void
xtk_fail(const char *fmt, ...)
{
   va_list ap;

   fprintf(stderr, "%s: FAIL: ", xtk_name);
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fputc('\n', stderr);
   exit(1);
}

static Display *
xtk_connect(const char *name)
{
   Display *dpy;

   xtk_name = name;
   dpy = XOpenDisplay(NULL);
   if (!dpy) xtk_fail("cannot open the display");
   return dpy;
}

/* Gives E time to handle what was just sent: X has no round trip that
 * waits for the window manager. */
static void
xtk_settle(Display *dpy)
{
   XSync(dpy, False);
   usleep(300 * 1000);
}

static void
xtk_geometry_get(Display *dpy, Window win, int *x, int *y, int *w, int *h)
{
   XWindowAttributes attr;
   Window child;

   XSync(dpy, False);
   XTranslateCoordinates(dpy, win, DefaultRootWindow(dpy), 0, 0, x, y, &child);
   XGetWindowAttributes(dpy, win, &attr);
   if (w) *w = attr.width;
   if (h) *h = attr.height;
}

static void
xtk_pointer_move_to(Display *dpy, int x, int y)
{
   XTestFakeMotionEvent(dpy, DefaultScreen(dpy), x, y, CurrentTime);
   XSync(dpy, False);
   usleep(50 * 1000);
}

/* Moves the pointer by dx,dy in steps, as a hand would. */
static void
xtk_pointer_drag(Display *dpy, int x, int y, int dx, int dy)
{
   int i;

   for (i = 1; i <= 10; i++)
     xtk_pointer_move_to(dpy, x + (i * dx / 10), y + (i * dy / 10));
}

/* A window with no server-side frame, as a GTK header bar window asks for,
 * mapped where it asked to be. Returns once it is mapped. */
static Window
xtk_csd_window_new(Display *dpy, int x, int y, int w, int h, const char *title)
{
   /* _MOTIF_WM_HINTS: flags = decorations, decorations = none */
   long mwm[5] = { 2, 0, 0, 0, 0 };
   XSizeHints *hints;
   XEvent ev;
   Window win;
   Atom motif;

   win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), x, y, w, h, 0,
                             0, 0xffffff);
   hints = XAllocSizeHints();
   hints->flags = USPosition | PPosition;
   hints->x = x;
   hints->y = y;
   XSetWMNormalHints(dpy, win, hints);
   XFree(hints);
   XStoreName(dpy, win, title);

   motif = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
   XChangeProperty(dpy, win, motif, motif, 32, PropModeReplace,
                   (unsigned char *)mwm, 5);

   XSelectInput(dpy, win, StructureNotifyMask);
   XMapWindow(dpy, win);
   do XNextEvent(dpy, &ev); while (ev.type != MapNotify);
   xtk_settle(dpy);
   return win;
}

/* What a CSD client does on a press in its own decorations: it gives up
 * the implicit grab the press took and hands the drag to the window
 * manager. The pointer must already be at x,y. */
static void
xtk_csd_drag_begin(Display *dpy, Window win, int x, int y, int direction)
{
   XEvent ev;

   XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
   XSync(dpy, False);
   XUngrabPointer(dpy, CurrentTime);

   memset(&ev, 0, sizeof(ev));
   ev.xclient.type = ClientMessage;
   ev.xclient.window = win;
   ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
   ev.xclient.format = 32;
   ev.xclient.data.l[0] = x;
   ev.xclient.data.l[1] = y;
   ev.xclient.data.l[2] = direction;
   ev.xclient.data.l[3] = 1;
   ev.xclient.data.l[4] = NET_WM_SOURCE_APPLICATION;
   XSendEvent(dpy, DefaultRootWindow(dpy), False,
              SubstructureRedirectMask | SubstructureNotifyMask, &ev);
   xtk_settle(dpy);
}

/* E moves and resizes only the composited frame during a drag; the X
 * window follows when the button is released. */
static void
xtk_csd_drag_end(Display *dpy)
{
   XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
   xtk_settle(dpy);
}

#endif
