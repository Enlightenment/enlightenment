if USE_MODULE_WL_TEXT_INPUT
wl_text_inputdir = $(MDIR)/wl_text_input

wl_text_inputpkgdir = $(MDIR)/wl_text_input/$(MODULE_ARCH)
wl_text_inputpkg_LTLIBRARIES = src/modules/wl_text_input/module.la

wl_text_input_wayland_sources = \
  src/modules/wl_text_input/text-input-unstable-v1-protocol.c          \
  src/modules/wl_text_input/text-input-unstable-v1-server-protocol.h   \
  src/modules/wl_text_input/input-method-unstable-v1-protocol.c        \
  src/modules/wl_text_input/input-method-unstable-v1-server-protocol.h
src_modules_wl_text_input_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_text_input_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WAYLAND_CFLAGS@
src_modules_wl_text_input_module_la_LIBADD   = $(MOD_LIBS) @WAYLAND_LIBS@
src_modules_wl_text_input_module_la_LDFLAGS = $(MOD_LDFLAGS)
nodist_src_modules_wl_text_input_module_la_SOURCES = \
 $(wl_text_input_wayland_sources)
src_modules_wl_text_input_module_la_SOURCES = \
  src/modules/wl_text_input/e_mod_main.c

MAINTAINERCLEANFILES += \
 $(wl_text_input_wayland_sources)

src/modules/wl_text_input/e_mod_main.c: src/modules/wl_text_input/text-input-unstable-v1-server-protocol.h\
 src/modules/wl_text_input/input-method-unstable-v1-server-protocol.h

PHONIES += wl_text_input install-wl_text_input
wl_text_input: $(wl_text_inputpkg_LTLIBRARIES) $(wl_text_input_DATA)
install-wl_text_input: install-wl_text_inputDATA install-wl_text_inputpkgLTLIBRARIES
endif
