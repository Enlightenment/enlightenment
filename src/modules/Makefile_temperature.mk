EXTRA_DIST += src/modules/temperature/module.desktop.in \
src/modules/temperature/e-module-temperature.edj
if USE_MODULE_TEMPERATURE
temperaturedir = $(MDIR)/temperature
temperature_DATA = src/modules/temperature/e-module-temperature.edj \
		   src/modules/temperature/module.desktop


temperaturepkgdir = $(MDIR)/temperature/$(MODULE_ARCH)
temperaturepkg_LTLIBRARIES = src/modules/temperature/module.la


src_modules_temperature_module_la_LIBADD = $(MOD_LIBS)
src_modules_temperature_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_temperature_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_temperature_module_la_SOURCES = src/modules/temperature/e_mod_main.c \
				 src/modules/temperature/e_mod_main.h \
				 src/modules/temperature/e_mod_config.c \
				 src/modules/temperature/e_mod_tempget.c

if HAVE_EEZE
src_modules_temperature_module_la_SOURCES += src/modules/temperature/e_mod_udev.c
endif

PHONIES += temperature install-temperature
temperature: $(temperaturepkg_LTLIBRARIES) $(temperature_DATA)
install-temperature: install-temperatureDATA install-temperaturepkgLTLIBRARIES
endif
