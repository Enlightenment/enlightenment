wd = src/modules/wizard/data/def-ibar.txt
wdd = src/modules/wizard/data/desktop/home.desktop \
	src/modules/wizard/data/desktop/root.desktop \
	src/modules/wizard/data/desktop/tmp.desktop

EXTRA_DIST += src/modules/wizard/module.desktop.in \
src/modules/wizard/e-module-wizard.edj \
$(wd) \
$(wdd)
if USE_MODULE_WIZARD
wizarddir = $(MDIR)/wizard
wizard_DATA = $(wd)

wizard_desktopdir = $(MDIR)/wizard/desktop
wizard_desktop_DATA = $(wdd)


### dont install these - this way e wont list the module for people to
#wizard_DATA = src/modules/wizard/e-module-wizard.edj \
#	      src/modules/wizard/module.desktop

wizardpkgdir = $(MDIR)/wizard/$(MODULE_ARCH)
wizardpkg_LTLIBRARIES  = src/modules/wizard/module.la \
			 src/modules/wizard/page_000.la \
			 src/modules/wizard/page_010.la \
			 src/modules/wizard/page_011.la \
			 src/modules/wizard/page_020.la \
			 src/modules/wizard/page_030.la \
			 src/modules/wizard/page_040.la \
			 src/modules/wizard/page_050.la \
			 src/modules/wizard/page_060.la \
			 src/modules/wizard/page_070.la \
			 src/modules/wizard/page_080.la \
			 src/modules/wizard/page_090.la \
			 src/modules/wizard/page_100.la \
			 src/modules/wizard/page_110.la \
			 src/modules/wizard/page_120.la \
			 src/modules/wizard/page_130.la \
			 src/modules/wizard/page_150.la \
			 src/modules/wizard/page_160.la \
			 src/modules/wizard/page_170.la \
			 src/modules/wizard/page_180.la \
			 src/modules/wizard/page_200.la

src_modules_wizard_module_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_module_la_SOURCES = src/modules/wizard/e_mod_main.c \
			   src/modules/wizard/e_wizard.c \
			   src/modules/wizard/e_wizard.h

src_modules_wizard_page_000_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_000_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_000_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_000_la_SOURCES = src/modules/wizard/page_000.c

src_modules_wizard_page_010_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_010_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_010_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_010_la_SOURCES = src/modules/wizard/page_010.c

src_modules_wizard_page_011_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_011_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_011_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_011_la_SOURCES = src/modules/wizard/page_011.c

src_modules_wizard_page_020_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_020_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_020_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_020_la_SOURCES = src/modules/wizard/page_020.c

src_modules_wizard_page_030_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_030_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_030_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_030_la_SOURCES = src/modules/wizard/page_030.c

src_modules_wizard_page_040_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_040_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_040_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_040_la_SOURCES = src/modules/wizard/page_040.c

src_modules_wizard_page_050_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_050_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_050_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_050_la_SOURCES = src/modules/wizard/page_050.c

src_modules_wizard_page_060_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_060_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_060_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_060_la_SOURCES = src/modules/wizard/page_060.c

src_modules_wizard_page_070_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_070_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_070_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_070_la_SOURCES = src/modules/wizard/page_070.c

src_modules_wizard_page_080_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_080_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_080_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_080_la_SOURCES = src/modules/wizard/page_080.c

src_modules_wizard_page_090_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_090_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_090_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_090_la_SOURCES = src/modules/wizard/page_090.c

src_modules_wizard_page_100_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_100_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_100_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_100_la_SOURCES = src/modules/wizard/page_100.c

src_modules_wizard_page_110_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_110_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_110_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_110_la_SOURCES = src/modules/wizard/page_110.c

src_modules_wizard_page_120_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_120_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_120_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_120_la_SOURCES = src/modules/wizard/page_120.c

src_modules_wizard_page_130_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_130_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_130_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_130_la_SOURCES = src/modules/wizard/page_130.c

src_modules_wizard_page_150_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_150_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_150_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_150_la_SOURCES = src/modules/wizard/page_150.c

src_modules_wizard_page_160_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_160_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_160_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_160_la_SOURCES = src/modules/wizard/page_160.c

src_modules_wizard_page_170_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_170_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_170_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_170_la_SOURCES = src/modules/wizard/page_170.c

src_modules_wizard_page_180_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_180_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_180_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_180_la_SOURCES = src/modules/wizard/page_180.c

src_modules_wizard_page_200_la_LIBADD = $(MOD_LIBS)
src_modules_wizard_page_200_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wizard_page_200_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wizard_page_200_la_SOURCES = src/modules/wizard/page_200.c

PHONIES += wizard install-wizard
wizard: $(wizardpkg_LTLIBRARIES) $(wizard_DATA)
install-wizard: install-wizardDATA install-wizardpkgLTLIBRARIES
endif
