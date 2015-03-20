#ifndef SESSION_RECOVERY_SERVER_PROTOCOL_H
#define SESSION_RECOVERY_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-util.h"

struct wl_client;
struct wl_resource;

struct session_recovery;

extern const struct wl_interface session_recovery_interface;

struct session_recovery_interface {
	/**
	 * provide_uuid - (none)
	 * @uuid: (none)
	 */
	void (*provide_uuid)(struct wl_client *client,
			     struct wl_resource *resource,
			     const char *uuid);
};

#define SESSION_RECOVERY_UUID	0

#define SESSION_RECOVERY_UUID_SINCE_VERSION	1

static inline void
session_recovery_send_uuid(struct wl_resource *resource_, const char *uuid)
{
	wl_resource_post_event(resource_, SESSION_RECOVERY_UUID, uuid);
}

#ifdef  __cplusplus
}
#endif

#endif
