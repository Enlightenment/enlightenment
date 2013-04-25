MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
  -I$(top_builddir) \
  -I$(top_builddir)/src/bin/e_wayland \
  -I$(top_srcdir) \
  -I$(top_srcdir)/src/bin/e_wayland \
  @e_wl_cflags@ \
  @cf_cflags@ \
  @VALGRIND_CFLAGS@ \
  @EDJE_DEF@ \
  @ELM_CFLAGS@ \
  @EMOTION_CFLAGS@ \
  @WAYLAND_CFLAGS@ \
  @WAYLAND_EGL_CFLAGS@ \
  -DPACKAGE_BIN_DIR=\"@PACKAGE_BIN_DIR@\" \
  -DPACKAGE_LIB_DIR=\"@PACKAGE_LIB_DIR@\" \
  -DPACKAGE_DATA_DIR=\"@PACKAGE_DATA_DIR@\" \
  -DLOCALE_DIR=\"@LOCALE_DIR@\" \
  -DPACKAGE_SYSCONF_DIR=\"@PACKAGE_SYSCONF_DIR@\"

bin_PROGRAMS = \
  enlightenment_wl

ENLIGHTENMENTHEADERS = \
  e.h \
  e_includes.h \
  e_intl.h \
  e_log.h \
  e_error.h \
  e_signals.h \
  e_utils.h \
  e_prefix.h \
  e_user.h \
  e_filereg.h \
  e_object.h \
  e_path.h \
  e_config_data.h \
  e_config.h \
  e_backlight.h \
  e_border.h \
  e_powersave.h \
  e_font.h \
  e_int_menus.h \
  e_env.h \
  e_module.h \
  e_comp.h \
  e_plane.h

enlightenment_wl_src = \
  e_log.c \
  e_error.c \
  e_signals.c \
  e_utils.c \
  e_prefix.c \
  e_user.c \
  e_filereg.c \
  e_object.c \
  e_path.c \
  e_config_data.c \
  e_config.c \
  e_powersave.c \
  e_env.c \
  e_module.c \
  e_comp.c \
  e_plane.c \
  $(ENLIGHTENMENTHEADERS)

enlightenment_wl_CFLAGS = $(AM_CPPFLAGS) @WAYLAND_CFLAGS@ @WAYLAND_EGL_CFLAGS@
enlightenment_wl_CPPFLAGS = $(AM_CPPFLAGS) -DE_LOGGING=1
enlightenment_wl_SOURCES = \
  e_main.c \
  $(enlightenment_wl_src)

enlightenment_wl_LDFLAGS = -export-dynamic
enlightenment_wl_LDADD = @e_wl_libs@ @ELM_LIBS@ @EMOTION_LIBS@ @dlopen_libs@ @cf_libs@ @VALGRIND_LIBS@ @WAYLAND_LIBS@ @WAYLAND_EGL_LIBS@ -lm

installed_headersdir = $(prefix)/include/enlightenment
installed_headers_DATA = $(ENLIGHTENMENTHEADERS)
