EXTRA_DIST += src/modules/conf_performance/module.desktop.in \
src/modules/conf_performance/e-module-conf_performance.edj
if USE_MODULE_CONF_PERFORMANCE
conf_performancedir = $(MDIR)/conf_performance
conf_performance_DATA = src/modules/conf_performance/e-module-conf_performance.edj \
src/modules/conf_performance/module.desktop


conf_performancepkgdir = $(MDIR)/conf_performance/$(MODULE_ARCH)
conf_performancepkg_LTLIBRARIES = src/modules/conf_performance/module.la

src_modules_conf_performance_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_performance_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_performance_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_performance_module_la_SOURCES = src/modules/conf_performance/e_mod_main.c \
			     src/modules/conf_performance/e_mod_main.h \
			     src/modules/conf_performance/e_int_config_performance.c \
			     src/modules/conf_performance/e_int_config_powermanagement.c

PHONIES += conf_performance install-conf_performance
conf_performance: $(conf_performancepkg_LTLIBRARIES) $(conf_performance_DATA)
install-conf_performance: install-conf_performanceDATA install-conf_performancepkgLTLIBRARIES
endif
