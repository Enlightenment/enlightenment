if USE_MODULE_WL_EGLFS
wl_eglfsdir = $(MDIR)/wl_eglfs

wl_eglfspkgdir = $(MDIR)/wl_eglfs/$(MODULE_ARCH)
wl_eglfspkg_LTLIBRARIES = src/modules/wl_eglfs/module.la

src_modules_wl_eglfs_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_eglfs_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WL_EGLFS_CFLAGS@ @WAYLAND_CFLAGS@
src_modules_wl_eglfs_module_la_LIBADD   = $(LIBS) @WL_EGLFS_LIBS@ @WAYLAND_LIBS@
src_modules_wl_eglfs_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wl_eglfs_module_la_SOURCES = src/modules/wl_eglfs/e_mod_main.c

# TODO: incomplete
#.PHONY: wl_eglfs install-wl_eglfs
#wl_eglfs: $(wl_eglfspkg_LTLIBRARIES) $(wl_eglfs_DATA)
#install-wl_eglfs: install-wl_eglfsDATA install-wl_eglfspkgLTLIBRARIES
endif
