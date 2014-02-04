if USE_MODULE_ILLUME-INDICATOR
illume_indicatordir = $(MDIR)/illume-indicator
illume_indicator_DATA = src/modules/illume-indicator/e-module-illume-indicator.edj \
src/modules/illume-indicator/module.desktop
	      src/modules/illume-indicator/e-module-illume-indicator.edj

illume_indicatorpkgdir = $(MDIR)/illume-indicator/$(MODULE_ARCH)
illume_indicatorpkg_LTLIBRARIES = src/modules/illume-indicator/module.la

src_modules_illume_indicator_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume_indicator_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume_indicator_module_la_SOURCES = src/modules/illume-indicator/e_mod_main.c \
				      src/modules/illume-indicator/e_mod_main.h \
				      src/modules/illume-indicator/e_mod_config.c \
				      src/modules/illume-indicator/e_mod_config.h \
				      src/modules/illume-indicator/e_mod_ind_win.c \
				      src/modules/illume-indicator/e_mod_ind_win.h

if HAVE_ENOTIFY
src_modules_illume_indicator_module_la_SOURCES += src/modules/illume-indicator/e_mod_notify.c \
				      src/modules/illume-indicator/e_mod_notify.h
endif

src_modules_illume_indicator_module_la_LIBADD = $(MOD_LIBS) @ENOTIFY_LIBS@

PHONIES += illume_indicator install-illume_indicator
illume_indicator: $(illume_indicatorpkg_LTLIBRARIES) $(illume_indicator_DATA)
install-illume_indicator: install-illume_indicatorDATA install-illume_indicatorpkgLTLIBRARIES
endif
