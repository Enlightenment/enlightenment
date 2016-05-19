EXTRA_DIST += src/modules/teamwork/module.desktop.in \
src/modules/teamwork/e-module-teamwork.edj
if USE_MODULE_TEAMWORK
teamworkdir = $(MDIR)/teamwork
teamwork_DATA = src/modules/teamwork/e-module-teamwork.edj \
		   src/modules/teamwork/module.desktop


teamworkpkgdir = $(MDIR)/teamwork/$(MODULE_ARCH)
teamworkpkg_LTLIBRARIES = src/modules/teamwork/module.la

src_modules_teamwork_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_teamwork_module_la_LIBADD = $(MOD_LIBS)

src_modules_teamwork_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_teamwork_module_la_SOURCES = src/modules/teamwork/e_mod_main.c \
src/modules/teamwork/e_mod_config.c \
src/modules/teamwork/e_mod_main.h \
src/modules/teamwork/e_mod_tw.c \
src/modules/teamwork/sha1.c \
src/modules/teamwork/sha1.h

if HAVE_WAYLAND
src_modules_teamwork_module_la_SOURCES += \
src/modules/teamwork/wl.c \
src/modules/teamwork/wl_teamwork.c \
src/modules/teamwork/wl_teamwork.h
endif

if ! HAVE_WAYLAND_ONLY
src_modules_teamwork_module_la_SOURCES += \
src/modules/teamwork/x11.c
endif

PHONIES += teamwork install-teamwork
teamwork: $(teamworkpkg_LTLIBRARIES) $(teamwork_DATA)
install-teamwork: install-teamworkDATA install-teamworkpkgLTLIBRARIES
endif
