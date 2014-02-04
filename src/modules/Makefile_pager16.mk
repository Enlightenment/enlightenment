EXTRA_DIST += src/modules/pager16/module.desktop.in \
src/modules/pager16/e-module-pager16.edj
if USE_MODULE_PAGER16
pager16dir = $(MDIR)/pager16
pager16_DATA = src/modules/pager16/e-module-pager16.edj \
	     src/modules/pager16/module.desktop


pager16pkgdir = $(MDIR)/pager16/$(MODULE_ARCH)
pager16pkg_LTLIBRARIES = src/modules/pager16/module.la

src_modules_pager16_module_la_LIBADD = $(MOD_LIBS)
src_modules_pager16_module_la_CPPFLAGS = $(MOD_CPPFLAGS) -DNEED_X=1
src_modules_pager16_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_pager16_module_la_SOURCES = src/modules/pager16/e_mod_main.h \
			  src/modules/pager16/e_mod_main.c \
			  src/modules/pager16/e_mod_config.c

PHONIES += pager16 install-pager16
pager16: $(pager16pkg_LTLIBRARIES) $(pager16_DATA)
install-pager16: install-pager16DATA install-pager16pkgLTLIBRARIES
endif
