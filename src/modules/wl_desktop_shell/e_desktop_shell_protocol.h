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
   XDG_SHELL_VERSION_CURRENT = 4,
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
# define XDG_SHELL_PING_SINCE_VERSION 1

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
   XDG_SURFACE_STATE_RESIZING = 3,
   XDG_SURFACE_STATE_ACTIVATED = 4,
};
# endif

struct xdg_surface_interface
{
   void (*destroy)(struct wl_client *client, struct wl_resource *resource);
   void (*parent_set)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent_resource);
   void (*title_set)(struct wl_client *client, struct wl_resource *resource, const char *title);
   void (*app_id_set)(struct wl_client *client, struct wl_resource *resource, const char *id);
   void (*window_menu_show)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y);
   void (*move)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat_resource, uint32_t serial);
   void (*resize)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat_resource, uint32_t serial, uint32_t edges);
   void (*ack_configure)(struct wl_client *client, struct wl_resource *resource, uint32_t serial);
   void (*window_geometry_set)(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h);
   void (*maximized_set)(struct wl_client *client, struct wl_resource *resource);
   void (*maximized_unset)(struct wl_client *client, struct wl_resource *resource);
   void (*fullscreen_set)(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output);
   void (*fullscreen_unset)(struct wl_client *client, struct wl_resource *resource);
   void (*minimized_set)(struct wl_client *client, struct wl_resource *resource);
};

# define XDG_SURFACE_CONFIGURE 0
# define XDG_SURFACE_CLOSE 1
# define XDG_SURFACE_CONFIGURE_SINCE_VERSION 1
# define XDG_SURFACE_CLOSE_SINCE_VERSION 1

static inline void
xdg_surface_send_configure(struct wl_resource *resource_, int32_t w, int32_t h, struct wl_array *states, uint32_t serial)
{
   wl_resource_post_event(resource_, XDG_SURFACE_CONFIGURE, w, h, states, serial);
}

static inline void
xdg_surface_send_close(struct wl_resource *resource_)
{
   wl_resource_post_event(resource_, XDG_SURFACE_CLOSE);
};

struct xdg_popup_interface
{
   void (*destroy)(struct wl_client *client, struct wl_resource *resource);
};

# define XDG_POPUP_POPUP_DONE 0
# define XDG_POPUP_POPUP_DONE_SINCE_VERSION 1

static inline void
xdg_popup_send_popup_done(struct wl_resource *resource_, uint32_t serial)
{
   wl_resource_post_event(resource_, XDG_POPUP_POPUP_DONE, serial);
};

# ifdef __cplusplus
}
# endif

#endif
