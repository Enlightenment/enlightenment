EXTRA_DIST += src/modules/battery/module.desktop.in \
src/modules/battery/e-module-battery.edj
if USE_MODULE_BATTERY
batterydir = $(MDIR)/battery
battery_DATA = src/modules/battery/e-module-battery.edj \
	       src/modules/battery/module.desktop


batterypkgdir = $(MDIR)/battery/$(MODULE_ARCH)
batterypkg_LTLIBRARIES = src/modules/battery/module.la

src_modules_battery_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_battery_module_la_SOURCES = src/modules/battery/e_mod_main.h \
			     src/modules/battery/e_mod_main.c \
			     src/modules/battery/e_mod_config.c

if HAVE_EEZE
src_modules_battery_module_la_SOURCES += src/modules/battery/e_mod_udev.c
else
if HAVE_OPENBSD
src_modules_battery_module_la_SOURCES += src/modules/battery/e_mod_openbsd.c
else
src_modules_battery_module_la_SOURCES += src/modules/battery/e_mod_upower.c
endif
endif

src_modules_battery_module_la_LIBADD = $(MOD_LIBS)
src_modules_battery_module_la_LDFLAGS = $(MOD_LDFLAGS)

if HAVE_ENOTIFY
src_modules_battery_module_la_CPPFLAGS += @ENOTIFY_CFLAGS@
src_modules_battery_module_la_LIBADD += @ENOTIFY_LIBS@
endif

src_modules_battery_batgetdir = $(batterypkgdir)
src_modules_battery_batget_PROGRAMS = src/modules/battery/batget

src_modules_battery_batget_CPPFLAGS = $(MOD_CPPFLAGS) @BATTERY_CFLAGS@
src_modules_battery_batget_SOURCES = src/modules/battery/batget.c
src_modules_battery_batget_LDADD   = @BATTERY_LIBS@
src_modules_battery_batget_LDFLAGS = @BATTERY_LDFLAGS@

PHONIES += battery install-battery
battery: $(batterypkg_LTLIBRARIES) $(battery_DATA) $(src_modules_battery_batget_PROGRAMS)
install-battery: install-batteryDATA install-batterypkgLTLIBRARIES install-src_modules_battery_batgetPROGRAMS
endif
