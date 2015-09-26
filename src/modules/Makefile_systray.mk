EXTRA_DIST += src/modules/systray/module.desktop.in \
src/modules/systray/e-module-systray.edj
if USE_MODULE_SYSTRAY
systraydir = $(MDIR)/systray
systray_DATA = src/modules/systray/e-module-systray.edj \
	       src/modules/systray/module.desktop


systraypkgdir = $(MDIR)/systray/$(MODULE_ARCH)
systraypkg_LTLIBRARIES = src/modules/systray/module.la

src_modules_systray_module_la_LIBADD = $(MOD_LIBS)
src_modules_systray_module_la_CPPFLAGS = $(MOD_CPPFLAGS) -DNEED_X=1
src_modules_systray_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_systray_module_la_SOURCES = src/modules/systray/e_mod_main.h \
			    src/modules/systray/e_mod_main.c \
			    src/modules/systray/e_mod_notifier_host_private.h \
			    src/modules/systray/e_mod_notifier_host.c \
			    src/modules/systray/e_mod_notifier_host_dbus.c \
			    src/modules/systray/e_mod_notifier_watcher.c

src_modules_systray_module_la_SOURCES += src/modules/systray/e_mod_xembed.c

PHONIES += systray install-systray
systray: $(systraypkg_LTLIBRARIES) $(systray_DATA)
install-systray: install-systrayDATA install-systraypkgLTLIBRARIES
endif
