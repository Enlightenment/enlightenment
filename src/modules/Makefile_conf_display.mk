EXTRA_DIST += src/modules/conf_display/module.desktop.in
if USE_MODULE_CONF_DISPLAY
conf_displaydir = $(MDIR)/conf_display
conf_display_DATA = src/modules/conf_display/module.desktop


conf_displaypkgdir = $(MDIR)/conf_display/$(MODULE_ARCH)
conf_displaypkg_LTLIBRARIES = src/modules/conf_display/module.la

src_modules_conf_display_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_display_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_display_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_display_module_la_SOURCES = src/modules/conf_display/e_mod_main.c \
				 src/modules/conf_display/e_mod_main.h \
				 src/modules/conf_display/e_int_config_screensaver.c \
				 src/modules/conf_display/e_int_config_dpms.c \
				 src/modules/conf_display/e_int_config_desklock.c \
				 src/modules/conf_display/e_int_config_desklock_fsel.c \
				 src/modules/conf_display/e_int_config_desks.c \
				 src/modules/conf_display/e_int_config_desk.c

PHONIES += conf_display install-conf_display
conf_display: $(conf_displaypkg_LTLIBRARIES) $(conf_display_DATA)
install-conf_display: install-conf_displayDATA install-conf_displaypkgLTLIBRARIES
endif
