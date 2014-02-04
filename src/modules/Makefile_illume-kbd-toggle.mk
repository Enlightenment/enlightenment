if USE_MODULE_ILLUME-KBD-TOGGLE
illume_kbd_toggledir = $(MDIR)/illume-kbd-toggle
illume_kbd_toggle_DATA = src/modules/illume-kbd-toggle/e-module-illume-kbd-toggle.edj \
			 src/modules/illume-kbd-toggle/module.desktop


illume_kbd_togglepkgdir = $(MDIR)/illume-kbd-toggle/$(MODULE_ARCH)
illume_kbd_togglepkg_LTLIBRARIES = src/modules/illume-kbd-toggle/module.la

src_modules_illume_kbd_toggle_module_la_LIBADD = $(MOD_LIBS)
src_modules_illume_kbd_toggle_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume_kbd_toggle_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume_kbd_toggle_module_la_SOURCES = src/modules/illume-kbd-toggle/e_mod_main.c

PHONIES += illume_kbd_toggle install-illume_kbd_toggle
illume_kbd_toggle: $(illume_kbd_togglepkg_LTLIBRARIES) $(illume_kbd_toggle_DATA)
install-illume_kbd_toggle: install-illume_kbd_toggleDATA install-illume_kbd_togglepkgLTLIBRARIES
endif
