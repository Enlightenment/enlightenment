if USE_MODULE_ILLUME-SOFTKEY
illume_softkeydir = $(MDIR)/illume-softkey
illume_softkey_DATA = src/modules/illume-softkey/e-module-illume-softkey.edj \
		      src/modules/illume-softkey/module.desktop

	      src/modules/illume-softkey/module.desktop.in


illume_softkeypkgdir = $(MDIR)/illume-softkey/$(MODULE_ARCH)
illume_softkeypkg_LTLIBRARIES = src/modules/illume-softkey/module.la

src_modules_illume_softkey_module_la_LIBADD = $(MOD_LIBS)
src_modules_illume_softkey_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume_softkey_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume_softkey_module_la_SOURCES = src/modules/illume-softkey/e_mod_main.c \
				   src/modules/illume-softkey/e_mod_main.h \
				   src/modules/illume-softkey/e_mod_sft_win.c \
				   src/modules/illume-softkey/e_mod_sft_win.h \
				   src/modules/illume-softkey/e_mod_config.c \
				   src/modules/illume-softkey/e_mod_config.h

PHONIES += illume_softkey install-illume_softkey
illume_softkey: $(illume_softkeypkg_LTLIBRARIES) $(illume_softkey_DATA)
install-illume_softkey: install-illume_softkeyDATA install-illume_softkeypkgLTLIBRARIES
endif
