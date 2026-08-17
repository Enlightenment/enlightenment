/* The test clients do not include config.h, which is where the rest of the
 * tree gets _GNU_SOURCE, and memfd_create needs it. */
#define _GNU_SOURCE 1
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "e_wl_testkit.h"

struct _Tk_Toplevel
{
   Tk *tk;
   struct wl_surface *surface;
   struct xdg_surface *xdg_surface;
   struct xdg_toplevel *toplevel;
   int w, h;
   int pending_w, pending_h;
   int configures;
};

struct _Tk
{
   const char *prog;

   struct wl_display *disp;
   struct wl_registry *registry;
   struct wl_compositor *compositor;
   struct wl_shm *shm;
   struct xdg_wm_base *wm_base;
   struct wl_test *tester;
   uint32_t tester_version;

   struct
     {
        char iface[64];
        uint32_t name;
        uint32_t version;
     } globals[TK_MAX_GLOBALS];
   int global_count;

   Tk_Client clients[TK_MAX_CLIENTS];
   int client_count;
   int done_count;

   int configured;
};

static void
_client_serial(void *data, struct wl_test *t, uint32_t serial)
{
   (void)data; (void)t; (void)serial;
}

static void
_surface_info(void *data, struct wl_test *t, struct wl_surface *s,
              int32_t x, int32_t y, int32_t w, int32_t h,
              uint32_t vis, uint32_t foc)
{
   (void)data; (void)t; (void)s; (void)x; (void)y; (void)w; (void)h;
   (void)vis; (void)foc;
}

static void
_surface_unknown(void *data, struct wl_test *t, struct wl_surface *s)
{
   (void)data; (void)t; (void)s;
}

static void
_sync_done(void *data, struct wl_test *t)
{
   (void)data; (void)t;
}

static void
_client_info(void *data, struct wl_test *t, uint32_t id, int32_t pid,
             const char *title, const char *app_id, uint32_t parent,
             int32_t x, int32_t y, int32_t w, int32_t h,
             uint32_t states, uint32_t output)
{
   Tk *tk = data;
   Tk_Client *c;

   (void)t;
   if (tk->client_count >= TK_MAX_CLIENTS) return;

   c = &tk->clients[tk->client_count++];
   c->id = id;
   c->pid = pid;
   snprintf(c->title, sizeof(c->title), "%s", title ?: "");
   snprintf(c->app_id, sizeof(c->app_id), "%s", app_id ?: "");
   c->parent = parent;
   c->x = x; c->y = y; c->w = w; c->h = h;
   c->states = states;
   c->output = output;
}

static void
_clients_done(void *data, struct wl_test *t)
{
   Tk *tk = data;

   (void)t;
   tk->done_count++;
}

static const struct wl_test_listener _test_listener =
{
   _client_serial,
   _surface_info,
   _surface_unknown,
   _sync_done,
   _client_info,
   _clients_done,
};

static void
_wm_base_ping(void *data, struct xdg_wm_base *b, uint32_t serial)
{
   (void)data;
   xdg_wm_base_pong(b, serial);
}

static const struct xdg_wm_base_listener _wm_base_listener = { _wm_base_ping };

static struct wl_buffer *_buffer_make(Tk *tk, int w, int h);

static void
_xdg_surface_configure(void *data, struct xdg_surface *s, uint32_t serial)
{
   Tk_Toplevel *top = data;
   struct wl_buffer *buffer;

   xdg_surface_ack_configure(s, serial);

   if (top->pending_w > 0) top->w = top->pending_w;
   if (top->pending_h > 0) top->h = top->pending_h;

   buffer = _buffer_make(top->tk, top->w, top->h);
   if (!buffer) tk_fail(top->tk, "cannot make a %dx%d buffer", top->w, top->h);

   wl_surface_attach(top->surface, buffer, 0, 0);
   wl_surface_damage(top->surface, 0, 0, top->w, top->h);
   wl_surface_commit(top->surface);

   top->configures++;
}

static const struct xdg_surface_listener _xdg_surface_listener =
{
   _xdg_surface_configure
};

static void
_toplevel_configure(void *data, struct xdg_toplevel *t, int32_t w, int32_t h,
                    struct wl_array *states)
{
   Tk_Toplevel *top = data;

   (void)t; (void)states;
   top->pending_w = w;
   top->pending_h = h;
}

static void
_toplevel_close(void *data, struct xdg_toplevel *t)
{
   (void)data; (void)t;
}

static const struct xdg_toplevel_listener _toplevel_listener =
{
   _toplevel_configure,
   _toplevel_close
};

static void
_global_add(void *data, struct wl_registry *reg, uint32_t id,
            const char *iface, uint32_t version)
{
   Tk *tk = data;

   if (tk->global_count < TK_MAX_GLOBALS)
     {
        snprintf(tk->globals[tk->global_count].iface,
                 sizeof(tk->globals[tk->global_count].iface), "%s", iface);
        tk->globals[tk->global_count].name = id;
        tk->globals[tk->global_count].version = version;
        tk->global_count++;
     }

   if (!strcmp(iface, "wl_compositor"))
     tk->compositor = wl_registry_bind(reg, id, &wl_compositor_interface,
                                       version < 4 ? version : 4);
   else if (!strcmp(iface, "wl_shm"))
     tk->shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
   else if (!strcmp(iface, "xdg_wm_base"))
     tk->wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, 1);
   else if (!strcmp(iface, "wl_test"))
     {
        tk->tester_version = version;
        if (version >= TK_WL_TEST_VERSION)
          tk->tester = wl_registry_bind(reg, id, &wl_test_interface,
                                        TK_WL_TEST_VERSION);
     }
}

static void
_global_remove(void *data, struct wl_registry *reg, uint32_t id)
{
   (void)data; (void)reg; (void)id;
}

static const struct wl_registry_listener _registry_listener =
{
   _global_add, _global_remove
};

static void
_buffer_release(void *data, struct wl_buffer *buffer)
{
   (void)data;
   wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener _buffer_listener = { _buffer_release };

static struct wl_buffer *
_buffer_make(Tk *tk, int w, int h)
{
   struct wl_shm_pool *pool;
   struct wl_buffer *buf;
   int stride = w * 4, size = stride * h, fd;
   void *map;

   fd = memfd_create("e-wl-testkit", MFD_CLOEXEC);
   if (fd < 0) return NULL;
   if (ftruncate(fd, size) < 0) { close(fd); return NULL; }

   map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (map == MAP_FAILED) { close(fd); return NULL; }
   memset(map, 0xff, size);
   munmap(map, size);

   pool = wl_shm_create_pool(tk->shm, fd, size);
   buf = wl_shm_pool_create_buffer(pool, 0, w, h, stride,
                                   WL_SHM_FORMAT_ARGB8888);
   wl_shm_pool_destroy(pool);
   close(fd);
   wl_buffer_add_listener(buf, &_buffer_listener, NULL);
   return buf;
}

Tk *
tk_connect(const char *progname)
{
   Tk *tk;

   tk = calloc(1, sizeof(*tk));
   if (!tk)
     {
        fprintf(stderr, "%s: out of memory\n", progname);
        exit(1);
     }
   tk->prog = progname;

   tk->disp = wl_display_connect(NULL);
   if (!tk->disp)
     {
        fprintf(stderr, "%s: cannot connect to WAYLAND_DISPLAY=%s\n",
                progname, getenv("WAYLAND_DISPLAY") ?: "(unset)");
        exit(1);
     }

   tk->registry = wl_display_get_registry(tk->disp);
   wl_registry_add_listener(tk->registry, &_registry_listener, tk);
   wl_display_roundtrip(tk->disp);

   if (!tk->compositor) tk_fail(tk, "no wl_compositor");
   if (!tk->shm) tk_fail(tk, "no wl_shm");
   if (!tk->wm_base) tk_fail(tk, "no xdg_wm_base");
   if (!tk->tester)
     tk_fail(tk, "no wl_test at version %d (advertised %u); is the wl_test "
                 "module loaded?", TK_WL_TEST_VERSION, tk->tester_version);

   wl_test_add_listener(tk->tester, &_test_listener, tk);
   xdg_wm_base_add_listener(tk->wm_base, &_wm_base_listener, tk);

   return tk;
}

void
tk_disconnect(Tk *tk)
{
   if (!tk) return;
   if (tk->disp) wl_display_disconnect(tk->disp);
   free(tk);
}

void
tk_fail(Tk *tk, const char *fmt, ...)
{
   va_list args;
   int i;

   fprintf(stderr, "%s: ", tk->prog);
   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
   fprintf(stderr, "\n");

   if (tk->tester)
     {
        int n = tk_clients(tk, NULL, 0);

        fprintf(stderr, "%s: compositor knows %d window(s):\n", tk->prog, n);
        for (i = 0; i < tk->client_count; i++)
          fprintf(stderr,
                  "  id=%u pid=%d app_id='%s' title='%s' %dx%d+%d+%d "
                  "states=0x%x parent=%u output=%u\n",
                  tk->clients[i].id, tk->clients[i].pid,
                  tk->clients[i].app_id, tk->clients[i].title,
                  tk->clients[i].w, tk->clients[i].h,
                  tk->clients[i].x, tk->clients[i].y,
                  tk->clients[i].states, tk->clients[i].parent,
                  tk->clients[i].output);
     }

   exit(1);
}

static Tk_Toplevel *
_toplevel_make(Tk *tk, const char *app_id, const char *title, int w, int h,
               Tk_Toplevel *parent)
{
   Tk_Toplevel *top;

   top = calloc(1, sizeof(*top));
   if (!top) tk_fail(tk, "out of memory");
   top->tk = tk;
   top->w = w;
   top->h = h;

   top->surface = wl_compositor_create_surface(tk->compositor);
   top->xdg_surface = xdg_wm_base_get_xdg_surface(tk->wm_base, top->surface);
   xdg_surface_add_listener(top->xdg_surface, &_xdg_surface_listener, top);
   top->toplevel = xdg_surface_get_toplevel(top->xdg_surface);
   xdg_toplevel_add_listener(top->toplevel, &_toplevel_listener, top);
   if (title) xdg_toplevel_set_title(top->toplevel, title);
   if (app_id) xdg_toplevel_set_app_id(top->toplevel, app_id);
   if (parent) xdg_toplevel_set_parent(top->toplevel, parent->toplevel);

   wl_surface_commit(top->surface);
   while (top->configures < 1)
     if (wl_display_dispatch(tk->disp) < 0)
       tk_fail(tk, "disconnected waiting for the initial configure");

   tk_settle(tk);
   return top;
}

Tk_Toplevel *
tk_toplevel_new(Tk *tk, const char *app_id, const char *title, int w, int h)
{
   return _toplevel_make(tk, app_id, title, w, h, NULL);
}

Tk_Toplevel *
tk_toplevel_new_child(Tk *tk, const char *app_id, const char *title,
                      int w, int h, Tk_Toplevel *parent)
{
   return _toplevel_make(tk, app_id, title, w, h, parent);
}

void
tk_toplevel_paint(Tk_Toplevel *top, int w, int h)
{
   struct wl_buffer *buffer;

   buffer = _buffer_make(top->tk, w, h);
   if (!buffer) tk_fail(top->tk, "cannot make a %dx%d buffer", w, h);
   top->w = w;
   top->h = h;
   wl_surface_attach(top->surface, buffer, 0, 0);
   wl_surface_damage(top->surface, 0, 0, w, h);
   wl_surface_commit(top->surface);
}

void
tk_toplevel_del(Tk_Toplevel *top)
{
   Tk *tk = top->tk;

   xdg_toplevel_destroy(top->toplevel);
   xdg_surface_destroy(top->xdg_surface);
   wl_surface_destroy(top->surface);
   free(top);
   tk_settle(tk);
}

struct wl_surface *
tk_toplevel_surface(Tk_Toplevel *top)
{
   return top->surface;
}

uint32_t
tk_global_version(Tk *tk, const char *iface)
{
   int i;

   for (i = 0; i < tk->global_count; i++)
     if (!strcmp(tk->globals[i].iface, iface)) return tk->globals[i].version;
   return 0;
}

void *
tk_bind(Tk *tk, const struct wl_interface *iface, uint32_t version)
{
   int i;

   for (i = 0; i < tk->global_count; i++)
     {
        if (strcmp(tk->globals[i].iface, iface->name)) continue;
        return wl_registry_bind(tk->registry, tk->globals[i].name, iface,
                                version);
     }
   return NULL;
}

void
tk_sync(Tk *tk)
{
   wl_test_sync(tk->tester);
   if (wl_display_roundtrip(tk->disp) < 0)
     {
        fprintf(stderr, "%s: disconnected during sync\n", tk->prog);
        exit(1);
     }
}

void
tk_toplevel_configured(Tk_Toplevel *top, int *w, int *h, int *count)
{
   if (w) *w = top->pending_w;
   if (h) *h = top->pending_h;
   if (count) *count = top->configures;
}

void
tk_toplevel_painted(Tk_Toplevel *top, int *w, int *h)
{
   if (w) *w = top->w;
   if (h) *h = top->h;
}

void
tk_settle(Tk *tk)
{
   tk_sync(tk);
   tk_sync(tk);
}

int
tk_clients(Tk *tk, Tk_Client *out, int max)
{
   int i;

   tk->client_count = 0;
   tk->done_count = 0;
   wl_test_get_clients(tk->tester);
   if (wl_display_roundtrip(tk->disp) < 0)
     {
        fprintf(stderr, "%s: disconnected during get_clients\n", tk->prog);
        exit(1);
     }
   if (tk->done_count != 1)
     {
        fprintf(stderr, "%s: expected one clients_done, got %d\n",
                tk->prog, tk->done_count);
        exit(1);
     }

   for (i = 0; (i < tk->client_count) && (i < max) && out; i++)
     out[i] = tk->clients[i];

   return tk->client_count;
}

Tk_Client *
tk_find(Tk *tk, const char *app_id)
{
   int i;

   tk_clients(tk, NULL, 0);
   for (i = 0; i < tk->client_count; i++)
     if (!strcmp(tk->clients[i].app_id, app_id)) return &tk->clients[i];

   return NULL;
}

Tk_Client *
tk_expect(Tk *tk, const char *app_id)
{
   Tk_Client *c = tk_find(tk, app_id);

   if (!c) tk_fail(tk, "no window with app_id '%s'", app_id);
   return c;
}

void
tk_action(Tk *tk, unsigned int id, const char *name, const char *params)
{
   wl_test_client_action(tk->tester, id, name, params ?: "");
   tk_settle(tk);
}

void
tk_action_end(Tk *tk, unsigned int id, const char *name, const char *params)
{
   wl_test_client_action_end(tk->tester, id, name, params ?: "");
   tk_settle(tk);
}

void
tk_pointer_warp(Tk *tk, int x, int y)
{
   wl_test_pointer_warp(tk->tester, x, y);
   tk_settle(tk);
}

void
tk_pointer_move(Tk *tk, int dx, int dy)
{
   wl_test_pointer_move(tk->tester, dx, dy);
   tk_settle(tk);
}
