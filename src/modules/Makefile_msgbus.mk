EXTRA_DIST += src/modules/msgbus/module.desktop.in \
src/modules/msgbus/e-module-msgbus.edj
if USE_MODULE_MSGBUS
msgbusdir = $(MDIR)/msgbus
msgbus_DATA = src/modules/msgbus/e-module-msgbus.edj \
	      src/modules/msgbus/module.desktop


msgbuspkgdir = $(MDIR)/msgbus/$(MODULE_ARCH)
msgbuspkg_LTLIBRARIES = src/modules/msgbus/module.la

src_modules_msgbus_module_la_LIBADD = $(MOD_LIBS)
src_modules_msgbus_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_msgbus_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_msgbus_module_la_SOURCES = \
src/modules/msgbus/e_mod_main.h \
src/modules/msgbus/e_mod_main.c \
src/modules/msgbus/msgbus_audit.c \
src/modules/msgbus/msgbus_desktop.c \
src/modules/msgbus/msgbus_lang.c \
src/modules/msgbus/msgbus_module.c \
src/modules/msgbus/msgbus_profile.c \
src/modules/msgbus/msgbus_window.c

PHONIES += msgbus install-msgbus
msgbus: $(msgbuspkg_LTLIBRARIES) $(msgbus_DATA)
install-msgbus: install-msgbusDATA install-msgbuspkgLTLIBRARIES
endif
