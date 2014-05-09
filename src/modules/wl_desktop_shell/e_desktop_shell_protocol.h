#ifndef E_DESKTOP_SHELL_PROTOCOL_H
# define E_DESKTOP_SHELL_PROTOCOL_H

# ifdef __cplusplus
extern "C" {
# endif

# include <stdint.h>
# include <stddef.h>
# include <wayland-util.h>

struct wl_client;
struct wl_resource;

struct xdg_shell;
struct xdg_surface;
struct xdg_popup;

extern const struct wl_interface xdg_shell_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_popup_interface;

# ifndef XDG_SHELL_VERSION_ENUM
#  define XDG_SHELL_VERSION_ENUM
enum xdg_shell_version
{
   XDG_SHELL_VERSION_CURRENT = 3,
};
# endif

struct xdg_shell_interface
{
   void (*use_unstable_version)(struct wl_client *client, struct wl_resource *resource, int32_t version);
   void (*xdg_surface_get)(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource);
   void (*xdg_popup_get)(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource, struct wl_resource *parent_resource, struct wl_resource *seat_resource, uint32_t serial, int32_t x, int32_t y, uint32_t flags);
   void (*pong)(struct wl_client *client, struct wl_resource *resource, uint32_t serial);
};

# define XDG_SHELL_PING 0
static inline void 
xdg_shell_send_ping(struct wl_resource *resource_, uint32_t serial)
{
   wl_resource_post_event(resource_, XDG_SHELL_PING, serial);
}

# ifndef XDG_SURFACE_RESIZE_EDGE_ENUM
#  define XDG_SURFACE_RESIZE_EDGE_ENUM
enum xdg_surface_resize_edge
{
   XDG_SURFACE_RESIZE_EDGE_NONE = 0,
   XDG_SURFACE_RESIZE_EDGE_TOP = 1,
   XDG_SURFACE_RESIZE_EDGE_BOTTOM = 2,
   XDG_SURFACE_RESIZE_EDGE_LEFT = 4,
   XDG_SURFACE_RESIZE_EDGE_TOP_LEFT = 5,
   XDG_SURFACE_RESIZE_EDGE_BOTTOM_LEFT = 6,
   XDG_SURFACE_RESIZE_EDGE_RIGHT = 8,
   XDG_SURFACE_RESIZE_EDGE_TOP_RIGHT = 9,
   XDG_SURFACE_RESIZE_EDGE_BOTTOM_RIGHT = 10,
};
# endif

# ifndef XDG_SURFACE_STATE_ENUM
#  define XDG_SURFACE_STATE_ENUM
enum xdg_surface_state
{
   XDG_SURFACE_STATE_MAXIMIZED = 1,
   XDG_SURFACE_STATE_FULLSCREEN = 2,
};
# endif

struct xdg_surface_interface
{
   void (*destroy)(struct wl_client *client, struct wl_resource *resource);
   void (*transient_for_set)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent_resource);
   void (*margin_set)(struct wl_client *client, struct wl_resource *resource, int32_t l, int32_t r, int32_t t, int32_t b);
   void (*title_set)(struct wl_client *client, struct wl_resource *resource, const char *title);
   void (*app_id_set)(struct wl_client *client, struct wl_resource *resource, const char *id);
   void (*move)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat_resource, uint32_t serial);
   void (*resize)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat_resource, uint32_t serial, uint32_t edges);
   void (*output_set)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output_resource);
   void (*state_change_request)(struct wl_client *client, struct wl_resource *resource, uint32_t state, uint32_t value, uint32_t serial);
   void (*state_change_acknowledge)(struct wl_client *client, struct wl_resource *resource, uint32_t state, uint32_t value, uint32_t serial);
   void (*minimize)(struct wl_client *client, struct wl_resource *resource);
};

# define XDG_SURFACE_CONFIGURE 0
# define XDG_SURFACE_CHANGE_STATE 1
# define XDG_SURFACE_ACTIVATED 2
# define XDG_SURFACE_DEACTIVATED 3
# define XDG_SURFACE_CLOSE 4

static inline void 
xdg_surface_send_configure(struct wl_resource *resource_, int32_t w, int32_t h)
{
   wl_resource_post_event(resource_, XDG_SURFACE_CONFIGURE, w, h);
}

static inline void 
xdg_surface_send_state_change(struct wl_resource *resource_, uint32_t state, uint32_t value, uint32_t serial)
{
   wl_resource_post_event(resource_, XDG_SURFACE_CHANGE_STATE, state, value, serial);
}

static inline void 
xdg_surface_send_activated(struct wl_resource *resource_)
{
   wl_resource_post_event(resource_, XDG_SURFACE_ACTIVATED);
};

static inline void 
xdg_surface_send_deactivated(struct wl_resource *resource_)
{
   wl_resource_post_event(resource_, XDG_SURFACE_DEACTIVATED);
};

static inline void 
xdg_surface_send_close(struct wl_resource *resource_)
{
   wl_resource_post_event(resource_, XDG_SURFACE_CLOSE);
};

struct xdg_popup_interface
{
   void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

# define XDG_POPUP_DONE 0
static inline void 
xdg_popup_send_done(struct wl_resource *resource_, uint32_t serial)
{
   wl_resource_post_event(resource_, XDG_POPUP_DONE, serial);
};

# ifdef __cplusplus
}
# endif

#endif
