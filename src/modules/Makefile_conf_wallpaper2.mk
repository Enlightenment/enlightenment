EXTRA_DIST += src/modules/conf_wallpaper2/module.desktop.in
if USE_MODULE_CONF_WALLPAPER2
conf_wallpaper2dir = $(MDIR)/conf_wallpaper2
conf_wallpaper2_DATA = src/modules/conf_wallpaper2/module.desktop


conf_wallpaper2pkgdir = $(MDIR)/conf_wallpaper2/$(MODULE_ARCH)
conf_wallpaper2pkg_LTLIBRARIES = src/modules/conf_wallpaper2/module.la

src_modules_conf_wallpaper2_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_wallpaper2_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_wallpaper2_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_wallpaper2_module_la_SOURCES = src/modules/conf_wallpaper2/e_mod_main.c \
				    src/modules/conf_wallpaper2/e_mod_main.h \
				    src/modules/conf_wallpaper2/e_int_config_wallpaper.c

PHONIES += conf_wallpaper2 install-conf_wallpaper2
conf_wallpaper2: $(conf_wallpaper2pkg_LTLIBRARIES) $(conf_wallpaper2_DATA)
install-conf_wallpaper2: install-conf_wallpaper2DATA install-conf_wallpaper2pkgLTLIBRARIES
endif
