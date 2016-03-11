EXTRA_DIST += src/modules/bluez4/module.desktop.in \
src/modules/bluez4/e-module-bluez4.edj
if USE_MODULE_BLUEZ4
bluez4dir = $(MDIR)/bluez4
bluez4_DATA = src/modules/bluez4/e-module-bluez4.edj \
	      src/modules/bluez4/module.desktop


bluez4pkgdir = $(MDIR)/bluez4/$(MODULE_ARCH)
bluez4pkg_LTLIBRARIES = src/modules/bluez4/module.la

src_modules_bluez4_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_bluez4_module_la_SOURCES = src/modules/bluez4/e_mod_main.h \
			   src/modules/bluez4/e_mod_main.c \
			   src/modules/bluez4/ebluez4.h \
			   src/modules/bluez4/ebluez4.c \
			   src/modules/bluez4/agent.h \
			   src/modules/bluez4/agent.c

src_modules_bluez4_module_la_CPPFLAGS = $(MOD_CPPFLAGS) -Wno-unused-parameter
src_modules_bluez4_module_la_LIBADD = $(MOD_LIBS)

PHONIES += bluez4 install-bluez4
bluez4: $(bluez4pkg_LTLIBRARIES) $(bluez4_DATA)
install-bluez4: install-bluez4DATA install-bluez4pkgLTLIBRARIES
endif
