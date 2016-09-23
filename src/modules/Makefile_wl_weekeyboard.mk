EXTRA_DIST += \
   src/modules/wl_weekeyboard/themes/default/default_600.edj \
   src/modules/wl_weekeyboard/themes/default/default_720.edj \
   src/modules/wl_weekeyboard/themes/default/default_1080.edj

if USE_MODULE_WL_WEEKEYBOARD
wl_weekeyboarddir = $(MDIR)/wl_weekeyboard
wl_weekeyboard_DATA = \
   src/modules/wl_weekeyboard/themes/default/default_600.edj \
   src/modules/wl_weekeyboard/themes/default/default_720.edj \
   src/modules/wl_weekeyboard/themes/default/default_1080.edj

wl_weekeyboardpkgdir = $(MDIR)/wl_weekeyboard/$(MODULE_ARCH)
wl_weekeyboardpkg_LTLIBRARIES = src/modules/wl_weekeyboard/module.la

src_modules_wl_weekeyboard_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_weekeyboard_module_la_CPPFLAGS  = \
   $(MOD_CPPFLAGS)               \
   @WAYLAND_CFLAGS@              \
   -DPKGDATADIR='"$pkgdatadir)"'
src_modules_wl_weekeyboard_module_la_LIBADD   = $(MOD_LIBS) @WAYLAND_LIBS@
src_modules_wl_weekeyboard_module_la_LDFLAGS = $(MOD_LDFLAGS)
wl_weekeyboard_wayland_sources = \
   src/modules/wl_weekeyboard/input-method-unstable-v1-protocol.c          \
   src/modules/wl_weekeyboard/input-method-unstable-v1-client-protocol.h   \
   src/modules/wl_weekeyboard/text-input-unstable-v1-protocol.c            \
   src/modules/wl_weekeyboard/text-input-unstable-v1-client-protocol.h
nodist_src_modules_wl_weekeyboard_module_la_SOURCES = \
 $(wl_weekeyboard_wayland_sources)
src_modules_wl_weekeyboard_module_la_SOURCES =                 \
   src/modules/wl_weekeyboard/e_mod_main.c                     \
   src/modules/wl_weekeyboard/wkb-log.h                        \
   src/modules/wl_weekeyboard/wkb-log.c

src/modules/wl_weekeyboard/e_mod_main.c:\
 src/modules/wl_weekeyboard/input-method-unstable-v1-client-protocol.h\
 src/modules/wl_weekeyboard/text-input-unstable-v1-client-protocol.h

MAINTAINERCLEANFILES += \
 $(wl_weekeyboard_wayland_sources)

PHONIES += wl_weekeyboard install-wl_weekeyboard
wl_weekeyboard: $(wl_weekeyboardpkg_LTLIBRARIES) $(wl_weekeyboard_DATA)
install-wl_weekeyboard: install-wl_weekeyboardpkgLTLIBRARIES
endif
