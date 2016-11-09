EXTRA_DIST += src/modules/luncher/module.desktop.in \
src/modules/luncher/e-module-luncher.edj
if USE_MODULE_IBAR
luncherdir = $(MDIR)/luncher
luncher_DATA = src/modules/luncher/e-module-luncher.edj \
	    src/modules/luncher/module.desktop


luncherpkgdir = $(MDIR)/luncher/$(MODULE_ARCH)
luncherpkg_LTLIBRARIES = src/modules/luncher/module.la

src_modules_luncher_module_la_LIBADD = $(MOD_LIBS)
src_modules_luncher_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_luncher_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_luncher_module_la_SOURCES = src/modules/luncher/mod.c \
			 src/modules/luncher/luncher.h \
			 src/modules/luncher/bar.c \
                         src/modules/luncher/config.c

PHONIES += luncher install-luncher
luncher: $(luncherpkg_LTLIBRARIES) $(luncher_DATA)
install-luncher: install-luncherDATA install-luncherpkgLTLIBRARIES
endif
