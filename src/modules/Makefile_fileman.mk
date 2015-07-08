EXTRA_DIST += src/modules/fileman/module.desktop.in \
src/modules/fileman/e-module-fileman.edj
if USE_MODULE_FILEMAN
filemandir = $(MDIR)/fileman
fileman_DATA = src/modules/fileman/e-module-fileman.edj \
	       src/modules/fileman/module.desktop


filemanpkgdir = $(MDIR)/fileman/$(MODULE_ARCH)
filemanpkg_LTLIBRARIES = src/modules/fileman/module.la

src_modules_fileman_module_la_LIBADD = $(MOD_LIBS)
src_modules_fileman_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_fileman_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_fileman_module_la_SOURCES = src/modules/fileman/e_mod_main.c \
			    src/modules/fileman/e_mod_main.h \
			    src/modules/fileman/e_fwin.c \
			    src/modules/fileman/e_fwin_nav.c \
			    src/modules/fileman/e_mod_config.c \
			    src/modules/fileman/e_int_config_mime.c \
			    src/modules/fileman/e_int_config_mime_edit.c \
			    src/modules/fileman/e_mod_dbus.c \
			    src/modules/fileman/e_mod_menu.c

PHONIES += fileman install-fileman
fileman: $(filemanpkg_LTLIBRARIES) $(fileman_DATA)
install-fileman: install-filemanDATA install-filemanpkgLTLIBRARIES
endif
