EXTRA_DIST += src/modules/conf_shelves/module.desktop.in \
src/modules/conf_shelves/e-module-conf_shelves.edj
if USE_MODULE_CONF_SHELVES
conf_shelvesdir = $(MDIR)/conf_shelves
conf_shelves_DATA = src/modules/conf_shelves/e-module-conf_shelves.edj \
		    src/modules/conf_shelves/module.desktop


conf_shelvespkgdir = $(MDIR)/conf_shelves/$(MODULE_ARCH)
conf_shelvespkg_LTLIBRARIES = src/modules/conf_shelves/module.la

src_modules_conf_shelves_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_shelves_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_shelves_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_shelves_module_la_SOURCES = src/modules/conf_shelves/e_mod_main.c \
				 src/modules/conf_shelves/e_mod_main.h \
				 src/modules/conf_shelves/e_int_config_shelf.c \
				 src/modules/conf_shelves/e_int_config_shelf.h

PHONIES += conf_shelves install-conf_shelves
conf_shelves: $(conf_shelvespkg_LTLIBRARIES) $(conf_shelves_DATA)
install-conf_shelves: install-conf_shelvesDATA install-conf_shelvespkgLTLIBRARIES
endif
