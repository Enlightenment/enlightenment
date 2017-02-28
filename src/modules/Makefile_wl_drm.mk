if USE_MODULE_WL_DRM
wl_drmdir = $(MDIR)/wl_drm

wl_drmpkgdir = $(MDIR)/wl_drm/$(MODULE_ARCH)
wl_drmpkg_LTLIBRARIES = src/modules/wl_drm/module.la

src_modules_wl_drm_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_drm_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WL_DRM_CFLAGS@ @WAYLAND_CFLAGS@
src_modules_wl_drm_module_la_LIBADD   = $(MOD_LIBS) @WL_DRM_LIBS@ @WAYLAND_LIBS@ @dlopen_libs@
src_modules_wl_drm_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wl_drm_module_la_SOURCES = src/modules/wl_drm/e_mod_main.c

PHONIES += wl_drm install-wl_drm
wl_drm: $(wl_drmpkg_LTLIBRARIES) $(wl_drm_DATA)
install-wl_drm: install-wl_drmpkgLTLIBRARIES
endif
