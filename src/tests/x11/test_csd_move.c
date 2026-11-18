/* Move a client-side-decorated window with _NET_WM_MOVERESIZE, the way GTK
 * starts a drag from its header bar. E records where the drag starts from
 * the message itself, then runs window_move with no mouse event: the window
 * must follow the pointer by how far the pointer travelled. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#define W 300
#define H 200
#define DX 60
#define DY 40
#define STEPS 10

/* _NET_WM_MOVERESIZE in the EWMH spec */
#define NET_WM_MOVERESIZE_MOVE 8
#define NET_WM_SOURCE_APPLICATION 1

static void
fail(const char *fmt, ...)
{
   va_list ap;

   fprintf(stderr, "test-x11-csd-move: FAIL: ");
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fputc('\n', stderr);
   exit(1);
}

static void
root_pos_get(Display *dpy, Window win, int *x, int *y)
{
   Window child;

   XSync(dpy, False);
   XTranslateCoordinates(dpy, win, DefaultRootWindow(dpy), 0, 0, x, y, &child);
}

static void
settle(Display *dpy)
{
   XSync(dpy, False);
   usleep(300 * 1000);
}

static void
pointer_move_to(Display *dpy, int x, int y)
{
   XTestFakeMotionEvent(dpy, DefaultScreen(dpy), x, y, CurrentTime);
   XSync(dpy, False);
   usleep(50 * 1000);
}

static void
moveresize_send(Display *dpy, Window win, int x, int y, int button)
{
   XEvent ev;

   memset(&ev, 0, sizeof(ev));
   ev.xclient.type = ClientMessage;
   ev.xclient.window = win;
   ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
   ev.xclient.format = 32;
   ev.xclient.data.l[0] = x;
   ev.xclient.data.l[1] = y;
   ev.xclient.data.l[2] = NET_WM_MOVERESIZE_MOVE;
   ev.xclient.data.l[3] = button;
   ev.xclient.data.l[4] = NET_WM_SOURCE_APPLICATION;
   XSendEvent(dpy, DefaultRootWindow(dpy), False,
              SubstructureRedirectMask | SubstructureNotifyMask, &ev);
}

int
main(void)
{
   Display *dpy;
   Window win;
   XSizeHints *hints;
   XEvent ev;
   Atom motif;
   /* _MOTIF_WM_HINTS: flags = decorations, decorations = none */
   long mwm[5] = { 2, 0, 0, 0, 0 };
   int x0, y0, x1, y1, px, py, i;

   dpy = XOpenDisplay(NULL);
   if (!dpy) fail("cannot open the display");

   /* Bottom right, which is the right-hand screen with two heads: a drag
    * that loses its starting point falls back to 0,0 and has to show. */
   x0 = DisplayWidth(dpy, DefaultScreen(dpy)) - W - 150;
   y0 = DisplayHeight(dpy, DefaultScreen(dpy)) - H - 150;
   win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), x0, y0, W, H, 0,
                             0, 0xffffff);
   hints = XAllocSizeHints();
   hints->flags = USPosition | PPosition;
   hints->x = x0;
   hints->y = y0;
   XSetWMNormalHints(dpy, win, hints);
   XFree(hints);
   XStoreName(dpy, win, "csd move under test");

   motif = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
   XChangeProperty(dpy, win, motif, motif, 32, PropModeReplace,
                   (unsigned char *)mwm, 5);

   XSelectInput(dpy, win, StructureNotifyMask);
   XMapWindow(dpy, win);
   do XNextEvent(dpy, &ev); while (ev.type != MapNotify);
   settle(dpy);
   root_pos_get(dpy, win, &x0, &y0);

   /* A press on the header bar; the client then gives up the implicit
    * grab and hands the drag to the window manager. */
   px = x0 + (W / 2);
   py = y0 + 10;
   pointer_move_to(dpy, px, py);
   settle(dpy);
   XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
   XSync(dpy, False);
   XUngrabPointer(dpy, CurrentTime);
   moveresize_send(dpy, win, px, py, 1);
   settle(dpy);

   for (i = 1; i <= STEPS; i++)
     pointer_move_to(dpy, px + (i * DX / STEPS), py + (i * DY / STEPS));
   settle(dpy);

   /* E moves only the composited frame during the drag; the X window
    * follows when the button is released. */
   XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
   settle(dpy);
   root_pos_get(dpy, win, &x1, &y1);

   if ((x1 != x0 + DX) || (y1 != y0 + DY))
     fail("a %d,%d drag from the header bar moved the window from +%d+%d "
          "to +%d+%d, expected +%d+%d", DX, DY, x0, y0, x1, y1,
          x0 + DX, y0 + DY);

   printf("test-x11-csd-move: ok (+%d+%d -> +%d+%d)\n", x0, y0, x1, y1);
   XCloseDisplay(dpy);
   return 0;
}
