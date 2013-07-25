conf_compdir = $(MDIR)/conf_comp
conf_comp_DATA = src/modules/conf_comp/module.desktop

EXTRA_DIST += $(comp_DATA)

conf_comppkgdir = $(MDIR)/conf_comp/$(MODULE_ARCH)
conf_comppkg_LTLIBRARIES = src/modules/conf_comp/module.la

src_modules_conf_comp_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_comp_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_comp_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_comp_module_la_SOURCES = src/modules/conf_comp/e_mod_main.h \
			       src/modules/conf_comp/e_mod_main.c \
			       src/modules/conf_comp/e_mod_config.c \
			       src/modules/conf_comp/e_mod_config_match.c

PHONIES += conf_comp install-conf_comp
conf_comp: $(conf_mppkg_LTLIBRARIES) $(conf_comp_DATA)
install-conf_comp: install-conf_compDATA install-conf_comppkgLTLIBRARIES
