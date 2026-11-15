/* xdg_surface.destroy while its role object still exists.
 *
 * Both xdg-shell versions say the client must destroy the toplevel or popup
 * before the xdg_surface, and that doing otherwise raises defunct_surfaces on
 * the shell - xdg_wm_base, or zxdg_shell_v6. E used to raise it on the
 * xdg_surface instead, from its resource destructor: the client then got an
 * error on an object it had already destroyed, and every client that simply
 * disconnected with a window open was sent one on the way out.
 *
 * Each case needs a connection of its own, since a protocol error ends it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-shell-unstable-v6-client-protocol.h"

#define FAIL(fmt, ...) \
  do \
    { \
       fprintf(stderr, "test-xdg-defunct: " fmt "\n", ##__VA_ARGS__); \
       return 1; \
    } \
  while (0)

#define UNUSED __attribute__((unused))

typedef struct
{
   struct wl_display *disp;
   struct wl_compositor *compositor;
   struct xdg_wm_base *wm_base;
   struct zxdg_shell_v6 *shell_v6;
} Conn;

static void
_global_add(void *data, struct wl_registry *reg, uint32_t id,
            const char *iface, uint32_t version UNUSED)
{
   Conn *c = data;

   if (!strcmp(iface, "wl_compositor"))
     c->compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 4);
   else if (!strcmp(iface, "xdg_wm_base"))
     c->wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, 1);
   else if (!strcmp(iface, "zxdg_shell_v6"))
     c->shell_v6 = wl_registry_bind(reg, id, &zxdg_shell_v6_interface, 1);
}

static void
_global_remove(void *data UNUSED, struct wl_registry *reg UNUSED,
               uint32_t id UNUSED)
{
}

static const struct wl_registry_listener _registry_listener =
{
   _global_add, _global_remove
};

static int
_connect(Conn *c)
{
   struct wl_registry *reg;

   memset(c, 0, sizeof(*c));
   c->disp = wl_display_connect(NULL);
   if (!c->disp) FAIL("cannot connect to the compositor");
   reg = wl_display_get_registry(c->disp);
   wl_registry_add_listener(reg, &_registry_listener, c);
   if (wl_display_roundtrip(c->disp) < 0) FAIL("registry roundtrip failed");
   if (!c->compositor) FAIL("no wl_compositor");
   if (!c->wm_base) FAIL("no xdg_wm_base");
   if (!c->shell_v6) FAIL("no zxdg_shell_v6");
   return 0;
}

static int
_expect_defunct(Conn *c, const struct wl_interface *shell, uint32_t code)
{
   const struct wl_interface *iface = NULL;
   uint32_t got;

   if (wl_display_roundtrip(c->disp) >= 0)
     FAIL("destroying the xdg_surface before its toplevel was accepted "
          "(%s)", shell->name);
   got = wl_display_get_protocol_error(c->disp, &iface, NULL);
   if ((iface != shell) || (got != code))
     FAIL("expected %s.defunct_surfaces (%u), got error %u on %s",
          shell->name, code, got, iface ? iface->name : "no object");
   wl_display_disconnect(c->disp);
   return 0;
}

static int
_test_stable(int role_first)
{
   Conn c;
   struct wl_surface *surface;
   struct xdg_surface *xdg_surface;
   struct xdg_toplevel *toplevel;

   if (_connect(&c)) return 1;
   surface = wl_compositor_create_surface(c.compositor);
   xdg_surface = xdg_wm_base_get_xdg_surface(c.wm_base, surface);
   toplevel = xdg_surface_get_toplevel(xdg_surface);
   if (wl_display_roundtrip(c.disp) < 0) FAIL("roundtrip failed creating a toplevel");

   if (!role_first)
     {
        xdg_surface_destroy(xdg_surface);
        return _expect_defunct(&c, &xdg_wm_base_interface,
                               XDG_WM_BASE_ERROR_DEFUNCT_SURFACES);
     }

   xdg_toplevel_destroy(toplevel);
   xdg_surface_destroy(xdg_surface);
   wl_surface_destroy(surface);
   if (wl_display_roundtrip(c.disp) < 0)
     FAIL("destroying a toplevel, then its xdg_surface, raised error %u",
          wl_display_get_error(c.disp));
   wl_display_disconnect(c.disp);
   return 0;
}

static int
_test_v6(int role_first)
{
   Conn c;
   struct wl_surface *surface;
   struct zxdg_surface_v6 *xdg_surface;
   struct zxdg_toplevel_v6 *toplevel;

   if (_connect(&c)) return 1;
   surface = wl_compositor_create_surface(c.compositor);
   xdg_surface = zxdg_shell_v6_get_xdg_surface(c.shell_v6, surface);
   toplevel = zxdg_surface_v6_get_toplevel(xdg_surface);
   if (wl_display_roundtrip(c.disp) < 0) FAIL("roundtrip failed creating a v6 toplevel");

   if (!role_first)
     {
        zxdg_surface_v6_destroy(xdg_surface);
        return _expect_defunct(&c, &zxdg_shell_v6_interface,
                               ZXDG_SHELL_V6_ERROR_DEFUNCT_SURFACES);
     }

   zxdg_toplevel_v6_destroy(toplevel);
   zxdg_surface_v6_destroy(xdg_surface);
   wl_surface_destroy(surface);
   if (wl_display_roundtrip(c.disp) < 0)
     FAIL("destroying a v6 toplevel, then its xdg_surface, raised error %u",
          wl_display_get_error(c.disp));
   wl_display_disconnect(c.disp);
   return 0;
}

int
main(void)
{
   if (_test_stable(1)) return 1;
   if (_test_stable(0)) return 1;
   if (_test_v6(1)) return 1;
   if (_test_v6(0)) return 1;
   return 0;
}
