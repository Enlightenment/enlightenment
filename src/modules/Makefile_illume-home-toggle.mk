if USE_MODULE_ILLUME-HOME-TOGGLE
illume_home_toggledir = $(MDIR)/illume-home-toggle
illume_home_toggle_DATA = src/modules/illume-home-toggle/e-module-illume-home-toggle.edj \
			  src/modules/illume-home-toggle/module.desktop


illume_home_togglepkgdir = $(MDIR)/illume-home-toggle/$(MODULE_ARCH)
illume_home_togglepkg_LTLIBRARIES = src/modules/illume-home-toggle/module.la

src_modules_illume_home_toggle_module_la_LIBADD = $(MOD_LIBS)
src_modules_illume_home_toggle_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume_home_toggle_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume_home_toggle_module_la_SOURCES = src/modules/illume-home-toggle/e_mod_main.c

PHONIES += illume_home_toggle install-illume_home_toggle
illume_home_toggle: $(illume_home_togglepkg_LTLIBRARIES) $(illume_home_toggle_DATA)
install-illume_home_toggle: install-illume_home_toggleDATA install-illume_home_togglepkgLTLIBRARIES
endif
