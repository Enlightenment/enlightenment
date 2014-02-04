EXTRA_DIST += src/modules/conf_menus/module.desktop.in \
src/modules/conf_menus/e-module-conf_menus.edj
if USE_MODULE_CONF_MENUS
conf_menusdir = $(MDIR)/conf_menus
conf_menus_DATA = src/modules/conf_menus/e-module-conf_menus.edj \
		  src/modules/conf_menus/module.desktop


conf_menuspkgdir = $(MDIR)/conf_menus/$(MODULE_ARCH)
conf_menuspkg_LTLIBRARIES = src/modules/conf_menus/module.la

src_modules_conf_menus_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_menus_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_menus_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_menus_module_la_SOURCES = src/modules/conf_menus/e_mod_main.c \
			       src/modules/conf_menus/e_mod_main.h \
			       src/modules/conf_menus/e_int_config_menus.c

PHONIES += conf_menus install-conf_menus
conf_menus: $(conf_menuspkg_LTLIBRARIES) $(conf_menus_DATA)
install-conf_menus: install-conf_menusDATA install-conf_menuspkgLTLIBRARIES
endif
