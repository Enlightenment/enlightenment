if USE_MODULE_ILLUME-MODE-TOGGLE
illume_mode_toggledir = $(MDIR)/illume-mode-toggle
illume_mode_toggle_DATA = src/modules/illume-mode-toggle/e-module-illume-mode-toggle.edj \
			 src/modules/illume-mode-toggle/module.desktop


illume_mode_togglepkgdir = $(MDIR)/illume-mode-toggle/$(MODULE_ARCH)
illume_mode_togglepkg_LTLIBRARIES = src/modules/illume-mode-toggle/module.la

src_modules_illume_mode_toggle_module_la_LIBADD = $(MOD_LIBS)
src_modules_illume_mode_toggle_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume_mode_toggle_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume_mode_toggle_module_la_SOURCES = src/modules/illume-mode-toggle/e_mod_main.c

PHONIES += illume_mode_toggle install-illume_mode_toggle
illume_mode_toggle: $(illume_mode_togglepkg_LTLIBRARIES) $(illume_mode_toggle_DATA)
install-illume_mode_toggle: install-illume_mode_toggleDATA install-illume_mode_togglepkgLTLIBRARIES
endif
