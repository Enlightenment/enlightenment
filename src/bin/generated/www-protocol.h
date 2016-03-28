#ifndef ZWP_WWW_SERVER_PROTOCOL_H
#define ZWP_WWW_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct wl_surface;
struct www;
struct www_surface;

extern const struct wl_interface www_interface;
extern const struct wl_interface www_surface_interface;

struct www_interface {
	/**
	 * create - Create an object for WWW notifications
	 * @id: (none)
	 * @surface: (none)
	 *
	 * 
	 */
	void (*create)(struct wl_client *client,
		       struct wl_resource *resource,
		       uint32_t id,
		       struct wl_resource *surface);
};


struct www_surface_interface {
	/**
	 * destroy - Destroy a www_surface
	 *
	 * 
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define WWW_SURFACE_STATUS	0
#define WWW_SURFACE_START_DRAG	1
#define WWW_SURFACE_END_DRAG	2

#define WWW_SURFACE_STATUS_SINCE_VERSION	1
#define WWW_SURFACE_START_DRAG_SINCE_VERSION	1
#define WWW_SURFACE_END_DRAG_SINCE_VERSION	1

static inline void
www_surface_send_status(struct wl_resource *resource_, int32_t x_rel, int32_t y_rel, uint32_t timestamp)
{
	wl_resource_post_event(resource_, WWW_SURFACE_STATUS, x_rel, y_rel, timestamp);
}

static inline void
www_surface_send_start_drag(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WWW_SURFACE_START_DRAG);
}

static inline void
www_surface_send_end_drag(struct wl_resource *resource_)
{
	wl_resource_post_event(resource_, WWW_SURFACE_END_DRAG);
}

#ifdef  __cplusplus
}
#endif

#endif
