EXTRA_DIST += src/modules/conf_intl/module.desktop.in
if USE_MODULE_CONF_INTL
conf_intldir = $(MDIR)/conf_intl
conf_intl_DATA = src/modules/conf_intl/module.desktop


conf_intlpkgdir = $(MDIR)/conf_intl/$(MODULE_ARCH)
conf_intlpkg_LTLIBRARIES = src/modules/conf_intl/module.la

src_modules_conf_intl_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_intl_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_intl_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_intl_module_la_SOURCES = src/modules/conf_intl/e_mod_main.c \
			      src/modules/conf_intl/e_mod_main.h \
			      src/modules/conf_intl/e_int_config_intl.c \
			      src/modules/conf_intl/e_int_config_imc_import.c \
			      src/modules/conf_intl/e_int_config_imc.c

PHONIES += conf_intl install-conf_intl
conf_intl: $(conf_intlpkg_LTLIBRARIES) $(conf_intl_DATA)
install-conf_intl: install-conf_intlDATA install-conf_intlpkgLTLIBRARIES
endif
