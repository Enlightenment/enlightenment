EXTRA_DIST += src/modules/mixer/module.desktop.in \
src/modules/mixer/e-module-mixer.edj
if USE_MODULE_MIXER
mixerdir = $(MDIR)/mixer
mixer_DATA = src/modules/mixer/e-module-mixer.edj \
	     src/modules/mixer/module.desktop


mixerpkgdir = $(MDIR)/mixer/$(MODULE_ARCH)
mixerpkg_LTLIBRARIES = src/modules/mixer/module.la

src_modules_mixer_module_la_CPPFLAGS = $(MOD_CPPFLAGS) @SOUND_CFLAGS@

src_modules_mixer_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_mixer_module_la_SOURCES = src/modules/mixer/e_mod_main.c \
			  src/modules/mixer/e_mod_main.h \
			  src/modules/mixer/e_mod_mixer.h \
			  src/modules/mixer/e_mod_mixer.c \
			  src/modules/mixer/app_mixer.c \
			  src/modules/mixer/conf_gadget.c \
			  src/modules/mixer/conf_module.c \
			  src/modules/mixer/msg.c \
			  src/modules/mixer/Pulse.h \
			  src/modules/mixer/pa.h \
			  src/modules/mixer/pa.c \
			  src/modules/mixer/serial.c \
			  src/modules/mixer/sink.c \
			  src/modules/mixer/sys_pulse.c \
			  src/modules/mixer/tag.c

if HAVE_ALSA
src_modules_mixer_module_la_SOURCES += src/modules/mixer/sys_alsa.c
else
src_modules_mixer_module_la_SOURCES += src/modules/mixer/sys_dummy.c
endif

src_modules_mixer_module_la_LIBADD = $(MOD_LIBS) @SOUND_LIBS@

if HAVE_ENOTIFY
src_modules_mixer_module_la_CPPFLAGS += @ENOTIFY_CFLAGS@
src_modules_mixer_module_la_LIBADD += @ENOTIFY_LIBS@
endif

PHONIES += mixer install-mixer
mixer: $(mixerpkg_LTLIBRARIES) $(mixer_DATA)
install-mixer: install-mixerDATA install-mixerpkgLTLIBRARIES
endif
