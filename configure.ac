##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_define([v_maj], [0])
m4_define([v_min], [19])
m4_define([v_mic], [99])
m4_define([v_rev], m4_esyscmd([(git rev-list --count HEAD 2>/dev/null || echo 0) | tr -d '\n']))dnl
##--   When released, remove the dnl on the below line
dnl m4_undefine([v_rev])
m4_define([relname], [0.20.0])
##--   When doing snapshots - change soname. remove dnl on below line
m4_define([relname], [ver-autocannoli-0.20])
dnl m4_define([v_rel], [-release relname])
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_ifdef([v_rev], [m4_define([v_ver], [v_maj.v_min.v_mic.v_rev])], [m4_define([v_ver], [v_maj.v_min.v_mic])])
m4_define([lt_cur], m4_eval(v_maj + v_min))
m4_define([lt_rev], v_mic)
m4_define([lt_age], v_min)
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##

AC_INIT([enlightenment], [v_ver], [enlightenment-devel@lists.sourceforge.net])
AC_PREREQ([2.52])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AC_CONFIG_HEADERS([config.h])
AH_TOP([
#ifndef EFL_CONFIG_H__
#define EFL_CONFIG_H__
])
AH_BOTTOM([
#endif /* EFL_CONFIG_H__ */
])

AM_INIT_AUTOMAKE([1.11 dist-xz -Wno-portability])
AM_SILENT_RULES([yes])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_MKDIR_P
AC_C_BIGENDIAN
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_FUNC_ALLOCA

LT_INIT([disable-static])

##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_ifdef([v_rev], , [m4_define([v_rev], [0])])
m4_ifdef([v_rel], , [m4_define([v_rel], [])])
AC_DEFINE_UNQUOTED(VMAJ, [v_maj], [Major version])
AC_DEFINE_UNQUOTED(VMIN, [v_min], [Minor version])
AC_DEFINE_UNQUOTED(VMIC, [v_mic], [Micro version])
AC_DEFINE_UNQUOTED(VREV, [v_rev], [Revison])
##AC_DEFINE_UNQUOTED(E_RELEASE_BUILD, [1], [doin it])
version_info="lt_cur:lt_rev:lt_age"
release_info="v_rel"
AC_SUBST([version_info])
AC_SUBST([release_info])
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##

release=relname
AC_SUBST([release])

PKG_PROG_PKG_CONFIG

EFL_CHECK_PATH_MAX

AC_CHECK_FUNCS(setenv)
AC_CHECK_FUNCS(unsetenv)
AC_CHECK_FUNCS(clearenv)
AC_CHECK_HEADERS(features.h)

case "$host_os" in
   darwin*)
      AC_DEFINE([environ], [(*_NSGetEnviron())],
                ["apple doesn't follow POSIX in this case."])
      ;;
   *)
      ;;
esac
AC_TRY_COMPILE([
#define _GNU_SOURCE 1
#include <unistd.h>
    ],[
extern char **environ;
    ],[
AC_DEFINE(HAVE_ENVIRON, 1, [Have environ var])
    ])

efl_version="1.15.2"
AC_SUBST(efl_version)

AC_CHECK_HEADERS([sys/timerfd.h sys/ptrace.h arpa/inet.h netinet/in.h])

dnl AC_CHECK_HEADERS(X11/extensions/shape.h,, AC_MSG_ERROR([Cannot find X11/extensions/shape.h. Make sure your CFLAGS environment variable contains include lines for the location of this file]))

AC_CHECK_HEADERS(fnmatch.h,, AC_MSG_ERROR([Cannot find fnmatch.h. Make sure your CFLAGS environment variable contains include lines for the location of this file]))

AC_CHECK_FUNCS(fnmatch, res=yes, res=no)
if test "x$res" = "xno"; then
  AC_CHECK_LIB(fnmatch, fnmatch, res=yes, res=no)
  if test "x$res" = "xno"; then
    AC_MSG_ERROR([Cannot find fnmatch() in neither libc nor libfnmatch])
  else
    fnmatch_libs="-lfnmatch"
  fi
fi

AC_SUBST([fnmatch_libs])

have_pam=no
AC_ARG_ENABLE(pam,
  AS_HELP_STRING([--enable-pam], [enable PAM support @<:@default=detect@:>@]),
  [want_pam=$enableval],
  [want_pam=auto]
)
if test "x$want_pam" != "xno" ; then
  AC_CHECK_HEADERS(security/pam_appl.h, [
    LIBS="$LIBS -lpam"
    have_pam=yes
    AC_DEFINE(HAVE_PAM, 1, [PAM Authentication Support])])
    if test "x$want_pam$have_pam" = "xyesno" ; then
      AC_MSG_ERROR([PAM support was requested but not found])
    fi
fi

dnl AC_E_CHECK_PKG(VALGRIND, [valgrind >= 2.4.0], [], [:])
AC_SUBST([VALGRIND_CFLAGS])
AC_SUBST([VALGRIND_LIBS])

MODULE_ARCH="$host_os-$host_cpu-relname"
AC_SUBST([MODULE_ARCH])
AC_DEFINE_UNQUOTED(MODULE_ARCH, "$MODULE_ARCH", "Module architecture")

if test "x${bindir}" = 'xNONE'; then
  if test "x${prefix}" = "xNONE"; then
    PACKAGE_BIN_DIR="${ac_default_prefix}/bin"
  else
    PACKAGE_BIN_DIR="${prefix}/bin"
  fi
else
  PACKAGE_BIN_DIR="${bindir}"
fi
AC_SUBST([PACKAGE_BIN_DIR])

if test "x${libdir}" = 'xNONE'; then
  if test "x${prefix}" = "xNONE"; then
    PACKAGE_LIB_DIR="${ac_default_prefix}/lib"
  else
    PACKAGE_LIB_DIR="${prefix}/lib"
  fi
else
  PACKAGE_LIB_DIR="${libdir}"
fi
AC_SUBST([PACKAGE_LIB_DIR])

if test "x${prefix}" = "xNONE"; then
  PACKAGE_DATA_DIR="${ac_default_prefix}/share/${PACKAGE}"
  LOCALE_DIR="${ac_default_prefix}/share/locale"
  PACKAGE_SYSCONF_DIR="${ac_default_prefix}/etc"
else
  PACKAGE_DATA_DIR="${prefix}/share/${PACKAGE}"
  LOCALE_DIR="${prefix}/share/locale"
  PACKAGE_SYSCONF_DIR="${sysconfdir}"
fi
AC_SUBST([PACKAGE_DATA_DIR])
AC_SUBST([LOCALE_DIR])
AC_SUBST([PACKAGE_SYSCONF_DIR])

#######################################
## Simple X11 build/link

AC_ARG_ENABLE(simple-x11,
  AS_HELP_STRING([--enable-simple-x11], [enable simple x11 linking]),
  [ want_evas_simple_x11=$enableval ]
)
    
dnl x_dir=""
dnl x_cflags=""
dnl x_libs=""
dnl AC_PATH_XTRA
dnl AC_CHECK_HEADER(X11/X.h,
dnl   [
dnl     if test "x$want_evas_simple_x11" = "xyes"; then
dnl       x_libs="${x_libs} -lX11 -lXext"
dnl     else
dnl       x_dir=${x_dir:-/usr/X11R6}
dnl       x_cflags=${x_cflags:--I${x_includes:-$x_dir/include}}
dnl       x_libs="${x_libs:--L${x_libraries:-$x_dir/lib}} -lX11 -lXext"
dnl     fi
dnl   ],[
dnl     AC_MSG_ERROR([Cannot find X headers and libraries.])
dnl   ]
dnl )
dnl AC_SUBST([x_cflags])
dnl AC_SUBST([x_libs])

cf_cflags=""
cf_libs=""
PCPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${CPPFLAGS} -I /System/Library/Frameworks/CoreFoundation.framework/Headers"
AC_CHECK_HEADERS(CFBase.h,
  [
    cf_cflags="-I /System/Library/Frameworks/CoreFoundation.framework/Headers"
    cf_cflags="${cf_cflags} -I /System/Library/Frameworks/IOKit.framework/Headers"
    cf_libs="-framework 'CoreFoundation' -framework 'IOKit'"
  ]
)
CPPFLAGS="${PCPPFLAGS}"
AC_SUBST([cf_cflags])
AC_SUBST([cf_libs])

PKG_CHECK_MODULES([BLUEZ], [bluez],
	[have_bluetooth="yes"],
	[have_bluetooth="no"])
AM_CONDITIONAL([HAVE_BLUETOOTH], [test "x${have_bluetooth}"])
if test "x${have_bluetooth}" = "xyes"; then
   AC_DEFINE_UNQUOTED([HAVE_BLUETOOTH], [1], [Bluetooth is there])
fi

AC_ARG_ENABLE([systemd],
  AC_HELP_STRING([--disable-systemd], [disable systemd support @<:@default=detect@:>@]),
  [want_systemd=${enableval}], [want_systemd=auto])

AC_ARG_WITH([systemdunitdir],
         AC_HELP_STRING([--with-systemdunitdir=DIR],
                [path to systemd user services directory]),
         [USER_SESSION_DIR=${withval}])

if test "$want_systemd" == "no"; then
  have_systemd_user_session="no"
elif test -n "${USER_SESSION_DIR}"; then
  have_systemd_user_session="yes"
  AC_MSG_NOTICE([Using systemd user services directory as ${USER_SESSION_DIR}])
else
  # Detect systemd user session directory properly
  EFL_PKG_CHECK_VAR([USER_SESSION_DIR], [systemd >= 192], [systemduserunitdir],
	[have_systemd_user_session="yes"], [have_systemd_user_session="no"])

  if test "$want_systemd" = "yes" -a "$have_systemd_user_session" = "no"; then
     AC_MSG_ERROR([systemd support wanted, but systemd was not found.])
  fi
fi

AM_CONDITIONAL([HAVE_SYSTEMD_USER_SESSION], [test "x${have_systemd_user_session}" = "xyes"])
AC_SUBST([USER_SESSION_DIR])

execinfo_libs=""
AC_CHECK_HEADERS([execinfo.h], [have_execinfo="yes"], [have_execinfo="no"])
if test "x${have_execinfo}" = "xyes" ; then
  AC_MSG_CHECKING([whether backtrace_symbols_fd() is in libc])

  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
      [[
#include <stdlib.h>
#include <execinfo.h>
      ]],
      [[
backtrace_symbols_fd(NULL, 0, 0);
      ]])],
    [link_execinfo="yes"],
    [link_execinfo="no"])

  AC_MSG_RESULT([${link_execinfo}])
fi

if test "x${link_execinfo}" = "xno" ; then
  AC_MSG_CHECKING([whether backtrace_symbols_fd() is in libexecinfo])

  LDFLAGS_save="${LDFLAGS}"
  LDFLAGS="${LDFLAGS} -lexecinfo"
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
      [[
#include <stdlib.h>
#include <execinfo.h>
      ]],
      [[
backtrace_symbols_fd(NULL, 0, 0);
      ]])],
    [
      link_execinfo="yes"
      execinfo_libs="-lexecinfo"
    ],
    [link_execinfo="no"])
  LDFLAGS="${LDFLAGS_save}"

  AC_MSG_RESULT([${link_execinfo}])
fi

dnl if test "x${link_execinfo}" = "xno" ; then
dnl   AC_MSG_ERROR([Cannot use backtrace_symbols_fd()])
dnl fi


#ALL_LINGUAS="bg de eo es fi fr ja pl pt ru zh_CN hu sl it cs da sk sv nb nl zh_TW ko"
#ALL_LINGUAS="bg de eo es fi fr ja pt ru zh_CN hu sl it cs da sk sv nb nl zh_TW"
# a lot of .po's are broken
ALL_LINGUAS="bg ca cs da de el eo es fi fr fr_CH he hu it ja ko nb nl pl pt_BR ru sk sl sv tr zh_CN zh_TW"
AC_SUBST([ALL_LINGUAS])

m4_ifdef([AM_GNU_GETTEXT_VERSION], [
AM_GNU_GETTEXT_VERSION([0.18])
])

m4_ifdef([AC_GNU_GETTEXT], [
AC_GNU_GETTEXT([external], [need-ngettext])
po_makefile_in=po/Makefile.in
have_po="yes"
],[
m4_ifdef([AM_GNU_GETTEXT], [
AM_GNU_GETTEXT([external], [need-ngettext])
po_makefile_in=po/Makefile.in
have_po="yes"
],[
have_po="no"
])
])
AC_SUBST([LTLIBINTL])
if test "x$LIBINTL" = "x"; then
  LIBINTL="$INTLLIBS"
fi

if test "x${POSUB}" = "x" ; then
   have_po="no"
fi

AM_CONDITIONAL([HAVE_PO], [test "x${have_po}" = "xyes"])

AC_CHECK_LIB(dl, dlopen, dlopen_libs=-ldl)
AC_SUBST([dlopen_libs])

AC_MSG_CHECKING([whether to install sysactions.conf])
AC_ARG_ENABLE([install-sysactions],
  AS_HELP_STRING([--disable-install-sysactions],[disable installing /etc/enlightenment/sysactions.conf @<:@default=enabled@:>@]),
    [e_cv_enable_install_sysactions=$enableval],
    AC_CACHE_VAL([e_cv_enable_install_sysactions], [e_cv_enable_install_sysactions=yes]))
AC_MSG_RESULT([$e_cv_enable_install_sysactions])
AM_CONDITIONAL(INSTALL_SYSACTIONS, test x${e_cv_enable_install_sysactions} = xyes)

AC_MSG_CHECKING([whether to install enlightenment.menu])
AC_ARG_ENABLE([install-enlightenment-menu],
  AS_HELP_STRING([--disable-install-enlightenment-menu],[disable installing /etc/xdg/menus/e-applications.menu @<:@default=enabled@:>@]),
    [e_cv_enable_install_enlightenment_menu=$enableval],
    AC_CACHE_VAL([e_cv_enable_install_enlightenment_menu], [e_cv_enable_install_enlightenment_menu=yes]))
AC_MSG_RESULT([$e_cv_enable_install_enlightenment_menu])
AM_CONDITIONAL(INSTALL_ENLIGHTENMENT_MENU, test x${e_cv_enable_install_enlightenment_menu} = xyes)

AC_MSG_CHECKING([whether to enable Files menu item])
AC_ARG_ENABLE([files],
  AS_HELP_STRING([--disable-files],[disable Files menu item @<:@default=enabled@:>@]),
    [e_cv_enable_files=$enableval],
    AC_CACHE_VAL([e_cv_enable_files], [e_cv_enable_files=yes]))
if test "[${e_cv_enable_files}]" = yes; then
  AC_DEFINE_UNQUOTED([ENABLE_FILES],[1],[enable Files menu item])
fi
AC_MSG_RESULT([$e_cv_enable_files])

AC_ARG_ENABLE([device-udev],
  AS_HELP_STRING([--enable-device-udev],[enable udev device backend @<:@default=enabled@:>@]),
  [e_cv_want_device_udev=$enableval],
  [e_cv_want_device_udev=yes])

AC_MSG_CHECKING([whether the UDEV device backend is enabled])
AC_MSG_RESULT([${e_cv_want_device_udev}])

device_backend=
device_libs=

if test "x$e_cv_want_device_udev" != "xno";then
  PKG_CHECK_MODULES([EEZE], [eeze >= ${efl_version} ecore >= ${efl_version} eina >= ${efl_version}],
    [
      device_backend=eeze
      device_libs=eeze
      AC_DEFINE_UNQUOTED([HAVE_EEZE],[1],[enable udev support])
    ])
fi

AC_MSG_CHECKING([which device backend to use])
AC_MSG_RESULT($device_backend)

AM_CONDITIONAL([HAVE_EEZE], [test "x${device_backend}" = "xeeze"])

DBUS_MOUNT_CONFIG=1
dbus_mount="dbus-1"

AC_ARG_ENABLE([mount-udisks],
  AS_HELP_STRING([--enable-mount-udisks],[enable udisks mounting support @<:@default=enabled@:>@]),
  [e_cv_want_mount_udisks=$enableval],
  [e_cv_want_mount_udisks=yes])

AC_MSG_CHECKING([whether the UDISKS mounting support is enabled])
AC_MSG_RESULT([${e_cv_want_mount_udisks}])


if test "x${e_cv_want_mount_udisks}" = "xno" ; then
  DBUS_MOUNT_CONFIG=0
  dbus_mount=
fi

AC_ARG_ENABLE([mount-eeze],
  AS_HELP_STRING([--enable-mount-eeze],[enable eeze mounting support @<:@default=disabled@:>@]),
  [e_cv_want_mount_eeze=$enableval],
  [e_cv_want_mount_eeze=no])

AC_MSG_CHECKING([whether the Eeze mounting support is enabled])
AC_MSG_RESULT([${e_cv_want_mount_eeze}])

eeze_mount=
EEZE_MOUNT_CONFIG=0
if test "x$e_cv_want_mount_eeze" != "xno" && test -z "$dbus_mount" ; then
      CPPFLAGS_save="$CPPFLAGS"
      LIBS_save="$LIBS"
      CPPFLAGS="$EEZE_CFLAGS $CPPFLAGS"
      LIBS="$EEZE_LIBS $LIBS"
      AC_CHECK_LIB([eeze], [eeze_disk_function],
        [
           eeze_mount="eeze >= $efl_version ecore-con >= $efl_version"
           EEZE_MOUNT_CONFIG=1
           AC_DEFINE_UNQUOTED([HAVE_EEZE_MOUNT], [1], [enable eeze mounting])
           AC_MSG_NOTICE([eeze mounting enabled])
        ],
        [
           AC_MSG_NOTICE([eeze mounting disabled])
           e_cv_want_mount_eeze=no
        ]
      )
      CPPFLAGS="$CPPFLAGS_save"
      LIBS="$LIBS_save"
else
  AC_MSG_NOTICE([eeze mounting disabled])
fi

AC_SUBST([EEZE_MOUNT_CONFIG])
AC_SUBST([DBUS_MOUNT_CONFIG])
AM_CONDITIONAL([HAVE_EEZE_MOUNT], [false])
test -n "$dbus_mount" && AC_DEFINE_UNQUOTED([HAVE_UDISKS_MOUNT], [1], [enable Udisks mounting])
AM_CONDITIONAL([HAVE_UDISKS_MOUNT], [test -n "$dbus_mount"])
AM_CONDITIONAL([HAVE_EEZE_MOUNT], [test -n "$eeze_mount"])

# verify "wayland-only" early to adapt dependencies

have_wayland_only=no
AC_ARG_ENABLE([wayland],
  AS_HELP_STRING([--enable-wayland],[enable wayland support in enlightenment @<:@default=disabled@:>@]),
  [have_wayland_only=$enableval],
  [have_wayland_only=no])
AC_MSG_CHECKING([whether wayland support is enabled])
AC_MSG_RESULT([${have_wayland_only}])

# doxygen program for documentation building

EFL_CHECK_DOXYGEN([build_doc="yes"], [build_doc="no"])

# explicit libs for each binary to reduce linkage requirements
PKG_CHECK_MODULES(E_REMOTE, [
  ecore >= ${efl_version}
  ecore-ipc >= ${efl_version}
  eet >= ${efl_version}
  eina >= ${efl_version}
])

PKG_CHECK_MODULES(E_START, [
  eina >= ${efl_version}
  evas >= ${efl_version}
])

PKG_CHECK_MODULES(E_IMC, [
  ecore-file >= ${efl_version}
  eet >= ${efl_version}
  eina >= ${efl_version}
])

PKG_CHECK_MODULES(E_THUMB, [
  eina >= ${efl_version}
  evas >= ${efl_version}
  ecore >= ${efl_version}
  ecore-evas >= ${efl_version}
  edje >= ${efl_version}
  ecore-file >= ${efl_version}
  ecore-ipc >= ${efl_version}
  eet >= ${efl_version}
  efreet >= ${efl_version}
])

PKG_CHECK_MODULES(E_GRABBER, [
  eina >= ${efl_version}
  eet >= ${efl_version}
])

efm_requires="\
ecore >= $efl_version \
ecore-file >= $efl_version \
ecore-ipc >= $efl_version \
eet >= $efl_version \
efreet >= $efl_version \
eldbus >= $efl_version \
eina >= $efl_version"
efm_requires="$efm_requires $eeze_mount $dbus_mount $udisks_mount"

PKG_CHECK_MODULES(E_FM, [$efm_requires])

PKG_CHECK_MODULES(E_FM_CMDLINE, [
  eina >= ${efl_version}
  ecore >= ${efl_version}
  ecore-file >= ${efl_version}
  eldbus >= ${efl_version}
  efreet >= ${efl_version}
])

PKG_CHECK_MODULES(E_FM_OP, [
  ecore >= ${efl_version}
  ecore-file >= ${efl_version}
  eina >= ${efl_version}
])

PKG_CHECK_MODULES(E_FM_OPEN, [
  eina >= ${efl_version}
  eet >= ${efl_version}
  ecore >= ${efl_version}
  ecore-ipc >= ${efl_version}
  ecore-con >= ${efl_version}
])

PKG_CHECK_MODULES(E_SYS, [
  eina >= ${efl_version}
  ecore >= ${efl_version}
])

e_requires="\
  evas >= $efl_version \
  ecore >= $efl_version \
  ecore-evas >= $efl_version \
  ecore-input >= $efl_version \
  ecore-input-evas >= $efl_version \
  ecore-con >= $efl_version \
  ecore-ipc >= $efl_version \
  ecore-file >= $efl_version \
  eet >= $efl_version \
  edje >= $efl_version \
  efreet >= $efl_version \
  efreet-mime >= $efl_version \
  efreet-trash >= $efl_version \
  eina >= $efl_version \
  eldbus >= $efl_version \
  eio >= $efl_version \
  eo >= $efl_version \
  elementary >= $efl_version \
  emotion >= $efl_version \
  $eeze_mount \
  $udisks_mount \
  $device_libs"


PKG_CHECK_MODULES(E_OPEN, [
  eina >= ${efl_version}
  ecore >= ${efl_version}
  efreet >= ${efl_version}
  efreet-mime >= ${efl_version}
])

PKG_CHECK_EXISTS([xkeyboard-config],[
  xkb_base=`$PKG_CONFIG --variable=xkb_base xkeyboard-config 2>/dev/null`
  AC_DEFINE_UNQUOTED([XKB_BASE],["$xkb_base"],[Define to the base directory for X keyboard configuration data])
])

profile="SLOW_PC"
AC_ARG_WITH(profile,
  AS_HELP_STRING([--with-profile=TARGET_PROFILE], 
  [specify a target format profile of:
                                    LOWRES_PDA, MEDIUMRES_PDA, HIRES_PDA, SLOW_PC, MEDIUM_PC, FAST_PC]),
  [profile=$withval])
AC_MSG_NOTICE([Enlightenment build profile set to $profile])
EDJE_DEF="-DLOWRES_PDA=1 -DMEDIUMRES_PDA=2 -DHIRES_PDA=3 -DSLOW_PC=4 -DMEDIUM_PC=5 -DFAST_PC=6"
EDJE_DEF=$EDJE_DEF" -DE_PROFILE"=$profile
AC_SUBST([EDJE_DEF])

PKG_CHECK_MODULES(EET, [eet >= ${efl_version}])
EFL_WITH_BIN([edje], [edje-cc], [edje_cc])
EFL_WITH_BIN([eet], [eet-eet], [eet])
EFL_WITH_BIN([eldbus], [eldbus_codegen], [eldbus-codegen])

AC_DEFINE(E_INTERNAL, 1, "This define can be used to wrap internal E stuff, as config.h isn't exported")

AM_CONDITIONAL(HAVE_BATTERY, false)
define([CHECK_MODULE_BATTERY],
[
  AC_E_CHECK_PKG(BATTERY, [ ecore >= $efl_version ecore-file >= $efl_version ecore-con >= $efl_version eina >= $efl_version ], [], [BATTERY=false])
])

have_openbsd="no"
have_freebsd="no"
BATTERY_LDFLAGS=
case "$host_os" in
  darwin*)
    BATTERY_LDFLAGS="${cf_libs}"
    BATTERY_CFLAGS="${BATTERY_CFLAGS} ${cf_cflags}"
  ;;
  openbsd*)
    have_openbsd="yes"
  ;;
  freebsd*)
    have_freebsd="yes"
  ;;
esac

AC_SUBST([BATTERY_LDFLAGS])
AM_CONDITIONAL([HAVE_OPENBSD], [test "x${have_openbsd}" = "xyes"])
AM_CONDITIONAL([HAVE_FREEBSD], [test "x${have_freebsd}" = "xyes"])

AM_CONDITIONAL(HAVE_TEMPERATURE, false)
define([CHECK_MODULE_TEMPERATURE],
[
  AC_E_CHECK_PKG(TEMPERATURE, [ ecore >= $efl_version ecore-file >= $efl_version eina >= $efl_version ], [], [TEMPERATURE=false])
])

AM_CONDITIONAL(HAVE_MUSIC_CONTROL, false)
define([CHECK_MODULE_MUSIC_CONTROL],
[
  AC_E_CHECK_PKG(MUSIC_CONTROL, [ ecore >= $efl_version eina >= $efl_version eldbus ], [], [MUSIC_CONTROL=false])
])

##have_exchange=no
##AC_ARG_ENABLE(exchange,
##  AS_HELP_STRING([--disable-exchange], [disable Exchange support @<:@default=detect@:>@]),
##  [want_exchange=$enableval],
##  [want_exchange=auto]
##)

AM_CONDITIONAL(HAVE_EXCHANGE, false)
##define([CHECK_MODULE_CONF_THEME],
##[
##  if test "x$want_exchange" != "xno" ; then
##    AC_E_CHECK_PKG(EXCHANGE, [exchange], [], [:])
##  else
##    AC_MSG_NOTICE([requested to not use exchange library])
##  fi
##])


have_enotify=no
AM_CONDITIONAL([HAVE_ENOTIFY], [false])
AC_ARG_ENABLE([enotify],
  AS_HELP_STRING([--disable-enotify], [disable Enotify support @<:@default=detect@:>@]),
  [e_cv_want_enotify=$enableval],
  AC_CACHE_VAL([e_cv_want_enotify], [e_cv_want_enotify=yes])
)
if test "x$e_cv_want_enotify" != "xno" ; then
  AC_E_CHECK_PKG(ENOTIFY, [ eldbus >= $efl_version ],
  [
    AC_DEFINE_UNQUOTED([HAVE_ENOTIFY], [1], [enable enotify])
  ], 
  [
    AC_MSG_NOTICE([enotify disabled])
    e_cv_want_enotify=no
  ])
else
  AC_MSG_NOTICE([enotify disabled])
    e_cv_want_enotify=no
fi
AC_SUBST([ENOTIFY_CFLAGS])
AC_SUBST([ENOTIFY_LIBS])

AM_CONDITIONAL(HAVE_NOTIFICATION, false)
define([CHECK_MODULE_NOTIFICATION],
[
  if test "x$e_cv_want_enotify" = "xno" ; then
    NOTIFICATION=false
  fi
])


AM_CONDITIONAL(HAVE_ALSA, false)
AM_CONDITIONAL(HAVE_PULSE, false)
define([CHECK_MODULE_MIXER],
[
  AC_E_CHECK_PKG(ALSA, [alsa >= 1.0.8],
    [ ], [ ])
 AC_E_CHECK_PKG([PULSE], [libpulse-simple libpulse],
    [AC_DEFINE_UNQUOTED([HAVE_PULSE], [1], [have pulseaudio])], [ ])
])

SHM_OPEN_LIBS=""
AC_MSG_CHECKING([whether shm_open() is present])
LIBS_save=${LIBS}
LIBS="${LIBS} -lrt"
AC_LINK_IFELSE(
   [AC_LANG_PROGRAM(
      [[
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
      ]],
      [[
int fd;
fd = shm_open("/", O_RDWR | O_CREAT, S_IRUSR |
S_IWUSR);
shm_unlink("/");
      ]])],
   [
    have_shm_open="yes"
    AC_DEFINE(HAVE_SHM_OPEN, 1, [Have shm_open() call])
    SHM_OPEN_LIBS="-lrt"
   ])
LIBS=${LIBS_save}
AC_MSG_RESULT([${have_shm_open}])
AC_SUBST(SHM_OPEN_LIBS)
                           
AC_SUBST([SOUND_CFLAGS])
AC_SUBST([SOUND_LIBS])

AM_CONDITIONAL(HAVE_ECONNMAN, false)
AM_CONDITIONAL(HAVE_DUMMY, false)
define([CHECK_MODULE_CONNMAN],
[
  AC_E_CHECK_PKG(ECONNMAN, [ eldbus >= $efl_version ], [], [CONNMAN=false])
dnl  AC_E_CHECK_PKG(DUMMY, [ connman >= 1.0 ], [], [CONNMAN=false])
])
AC_SUBST([ECONNMAN_CFLAGS])
AC_SUBST([ECONNMAN_LIBS])

AM_CONDITIONAL(HAVE_BLUEZ4, false)
AM_CONDITIONAL(HAVE_DUMMY, false)
define([CHECK_MODULE_BLUEZ4],
[
  AC_E_CHECK_PKG(BLUEZ4, [ eldbus ],)
])
AC_SUBST([EBLUEZ4_CFLAGS])
AC_SUBST([EBLUEZ4_LIBS])

have_wayland_egl=no
AC_ARG_ENABLE([wayland-egl],
  AS_HELP_STRING([--enable-wayland-egl],[enable wayland to render using EGL @<:@default=disabled@:>@]),
  [e_cv_want_wayland_egl=$enableval],
  [e_cv_want_wayland_egl=no])
AC_MSG_CHECKING([whether wayland EGL support is enabled])
AC_MSG_RESULT([${e_cv_want_wayland_egl}])

if test "x${e_cv_want_wayland_only}" != "xno" ;then
  PKG_CHECK_MODULES([WAYLAND], [ecore-wayland >= 1.16 wayland-server >= 1.8.0 wayland-client >= 1.8.0 xkbcommon uuid],
    [
      have_wayland=yes
      AC_DEFINE_UNQUOTED([HAVE_WAYLAND],[1],[enable wayland support])
    ])
else
  have_wayland=no
fi

if test "x${have_wayland}" = "xyes"; then
  if test "x${e_cv_want_wayland_egl}" != "xno";then
    PKG_CHECK_MODULES([WAYLAND_EGL], [egl >= 7.10],
      [
        have_wayland_egl=yes
        AC_DEFINE_UNQUOTED([HAVE_WAYLAND_EGL],[1],[enable wayland client EGL support])
      ],
      [have_wayland_egl=no])
  else
    have_wayland_egl=no
  fi
fi

AM_CONDITIONAL([HAVE_WAYLAND], [test "x${have_wayland}" = "xyes"])
AM_CONDITIONAL([HAVE_WAYLAND_EGL], [test "x${have_wayland_egl}" = "xyes"])

if test "x${have_wayland_only}" != "xyes"; then
  PKG_CHECK_MODULES([ECORE_X], [ecore-x >= ${efl_version}])
  if test -n "$ECORE_X_CFLAGS" ; then
     ecore_x=true
  else
     ecore_x=false
  fi
else
  ecore_x=false
fi

WL_X11=false
define([CHECK_MODULE_WL_X11],
[
  if test "x${have_wayland}" = "xyes"; then
    AC_E_CHECK_PKG(WL_X11, [ ecore >= ${efl_version} ecore-x >= ${efl_version} ecore-evas >= ${efl_version} ], [WL_X11=true], [WL_X11=false])
  else
    WL_X11=false
  fi
])
AM_CONDITIONAL([HAVE_WL_X11], [test "x${WL_X11}" = "xtrue"])

WL_WL=false
define([CHECK_MODULE_WL_WL],
[
  if test "x${have_wayland}" = "xyes"; then
    WL_WL=true
  else
    WL_WL=false
  fi
])
AM_CONDITIONAL([HAVE_WL_WL], [test "x${WL_WL}" = "xtrue"])

WL_DESKTOP_SHELL=false
define([CHECK_MODULE_WL_DESKTOP_SHELL],
[
  if test "x${have_wayland}" = "xyes"; then
    AC_E_CHECK_PKG(WL_DESKTOP_SHELL, [ ecore >= $efl_version eina >= $efl_version ], [WL_DESKTOP_SHELL=true], [WL_DESKTOP_SHELL=false])
  else
    WL_DESKTOP_SHELL=false
  fi
])
AM_CONDITIONAL([HAVE_WL_DESKTOP_SHELL], [test "x${WL_DESKTOP_SHELL}" = "xtrue"])

WL_FB=false
define([CHECK_MODULE_WL_FB],
[
  if test "x${have_wayland}" = "xyes"; then
    AC_E_CHECK_PKG(WL_FB, [ ecore-fb >= $efl_version ecore >= $efl_version eina >= $efl_version ], [WL_FB=true], [WL_FB=false])
  else
    WL_FB=false
  fi
])
AM_CONDITIONAL([HAVE_WL_FB], [test "x${WL_FB}" = "xtrue"])

WL_EGLFS=false
define([CHECK_MODULE_WL_EGLFS],
[
  if test "x${have_wayland}" = "xyes" ; then
    AC_E_CHECK_PKG(WL_EGLFS, [ ecore-fb >= $efl_version ecore >= $efl_version eina >= $efl_version ], [WL_EGLFS=true], [WL_EGLFS=false])
  else
    WL_EGLFS=false
  fi
])
AM_CONDITIONAL([HAVE_WL_EGLFS], [test "x${WL_EGLFS}" = "xtrue"])

WL_DRM=false
define([CHECK_MODULE_WL_DRM],
[
  if test "x${have_wayland}" = "xyes" ; then
    AC_E_CHECK_PKG(WL_DRM, [ ecore-drm >= $efl_version ecore >= $efl_version eina >= $efl_version ], [WL_DRM=true], [WL_DRM=false])
  else
    WL_DRM=false
  fi
])
AM_CONDITIONAL([HAVE_WL_DRM], [test "x${WL_DRM}" = "xtrue"])

HAVE_XWAYLAND_DEPS=false
define([CHECK_MODULE_XWAYLAND],
[
  if test "x${have_wayland}" = "xyes"; then
    AC_E_CHECK_PKG(XWAYLAND, [ ecore >= ${efl_version} ecore-x >= ${efl_version} ecore-audio >= ${efl_version}  ], [HAVE_XWAYLAND_DEPS=true], [HAVE_XWAYLAND_DEPS=false])
    AC_PATH_PROG([XWAYLAND_BIN], [Xwayland], [false])
  else
    HAVE_XWAYLAND_DEPS=false
  fi
])
AM_CONDITIONAL([HAVE_XWAYLAND], [test "x${HAVE_XWAYLAND}" != "xno"])

define([CHECK_MODULE_WL_TEXT_INPUT],
[
  if test "x${have_wayland}" = "xyes"; then
    AC_E_CHECK_PKG(WL_TEXT_INPUT, [ ecore >= $efl_version eina >= $efl_version ], [WL_TEXT_INPUT=true], [WL_TEXT_INPUT=false])
  else
    WL_TEXT_INPUT=false
  fi
])
AM_CONDITIONAL([HAVE_WL_TEXT_INPUT], [test "x${WL_TEXT_INPUT}" = "xtrue"])

WL_WEEKEYBOARD=false
define([CHECK_MODULE_WL_WEEKEYBOARD],
[
   if test "x${have_wayland}" = "xyes" ; then
      AC_E_CHECK_PKG(WL_WEEKEYBOARD, [ eina >= 1.8.0 evas >= 1.8.0 ecore >= 1.8.0 ecore-evas >= 1.8.0 ecore-wayland >= 1.8.0 edje >= 1.8.0 ], [WL_WEEKEYBOARD=true], [WL_WEEKEYBOARD=false])
   else
      WL_WEEKEYBOARD=false
   fi
])
AM_CONDITIONAL([HAVE_WL_WEEKEYBOARD], [test "x${WL_WEEKEYBOARD}" = "xtrue"])

AC_E_OPTIONAL_MODULE([ibar], true)
AC_E_OPTIONAL_MODULE([clock], true)
AC_E_OPTIONAL_MODULE([pager], true)
AC_E_OPTIONAL_MODULE([pager_plain], true)
AC_E_OPTIONAL_MODULE([battery], true, [CHECK_MODULE_BATTERY])
AC_E_OPTIONAL_MODULE([temperature], true, [CHECK_MODULE_TEMPERATURE])
AC_E_OPTIONAL_MODULE([notification], true, [CHECK_MODULE_NOTIFICATION])
AC_E_OPTIONAL_MODULE([cpufreq], true)
AC_E_OPTIONAL_MODULE([ibox], true)
AC_E_OPTIONAL_MODULE([start], true)
AC_E_OPTIONAL_MODULE([winlist], true)
AC_E_OPTIONAL_MODULE([fileman], true)
AC_E_OPTIONAL_MODULE([fileman_opinfo], true)
AC_E_OPTIONAL_MODULE([wizard], true)
AC_E_OPTIONAL_MODULE([conf], true)
AC_E_OPTIONAL_MODULE([conf_theme], true, true)
AC_E_OPTIONAL_MODULE([conf_intl], true)
AC_E_OPTIONAL_MODULE([msgbus], true)
AC_E_OPTIONAL_MODULE([conf_applications], true)
AC_E_OPTIONAL_MODULE([conf_display], true)
AC_E_OPTIONAL_MODULE([conf_shelves], true)
AC_E_OPTIONAL_MODULE([conf_bindings], true)
AC_E_OPTIONAL_MODULE([conf_window_remembers], true)
AC_E_OPTIONAL_MODULE([conf_window_manipulation], true)
AC_E_OPTIONAL_MODULE([conf_menus], true)
AC_E_OPTIONAL_MODULE([conf_dialogs], true)
AC_E_OPTIONAL_MODULE([conf_performance], true)
AC_E_OPTIONAL_MODULE([conf_paths], true)
AC_E_OPTIONAL_MODULE([conf_interaction], true)
AC_E_OPTIONAL_MODULE([gadman], true)
AC_E_OPTIONAL_MODULE([mixer], true, [CHECK_MODULE_MIXER])
AC_E_OPTIONAL_MODULE([connman], true, [CHECK_MODULE_CONNMAN])
AC_E_OPTIONAL_MODULE([bluez4], true, [CHECK_MODULE_BLUEZ4])
AC_E_OPTIONAL_MODULE([syscon], true)
AC_E_OPTIONAL_MODULE([everything], true)
AC_E_OPTIONAL_MODULE([systray], true)
AC_E_OPTIONAL_MODULE([appmenu], true)
AC_E_OPTIONAL_MODULE([quickaccess], true)
AC_E_OPTIONAL_MODULE([teamwork], true)
AC_E_OPTIONAL_MODULE([lokker], true)
AC_E_OPTIONAL_MODULE([shot], true, $ecore_x)
AC_E_OPTIONAL_MODULE([backlight], true)
AC_E_OPTIONAL_MODULE([tasks], true)
AC_E_OPTIONAL_MODULE([conf_randr], true)
AC_E_OPTIONAL_MODULE([xkbswitch], true, $ecore_x)
AC_E_OPTIONAL_MODULE([tiling], true)
AC_E_OPTIONAL_MODULE([music_control], true, [CHECK_MODULE_MUSIC_CONTROL])
AC_E_OPTIONAL_MODULE([packagekit], true)
AC_E_OPTIONAL_MODULE([wl_desktop_shell], $have_wayland, [CHECK_MODULE_WL_DESKTOP_SHELL])
AC_E_OPTIONAL_MODULE([wl_x11], $have_wayland, [CHECK_MODULE_WL_X11])
AC_E_OPTIONAL_MODULE([wl_wl], $have_wayland, [CHECK_MODULE_WL_WL])
#AC_E_OPTIONAL_MODULE([wl_fb], $have_wayland, [CHECK_MODULE_WL_FB])
AC_E_OPTIONAL_MODULE([wl_eglfs], $have_wayland, [CHECK_MODULE_WL_EGLFS])
AC_E_OPTIONAL_MODULE([wl_drm], $have_wayland, [CHECK_MODULE_WL_DRM])
AC_E_OPTIONAL_MODULE([wl_text_input], $have_wayland, [CHECK_MODULE_WL_TEXT_INPUT])
AC_E_OPTIONAL_MODULE([wl_weekeyboard], $have_wayland, [CHECK_MODULE_WL_WEEKEYBOARD])
AC_E_OPTIONAL_MODULE([policy_mobile], true)
AC_E_OPTIONAL_MODULE([geolocation], true)
AC_E_OPTIONAL_MODULE([xwayland], $have_wayland, [CHECK_MODULE_XWAYLAND])

if test "x${HAVE_WL_X11}" != "xyes" && test "x${have_wayland}" = "xyes" && test "x${HAVE_XWAYLAND}" != "xyes"; then
  AC_DEFINE_UNQUOTED([HAVE_WAYLAND_ONLY],[1],[enable wayland-only version of enlightenment])
  have_wayland_only=yes
else
  have_wayland_only=no
fi

AM_CONDITIONAL([HAVE_WAYLAND_ONLY], [test "x${have_wayland_only}" = "xyes"])

if test "x${have_wayland_only}" != "xyes"; then
  PKG_CHECK_MODULES(E_ALERT, [
    xcb
    xcb-shape
    xcb-keysyms
    eina >= ${efl_version}
    ecore >= ${efl_version}
    ecore-ipc >= ${efl_version}
  ])
fi

if test "x${have_wayland_only}" != "xyes"; then
  e_requires="$e_requires ecore-x >= $efl_version"
fi

PKG_CHECK_MODULES(E, [$e_requires])
requirements_e="\
evas >= ${efl_version} \
ecore >= ${efl_version} \
ecore-x >= ${efl_version} \
ecore-evas >= ${efl_version} \
ecore-con >= ${efl_version} \
ecore-ipc >= ${efl_version} \
ecore-file >= ${efl_version} \
eet >= ${efl_version} \
edje >= ${efl_version} \
efreet >= ${efl_version} \
efreet-mime >= ${efl_version} \
efreet-trash >= ${efl_version} \
eina >= ${efl_version} \
eldbus >= ${efl_version} \
eio >= ${efl_version} \
eo >= ${efl_version} \
elementary >= ${efl_version} \
emotion >= ${efl_version} \
$udisks_mount \
$eeze_mount \
$device_libs"
if test "x${have_wayland_only}" != "xyes"; then
  requirements_e="$requirements_e ecore-x >= $efl_version"
fi

e_libs="$E_LIBS $LIBINTL $fnmatch_libs $execinfo_libs"
e_cflags="-DUSE_E_CONFIG_H $E_CFLAGS"
e_configflags="-DUSE_E_CONFIG_H"

AC_SUBST([e_libs])
AC_SUBST([e_cflags])
AC_SUBST([e_configflags])
AC_SUBST([requirements_e])

HALT="/sbin/shutdown -h now"
REBOOT="/sbin/shutdown -r now"
SUSPEND=""
HIBERNATE=""
MOUNT="/bin/mount"
UMOUNT="/bin/umount"
EJECT="/usr/bin/eject"
case "$host_os" in
   freebsd*|pcbsd*)
      #SUSPEND="acpiconf -s3"
      SUSPEND="/usr/sbin/zzz"
      AC_MSG_NOTICE([use suspend method: $SUSPEND])
      HIBERNATE="acpiconf -s4"
      AC_MSG_NOTICE([use hibernate method: $HIBERNATE])
      HALT="/sbin/shutdown -p now"
      MOUNT="/sbin/mount"
      UMOUNT="/sbin/umount"
      EJECT="/usr/sbin/cdcontrol eject"
      ;;
   *)
      if test "x${have_systemd_user_session}" = "xyes"; then
         HALT="/usr/bin/systemctl poweroff"
         REBOOT="/usr/bin/systemctl reboot"
         SUSPEND="/usr/bin/systemctl suspend"
         HIBERNATE="/usr/bin/systemctl hibernate"
         AC_MSG_NOTICE([use systemctl for halt, reboot, suspend and hibernate])
      else
         AC_PATH_PROGS([SUSPEND], [sleep.sh pm-suspend],[/etc/acpi/sleep.sh], [/etc/acpi:/sbin:/usr/sbin:$PATH])
         if test "$SUSPEND" = "/etc/acpi/sleep.sh" ; then
            SUSPEND="/etc/acpi/sleep.sh force"
            AC_MSG_NOTICE([use suspend method: $SUSPEND])
         fi

         AC_PATH_PROGS([HIBERNATE], [hibernate.sh pm-hibernate],[/etc/acpi/hibernate.sh], [/etc/acpi:/sbin:/usr/sbin:$PATH])
         if test "$HIBERNATE" = "/etc/acpi/hibernate.sh" ; then
            HIBERNATE="/etc/acpi/hibernate.sh force"
            AC_MSG_NOTICE([use hibernate method: $HIBERNATE])
         fi
      fi
      ;;
esac
AC_SUBST([HALT])
AC_SUBST([REBOOT])
AC_SUBST([SUSPEND])
AC_SUBST([HIBERNATE])
AC_SUBST([MOUNT])
AC_SUBST([UMOUNT])
AC_SUBST([EJECT])

m4_ifndef([v_rel],
   [
    EFL_COMPILER_FLAG([-Wshadow])
    EFL_COMPILER_FLAG([-Wall])
    EFL_COMPILER_FLAG([-Wpointer-arith])
    EFL_COMPILER_FLAG([-W])
    EFL_COMPILER_FLAG([-Wno-missing-field-initializers])
    EFL_COMPILER_FLAG([-Wfloat-equal])
    EFL_COMPILER_FLAG([-Wuninitialized])
    EFL_COMPILER_FLAG([-Wundef])
    EFL_COMPILER_FLAG([-Wcast-align])
    EFL_COMPILER_FLAG([-Wformat=2])
   ])

SUID_CFLAGS=-fPIE
SUID_LDFLAGS=-pie
case "$host_os" in
   freebsd*|pcbsd*)
      SUID_CFLAGS=
      SUID_LDFLAGS=
      ;;
esac
AC_SUBST([SUID_CFLAGS])
AC_SUBST([SUID_LDFLAGS])

AC_OUTPUT([
Makefile
enlightenment.spec
enlightenment.pc
src/bin/e_fm_shared_types.h
src/modules/ibar/module.desktop
src/modules/clock/module.desktop
src/modules/pager/module.desktop
src/modules/pager_plain/module.desktop
src/modules/battery/module.desktop
src/modules/temperature/module.desktop
src/modules/notification/module.desktop
src/modules/cpufreq/module.desktop
src/modules/ibox/module.desktop
src/modules/start/module.desktop
src/modules/winlist/module.desktop
src/modules/fileman/module.desktop
src/modules/fileman_opinfo/module.desktop
src/modules/conf/module.desktop
src/modules/conf_theme/module.desktop
src/modules/conf_intl/module.desktop
src/modules/conf_applications/module.desktop
src/modules/conf_display/module.desktop
src/modules/conf_shelves/module.desktop
src/modules/conf_bindings/module.desktop
src/modules/conf_window_remembers/module.desktop
src/modules/conf_window_manipulation/module.desktop
src/modules/conf_menus/module.desktop
src/modules/conf_dialogs/module.desktop
src/modules/conf_performance/module.desktop
src/modules/conf_paths/module.desktop
src/modules/conf_interaction/module.desktop
src/modules/msgbus/module.desktop
src/modules/conf_randr/module.desktop
src/modules/gadman/module.desktop
src/modules/mixer/module.desktop
src/modules/connman/module.desktop
src/modules/bluez4/module.desktop
src/modules/syscon/module.desktop
src/modules/everything/module.desktop
src/modules/everything/everything.pc
src/modules/systray/module.desktop
src/modules/appmenu/module.desktop
src/modules/quickaccess/module.desktop
src/modules/teamwork/module.desktop
src/modules/shot/module.desktop
src/modules/backlight/module.desktop
src/modules/geolocation/module.desktop
src/modules/tasks/module.desktop
src/modules/xkbswitch/module.desktop
src/modules/tiling/module.desktop
src/modules/music-control/module.desktop
src/modules/packagekit/module.desktop
src/modules/wl_desktop_shell/module.desktop
data/xsession/enlightenment.desktop
data/etc/sysactions.conf
data/units/enlightenment.service
doc/Doxyfile
doc/e.dox
$po_makefile_in
])

# report
txt_strip() {
  echo "[$]@" | sed -e 's/^[[ \t]]*\([[^ \t]]*\)[[ \t]]*$/\1/g'
}

echo
cat << SUMMARY_EOF
Summary:
 * project.........: $PACKAGE $VERSION
 * prefix..........: $(txt_strip $prefix)
 * CFLAGS..........: $(txt_strip $CFLAGS)
 * LDFLAGS.........: $(txt_strip $LDFLAGS)
 * edje_cc.........: ${edje_cc}
 * eet.............: ${eet_eet}
 * eldbus-codegen..: ${eldbus_codegen}
SUMMARY_EOF

if test "$have_systemd_user_session" = "yes"; then
  echo " * systemd.........: yes (${USER_SESSION_DIR})"
else
  echo " * systemd.........: no"
fi

echo
cat << ACTIONS_EOF
System Actions:
 * Halt.....: ${HALT}
 * Reboot...: ${REBOOT}
 * Suspend..: ${SUSPEND}
 * Hibernate: ${HIBERNATE}

ACTIONS_EOF

cat << DEVICE_EOF
Preferred Backends:
 * device..............: $device_backend
 * udisks_mount........: $e_cv_want_mount_udisks
 * eeze_mount..........: $e_cv_want_mount_eeze
DEVICE_EOF
echo

UNUSED_MODS=""
for mod in $UNUSED_OPTIONAL_MODULES; do
  UNUSED_MODS="$UNUSED_MODS${COLOR_HRED}$mod${COLOR_END} "
done

cat << MODULES_EOF
Modules:
 * disabled........: $(txt_strip $UNUSED_MODS)
MODULES_EOF
echo

cat << DOCS_EOF
Documentation:
 * enabled.........: ${build_doc}
DOCS_EOF
if test "x${build_doc}" = "xyes" ; then
  echo " Building..........: make doc"
fi
echo

cat << COMPILE_EOF
Compilation........: make (or gmake)
COMPILE_EOF
echo

cat << INSTALL_EOF
Installation.......: make all install (as root if needed, with 'su' or 'sudo')
INSTALL_EOF
echo
