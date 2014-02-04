if USE_MODULE_WL_X11
wl_x11dir = $(MDIR)/wl_x11

wl_x11pkgdir = $(MDIR)/wl_x11/$(MODULE_ARCH)
wl_x11pkg_LTLIBRARIES = src/modules/wl_x11/module.la

src_modules_wl_x11_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_x11_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @ECORE_X_CFLAGS@ @WAYLAND_CFLAGS@ -DNEED_X=1
src_modules_wl_x11_module_la_LIBADD   = $(LIBS) @ECORE_X_LIBS@ @WAYLAND_LIBS@
src_modules_wl_x11_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wl_x11_module_la_SOURCES = src/modules/wl_x11/e_mod_main.c

# TODO: incomplete
#.PHONY: wl_x11 install-wl_x11
#wl_x11: $(wl_x11pkg_LTLIBRARIES) $(wl_x11_DATA)
#install-wl_x11: install-wl_x11DATA install-wl_x11pkgLTLIBRARIES
endif
