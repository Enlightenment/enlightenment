EXTRA_DIST += src/modules/geoclue2/module.desktop.in \
src/modules/geoclue2/e-module-geoclue2.edj
if USE_MODULE_GEOLOCATION
geolocationdir = $(MDIR)/geoclue2
geolocation_DATA = src/modules/geoclue2/e-module-geoclue2.edj \
		 src/modules/geoclue2/module.desktop


geolocationpkgdir = $(MDIR)/geoclue2/$(MODULE_ARCH)
geolocationpkg_LTLIBRARIES = src/modules/geoclue2/module.la

src_modules_geoclue2_module_la_LIBADD = $(MOD_LIBS)
src_modules_geoclue2_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_geoclue2_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_geoclue2_module_la_SOURCES = \
src/modules/geoclue2/e_mod_main.c \
src/modules/geoclue2/gen/eldbus_geo_clue2_client.c \
src/modules/geoclue2/gen/eldbus_geo_clue2_client.h \
src/modules/geoclue2/gen/eldbus_geo_clue2_location.c \
src/modules/geoclue2/gen/eldbus_geo_clue2_location.h \
src/modules/geoclue2/gen/eldbus_geo_clue2_manager.c \
src/modules/geoclue2/gen/eldbus_geo_clue2_manager.h \
src/modules/geoclue2/gen/eldbus_utils.h

PHONIES += geolocation install-geolocation
geolocation: $(geolocationpkg_LTLIBRARIES) $(geolocation_DATA)
install-geolocation: install-geolocationDATA install-geolocationpkgLTLIBRARIES
endif
