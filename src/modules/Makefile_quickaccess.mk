EXTRA_DIST += src/modules/quickaccess/module.desktop.in \
src/modules/quickaccess/e-module-quickaccess.edj
if USE_MODULE_QUICKACCESS
quickaccessdir = $(MDIR)/quickaccess
quickaccess_DATA = src/modules/quickaccess/e-module-quickaccess.edj \
		   src/modules/quickaccess/module.desktop


quickaccesspkgdir = $(MDIR)/quickaccess/$(MODULE_ARCH)
quickaccesspkg_LTLIBRARIES = src/modules/quickaccess/module.la

src_modules_quickaccess_module_la_LIBADD = $(MOD_LIBS)
src_modules_quickaccess_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_quickaccess_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_quickaccess_module_la_SOURCES = src/modules/quickaccess/e_mod_main.c \
src/modules/quickaccess/e_mod_main.h \
src/modules/quickaccess/e_mod_config.c \
src/modules/quickaccess/e_mod_quickaccess.c \
src/modules/quickaccess/e_quickaccess_bindings.c \
src/modules/quickaccess/e_quickaccess_db.c

PHONIES += quickaccess install-quickaccess
quickaccess: $(quickaccesspkg_LTLIBRARIES) $(quickaccess_DATA)
install-quickaccess: install-quickaccessDATA install-quickaccesspkgLTLIBRARIES
endif
