EXTRA_DIST += src/modules/backlight/module.desktop.in \
src/modules/backlight/e-module-backlight.edj
if USE_MODULE_BACKLIGHT
backlightdir = $(MDIR)/backlight
backlight_DATA = src/modules/backlight/e-module-backlight.edj \
		 src/modules/backlight/module.desktop


backlightpkgdir = $(MDIR)/backlight/$(MODULE_ARCH)
backlightpkg_LTLIBRARIES = src/modules/backlight/module.la

src_modules_backlight_module_la_LIBADD = $(MOD_LIBS)
src_modules_backlight_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_backlight_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_backlight_module_la_SOURCES = src/modules/backlight/e_mod_main.c \
                                          src/modules/backlight/gadget/backlight.h \
                                          src/modules/backlight/gadget/backlight.c \
                                          src/modules/backlight/gadget/mod.c

PHONIES += backlight install-backlight
backlight: $(backlightpkg_LTLIBRARIES) $(backlight_DATA)
install-backlight: install-backlightDATA install-backlightpkgLTLIBRARIES
endif
