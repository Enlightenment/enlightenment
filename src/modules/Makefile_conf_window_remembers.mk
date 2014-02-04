EXTRA_DIST += src/modules/conf_window_remembers/module.desktop.in \
src/modules/conf_window_remembers/e-module-conf_window_remembers.edj
if USE_MODULE_CONF_WINDOW_REMEMBERS
conf_window_remembersdir = $(MDIR)/conf_window_remembers
conf_window_remembers_DATA = src/modules/conf_window_remembers/e-module-conf_window_remembers.edj \
			     src/modules/conf_window_remembers/module.desktop


conf_window_rememberspkgdir = $(MDIR)/conf_window_remembers/$(MODULE_ARCH)
conf_window_rememberspkg_LTLIBRARIES = src/modules/conf_window_remembers/module.la

src_modules_conf_window_remembers_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_window_remembers_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_window_remembers_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_window_remembers_module_la_SOURCES = \
src/modules/conf_window_remembers/e_mod_main.c \
src/modules/conf_window_remembers/e_mod_main.h \
src/modules/conf_window_remembers/e_int_config_remembers.c

PHONIES += conf_window_remembers install-conf_window_remembers
conf_window_remembers: $(conf_window_rememberspkg_LTLIBRARIES) $(conf_window_remembers_DATA)
install-conf_window_remembers: install-conf_window_remembersDATA install-conf_window_rememberspkgLTLIBRARIES
endif
