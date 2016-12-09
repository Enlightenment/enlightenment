EXTRA_DIST += src/modules/ibar/module.desktop.in \
src/modules/ibar/e-module-ibar.edj
if USE_MODULE_IBAR
ibardir = $(MDIR)/ibar
ibar_DATA = src/modules/ibar/e-module-ibar.edj \
	    src/modules/ibar/module.desktop


ibarpkgdir = $(MDIR)/ibar/$(MODULE_ARCH)
ibarpkg_LTLIBRARIES = src/modules/ibar/module.la

src_modules_ibar_module_la_LIBADD = $(MOD_LIBS)
src_modules_ibar_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_ibar_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_ibar_module_la_SOURCES = src/modules/ibar/e_mod_main.c \
			 src/modules/ibar/config_descriptor.c \
			 src/modules/ibar/config_descriptor.h \
			 src/modules/ibar/e_mod_main.h \
			 src/modules/ibar/e_mod_config.c

PHONIES += ibar install-ibar
ibar: $(ibarpkg_LTLIBRARIES) $(ibar_DATA)
install-ibar: install-ibarDATA install-ibarpkgLTLIBRARIES
endif
