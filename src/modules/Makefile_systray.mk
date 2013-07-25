systraydir = $(MDIR)/systray
systray_DATA = src/modules/systray/e-module-systray.edj \
	       src/modules/systray/module.desktop

EXTRA_DIST += $(systray_DATA)

systraypkgdir = $(MDIR)/systray/$(MODULE_ARCH)
systraypkg_LTLIBRARIES = src/modules/systray/module.la

src_modules_systray_module_la_LIBADD = $(MOD_LIBS)
src_modules_systray_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_systray_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_systray_module_la_SOURCES = src/modules/systray/e_mod_main.h \
			    src/modules/systray/e_mod_main.c \
			    src/modules/systray/e_mod_xembed.c \
			    src/modules/systray/e_mod_notifier_host_private.h \
			    src/modules/systray/e_mod_notifier_host.c \
			    src/modules/systray/e_mod_notifier_host_dbus.c \
			    src/modules/systray/e_mod_notifier_watcher.c

PHONIES += systray install-systray
systray: $(systraypkg_LTLIBRARIES) $(systray_DATA)
install-systray: install-systrayDATA install-systraypkgLTLIBRARIES
