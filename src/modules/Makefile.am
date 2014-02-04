MAINTAINERCLEANFILES = Makefile.in
AUTOMAKE_OPTIONS = subdir-objects

SUBDIRS =
CLEANFILES =
EXTRA_DIST =
INSTALL_DATA_HOOKS =

MDIR = $(libdir)/enlightenment/modules
AM_LDFLAGS = -module -avoid-version
AM_CPPFLAGS = -I. \
	      -I$(top_srcdir) \
	      -I$(top_srcdir)/src/bin \
	      -I$(top_builddir)/src/bin \
	      -I$(top_srcdir)/src/modules \
              -DE_BINDIR=\"$(bindir)\"
AM_CFLAGS = @e_cflags@
LIBS += @e_libs@ @dlopen_libs@

EDJE_CC = $(EDJE_CC_$(V))
EDJE_CC_ = $(EDJE_CC_$(AM_DEFAULT_VERBOSITY))
EDJE_CC_0 = @echo "  EDJE_CC " $@; @edje_cc@
EDJE_CC_1 = @edje_cc@

EDJE_FLAGS_VERBOSE_ = $(EDJE_FLAGS_VERBOSE_$(AM_DEFAULT_VERBOSITY))
EDJE_FLAGS_VERBOSE_0 =
EDJE_FLAGS_VERBOSE_1 = -v
EDJE_FLAGS = $(EDJE_FLAGS_VERBOSE_$(V)) @EDJE_DEF@
PHONIES =
include Makefile_connman.am

include Makefile_bluez4.am

include Makefile_ibar.am

include Makefile_clock.am

include Makefile_pager.am

include Makefile_battery.am

include Makefile_temperature.am

include Makefile_notification.am

include Makefile_cpufreq.am

include Makefile_ibox.am

include Makefile_start.am

include Makefile_winlist.am

include Makefile_fileman.am

include Makefile_fileman_opinfo.am

include Makefile_wizard.am

include Makefile_conf.am

include Makefile_conf_comp.am

include Makefile_conf_wallpaper2.am

include Makefile_conf_theme.am

include Makefile_conf_intl.am

include Makefile_msgbus.am

include Makefile_conf_applications.am

include Makefile_conf_display.am

include Makefile_conf_shelves.am

include Makefile_conf_bindings.am

include Makefile_conf_window_remembers.am

include Makefile_conf_window_manipulation.am

include Makefile_conf_menus.am

include Makefile_conf_dialogs.am

include Makefile_conf_performance.am

include Makefile_conf_paths.am

include Makefile_conf_interaction.am

include Makefile_conf_randr.am

include Makefile_gadman.am

include Makefile_mixer.am

include Makefile_illume2.am
include Makefile_illume-home.am
include Makefile_illume-home-toggle.am
include Makefile_illume-softkey.am
include Makefile_illume-keyboard.am
include Makefile_illume-indicator.am
include Makefile_illume-kbd-toggle.am
include Makefile_illume-mode-toggle.am
include Makefile_illume-bluetooth.am

include Makefile_syscon.am

include Makefile_everything.am

include Makefile_systray.am

include Makefile_appmenu.am

include Makefile_quickaccess.am

include Makefile_teamwork.am

include Makefile_shot.am

include Makefile_backlight.am

include Makefile_tasks.am

include Makefile_xkbswitch.am

include Makefile_tiling.am

include Makefile_access.am

include Makefile_music_control.am

include Makefile_contact.am

#if HAVE_WAYLAND_DRM
#include Makefile_wl_drm.am

include Makefile_wl_desktop_shell.am

include Makefile_wl_screenshot.am

install-data-hook: $(INSTALL_DATA_HOOKS)
.PHONY: $(PHONIES)
