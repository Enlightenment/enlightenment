EXTRA_DIST += src/modules/pager_plain/module.desktop.in \
src/modules/pager_plain/e-module-pager-plain.edj
if USE_MODULE_PAGER_PLAIN
pager_plaindir = $(MDIR)/pager_plain
pager_plain_DATA = src/modules/pager_plain/e-module-pager-plain.edj \
	     src/modules/pager_plain/module.desktop


pager_plainpkgdir = $(MDIR)/pager_plain/$(MODULE_ARCH)
pager_plainpkg_LTLIBRARIES = src/modules/pager_plain/module.la

src_modules_pager_plain_module_la_LIBADD = $(MOD_LIBS)
src_modules_pager_plain_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_pager_plain_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_pager_plain_module_la_SOURCES = src/modules/pager_plain/e_mod_main.h \
			  src/modules/pager_plain/e_mod_main.c \
			  src/modules/pager_plain/e_mod_config.c

PHONIES += pager-plain install-pager-plain
pager_plain: $(pager_plainpkg_LTLIBRARIES) $(pager_plain_DATA)
install-pager-plain: install-pager-plainDATA install-pager-plainpkgLTLIBRARIES
endif
