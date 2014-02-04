if USE_MODULE_ILLUME-KEYBOARD
illume_keyboarddir = $(MDIR)/illume-keyboard
illume_keyboard_DATA = src/modules/illume-keyboard/e-module-illume-keyboard.edj \
		       src/modules/illume-keyboard/module.desktop

	      src/modules/illume-keyboard/module.desktop.in


# keyboards
illumekeyboardsdir = $(MDIR)/illume-keyboard/keyboards
illumekeyboards_DATA = src/modules/illume-keyboard/keyboards/ignore_built_in_keyboards \
		       src/modules/illume-keyboard/keyboards/Default.kbd \
		       src/modules/illume-keyboard/keyboards/alpha.png \
		       src/modules/illume-keyboard/keyboards/Numbers.kbd \
		       src/modules/illume-keyboard/keyboards/numeric.png \
		       src/modules/illume-keyboard/keyboards/Terminal.kbd \
		       src/modules/illume-keyboard/keyboards/qwerty.png \
		       src/modules/illume-keyboard/keyboards/up.png \
		       src/modules/illume-keyboard/keyboards/down.png \
		       src/modules/illume-keyboard/keyboards/left.png \
		       src/modules/illume-keyboard/keyboards/right.png \
		       src/modules/illume-keyboard/keyboards/shift.png \
		       src/modules/illume-keyboard/keyboards/tab.png \
		       src/modules/illume-keyboard/keyboards/enter.png \
		       src/modules/illume-keyboard/keyboards/backspace.png


# dicts
illumedictsdir = $(MDIR)/illume-keyboard/dicts
illumedicts_DATA = src/modules/illume-keyboard/dicts/English_US.dic \
		   src/modules/illume-keyboard/dicts/English_US_Small.dic



illume_keyboardpkgdir = $(MDIR)/illume-keyboard/$(MODULE_ARCH)
illume_keyboardpkg_LTLIBRARIES = src/modules/illume-keyboard/module.la

src_modules_illume_keyboard_module_la_LIBADD = $(MOD_LIBS)
src_modules_illume_keyboard_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume_keyboard_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume_keyboard_module_la_SOURCES = src/modules/illume-keyboard/e_mod_main.c \
				    src/modules/illume-keyboard/e_mod_main.h \
				    src/modules/illume-keyboard/e_kbd_int.c \
				    src/modules/illume-keyboard/e_kbd_int.h \
				    src/modules/illume-keyboard/e_kbd_dict.c \
				    src/modules/illume-keyboard/e_kbd_dict.h \
				    src/modules/illume-keyboard/e_kbd_buf.c \
				    src/modules/illume-keyboard/e_kbd_buf.h \
				    src/modules/illume-keyboard/e_kbd_send.c \
				    src/modules/illume-keyboard/e_kbd_send.h \
				    src/modules/illume-keyboard/e_mod_config.c \
				    src/modules/illume-keyboard/e_mod_config.h

# TODO: incomplete
PHONIES += illume_keyboard install-illume_keyboard
illume_keyboard: $(illume_keyboardpkg_LTLIBRARIES) $(illume_keyboard_DATA)
install-illume_keyboard: install-illume_keyboardDATA install-illume_keyboardpkgLTLIBRARIES
endif
