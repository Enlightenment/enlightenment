#ifndef E_SESSION_RECOVERY_SERVER_PROTOCOL_H
#define E_SESSION_RECOVERY_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct wl_surface;
struct zwp_e_session_recovery;

extern const struct wl_interface zwp_e_session_recovery_interface;

struct zwp_e_session_recovery_interface {
	/**
	 * get_uuid - (none)
	 * @surface: (none)
	 */
	void (*get_uuid)(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *surface);
	/**
	 * set_uuid - (none)
	 * @surface: (none)
	 * @uuid: (none)
	 */
	void (*set_uuid)(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *surface,
			 const char *uuid);
	/**
	 * destroy_uuid - (none)
	 * @surface: (none)
	 * @uuid: (none)
	 */
	void (*destroy_uuid)(struct wl_client *client,
			     struct wl_resource *resource,
			     struct wl_resource *surface,
			     const char *uuid);
};

#define ZWP_E_SESSION_RECOVERY_CREATE_UUID	0

#define ZWP_E_SESSION_RECOVERY_CREATE_UUID_SINCE_VERSION	1

static inline void
zwp_e_session_recovery_send_create_uuid(struct wl_resource *resource_, struct wl_resource *surface, const char *uuid)
{
	wl_resource_post_event(resource_, ZWP_E_SESSION_RECOVERY_CREATE_UUID, surface, uuid);
}

#ifdef  __cplusplus
}
#endif

#endif
