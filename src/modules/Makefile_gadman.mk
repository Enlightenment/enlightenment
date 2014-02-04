EXTRA_DIST += src/modules/gadman/module.desktop.in \
src/modules/gadman/e-module-gadman.edj
if USE_MODULE_GADMAN
gadmandir = $(MDIR)/gadman
gadman_DATA = src/modules/gadman/e-module-gadman.edj \
	      src/modules/gadman/module.desktop


gadmanpkgdir = $(MDIR)/gadman/$(MODULE_ARCH)
gadmanpkg_LTLIBRARIES = src/modules/gadman/module.la

src_modules_gadman_module_la_LIBADD = $(MOD_LIBS)
src_modules_gadman_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_gadman_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_gadman_module_la_SOURCES = src/modules/gadman/e_mod_main.c \
			   src/modules/gadman/e_mod_config.c \
			   src/modules/gadman/e_mod_gadman.c \
			   src/modules/gadman/e_mod_gadman.h

PHONIES += gadman install-gadman
gadman: $(gadmanpkg_LTLIBRARIES) $(gadman_DATA)
install-gadman: install-gadmanDATA install-gadmanpkgLTLIBRARIES
endif
