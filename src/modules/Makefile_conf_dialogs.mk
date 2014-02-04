EXTRA_DIST += src/modules/conf_dialogs/module.desktop.in \
src/modules/conf_dialogs/e-module-conf_dialogs.edj
if USE_MODULE_CONF_DIALOGS
conf_dialogsdir = $(MDIR)/conf_dialogs
conf_dialogs_DATA = src/modules/conf_dialogs/e-module-conf_dialogs.edj \
		    src/modules/conf_dialogs/module.desktop


conf_dialogspkgdir = $(MDIR)/conf_dialogs/$(MODULE_ARCH)
conf_dialogspkg_LTLIBRARIES = src/modules/conf_dialogs/module.la

src_modules_conf_dialogs_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_dialogs_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_dialogs_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_dialogs_module_la_SOURCES = src/modules/conf_dialogs/e_mod_main.c \
				 src/modules/conf_dialogs/e_mod_main.h \
				 src/modules/conf_dialogs/e_int_config_dialogs.c \
				 src/modules/conf_dialogs/e_int_config_profiles.c

PHONIES += conf_dialogs install-conf_dialogs
conf_dialogs: $(conf_dialogspkg_LTLIBRARIES) $(conf_dialogs_DATA)
install-conf_dialogs: install-conf_dialogsDATA install-conf_dialogspkgLTLIBRARIES
endif
