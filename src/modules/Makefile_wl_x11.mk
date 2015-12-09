if USE_MODULE_WL_X11
wl_x11dir = $(MDIR)/wl_x11

wl_x11pkgdir = $(MDIR)/wl_x11/$(MODULE_ARCH)
wl_x11pkg_LTLIBRARIES = src/modules/wl_x11/module.la

src_modules_wl_x11_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_x11_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WAYLAND_CFLAGS@
src_modules_wl_x11_module_la_LIBADD   = $(MOD_LIBS) @WAYLAND_LIBS@
src_modules_wl_x11_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wl_x11_module_la_SOURCES = src/modules/wl_x11/e_mod_main.c

PHONIES += wl_x11 install-wl_x11
wl_x11: $(wl_x11pkg_LTLIBRARIES)
install-wl_x11: install-wl_x11pkgLTLIBRARIES
endif
