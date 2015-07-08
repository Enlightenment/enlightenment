EXTRA_DIST += src/modules/tiling/module.desktop.in \
src/modules/tiling/e-module-tiling.edc \
src/modules/tiling/images/icon_floating.png \
src/modules/tiling/images/icon_horizontal.png \
src/modules/tiling/images/icon_vertical.png \
src/modules/tiling/images/module_icon.png
if USE_MODULE_TILING
tilingdir = $(MDIR)/tiling
tiling_DATA = src/modules/tiling/e-module-tiling.edj \
	      src/modules/tiling/module.desktop
CLEANFILES += src/modules/tiling/e-module-tiling.edj

tilingpkgdir = $(MDIR)/tiling/$(MODULE_ARCH)
tilingpkg_LTLIBRARIES = src/modules/tiling/module.la

TILING_EDJE_FLAGS = $(EDJE_FLAGS) -id $(top_srcdir)/src/modules/tiling/images

src/modules/tiling/%.edj: src/modules/tiling/%.edc Makefile
	$(EDJE_CC) $(TILING_EDJE_FLAGS) $< $@

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
