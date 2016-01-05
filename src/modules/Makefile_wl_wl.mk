if USE_MODULE_WL_WL
wl_wldir = $(MDIR)/wl_wl

wl_wlpkgdir = $(MDIR)/wl_wl/$(MODULE_ARCH)
wl_wlpkg_LTLIBRARIES = src/modules/wl_wl/module.la

src_modules_wl_wl_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_wl_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WAYLAND_CFLAGS@
src_modules_wl_wl_module_la_LIBADD   = $(LIBS) @WAYLAND_LIBS@
src_modules_wl_wl_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wl_wl_module_la_SOURCES = \
src/modules/wl_wl/e_mod_main.c

PHONIES += wl_wl install-wl_wl
wl_wl: $(wl_wlpkg_LTLIBRARIES) $(wl_wl_DATA)
install-wl_wl: install-wl_wlpkgLTLIBRARIES
endif
