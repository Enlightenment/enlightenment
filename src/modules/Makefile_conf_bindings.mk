EXTRA_DIST += src/modules/conf_bindings/module.desktop.in
if USE_MODULE_CONF_BINDINGS
conf_bindingsdir = $(MDIR)/conf_bindings
conf_bindings_DATA = src/modules/conf_bindings/module.desktop


conf_bindingspkgdir = $(MDIR)/conf_bindings/$(MODULE_ARCH)
conf_bindingspkg_LTLIBRARIES = src/modules/conf_bindings/module.la

src_modules_conf_bindings_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_bindings_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_bindings_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_bindings_module_la_SOURCES = src/modules/conf_bindings/e_mod_main.c \
			     src/modules/conf_bindings/e_mod_main.h \
			     src/modules/conf_bindings/e_int_config_keybindings.c \
			     src/modules/conf_bindings/e_int_config_mousebindings.c \
			     src/modules/conf_bindings/e_int_config_edgebindings.c \
			     src/modules/conf_bindings/e_int_config_signalbindings.c \
			     src/modules/conf_bindings/e_int_config_acpibindings.c

PHONIES += conf_bindings install-conf_bindings
conf_bindings: $(conf_bindingspkg_LTLIBRARIES) $(conf_bindings_DATA)
install-conf_bindings: install-conf_bindingsDATA install-conf_bindingspkgLTLIBRARIES
endif
