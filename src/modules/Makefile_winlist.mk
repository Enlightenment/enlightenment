EXTRA_DIST += src/modules/winlist/module.desktop.in \
src/modules/winlist/e-module-winlist.edj
if USE_MODULE_WINLIST
winlistdir = $(MDIR)/winlist
winlist_DATA = src/modules/winlist/e-module-winlist.edj \
	       src/modules/winlist/module.desktop


winlistpkgdir = $(MDIR)/winlist/$(MODULE_ARCH)
winlistpkg_LTLIBRARIES = src/modules/winlist/module.la

src_modules_winlist_module_la_LIBADD = $(MOD_LIBS)
src_modules_winlist_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_winlist_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_winlist_module_la_SOURCES = src/modules/winlist/e_mod_main.h \
			    src/modules/winlist/e_mod_main.c \
			    src/modules/winlist/e_int_config_winlist.c \
			    src/modules/winlist/e_winlist.h \
			    src/modules/winlist/e_winlist.c

PHONIES += winlist install-winlist
winlist: $(winlistpkg_LTLIBRARIES) $(winlist_DATA)
install-winlist: install-winlistDATA install-winlistpkgLTLIBRARIES
endif
