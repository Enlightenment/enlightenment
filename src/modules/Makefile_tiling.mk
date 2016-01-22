EXTRA_DIST += src/modules/tiling/module.desktop.in \
src/modules/tiling/e-module-tiling.edj
if USE_MODULE_TILING
tilingdir = $(MDIR)/tiling
tiling_DATA = src/modules/tiling/e-module-tiling.edj \
	      src/modules/tiling/module.desktop

tilingpkgdir = $(MDIR)/tiling/$(MODULE_ARCH)
tilingpkg_LTLIBRARIES = src/modules/tiling/module.la

src_modules_tiling_module_la_LIBADD = $(MOD_LIBS)
src_modules_tiling_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_tiling_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_tiling_module_la_SOURCES = src/modules/tiling/e_mod_tiling.c \
				       src/modules/tiling/e_mod_tiling.h \
				       src/modules/tiling/window_tree.c \
				       src/modules/tiling/window_tree.h \
				       src/modules/tiling/e_mod_config.c

PHONIES += tiling install-tiling
tiling: $(tilingpkg_LTLIBRARIES) $(tiling_DATA)
install-tiling: install-tilingDATA install-tilingpkgLTLIBRARIES
endif
