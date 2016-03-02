EXTRA_DIST += \
src/modules/wireless/module.desktop.in \
src/modules/wireless/e-module-wireless.edj

if USE_MODULE_WIRELESS
wirelessdir = $(MDIR)/wireless
wireless_DATA = \
src/modules/wireless/module.desktop \
src/modules/wireless/e-module-wireless.edj

wirelesspkgdir = $(MDIR)/wireless/$(MODULE_ARCH)
wirelesspkg_LTLIBRARIES = src/modules/wireless/module.la

src_modules_wireless_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_wireless_module_la_SOURCES = \
src/modules/wireless/connman.c \
src/modules/wireless/mod.c \
src/modules/wireless/wireless.c \
src/modules/wireless/wireless.h

src_modules_wireless_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_wireless_module_la_LIBADD = $(MOD_LIBS)

PHONIES += wireless install-wireless
wireless: $(wirelesspkg_LTLIBRARIES) $(wireless_DATA)
install-wireless: install-wirelessDATA install-wirelesspkgLTLIBRARIES
endif
