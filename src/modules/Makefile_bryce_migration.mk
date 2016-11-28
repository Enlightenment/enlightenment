EXTRA_DIST += src/modules/bryce_migration/module.desktop.in
if USE_MODULE_BRYCE_MIGRATION
bryce_migrationdir = $(MDIR)/bryce_migration
bryce_migration_DATA = src/modules/bryce_migration/module.desktop

bryce_migrationpkgdir = $(MDIR)/bryce_migration/$(MODULE_ARCH)
bryce_migrationpkg_LTLIBRARIES = src/modules/bryce_migration/module.la

src_modules_bryce_migration_module_la_LIBADD = $(MOD_LIBS)
src_modules_bryce_migration_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_bryce_migration_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_bryce_migration_module_la_SOURCES = src/modules/bryce_migration/e_mod_main.c


internal_bin_PROGRAMS += enlightenment_bryce_migration
enlightenment_bryce_migration_SOURCES = \
src/modules/bryce_migration/config_migration.c \
src/modules/clock/config_descriptor.c \
src/modules/time/config_descriptor.c \
src/modules/ibar/config_descriptor.c \
src/modules/luncher/config_descriptor.c \
src/bin/e_config_descriptor.c \
src/bin/e_config_data.c
enlightenment_bryce_migration_CPPFLAGS = $(E_CPPFLAGS)
enlightenment_bryce_migration_LDFLAGS = @E_SYS_LIBS@ @EET_LIBS@
#@EINA_LIBS@ @ECORE_LIBS@ @EET_LIBS@

PHONIES += bryce_migration install-bryce_migration
bryce_migration: $(bryce_migrationpkg_LTLIBRARIES) $(bryce_migration_DATA)
install-bryce_migration: install-bryce_migrationDATA install-bryce_migrationpkgLTLIBRARIES

endif
