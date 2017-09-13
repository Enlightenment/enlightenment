if ! HAVE_WAYLAND_ONLY
xsessionfilesdir = $(datadir)/xsessions
xsessionfiles_DATA = data/session/enlightenment.desktop
endif

if HAVE_WAYLAND
wlsessionfilesdir = $(datadir)/wayland-sessions
wlsessionfiles_DATA = data/session/enlightenment.desktop
endif

EXTRA_DIST += data/session/enlightenment.desktop.in
