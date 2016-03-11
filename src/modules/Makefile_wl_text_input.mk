EXTRA_DIST += \
src/modules/wl_text_input/text-protocol.h \
src/modules/wl_text_input/text-protocol.c \
src/modules/wl_text_input/input-method-protocol.h \
src/modules/wl_text_input/input-method-protocol.c

if USE_MODULE_WL_TEXT_INPUT
wl_text_inputdir = $(MDIR)/wl_text_input

wl_text_inputpkgdir = $(MDIR)/wl_text_input/$(MODULE_ARCH)
wl_text_inputpkg_LTLIBRARIES = src/modules/wl_text_input/module.la

src_modules_wl_text_input_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_text_input_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WAYLAND_CFLAGS@
src_modules_wl_text_input_module_la_LIBADD   = $(MOD_LIBS) @WAYLAND_LIBS@
src_modules_wl_text_input_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wl_text_input_module_la_SOURCES = \
  src/modules/wl_text_input/e_mod_main.c \
  src/modules/wl_text_input/text-protocol.c \
  src/modules/wl_text_input/text-protocol.h \
  src/modules/wl_text_input/input-method-protocol.c \
  src/modules/wl_text_input/input-method-protocol.h

PHONIES += wl_text_input install-wl_text_input
wl_text_input: $(wl_text_inputpkg_LTLIBRARIES) $(wl_text_input_DATA)
install-wl_text_input: install-wl_text_inputDATA install-wl_text_inputpkgLTLIBRARIES
endif
