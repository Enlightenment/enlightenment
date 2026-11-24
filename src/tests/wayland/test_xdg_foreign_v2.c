/* xdg-foreign v2, and handles crossing between v1 and v2: the imported
 * window becomes the parent of the importer's toplevel. */
#include <stdio.h>
#include <string.h>

#include "e_wl_testkit.h"
#include "xdg-foreign-unstable-v1-client-protocol.h"
#include "xdg-foreign-unstable-v2-client-protocol.h"

#define PROG "test-xdg-foreign-v2"
#define APP_PARENT "e.test.foreign.parent"
#define APP_CHILD "e.test.foreign.child"

typedef struct
{
   Tk *tk;
   Tk_Toplevel *top;
   struct zxdg_exporter_v1 *exporter_v1;
   struct zxdg_importer_v1 *importer_v1;
   struct zxdg_exporter_v2 *exporter_v2;
   struct zxdg_importer_v2 *importer_v2;
} Peer;

static char handle[256];
static int destroyed;

static void
_handle_v1(void *d __attribute__((unused)), struct zxdg_exported_v1 *e __attribute__((unused)),
           const char *h)
{
   snprintf(handle, sizeof(handle), "%s", h);
}

static void
_handle_v2(void *d __attribute__((unused)), struct zxdg_exported_v2 *e __attribute__((unused)),
           const char *h)
{
   snprintf(handle, sizeof(handle), "%s", h);
}

static void
_destroyed_v1(void *d __attribute__((unused)), struct zxdg_imported_v1 *i __attribute__((unused)))
{
   destroyed++;
}

static void
_destroyed_v2(void *d __attribute__((unused)), struct zxdg_imported_v2 *i __attribute__((unused)))
{
   destroyed++;
}

static const struct zxdg_exported_v1_listener _exported_v1_listener = { _handle_v1 };
static const struct zxdg_exported_v2_listener _exported_v2_listener = { _handle_v2 };
static const struct zxdg_imported_v1_listener _imported_v1_listener = { _destroyed_v1 };
static const struct zxdg_imported_v2_listener _imported_v2_listener = { _destroyed_v2 };

static void
_peer_new(Peer *p, const char *app_id)
{
   p->tk = tk_connect(PROG);
   p->exporter_v1 = tk_bind(p->tk, &zxdg_exporter_v1_interface, 1);
   p->importer_v1 = tk_bind(p->tk, &zxdg_importer_v1_interface, 1);
   p->exporter_v2 = tk_bind(p->tk, &zxdg_exporter_v2_interface, 1);
   p->importer_v2 = tk_bind(p->tk, &zxdg_importer_v2_interface, 1);
   if ((!p->exporter_v1) || (!p->importer_v1) || (!p->exporter_v2) || (!p->importer_v2))
     tk_fail(p->tk, "missing xdg-foreign global");
   p->top = tk_toplevel_new(p->tk, app_id, app_id, 200, 150);
}

/* Export with one version, import with the other, and check the parent.
 * Returns the exported object so the caller decides when it goes. */
static void *
_export(Peer *p, int v2)
{
   void *obj;

   handle[0] = 0;
   if (v2)
     {
        obj = zxdg_exporter_v2_export_toplevel(p->exporter_v2, tk_toplevel_surface(p->top));
        zxdg_exported_v2_add_listener(obj, &_exported_v2_listener, NULL);
     }
   else
     {
        obj = zxdg_exporter_v1_export(p->exporter_v1, tk_toplevel_surface(p->top));
        zxdg_exported_v1_add_listener(obj, &_exported_v1_listener, NULL);
     }
   tk_sync(p->tk);
   if (!handle[0]) tk_fail(p->tk, "no handle for a v%d export", v2 ? 2 : 1);
   return obj;
}

static void *
_import(Peer *p, int v2, struct wl_surface *child)
{
   void *obj;

   if (v2)
     {
        obj = zxdg_importer_v2_import_toplevel(p->importer_v2, handle);
        zxdg_imported_v2_add_listener(obj, &_imported_v2_listener, NULL);
        if (child) zxdg_imported_v2_set_parent_of(obj, child);
     }
   else
     {
        obj = zxdg_importer_v1_import(p->importer_v1, handle);
        zxdg_imported_v1_add_listener(obj, &_imported_v1_listener, NULL);
        if (child) zxdg_imported_v1_set_parent_of(obj, child);
     }
   tk_settle(p->tk);
   return obj;
}

static void
_imported_destroy(void *obj, int v2)
{
   if (v2) zxdg_imported_v2_destroy(obj);
   else zxdg_imported_v1_destroy(obj);
}

static void
_exported_destroy(void *obj, int v2)
{
   if (v2) zxdg_exported_v2_destroy(obj);
   else zxdg_exported_v1_destroy(obj);
}

static void
_test_pair(Peer *parent, Peer *child, int ex_v2, int im_v2)
{
   unsigned int parent_id = tk_expect(parent->tk, APP_PARENT)->id;
   void *ex, *im;

   ex = _export(parent, ex_v2);
   im = _import(child, im_v2, tk_toplevel_surface(child->top));
   if (tk_expect(child->tk, APP_CHILD)->parent != parent_id)
     tk_fail(child->tk, "export v%d, import v%d: not the parent", ex_v2 + 1, im_v2 + 1);
   _imported_destroy(im, im_v2);
   tk_settle(child->tk);
   if (tk_expect(child->tk, APP_CHILD)->parent)
     tk_fail(child->tk, "export v%d, import v%d: still the parent after destroy",
             ex_v2 + 1, im_v2 + 1);
   _exported_destroy(ex, ex_v2);
   tk_sync(parent->tk);
}

/* A popup role, which v2 refuses where it wants a toplevel. */
static struct wl_surface *
_popup_new(Peer *p, Tk_Toplevel *parent)
{
   struct xdg_positioner *pos;
   struct xdg_surface *xs;
   struct wl_surface *s;

   pos = xdg_wm_base_create_positioner(tk_wm_base(p->tk));
   xdg_positioner_set_size(pos, 60, 40);
   xdg_positioner_set_anchor_rect(pos, 10, 10, 1, 1);
   s = tk_surface_new(p->tk);
   xs = xdg_wm_base_get_xdg_surface(tk_wm_base(p->tk), s);
   xdg_surface_get_popup(xs, tk_toplevel_xdg_surface(parent), pos);
   return s;
}

static void
_expect_error(Peer *p, const struct wl_interface *want, const char *what)
{
   const struct wl_interface *iface = NULL;

   if ((tk_protocol_error(p->tk, &iface) != 0) || (iface != want))
     tk_fail(p->tk, "%s is not an invalid_surface error", what);
   tk_disconnect(p->tk);
}

int
main(void)
{
   Peer parent, child, bad;
   void *ex, *im;

   _peer_new(&parent, APP_PARENT);
   _peer_new(&child, APP_CHILD);

   _test_pair(&parent, &child, 1, 1);
   _test_pair(&parent, &child, 0, 1);
   _test_pair(&parent, &child, 1, 0);

   /* The exporter going away tells a v2 importer on its own interface. */
   ex = _export(&parent, 1);
   destroyed = 0;
   im = _import(&child, 1, tk_toplevel_surface(child.top));
   _exported_destroy(ex, 1);
   tk_sync(parent.tk);
   tk_sync(child.tk);
   if (destroyed != 1) tk_fail(child.tk, "no destroyed after the export went");
   _imported_destroy(im, 1);

   /* A handle nobody exported: destroyed, and set_parent_of sent before
    * the client could read it is ignored. */
   destroyed = 0;
   snprintf(handle, sizeof(handle), "no-such-handle");
   im = _import(&child, 1, tk_toplevel_surface(child.top));
   if (destroyed != 1) tk_fail(child.tk, "a failed v2 import was not destroyed");
   _imported_destroy(im, 1);

   _peer_new(&bad, "e.test.foreign.bad");
   zxdg_exporter_v2_export_toplevel(bad.exporter_v2, _popup_new(&bad, bad.top));
   _expect_error(&bad, &zxdg_exporter_v2_interface, "exporting a popup");

   ex = _export(&parent, 1);
   _peer_new(&bad, "e.test.foreign.bad");
   im = zxdg_importer_v2_import_toplevel(bad.importer_v2, handle);
   zxdg_imported_v2_set_parent_of(im, _popup_new(&bad, bad.top));
   _expect_error(&bad, &zxdg_imported_v2_interface, "set_parent_of a popup");
   _exported_destroy(ex, 1);
   tk_sync(parent.tk);

   tk_disconnect(child.tk);
   tk_disconnect(parent.tk);
   printf(PROG ": ok\n");
   return 0;
}
