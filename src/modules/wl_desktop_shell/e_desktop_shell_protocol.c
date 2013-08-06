#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_output_interface;

static const struct wl_interface *types[] = {
	NULL,
	&wl_output_interface,
	&wl_surface_interface,
	&wl_output_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	&wl_surface_interface,
	&wl_output_interface,
};

static const struct wl_message e_desktop_shell_requests[] = {
	{ "set_background", "oo", types + 1 },
	{ "set_panel", "oo", types + 3 },
	{ "set_lock_surface", "o", types + 5 },
	{ "unlock", "", types + 0 },
	{ "set_grab_surface", "o", types + 6 },
	{ "desktop_ready", "2", types + 0 },
};

static const struct wl_message e_desktop_shell_events[] = {
	{ "configure", "2uoii", types + 7 },
	{ "prepare_lock_surface", "2", types + 0 },
	{ "grab_cursor", "2u", types + 0 },
};

WL_EXPORT const struct wl_interface e_desktop_shell_interface = {
	"e_desktop_shell", 2,
	6, e_desktop_shell_requests,
	3, e_desktop_shell_events,
};

static const struct wl_message screensaver_requests[] = {
	{ "set_surface", "oo", types + 11 },
};

WL_EXPORT const struct wl_interface screensaver_interface = {
	"screensaver", 1,
	1, screensaver_requests,
	0, NULL,
};

