physicsdir = $(MDIR)/physics
physics_DATA = src/modules/physics/e-module-physics.edj \
	       src/modules/physics/module.desktop


src_modules_physics_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_physics_module_la_CPPFLAGS += @EPHYSICS_CFLAGS@

physicspkgdir = $(MDIR)/physics/$(MODULE_ARCH)
physicspkg_LTLIBRARIES = src/modules/physics/module.la

src_modules_physics_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_physics_module_la_SOURCES = src/modules/physics/e_mod_main.c \
			    src/modules/physics/e_mod_main.h \
			    src/modules/physics/e_mod_config.c \
			    src/modules/physics/e_mod_physics_cfdata.c \
			    src/modules/physics/e_mod_physics_cfdata.h \
			    src/modules/physics/e_mod_physics.c \
			    src/modules/physics/e_mod_physics.h

src_modules_physics_module_la_LIBADD = $(MOD_LIBS) @EPHYSICS_LIBS@

PHONIES += physics install-physics
physics: $(physicspkg_LTLIBRARIES) $(physics_DATA)
install-physics: install-physicsDATA install-physicspkgLTLIBRARIES
