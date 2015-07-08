EXTRA_DIST += src/modules/xkbswitch/module.desktop.in \
src/modules/xkbswitch/e-module-xkbswitch.edj
if USE_MODULE_XKBSWITCH
xkbswitchdir = $(MDIR)/xkbswitch
xkbswitch_DATA = src/modules/xkbswitch/e-module-xkbswitch.edj \
		 src/modules/xkbswitch/module.desktop


xkbswitchpkgdir = $(MDIR)/xkbswitch/$(MODULE_ARCH)
xkbswitchpkg_LTLIBRARIES = src/modules/xkbswitch/module.la

src_modules_xkbswitch_module_la_LIBADD = $(MOD_LIBS)
src_modules_xkbswitch_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_xkbswitch_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_xkbswitch_module_la_SOURCES = src/modules/xkbswitch/e_mod_main.c \
			      src/modules/xkbswitch/e_mod_main.h \
			      src/modules/xkbswitch/e_mod_config.c \
			      src/modules/xkbswitch/e_mod_parse.c \
			      src/modules/xkbswitch/e_mod_parse.h

PHONIES += xkbswitch install-xkbswitch
xkbswitch: $(xkbswitchpkg_LTLIBRARIES) $(xkbswitch_DATA)
install-xkbswitch: install-xkbswitchDATA install-xkbswitchpkgLTLIBRARIES
endif
