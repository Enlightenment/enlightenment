#ifndef ZWP_DRAW_MODES_SERVER_PROTOCOL_H
#define ZWP_DRAW_MODES_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct draw_modes;

extern const struct wl_interface draw_modes_interface;

#ifndef DRAW_MODES_STATE_ENUM
#define DRAW_MODES_STATE_ENUM
/**
 * draw_modes_state - the surface has CSD without dropshadow
 * @DRAW_MODES_STATE_DRAW_NOSHADOW: CSD with no dropshadow
 *
 * The surface contains a CSD region which does not include a dropshadow.
 */
enum draw_modes_state {
	DRAW_MODES_STATE_DRAW_NOSHADOW = 0x2000,
};
#endif /* DRAW_MODES_STATE_ENUM */

struct draw_modes_interface {
	/**
	 * set_available_draw_modes - advertise optional draw modes for
	 *	the window
	 * @surface: (none)
	 * @states: (none)
	 *
	 * Inform the compositor of optional draw modes which are
	 * available for the window.
	 *
	 * Calling this after an xdg_surface's first commit is a client
	 * error.
	 *
	 * Required modes are implemented by all clients and are not
	 * present in this array. If set_available_draw_modes is not
	 * called, only required modes are available.
	 */
	void (*set_available_draw_modes)(struct wl_client *client,
					 struct wl_resource *resource,
					 struct wl_resource *surface,
					 struct wl_array *states);
};


#ifdef  __cplusplus
}
#endif

#endif
