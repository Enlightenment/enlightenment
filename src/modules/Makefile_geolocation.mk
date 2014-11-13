EXTRA_DIST += src/modules/geolocation/module.desktop.in \
src/modules/geolocation/e-module-geolocation.edc \
src/modules/tiling/images/location_on.png \
src/modules/tiling/images/location_off.png
if USE_MODULE_GEOLOCATION
geolocationdir = $(MDIR)/geolocation
geolocation_DATA = src/modules/geolocation/e-module-geolocation.edj \
		 src/modules/geolocation/module.desktop
CLEANFILES += src/modules/geolocation/e-module-geolocation.edj


geolocationpkgdir = $(MDIR)/geolocation/$(MODULE_ARCH)
geolocationpkg_LTLIBRARIES = src/modules/geolocation/module.la

GEOLOCATION_EDJE_FLAGS = $(EDJE_FLAGS) -id $(top_srcdir)/src/modules/geolocation/images

src/modules/geolocation/%.edj: src/modules/geolocation/%.edc Makefile
	$(EDJE_CC) $(GEOLOCATION_EDJE_FLAGS) $< $@

src_modules_geolocation_module_la_LIBADD = $(MOD_LIBS)
src_modules_geolocation_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_geolocation_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_geolocation_module_la_SOURCES = \
src/modules/geolocation/e_mod_main.c \
src/modules/geolocation/gen/eldbus_geo_clue2_client.c \
src/modules/geoclocation/gen/eldbus_geo_clue2_client.h \
src/modules/geolocation/gen/eldbus_geo_clue2_location.c \
src/modules/geolocation/gen/eldbus_geo_clue2_location.h \
src/modules/geolocation/gen/eldbus_geo_clue2_manager.c \
src/modules/geolocation/gen/eldbus_geo_clue2_manager.h \
src/modules/geolocation/gen/eldbus_utils.h

PHONIES += geolocation install-geolocation
geolocation: $(geolocationpkg_LTLIBRARIES) $(geolocation_DATA)
install-geolocation: install-geolocationDATA install-geolocationpkgLTLIBRARIES
endif
