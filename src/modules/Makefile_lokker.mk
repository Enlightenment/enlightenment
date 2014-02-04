if USE_MODULE_LOKKER
lokkerdir = $(MDIR)/lokker
##lokker_DATA = \
##src/modules/lokker/e-module-lokker.edj

lokkerpkgdir = $(MDIR)/lokker/$(MODULE_ARCH)
lokkerpkg_LTLIBRARIES = src/modules/lokker/module.la

src_modules_lokker_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_lokker_module_la_LIBADD = $(MOD_LIBS)

src_modules_lokker_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_lokker_module_la_SOURCES = src/modules/lokker/e_mod_main.c \
src/modules/lokker/lokker.c \
src/modules/lokker/e_mod_main.h

PHONIES += lokker install-lokker
lokker: $(lokkerpkg_LTLIBRARIES) #$(lokker_DATA)
install-lokker: install-lokkerpkgLTLIBRARIES #install-lokkerDATA
endif
