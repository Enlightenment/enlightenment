EXTRA_DIST += src/modules/appmenu/module.desktop.in \
src/modules/appmenu/e-module-appmenu.edj
if USE_MODULE_APPMENU
appmenudir = $(MDIR)/appmenu
appmenu_DATA = src/modules/appmenu/module.desktop \
               src/modules/appmenu/e-module-appmenu.edj


appmenupkgdir = $(MDIR)/appmenu/$(MODULE_ARCH)
appmenupkg_LTLIBRARIES = src/modules/appmenu/module.la

src_modules_appmenu_module_la_LIBADD = $(MOD_LIBS)
src_modules_appmenu_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_appmenu_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_appmenu_module_la_SOURCES = src/modules/appmenu/e_mod_main.c \
                            src/modules/appmenu/e_mod_dbus_registrar_server.c \
                            src/modules/appmenu/e_mod_appmenu_render.c \
                            src/modules/appmenu/e_mod_appmenu_private.h


PHONIES += appmenu install-appmenu
appmenu: $(appmenupkg_LTLIBRARIES) $(appmenu_DATA)
install-appmenu: install-appmenuDATA install-appmenupkgLTLIBRARIES
endif
