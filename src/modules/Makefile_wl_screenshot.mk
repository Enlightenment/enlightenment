EXTRA_DIST += src/modules/wl_screenshot/module.desktop.in \
src/modules/wl_screenshot/e-module-wl_screenshot.edj \
src/modules/wl_screenshot/e_screenshooter_client_protocol.h \
src/modules/wl_screenshot/e_screenshooter_client_protocol.c
if USE_MODULE_WL_SCREENSHOT
wl_screenshotdir = $(MDIR)/wl_screenshot
wl_screenshot_DATA = src/modules/wl_screenshot/e-module-wl_screenshot.edj \
	       src/modules/wl_screenshot/module.desktop

wl_screenshotpkgdir = $(MDIR)/wl_screenshot/$(MODULE_ARCH)
wl_screenshotpkg_LTLIBRARIES = src/modules/wl_screenshot/module.la

wl_screenshot_module_la_DEPENDENCIES = $(MDEPENDENCIES)
wl_screenshot_module_la_CPPFLAGS  = $(MOD_CPPFLAGS) @WL_SCREENSHOT_CFLAGS@ @WAYLAND_CFLAGS@
wl_screenshot_module_la_LIBADD   = $(MOD_LIBS) @WL_SCREENSHOT_LIBS@ @WAYLAND_LIBS@
wl_screenshot_module_la_LDFLAGS = $(MOD_LDFLAGS) # @WL_SCREENSHOT_LDFLAGS@ @WAYLAND_LDFLAGS@
wl_screenshot_module_la_SOURCES = src/modules/wl_screenshot/e_mod_main.c \
			    src/modules/wl_screenshot/e_mod_main.h \
                            src/modules/wl_screenshot/e_screenshooter_client_protocol.h \
                            src/modules/wl_screenshot/e_screenshooter_client_protocol.c

PHONIES: wl_screenshot install-wl_screenshot
wl_screenshot: $(wl_screenshotpkg_LTLIBRARIES) $(wl_screenshot_DATA)
install-wl_screenshot: install-wl_screenshotDATA install-wl_screenshotpkgLTLIBRARIES
endif
