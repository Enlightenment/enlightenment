EXTRA_DIST += src/modules/geolocation/module.desktop.in \
src/modules/geolocation/e-module-geolocation.edj \
src/modules/geolocation/org.freedesktop.GeoClue2.xml

if USE_MODULE_GEOLOCATION
geolocationdir = $(MDIR)/geolocation
geolocation_DATA = src/modules/geolocation/e-module-geolocation.edj \
		 src/modules/geolocation/module.desktop

geolocationpkgdir = $(MDIR)/geolocation/$(MODULE_ARCH)
geolocationpkg_LTLIBRARIES = src/modules/geolocation/module.la

GEO_GEN = \
src/modules/geolocation/eldbus_geo_clue2_client.c \
src/modules/geolocation/eldbus_geo_clue2_client.h \
src/modules/geolocation/eldbus_geo_clue2_location.c \
src/modules/geolocation/eldbus_geo_clue2_location.h \
src/modules/geolocation/eldbus_geo_clue2_manager.c \
src/modules/geolocation/eldbus_geo_clue2_manager.h \
src/modules/geolocation/eldbus_utils.h

MAINTAINERCLEANFILES += $(GEO_GEN)

src/modules/geolocation/e_mod_main.c: $(GEO_GEN)
$(GEO_GEN): src/modules/geolocation/org.freedesktop.GeoClue2.xml
	@cd $(top_builddir)/src/modules/geolocation && \
	eldbus-codegen $(abs_top_srcdir)/src/modules/geolocation/org.freedesktop.GeoClue2.xml

src_modules_geolocation_module_la_LIBADD = $(MOD_LIBS)
src_modules_geolocation_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_geolocation_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_geolocation_module_la_SOURCES = \
src/modules/geolocation/e_mod_main.c \
$(GEO_GEN)

PHONIES += geolocation install-geolocation
geolocation: $(geolocationpkg_LTLIBRARIES) $(geolocation_DATA)
install-geolocation: install-geolocationDATA install-geolocationpkgLTLIBRARIES
endif
