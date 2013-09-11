tilingdir = $(MDIR)/tiling
tiling_DATA = src/modules/tiling/e-module-tiling.edj \
	      src/modules/tiling/module.desktop
CLEANFILES += src/modules/tiling/e-module-tiling.edj

EXTRA_DIST += $(tiling_DATA) \
	      src/modules/tiling/e-module-tiling.edc \
	      src/modules/tiling/module.desktop.in \
	      src/modules/tiling/images/module_icon.png \
	      src/modules/tiling/images/arrow_e.png \
	      src/modules/tiling/images/arrow_ne.png \
	      src/modules/tiling/images/arrow_n.png \
	      src/modules/tiling/images/arrow_nw.png \
	      src/modules/tiling/images/arrow_se.png \
	      src/modules/tiling/images/arrow_s.png \
	      src/modules/tiling/images/arrow_sw.png \
	      src/modules/tiling/images/arrow_w.png

tilingpkgdir = $(MDIR)/tiling/$(MODULE_ARCH)
tilingpkg_LTLIBRARIES = src/modules/tiling/module.la

TILING_EDJE_FLAGS = $(EDJE_FLAGS) -id $(top_srcdir)/src/modules/tiling/images

src/modules/tiling/%.edj: src/modules/tiling/%.edc Makefile
	$(EDJE_CC) $(TILING_EDJE_FLAGS) $< $@

src_modules_tiling_module_la_LIBADD = $(MOD_LIBS)
src_modules_tiling_module_la_CPPFLAGS = $(MOD_CPPFLAGS) -DNEED_X=1
src_modules_tiling_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_tiling_module_la_SOURCES = src/modules/tiling/e_mod_tiling.c \
			   src/modules/tiling/e_mod_tiling.h \
			   src/modules/tiling/e_mod_config.c

PHONIES += tiling install-tiling
tiling: $(tilingpkg_LTLIBRARIES) $(tiling_DATA)
install-tiling: install-tilingDATA install-tilingpkgLTLIBRARIES
