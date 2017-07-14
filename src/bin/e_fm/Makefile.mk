EFM_CPPFLAGS = \
-I$(top_builddir) \
-I$(top_builddir)/src/bin \
-I$(top_srcdir) \
-I$(top_srcdir)/src/bin \
-I$(top_srcdir)/src/bin/efx \
@e_cflags@ \
@cf_cflags@ \
@VALGRIND_CFLAGS@ \
@EDJE_DEF@ \
@WAYLAND_CFLAGS@ \
-DPACKAGE_BIN_DIR=\"@PACKAGE_BIN_DIR@\" \
-DPACKAGE_LIB_DIR=\"@PACKAGE_LIB_DIR@\" \
-DPACKAGE_DATA_DIR=\"@PACKAGE_DATA_DIR@\" \
-DLOCALE_DIR=\"@LOCALE_DIR@\" \
-DPACKAGE_SYSCONF_DIR=\"@PACKAGE_SYSCONF_DIR@\"

EFM_LIBS =

efm_bindir = $(libdir)/enlightenment/utils
efm_bin_PROGRAMS = \
src/bin/e_fm/enlightenment_fm 

if HAVE_UDISKS_MOUNT
udisks_s = \
src/bin/e_fm/e_fm_main_udisks.h \
src/bin/e_fm/e_fm_main_udisks.c \
src/bin/e_fm/e_fm_main_udisks2.h \
src/bin/e_fm/e_fm_main_udisks2.c
else
udisks_s =
endif

if HAVE_EEZE_MOUNT
EFM_CPPFLAGS += @EEZE_CFLAGS@ @EET_CFLAGS@
EFM_LIBS += @EEZE_LIBS@ @EET_LIBS@
eeze_s = \
src/bin/e_prefix.c \
src/bin/e_fm/e_fm_main_eeze.h \
src/bin/e_fm/e_fm_main_eeze.c
else
eeze_s =
endif

src_bin_e_fm_enlightenment_fm_SOURCES = \
src/bin/e_fm/e_fm_main.h \
src/bin/e_fm/e_fm_ipc.h \
src/bin/e_fm/e_fm_main.c \
src/bin/e_fm/e_fm_ipc.c \
$(udisks_s) \
$(eeze_s) \
src/bin/e_fm_shared_codec.c \
src/bin/e_fm_shared_device.c \
src/bin/e_user.c \
src/bin/e_sha1.c

src_bin_e_fm_enlightenment_fm_LDADD = @E_FM_LIBS@ $(EFM_LIBS)
src_bin_e_fm_enlightenment_fm_CPPFLAGS = $(EFM_CPPFLAGS)
