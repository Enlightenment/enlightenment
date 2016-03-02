EXTRA_DIST += src/modules/start/module.desktop.in \
src/modules/start/e-module-start.edj
if USE_MODULE_START
startdir = $(MDIR)/start
start_DATA = src/modules/start/e-module-start.edj \
	     src/modules/start/module.desktop


startpkgdir = $(MDIR)/start/$(MODULE_ARCH)
startpkg_LTLIBRARIES = src/modules/start/module.la

src_modules_start_module_la_LIBADD = $(MOD_LIBS)
src_modules_start_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_start_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_start_module_la_SOURCES = \
src/modules/start/e_mod_main.c \
src/modules/start/start.c

PHONIES += start install-start
start: $(startpkg_LTLIBRARIES) $(start_DATA)
install-start: install-startDATA install-startpkgLTLIBRARIES
endif
