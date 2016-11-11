EXTRA_DIST += src/modules/wl_desktop_shell/module.desktop.in \
src/modules/wl_desktop_shell/e-module-wl_desktop_shell.edj \
src/modules/wl_desktop_shell/module.desktop.in

if USE_MODULE_WL_DESKTOP_SHELL
wl_desktop_shelldir = $(MDIR)/wl_desktop_shell
wl_desktop_shell_DATA = src/modules/wl_desktop_shell/e-module-wl_desktop_shell.edj \
	       src/modules/wl_desktop_shell/module.desktop

wl_desktop_shellpkgdir = $(MDIR)/wl_desktop_shell/$(MODULE_ARCH)
wl_desktop_shellpkg_LTLIBRARIES = src/modules/wl_desktop_shell/module.la

wl_desktop_shell_wayland_sources = \
  src/modules/wl_desktop_shell/xdg-shell-unstable-v5-protocol.c \
  src/modules/wl_desktop_shell/xdg-shell-unstable-v5-server-protocol.h \
  src/modules/wl_desktop_shell/xdg-shell-unstable-v6-protocol.c \
  src/modules/wl_desktop_shell/xdg-shell-unstable-v6-server-protocol.h \
  src/modules/wl_desktop_shell/input-method-unstable-v1-protocol.c \
  src/modules/wl_desktop_shell/input-method-unstable-v1-server-protocol.h

src_modules_wl_desktop_shell_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_desktop_shell_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WAYLAND_CFLAGS@ \
  -I$(top_builddir)/src/modules/wl_desktop_shell
src_modules_wl_desktop_shell_module_la_LIBADD   = $(MOD_LIBS) @WAYLAND_LIBS@
src_modules_wl_desktop_shell_module_la_LDFLAGS = $(MOD_LDFLAGS)

src_modules_wl_desktop_shell_module_la_SOURCES = \
src/modules/wl_desktop_shell/e_mod_main.c \
src/modules/wl_desktop_shell/e_mod_main.h \
src/modules/wl_desktop_shell/e_mod_input_panel.c \
src/modules/wl_desktop_shell/wl_shell.c \
src/modules/wl_desktop_shell/xdg5.c \
src/modules/wl_desktop_shell/xdg6.c


nodist_src_modules_wl_desktop_shell_module_la_SOURCES = \
 $(wl_desktop_shell_wayland_sources)

MAINTAINERCLEANFILES += \
 $(wl_desktop_shell_wayland_sources)

src/modules/wl_desktop_shell/e_mod_main.c: \
 src/modules/wl_desktop_shell/xdg-shell-unstable-v5-server-protocol.h \
 src/modules/wl_desktop_shell/xdg-shell-unstable-v6-server-protocol.h \
 src/modules/wl_desktop_shell/input-method-unstable-v1-server-protocol.h

PHONIES += wl_desktop_shell install-wl_desktop_shell
wl_desktop_shell: $(wl_desktop_shellpkg_LTLIBRARIES) $(wl_desktop_shell_DATA)
install-wl_desktop_shell: install-wl_desktop_shellDATA install-wl_desktop_shellpkgLTLIBRARIES
endif
