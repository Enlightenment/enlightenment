if USE_MODULE_WL_BUFFER
wl_bufferdir = $(MDIR)/wl_buffer

wl_bufferpkgdir = $(MDIR)/wl_buffer/$(MODULE_ARCH)
wl_bufferpkg_LTLIBRARIES = src/modules/wl_buffer/module.la

src_modules_wl_buffer_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_wl_buffer_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WAYLAND_CFLAGS@
src_modules_wl_buffer_module_la_LIBADD   = $(MOD_LIBS) @WAYLAND_LIBS@
src_modules_wl_buffer_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wl_buffer_module_la_SOURCES = \
src/modules/wl_buffer/e_mod_main.c

PHONIES += wl_buffer install-wl_buffer
wl_buffer: $(wl_bufferpkg_LTLIBRARIES) $(wl_buffer_DATA)
install-wl_buffer: install-wl_bufferpkgLTLIBRARIES
endif
