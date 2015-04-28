if USE_MODULE_XWAYLAND
xwaylandpkgdir = $(MDIR)/xwayland/$(MODULE_ARCH)
xwaylandpkg_LTLIBRARIES = src/modules/xwayland/module.la

src_modules_xwayland_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_xwayland_module_la_CPPFLAGS  = \
$(MOD_CPPFLAGS) \
@XWAYLAND_CFLAGS@ \
@WAYLAND_CFLAGS@ \
-DXWAYLAND_BIN=\"@XWAYLAND_BIN@\"

src_modules_xwayland_module_la_LIBADD   = $(MOD_LIBS) @XWAYLAND_LIBS@ @WAYLAND_LIBS@
src_modules_xwayland_module_la_LDFLAGS = $(MOD_LDFLAGS)

src_modules_xwayland_module_la_SOURCES = \
src/modules/xwayland/e_mod_main.c

PHONIES += xwayland install-xwayland
xwayland: $(xwaylandpkg_LTLIBRARIES)
install-xwayland: install-xwaylandpkgLTLIBRARIES
endif
