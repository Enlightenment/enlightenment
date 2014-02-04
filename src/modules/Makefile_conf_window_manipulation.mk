EXTRA_DIST += src/modules/conf_window_manipulation/module.desktop.in \
src/modules/conf_window_manipulation/e-module-conf_winmanip.edj
if USE_MODULE_CONF_WINDOW_MANIPULATION
conf_window_manipulationdir = $(MDIR)/conf_window_manipulation
conf_window_manipulation_DATA = src/modules/conf_window_manipulation/module.desktop


conf_window_manipulationpkgdir = $(MDIR)/conf_window_manipulation/$(MODULE_ARCH)
conf_window_manipulationpkg_LTLIBRARIES = src/modules/conf_window_manipulation/module.la

src_modules_conf_window_manipulation_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_window_manipulation_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_window_manipulation_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_window_manipulation_module_la_SOURCES = src/modules/conf_window_manipulation/e_mod_main.c \
		      src/modules/conf_window_manipulation/e_mod_main.h \
		      src/modules/conf_window_manipulation/e_int_config_window_geometry.c \
		      src/modules/conf_window_manipulation/e_int_config_window_process.c \
		      src/modules/conf_window_manipulation/e_int_config_window_display.c \
		      src/modules/conf_window_manipulation/e_int_config_focus.c \
		      src/modules/conf_window_manipulation/e_int_config_clientlist.c

PHONIES += conf_window_manipulation install-conf_window_manipulation
conf_window_manipulation: $(conf_window_manipulationpkg_LTLIBRARIES) $(conf_window_manipulation_DATA)
install-conf_window_manipulation: install-conf_window_manipulationDATA install-conf_window_manipulationpkgLTLIBRARIES
endif
