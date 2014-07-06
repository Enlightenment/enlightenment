EXTRA_DIST += src/modules/xwayland/module.desktop.in \
src/modules/xwayland/e-module-xwayland.edj \
src/modules/xwayland/module.desktop.in
if USE_MODULE_XWAYLAND
xwaylanddir = $(MDIR)/xwayland
xwayland_DATA = src/modules/xwayland/e-module-xwayland.edj \
	       src/modules/xwayland/module.desktop

xwaylandpkgdir = $(MDIR)/xwayland/$(MODULE_ARCH)
xwaylandpkg_LTLIBRARIES = src/modules/xwayland/module.la

src_modules_xwayland_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_xwayland_module_la_CPPFLAGS  = \
  $(MOD_CPPFLAGS) @XWAYLAND_CFLAGS@ @WAYLAND_CFLAGS@ \
  -DXWAYLAND_BIN=\"@XWAYLAND_BIN@\"

src_modules_xwayland_module_la_LIBADD   = $(LIBS) @XWAYLAND_LIBS@ @WAYLAND_LIBS@
src_modules_xwayland_module_la_LDFLAGS = $(MOD_LDFLAGS)

src_modules_xwayland_module_la_SOURCES = \
  src/modules/xwayland/e_mod_main.c

PHONIES += xwayland install-xwayland
xwayland: $(xwaylandpkg_LTLIBRARIES) $(xwayland_DATA)
install-xwayland: install-xwaylandDATA install-xwaylandpkgLTLIBRARIES
endif
