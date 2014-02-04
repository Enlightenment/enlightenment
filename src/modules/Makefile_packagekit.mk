EXTRA_DIST += src/modules/packagekit/module.desktop.in \
src/modules/packagekit/e-module-packagekit.edj
if USE_MODULE_PACKAGEKIT
packagekitdir = $(MDIR)/packagekit
packagekit_DATA = src/modules/packagekit/e-module-packagekit.edj \
                  src/modules/packagekit/module.desktop


packagekitpkgdir = $(MDIR)/packagekit/$(MODULE_ARCH)
packagekitpkg_LTLIBRARIES = src/modules/packagekit/module.la

src_modules_packagekit_module_la_LIBADD = $(MOD_LIBS)
src_modules_packagekit_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_packagekit_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_packagekit_module_la_SOURCES = \
    src/modules/packagekit/e_mod_main.c \
    src/modules/packagekit/e_mod_main.h \
    src/modules/packagekit/e_mod_config.c \
    src/modules/packagekit/e_mod_config.h \
    src/modules/packagekit/e_mod_packagekit.c \
    src/modules/packagekit/e_mod_packagekit.h

PHONIES += packagekit install-packagekit
packagekit: $(packagekitpkg_LTLIBRARIES) $(packagekit_DATA)
install-packagekit: install-packagekitDATA install-packagekitpkgLTLIBRARIES
endif
