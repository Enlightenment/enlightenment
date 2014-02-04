EXTRA_DIST += src/modules/conf/module.desktop.in \
src/modules/conf/e-module-conf.edj
if USE_MODULE_CONF
confdir = $(MDIR)/conf
conf_DATA = src/modules/conf/e-module-conf.edj \
	    src/modules/conf/module.desktop


confpkgdir = $(MDIR)/conf/$(MODULE_ARCH)
confpkg_LTLIBRARIES = src/modules/conf/module.la

src_modules_conf_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_module_la_SOURCES = src/modules/conf/e_mod_main.c \
			 src/modules/conf/e_mod_main.h \
			 src/modules/conf/e_conf.c \
			 src/modules/conf/e_mod_config.c

PHONIES += conf install-conf
conf: $(confpkg_LTLIBRARIES) $(conf_DATA)
install-conf: install-confDATA install-confpkgLTLIBRARIES
endif
