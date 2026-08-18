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
#define TK_WL_TEST_VERSION 3

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

/* The size the compositor last asked this toplevel to be, and the number of
 * configures it has answered. */
void tk_toplevel_configured(Tk_Toplevel *top, int *w, int *h, int *count);

/* Unmap and destroy it, and settle. */
void tk_toplevel_del(Tk_Toplevel *top);

/* The wl_surface behind a toplevel, owned by the kit. */
struct wl_surface *tk_toplevel_surface(Tk_Toplevel *top);

/* The version the compositor advertised for a global, or 0 if none. */
uint32_t tk_global_version(Tk *tk, const char *iface);

/* Bind a global the kit does not know about, at exactly this version, or
 * return NULL if it is not advertised. It does not clamp to what is
 * offered: check tk_global_version first when the version matters. */
void *tk_bind(Tk *tk, const struct wl_interface *iface, uint32_t version);

/* Round-trip through the compositor's own main loop, so effects E applies from
 * a job or an idler have landed. */
void tk_sync(Tk *tk);

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

/* Run one of E's own actions on a window. Empty or NULL params means none. */
void tk_action(Tk *tk, unsigned int id, const char *name, const char *params);

#endif
