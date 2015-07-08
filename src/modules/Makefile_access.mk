EXTRA_DIST += src/modules/access/module.desktop.in
if USE_MODULE_ACCESS
accessdir = $(MDIR)/access
access_DATA = src/modules/access/module.desktop


accesspkgdir = $(MDIR)/access/$(MODULE_ARCH)
accesspkg_LTLIBRARIES = src/modules/access/module.la

src_modules_access_module_la_LIBADD = $(MOD_LIBS)
src_modules_access_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_access_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_access_module_la_SOURCES = src/modules/access/e_mod_main.c \
			   src/modules/access/e_mod_main.h \
			   src/modules/access/e_mod_config.c

PHONIES += access install-access
access: $(accesspkg_LTLIBRARIES) $(access_DATA)
install-access: install-accessDATA install-accesspkgLTLIBRARIES
endif
