EXTRA_DIST += src/modules/mixer/module.desktop.in \
src/modules/mixer/e-module-mixer.edj \
src/modules/mixer/emixer.png \
src/modules/mixer/emixer.desktop
if USE_MODULE_MIXER
mixerdir = $(MDIR)/mixer
mixer_DATA = src/modules/mixer/e-module-mixer.edj \
	     src/modules/mixer/module.desktop

mixerpkgdir = $(MDIR)/mixer/$(MODULE_ARCH)
mixerpkg_LTLIBRARIES = src/modules/mixer/module.la

emixerlib = src/modules/mixer/lib/emix.c src/modules/mixer/lib/emix.h

if HAVE_ALSA
emixerlib += src/modules/mixer/lib/backends/alsa/alsa.c
endif

if HAVE_PULSE
emixerlib += src/modules/mixer/lib/backends/pulseaudio/pulse_ml.c
emixerlib += src/modules/mixer/lib/backends/pulseaudio/pulse.c
endif

src_modules_mixer_emixerdir = $(mixerpkgdir)
bin_PROGRAMS += src/modules/mixer/emixer
src_modules_mixer_emixer_SOURCES = src/modules/mixer/emixer.c \
                          $(emixerlib)
src_modules_mixer_emixer_CPPFLAGS = $(MOD_CPPFLAGS) -I$(top_srcdir)/src/modules/mixer/lib -DEMIXER_BUILD
src_modules_mixer_emixer_LDADD = $(MOD_LIBS) @PULSE_LIBS@ @ALSA_LIBS@

src_modules_mixer_module_la_CPPFLAGS = $(MOD_CPPFLAGS) @e_cflags@ @ALSA_CFLAGS@ @PULSE_CFLAGS@ -I$(top_srcdir)/src/modules/mixer/lib
src_modules_mixer_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_mixer_module_la_SOURCES = src/modules/mixer/e_mod_main.c \
			  src/modules/mixer/e_mod_main.h \
			  src/modules/mixer/e_mod_config.c \
			  src/modules/mixer/e_mod_config.h \
                          $(emixerlib)
src_modules_mixer_module_la_LIBADD = $(MOD_LIBS) @PULSE_LIBS@ @ALSA_LIBS@

PHONIES += mixer install-mixer
mixer: $(mixerpkg_LTLIBRARIES) $(mixer_DATA)
install-mixer: install-mixerDATA install-mixerpkgLTLIBRARIES

desktopfiledir = $(datadir)/applications
desktopfile_DATA = src/modules/mixer/emixer.desktop

iconsdir= $(datadir)/pixmaps
icons_DATA = src/modules/mixer/emixer.png

endif
