EXTRA_DIST += src/modules/pager/module.desktop.in \
src/modules/pager/e-module-pager.edj
if USE_MODULE_PAGER
pagerdir = $(MDIR)/pager
pager_DATA = src/modules/pager/e-module-pager.edj \
	     src/modules/pager/module.desktop

pagerpkgdir = $(MDIR)/pager/$(MODULE_ARCH)
pagerpkg_LTLIBRARIES = src/modules/pager/module.la

src_modules_pager_module_la_LIBADD = $(MOD_LIBS)
src_modules_pager_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_pager_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_pager_module_la_SOURCES = src/modules/pager/e_mod_main.h \
			  src/modules/pager/e_mod_main.c \
			  src/modules/pager/e_mod_config.c \
                          src/modules/pager/gadget/pager.h \
                          src/modules/pager/gadget/pager.c \
                          src/modules/pager/gadget/mod.c \
                          src/modules/pager/gadget/config.c

PHONIES += pager install-pager
pager: $(pagerpkg_LTLIBRARIES) $(pager_DATA)
install-pager: install-pagerDATA install-pagerpkgLTLIBRARIES
endif
