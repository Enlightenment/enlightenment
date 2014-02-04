if USE_MODULE_ILLUME-HOME
illume_homedir = $(MDIR)/illume-home
illume_home_DATA = src/modules/illume-home/e-module-illume-home.edj \
		   src/modules/illume-home/module.desktop


illume_homepkgdir = $(MDIR)/illume-home/$(MODULE_ARCH)
illume_homepkg_LTLIBRARIES = src/modules/illume-home/module.la

src_modules_illume_home_module_la_LIBADD = $(MOD_LIBS)
src_modules_illume_home_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume_home_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume_home_module_la_SOURCES = src/modules/illume-home/e_mod_main.c \
src/modules/illume-home/e_mod_main.h \
src/modules/illume-home/e_mod_config.c \
src/modules/illume-home/e_mod_config.h \
src/modules/illume-home/e_busycover.c \
src/modules/illume-home/e_busycover.h

PHONIES += illume_home install-illume_home
illume_home: $(illume_homepkg_LTLIBRARIES) $(illume_home_DATA)
install-illume_home: install-illume_homeDATA install-illume_homepkgLTLIBRARIES
endif
