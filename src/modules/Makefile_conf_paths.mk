EXTRA_DIST += src/modules/conf_paths/module.desktop.in \
src/modules/conf_paths/e-module-conf_paths.edj
if USE_MODULE_CONF_PATHS
conf_pathsdir = $(MDIR)/conf_paths
conf_paths_DATA = src/modules/conf_paths/e-module-conf_paths.edj \
		  src/modules/conf_paths/module.desktop


conf_pathspkgdir = $(MDIR)/conf_paths/$(MODULE_ARCH)
conf_pathspkg_LTLIBRARIES = src/modules/conf_paths/module.la

src_modules_conf_paths_module_la_LIBADD = $(MOD_LIBS)
src_modules_conf_paths_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_conf_paths_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf_paths_module_la_SOURCES = src/modules/conf_paths/e_mod_main.c \
			       src/modules/conf_paths/e_mod_main.h \
			       src/modules/conf_paths/e_int_config_paths.c \
			       src/modules/conf_paths/e_int_config_env.c

PHONIES += conf_paths install-conf_paths
conf_paths: $(conf_pathspkg_LTLIBRARIES) $(conf_paths_DATA)
install-conf_paths: install-conf_pathsDATA install-conf_pathspkgLTLIBRARIES
endif
