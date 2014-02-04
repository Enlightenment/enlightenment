EXTRA_DIST += src/modules/conf_applications/module.desktop.in \
src/modules/conf_applications/e-module-conf_applications.edj
if USE_MODULE_CONF_APPLICATIONS
conf_applicationsdir = $(MDIR)/conf_applications
conf_applications_DATA = src/modules/conf_applications/e-module-conf_applications.edj \
			 src/modules/conf_applications/module.desktop


conf_applicationspkgdir = $(MDIR)/conf_applications/$(MODULE_ARCH)
conf_applicationspkg_LTLIBRARIES = src/modules/conf_applications/module.la

src_modules_conf_applications_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_applications_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_applications_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_applications_module_la_SOURCES = src/modules/conf_applications/e_mod_main.c \
			      src/modules/conf_applications/e_mod_main.h \
			      src/modules/conf_applications/e_int_config_apps.c \
			      src/modules/conf_applications/e_int_config_defapps.c \
			      src/modules/conf_applications/e_int_config_deskenv.c \
			      src/modules/conf_applications/e_int_config_apps_personal.c

PHONIES += conf_applications install-conf_applications
conf_applications: $(conf_applicationspkg_LTLIBRARIES) $(conf_applications_DATA)
install-conf_applications: install-conf_applicationsDATA install-conf_applicationspkgLTLIBRARIES
endif
