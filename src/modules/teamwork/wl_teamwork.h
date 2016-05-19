#ifndef TEAMWORK_SERVER_PROTOCOL_H
#define TEAMWORK_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct wl_surface;
struct zwp_teamwork;

extern const struct wl_interface zwp_teamwork_interface;

struct zwp_teamwork_interface {
	/**
	 * preload_uri - (none)
	 * @surface: (none)
	 * @uri: (none)
	 */
	void (*preload_uri)(struct wl_client *client,
			    struct wl_resource *resource,
			    struct wl_resource *surface,
			    const char *uri);
	/**
	 * activate_uri - (none)
	 * @surface: (none)
	 * @uri: (none)
	 * @x: surface local coords
	 * @y: surface local coords
	 */
	void (*activate_uri)(struct wl_client *client,
			     struct wl_resource *resource,
			     struct wl_resource *surface,
			     const char *uri,
			     wl_fixed_t x,
			     wl_fixed_t y);
	/**
	 * deactivate_uri - (none)
	 * @surface: (none)
	 * @uri: (none)
	 */
	void (*deactivate_uri)(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface,
			       const char *uri);
	/**
	 * open_uri - (none)
	 * @surface: (none)
	 * @uri: (none)
	 */
	void (*open_uri)(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *surface,
			 const char *uri);
};

#define ZWP_TEAMWORK_FETCHING_URI	0
#define ZWP_TEAMWORK_COMPLETED_URI	1
#define ZWP_TEAMWORK_FETCH_INFO	2

#define ZWP_TEAMWORK_FETCHING_URI_SINCE_VERSION	1
#define ZWP_TEAMWORK_COMPLETED_URI_SINCE_VERSION	1
#define ZWP_TEAMWORK_FETCH_INFO_SINCE_VERSION	1

static inline void
zwp_teamwork_send_fetching_uri(struct wl_resource *resource_, struct wl_resource *surface, const char *uri)
{
	wl_resource_post_event(resource_, ZWP_TEAMWORK_FETCHING_URI, surface, uri);
}

static inline void
zwp_teamwork_send_completed_uri(struct wl_resource *resource_, struct wl_resource *surface, const char *uri, int32_t valid)
{
	wl_resource_post_event(resource_, ZWP_TEAMWORK_COMPLETED_URI, surface, uri, valid);
}

static inline void
zwp_teamwork_send_fetch_info(struct wl_resource *resource_, struct wl_resource *surface, const char *uri, uint32_t progress)
{
	wl_resource_post_event(resource_, ZWP_TEAMWORK_FETCH_INFO, surface, uri, progress);
}

#ifdef  __cplusplus
}
#endif

#endif
