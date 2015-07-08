EXTRA_DIST += src/modules/everything/module.desktop.in \
src/modules/everything/e-module-everything.edj \
src/modules/everything/e-module-everything-start.edj
if USE_MODULE_EVERYTHING
everythingdir = $(MDIR)/everything
everything_DATA = src/modules/everything/e-module-everything.edj \
		  src/modules/everything/e-module-everything-start.edj \
		  src/modules/everything/module.desktop


everythingpkgdir = $(MDIR)/everything/$(MODULE_ARCH)
everythingpkg_LTLIBRARIES = src/modules/everything/module.la

EVRYHEADERS = src/modules/everything/evry_api.h \
	      src/modules/everything/evry_types.h

src_modules_everything_module_la_LIBADD = $(MOD_LIBS)
src_modules_everything_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_everything_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_everything_module_la_SOURCES = $(EVRYHEADERS) \
			       src/modules/everything/e_mod_main.c \
			       src/modules/everything/e_mod_main.h \
			       src/modules/everything/evry.c \
			       src/modules/everything/evry_config.c \
			       src/modules/everything/evry_util.c \
			       src/modules/everything/evry_history.c \
			       src/modules/everything/evry_plugin.c \
			       src/modules/everything/evry_plug_aggregator.c \
			       src/modules/everything/evry_plug_actions.c \
			       src/modules/everything/evry_view.c \
			       src/modules/everything/evry_view_tabs.c \
			       src/modules/everything/evry_view_help.c \
			       src/modules/everything/evry_plug_text.c \
			       src/modules/everything/evry_plug_clipboard.c \
			       src/modules/everything/evry_plug_collection.c \
			       src/modules/everything/evry_gadget.c \
			       src/modules/everything/md5.c \
			       src/modules/everything/md5.h \
			       src/modules/everything/evry_plug_apps.c \
			       src/modules/everything/evry_plug_files.c \
			       src/modules/everything/evry_plug_windows.c \
			       src/modules/everything/evry_plug_settings.c \
			       src/modules/everything/evry_plug_calc.c

everything_headersdir = $(pkgincludedir)
dist_everything_headers_DATA = $(EVRYHEADERS)

if HAVE_FREEBSD
everything_pkgconfigdir = $(libdir)data/pkgconfig
else
everything_pkgconfigdir = $(libdir)/pkgconfig
endif

everything_pkgconfig_DATA = src/modules/everything/everything.pc
DISTCLEANFILES += src/modules/everything/everything.pc

PHONIES += everything install-everything
everything: $(everythingpkg_LTLIBRARIES) $(everything_DATA)
install-everything: install-everythingDATA install-everythingpkgLTLIBRARIES install-everything_pkgconfigDATA
endif
