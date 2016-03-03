EXTRA_DIST += src/modules/time/module.desktop.in \
src/modules/time/e-module-time.edj
if USE_MODULE_TIME
timedir = $(MDIR)/time
time_DATA = src/modules/time/e-module-time.edj \
	     src/modules/time/module.desktop


timepkgdir = $(MDIR)/time/$(MODULE_ARCH)
timepkg_LTLIBRARIES = src/modules/time/module.la

src_modules_time_module_la_LIBADD = $(MOD_LIBS)
src_modules_time_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_time_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_time_module_la_SOURCES = \
src/modules/time/clock.c \
src/modules/time/clock.h \
src/modules/time/config.c \
src/modules/time/mod.c \
src/modules/time/time.c

PHONIES += time install-time
time: $(timepkg_LTLIBRARIES) $(time_DATA)
install-time: install-timeDATA install-timepkgLTLIBRARIES
endif
