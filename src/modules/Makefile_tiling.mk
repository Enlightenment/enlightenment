MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -I. \
           -I$(top_srcdir) \
           -I$(includedir) \
           -DLOCALEDIR=\"$(datadir)/locale\" \
           -DPACKAGE_DATA_DIR=\"$(module_dir)/$(PACKAGE)\" \
           @E_CFLAGS@

pkgdir = $(module_dir)/$(PACKAGE)/$(MODULE_ARCH)
pkg_LTLIBRARIES = module.la

module_la_SOURCES = e_mod_tiling.c \
		    e_mod_tiling.h \
		    window_tree.c \
		    window_tree.h \
		    e_mod_config.c

module_la_LIBADD = @E_LIBS@
module_la_LDFLAGS = -module -avoid-version
module_la_DEPENDENCIES = $(top_builddir)/config.h

filesdir = $(module_dir)/$(PACKAGE)
files_DATA = module.desktop e-module-tiling.edj # images/icon.png

EXTRA_DIST = module.desktop.in \
	     e-module-tiling.edc

e-module-tiling.edj: e-module-tiling.edc
	$(EDJE_CC) -id $(top_srcdir)/src/images $< $@

clean-local:
	 rm -rf *.edj module.desktop *~

uninstall-local:
	 rm -rf $(DESTDIR)$(module_dir)/$(PACKAGE)

