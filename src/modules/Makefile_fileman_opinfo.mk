EXTRA_DIST += src/modules/fileman_opinfo/module.desktop.in \
src/modules/fileman_opinfo/e-module-fileman_opinfo.edj
if USE_MODULE_FILEMAN_OPINFO
fileman_opinfodir = $(MDIR)/fileman_opinfo
fileman_opinfo_DATA = src/modules/fileman_opinfo/e-module-fileman_opinfo.edj \
		      src/modules/fileman_opinfo/module.desktop


fileman_opinfopkgdir = $(MDIR)/fileman_opinfo/$(MODULE_ARCH)
fileman_opinfopkg_LTLIBRARIES = src/modules/fileman_opinfo/module.la

src_modules_fileman_opinfo_module_la_LIBADD = $(MOD_LIBS)
src_modules_fileman_opinfo_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_fileman_opinfo_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_fileman_opinfo_module_la_SOURCES = src/modules/fileman_opinfo/e_mod_main.c

PHONIES += fileman_opinfo install-fileman_opinfo
fileman_opinfo: $(fileman_opinfopkg_LTLIBRARIES) $(fileman_opinfo_DATA)
install-fileman_opinfo: install-fileman_opinfoDATA install-fileman_opinfopkgLTLIBRARIES
endif
