EXTRA_DIST += src/modules/wl_desktop_shell/module.desktop.in \
src/modules/wl_desktop_shell/e-module-wl_desktop_shell.edj \
src/modules/wl_desktop_shell/module.desktop.in \
src/modules/wl_desktop_shell/e_input_method_protocol.h \
src/modules/wl_desktop_shell/e_input_method_protocol.c \
src/modules/wl_desktop_shell/e_desktop_shell_protocol.h \
src/modules/wl_desktop_shell/e_desktop_shell_protocol.c
if USE_MODULE_WL_DESKTOP_SHELL
wl_desktop_shelldir = $(MDIR)/wl_desktop_shell
wl_desktop_shell_DATA = src/modules/wl_desktop_shell/e-module-wl_desktop_shell.edj \
	       src/modules/wl_desktop_shell/module.desktop

wl_desktop_shellpkgdir = $(MDIR)/wl_desktop_shell/$(MODULE_ARCH)
wl_desktop_shellpkg_LTLIBRARIES = src/modules/wl_desktop_shell/module.la

src_modules_wl_desktop_shell_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_desktop_shell_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WL_DESKTOP_SHELL_CFLAGS@ @WAYLAND_CFLAGS@
src_modules_wl_desktop_shell_module_la_LIBADD   = $(LIBS) @WL_DESKTOP_SHELL_LIBS@ @WAYLAND_LIBS@
src_modules_wl_desktop_shell_module_la_LDFLAGS = $(MOD_LDFLAGS)

src_modules_wl_desktop_shell_module_la_SOURCES = \
  src/modules/wl_desktop_shell/e_mod_main.c \
  src/modules/wl_desktop_shell/e_mod_main.h \
  src/modules/wl_desktop_shell/e_mod_input_panel.c \
  src/modules/wl_desktop_shell/e_input_method_protocol.c \
  src/modules/wl_desktop_shell/e_input_method_protocol.h \
  src/modules/wl_desktop_shell/e_desktop_shell_protocol.c \
  src/modules/wl_desktop_shell/e_desktop_shell_protocol.h \
src/modules/wl_desktop_shell/draw-mode.c \
src/modules/wl_desktop_shell/draw-mode.h

PHONIES += wl_desktop_shell install-wl_desktop_shell
wl_desktop_shell: $(wl_desktop_shellpkg_LTLIBRARIES) $(wl_desktop_shell_DATA)
install-wl_desktop_shell: install-wl_desktop_shellDATA install-wl_desktop_shellpkgLTLIBRARIES
endif
