/* Shared plumbing for the Wayland test clients: registry binding, toplevel
 * construction, window enumeration and a failure path that dumps the
 * compositor's window list. Nothing here sleeps: wl_test.sync runs a
 * compositor main loop iteration before it answers. */
#ifndef E_WL_TESTKIT_H
#define E_WL_TESTKIT_H

#include <wayland-client.h>
#include "wl-test-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define TK_MAX_CLIENTS 64
#define TK_MAX_GLOBALS 64
#define TK_WL_TEST_VERSION 4
#define TK_XDG_WM_BASE_VERSION 6

/* Mirrors wl_test.client_info. */
typedef struct
{
   unsigned int id;
   int pid;
   char title[256];
   char app_id[256];
   unsigned int parent;
   int x, y, w, h;
   unsigned int states;
   unsigned int output;
} Tk_Client;

typedef struct _Tk Tk;

typedef struct _Tk_Toplevel Tk_Toplevel;

/* Never returns NULL: anything missing fails the test. progname prefixes
 * every message. */
Tk *tk_connect(const char *progname);
void tk_disconnect(Tk *tk);

/* Print, dump every window the compositor knows about, and exit non-zero. */
void tk_fail(Tk *tk, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));

/* Map a toplevel and return once the compositor has a window for it. It
 * answers every configure for the life of the connection, by acking it and
 * committing a buffer of the configured size: E only applies a state such
 * as maximized once the client has done that. */
Tk_Toplevel *tk_toplevel_new(Tk *tk, const char *app_id, const char *title,
                             int w, int h);

/* A toplevel with a parent, set before its first commit: a dialog, not a
 * popup. */
Tk_Toplevel *tk_toplevel_new_child(Tk *tk, const char *app_id,
                                   const char *title, int w, int h,
                                   Tk_Toplevel *parent);

/* A toplevel that draws a shadow: its surface is larger than its window
 * geometry by this much on every side, and it says so with
 * xdg_surface.set_window_geometry, as client-side decorations do. */
Tk_Toplevel *tk_toplevel_new_shadowed(Tk *tk, const char *app_id,
                                      const char *title, int w, int h,
                                      int shadow);

/* Ask to be maximized or restored, as the client. */
void tk_toplevel_maximize(Tk_Toplevel *top, int on);

/* The size the compositor last asked this toplevel to be, and the number of
 * configures it has answered. */
void tk_toplevel_configured(Tk_Toplevel *top, int *w, int *h, int *count);

/* Commit a new buffer of this size, without waiting for anything. The kit
 * destroys a buffer as soon as it is released, as real clients do. */
void tk_toplevel_paint(Tk_Toplevel *top, int w, int h);

/* The xdg_toplevel states of the last configure answered, as a mask of
 * 1 << XDG_TOPLEVEL_STATE_*. */
unsigned int tk_toplevel_states(Tk_Toplevel *top);

/* The last xdg_toplevel.configure_bounds, 0x0 if none, and the
 * wm_capabilities as a mask of 1 << XDG_TOPLEVEL_WM_CAPABILITIES_*. */
void tk_toplevel_bounds(Tk_Toplevel *top, int *w, int *h);
unsigned int tk_toplevel_wm_capabilities(Tk_Toplevel *top);

/* The size this toplevel last painted at. Not always the last configure:
 * E ends an interactive resize with a 0x0 "you choose" configure. */
void tk_toplevel_painted(Tk_Toplevel *top, int *w, int *h);

/* Unmap and destroy it, and settle. */
void tk_toplevel_del(Tk_Toplevel *top);

/* The wl_surface, xdg_toplevel and xdg_surface behind a toplevel, owned by
 * the kit. */
struct wl_surface *tk_toplevel_surface(Tk_Toplevel *top);
struct xdg_toplevel *tk_toplevel_xdg(Tk_Toplevel *top);
struct xdg_surface *tk_toplevel_xdg_surface(Tk_Toplevel *top);

/* Unmap by committing a null buffer, then map again the way xdg-shell
 * requires: an empty commit, and a buffer in answer to the configure. */
void tk_toplevel_remap(Tk_Toplevel *top);

/* Round-trip, and return the code of the protocol error that ended the
 * connection, or -1 if there was none. The connection is unusable after an
 * error: only tk_disconnect it. */
int tk_protocol_error(Tk *tk, const struct wl_interface **iface);

/* The kit's xdg_wm_base, which answers pings, for building surfaces the kit
 * has no constructor for. */
struct xdg_wm_base *tk_wm_base(Tk *tk);

/* The version the compositor advertised for a global, or 0 if none. */
uint32_t tk_global_version(Tk *tk, const char *iface);

/* Bind a global the kit does not know about, at exactly this version, or
 * return NULL if it is not advertised. It does not clamp to what is
 * offered: check tk_global_version first when the version matters. */
void *tk_bind(Tk *tk, const struct wl_interface *iface, uint32_t version);

/* Round-trip through the compositor's own main loop, so effects E applies from
 * a job or an idler have landed. */
void tk_sync(Tk *tk);

/* For a test that has to dispatch on its own, waiting for an event the kit
 * does not track. */
struct wl_display *tk_display(Tk *tk);

/* Sync until a change that needs the client's answer has landed: one round
 * trip carries the configure out and the commit back, one more lets the
 * compositor apply it. */
void tk_settle(Tk *tk);

/* Enumerate. Returns how many windows were reported, filling up to max. */
int tk_clients(Tk *tk, Tk_Client *out, int max);

/* Enumerate and return the one window with this app_id, or NULL. The result
 * points into the kit and is invalidated by the next call. */
Tk_Client *tk_find(Tk *tk, const char *app_id);

/* Like tk_find, but a miss is a failure rather than a NULL to check. */
Tk_Client *tk_expect(Tk *tk, const char *app_id);

/* Some of what E does to a window is animated by the theme. Waits, one
 * compositor main loop iteration at a time, until the window's states
 * masked by mask are want, and fails saying what it waited for if that
 * takes more than TK_WAIT_SECONDS. */
#define TK_WAIT_SECONDS 10
Tk_Client *tk_wait_states(Tk *tk, const char *app_id, unsigned int mask,
                          unsigned int want, const char *what);

/* Run one of E's own actions on a window. Empty or NULL params means none. */
void tk_action(Tk *tk, unsigned int id, const char *name, const char *params);

/* Finish an action that took a grab, as a binding does on release. */
void tk_action_end(Tk *tk, unsigned int id, const char *name, const char *params);

/* Put the pointer somewhere, or move it by a delta. */
void tk_pointer_warp(Tk *tk, int x, int y);
void tk_pointer_move(Tk *tk, int dx, int dy);

/* A bare wl_surface, and a fresh buffer of this size attached, damaged and
 * committed to one. */
struct wl_surface *tk_surface_new(Tk *tk);
void tk_surface_paint(Tk *tk, struct wl_surface *surface, int w, int h);

/* Where the compositor has a surface, in compositor coordinates. */
void tk_surface_geometry(Tk *tk, struct wl_surface *surface,
                         int *x, int *y, int *w, int *h);

/* Press or release a linux/input.h BTN_* button. */
void tk_pointer_button(Tk *tk, unsigned int button, int pressed);

/* Put a finger down at a compositor position, or lift it. */
void tk_touch_down(Tk *tk, int id, int x, int y);
void tk_touch_up(Tk *tk, int id);

#endif
