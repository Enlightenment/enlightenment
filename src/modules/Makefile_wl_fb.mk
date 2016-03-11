if USE_MODULE_WL_FB
wl_fbdir = $(MDIR)/wl_fb

wl_fbpkgdir = $(MDIR)/wl_fb/$(MODULE_ARCH)
wl_fbpkg_LTLIBRARIES = src/modules/wl_fb/module.la

src_modules_wl_fb_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_fb_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WL_FB_CFLAGS@ @WAYLAND_CFLAGS@
src_modules_wl_fb_module_la_LIBADD   = $(MOD_LIBS) @WL_FB_LIBS@ @WAYLAND_LIBS@
src_modules_wl_fb_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wl_fb_module_la_SOURCES = src/modules/wl_fb/e_mod_main.c

# TODO: incomplete
#.PHONY: wl_fb install-wl_fb
#wl_fb: $(wl_fbpkg_LTLIBRARIES) $(wl_fb_DATA)
#install-wl_fb: install-wl_fbDATA install-wl_fbpkgLTLIBRARIES
endif
