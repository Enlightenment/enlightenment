EXTRA_DIST += src/modules/notification/module.desktop.in \
src/modules/notification/e-module-notification.edj
if USE_MODULE_NOTIFICATION
notificationdir = $(MDIR)/notification
notification_DATA = src/modules/notification/e-module-notification.edj \
		    src/modules/notification/module.desktop


notificationpkgdir = $(MDIR)/notification/$(MODULE_ARCH)
notificationpkg_LTLIBRARIES = src/modules/notification/module.la

src_modules_notification_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_notification_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_notification_module_la_SOURCES = src/modules/notification/e_mod_main.h \
				  src/modules/notification/e_mod_main.c \
				  src/modules/notification/e_mod_config.c \
				  src/modules/notification/e_mod_popup.c

src_modules_notification_module_la_LIBADD = $(MOD_LIBS)

PHONIES += notification install-notification
notification: $(notificationpkg_LTLIBRARIES) $(notification_DATA)
install-notification: install-notificationDATA install-notificationpkgLTLIBRARIES
endif
