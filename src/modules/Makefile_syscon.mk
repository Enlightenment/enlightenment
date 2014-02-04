EXTRA_DIST += src/modules/syscon/module.desktop.in \
src/modules/syscon/e-module-syscon.edj
if USE_MODULE_SYSCON
syscondir = $(MDIR)/syscon
syscon_DATA = src/modules/syscon/e-module-syscon.edj \
	      src/modules/syscon/module.desktop


sysconpkgdir = $(MDIR)/syscon/$(MODULE_ARCH)
sysconpkg_LTLIBRARIES = src/modules/syscon/module.la

src_modules_syscon_module_la_LIBADD = $(MOD_LIBS)
src_modules_syscon_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_syscon_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_syscon_module_la_SOURCES = src/modules/syscon/e_mod_main.c \
			   src/modules/syscon/e_mod_main.h \
			   src/modules/syscon/e_int_config_syscon.c \
			   src/modules/syscon/e_syscon.c \
			   src/modules/syscon/e_syscon_gadget.c

PHONIES += syscon install-syscon
syscon: $(sysconpkg_LTLIBRARIES) $(syscon_DATA)
install-syscon: install-sysconDATA install-sysconpkgLTLIBRARIES
endif
