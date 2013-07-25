conf_randrdir = $(MDIR)/conf_randr
conf_randr_DATA = src/modules/conf_randr/e-module-conf_randr.edj \
		  src/modules/conf_randr/module.desktop

EXTRA_DIST += $(conf_randr_DATA)

conf_randrpkgdir = $(MDIR)/conf_randr/$(MODULE_ARCH)
conf_randrpkg_LTLIBRARIES = src/modules/conf_randr/module.la

src_modules_conf_randr_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_randr_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_randr_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_randr_module_la_SOURCES = src/modules/conf_randr/e_mod_main.c \
			       src/modules/conf_randr/e_mod_main.h \
			       src/modules/conf_randr/e_int_config_randr.c \
			       src/modules/conf_randr/e_int_config_randr.h \
                               src/modules/conf_randr/e_smart_randr.c \
                               src/modules/conf_randr/e_smart_randr.h \
                               src/modules/conf_randr/e_smart_monitor.c \
                               src/modules/conf_randr/e_smart_monitor.h

PHONIES += conf_randr install-conf_randr
conf_randr: $(conf_randrpkg_LTLIBRARIES) $(conf_randr_DATA)
install-conf_randr: install-conf_randrDATA install-conf_randrpkgLTLIBRARIES
