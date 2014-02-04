EXTRA_DIST += src/modules/conf_interaction/module.desktop.in \
src/modules/conf_interaction/e-module-conf_interaction.edj
if USE_MODULE_CONF_INTERACTION
conf_interactiondir = $(MDIR)/conf_interaction
conf_interaction_DATA = src/modules/conf_interaction/e-module-conf_interaction.edj \
src/modules/conf_interaction/module.desktop


conf_interactionpkgdir = $(MDIR)/conf_interaction/$(MODULE_ARCH)
conf_interactionpkg_LTLIBRARIES = src/modules/conf_interaction/module.la

src_modules_conf_interaction_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_interaction_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_interaction_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_interaction_module_la_SOURCES = src/modules/conf_interaction/e_mod_main.c \
			     src/modules/conf_interaction/e_mod_main.h \
			     src/modules/conf_interaction/e_int_config_interaction.c \
			     src/modules/conf_interaction/e_int_config_mouse.c

PHONIES += conf_interaction install-conf_interaction
conf_interaction: $(conf_interactionpkg_LTLIBRARIES) $(conf_interaction_DATA)
install-conf_interaction: install-conf_interactionDATA install-conf_interactionpkgLTLIBRARIES
endif
