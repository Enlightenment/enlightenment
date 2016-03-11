EXTRA_DIST += src/modules/connman/module.desktop.in \
src/modules/connman/e-module-connman.edj
if USE_MODULE_CONNMAN
connmandir = $(MDIR)/connman
connman_DATA = src/modules/connman/e-module-connman.edj \
	       src/modules/connman/module.desktop


connmanpkgdir = $(MDIR)/connman/$(MODULE_ARCH)
connmanpkg_LTLIBRARIES = src/modules/connman/module.la

src_modules_connman_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_connman_module_la_SOURCES = src/modules/connman/e_mod_main.h \
			    src/modules/connman/e_mod_main.c \
			    src/modules/connman/e_mod_config.c \
			    src/modules/connman/e_connman.c \
			    src/modules/connman/agent.c \
			    src/modules/connman/E_Connman.h

src_modules_connman_module_la_CPPFLAGS = $(MOD_CPPFLAGS) -Wno-unused-parameter
src_modules_connman_module_la_LIBADD = $(MOD_LIBS)

PHONIES += connman install-connman
connman: $(connmanpkg_LTLIBRARIES) $(connman_DATA)
install-connman: install-connmanDATA install-connmanpkgLTLIBRARIES
endif
