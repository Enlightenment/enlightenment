if USE_MODULE_ILLUME-BLUETOOTH
illume_bluetoothdir = $(MDIR)/illume-bluetooth
illume_bluetooth_DATA = src/modules/illume-bluetooth/e-module-illume-bluetooth.edj \
src/modules/illume-bluetooth/module.desktop


illume_bluetoothpkgdir = $(MDIR)/illume-bluetooth/$(MODULE_ARCH)
illume_bluetoothpkg_LTLIBRARIES = src/modules/illume-bluetooth/module.la

src_modules_illume_bluetooth_module_la_LIBADD = $(MOD_LIBS)
src_modules_illume_bluetooth_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume_bluetooth_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume_bluetooth_module_la_SOURCES = src/modules/illume-bluetooth/e_mod_main.c

PHONIES += illume_bluetooth install-illume_bluetooth
illume_bluetooth: $(illume_bluetoothpkg_LTLIBRARIES) $(illume_bluetooth_DATA)
install-illume_bluetooth: install-illume_bluetoothDATA install-illume_bluetoothpkgLTLIBRARIES
endif
