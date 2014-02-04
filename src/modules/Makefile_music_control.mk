EXTRA_DIST += src/modules/music-control/module.desktop.in \
src/modules/music-control/e-module-music-control.edj
music_controldir = $(MDIR)/music-control
music_control_DATA = src/modules/music-control/module.desktop \
src/modules/music-control/e-module-music-control.edj


music_controlpkgdir = $(MDIR)/music-control/$(MODULE_ARCH)
music_controlpkg_LTLIBRARIES = src/modules/music-control/module.la

src_modules_music_control_module_la_DEPENDENCIES = $(MDEPENDENCIES)
src_modules_music_control_module_la_CPPFLAGS = $(MOD_CPPFLAGS) -Wno-unused-parameter
src_modules_music_control_module_la_LIBADD = @MUSIC_CONTROL_LIBS@ $(MOD_LIBS)
src_modules_music_control_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_music_control_module_la_SOURCES = \
src/modules/music-control/e_mod_main.h \
src/modules/music-control/e_mod_main.c \
src/modules/music-control/private.h \
src/modules/music-control/ui.c \
src/modules/music-control/gen/eldbus_utils.h \
src/modules/music-control/gen/eldbus_media_player2_player.c \
src/modules/music-control/gen/eldbus_media_player2_player.h \
src/modules/music-control/gen/eldbus_mpris_media_player2.c \
src/modules/music-control/gen/eldbus_mpris_media_player2.h

PHONIES += music-control install-music-control
music-control: $(music_controlpkg_LTLIBRARIES) $(music_control_DATA)
install-music-control: install-music_controlDATA install-music_controlpkgLTLIBRARIES
