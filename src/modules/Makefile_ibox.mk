EXTRA_DIST += src/modules/ibox/module.desktop.in \
src/modules/ibox/e-module-ibox.edj
if USE_MODULE_IBOX
iboxdir = $(MDIR)/ibox
ibox_DATA = src/modules/ibox/e-module-ibox.edj \
	    src/modules/ibox/module.desktop


iboxpkgdir = $(MDIR)/ibox/$(MODULE_ARCH)
iboxpkg_LTLIBRARIES = src/modules/ibox/module.la

src_modules_ibox_module_la_LIBADD = $(MOD_LIBS)
src_modules_ibox_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_ibox_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_ibox_module_la_SOURCES = src/modules/ibox/e_mod_main.c \
			 src/modules/ibox/e_mod_main.h \
			 src/modules/ibox/e_mod_config.c

PHONIES += ibox install-ibox
ibox: $(iboxpkg_LTLIBRARIES) $(ibox_DATA)
install-ibox: install-iboxDATA install-iboxpkgLTLIBRARIES
endif
