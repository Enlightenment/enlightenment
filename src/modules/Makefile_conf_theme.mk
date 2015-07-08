EXTRA_DIST += src/modules/conf_theme/module.desktop.in
if USE_MODULE_CONF_THEME
conf_themedir = $(MDIR)/conf_theme
conf_theme_DATA = src/modules/conf_theme/module.desktop


conf_themepkgdir = $(MDIR)/conf_theme/$(MODULE_ARCH)
conf_themepkg_LTLIBRARIES = src/modules/conf_theme/module.la

src_modules_conf_theme_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_theme_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_theme_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_theme_module_la_SOURCES = src/modules/conf_theme/e_mod_main.c \
			       src/modules/conf_theme/e_mod_main.h \
			       src/modules/conf_theme/e_int_config_borders.c \
			       src/modules/conf_theme/e_int_config_color_classes.c \
			       src/modules/conf_theme/e_int_config_fonts.c \
			       src/modules/conf_theme/e_int_config_scale.c \
			       src/modules/conf_theme/e_int_config_theme.c \
			       src/modules/conf_theme/e_int_config_theme_import.c \
			       src/modules/conf_theme/e_int_config_transitions.c \
			       src/modules/conf_theme/e_int_config_wallpaper.c \
			       src/modules/conf_theme/e_int_config_xsettings.c

PHONIES += conf_theme install-conf_theme
conf_theme: $(conf_themepkg_LTLIBRARIES) $(conf_theme_DATA)
install-conf_theme: install-conf_themeDATA install-conf_themepkgLTLIBRARIES
endif
