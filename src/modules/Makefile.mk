MDIR = $(libdir)/enlightenment/modules
MOD_LDFLAGS = -module -avoid-version
MOD_CPPFLAGS = -I. \
-I$(top_srcdir) \
-I$(top_srcdir)/src/bin \
-I$(top_builddir)/src/bin \
-I$(top_srcdir)/src/modules \
@e_cflags@ \
@WAYLAND_CFLAGS@ \
-DE_BINDIR=\"$(bindir)\"

MOD_LIBS = @e_libs@ @dlopen_libs@

include src/modules/Makefile_connman.mk

include src/modules/Makefile_bluez4.mk

include src/modules/Makefile_ibar.mk

include src/modules/Makefile_clock.mk

include src/modules/Makefile_pager.mk

include src/modules/Makefile_pager_plain.mk

include src/modules/Makefile_battery.mk

include src/modules/Makefile_temperature.mk

include src/modules/Makefile_notification.mk

include src/modules/Makefile_cpufreq.mk

include src/modules/Makefile_ibox.mk

include src/modules/Makefile_start.mk

include src/modules/Makefile_winlist.mk

include src/modules/Makefile_fileman.mk

include src/modules/Makefile_fileman_opinfo.mk

include src/modules/Makefile_wizard.mk

include src/modules/Makefile_conf.mk

include src/modules/Makefile_conf_theme.mk

include src/modules/Makefile_conf_intl.mk

include src/modules/Makefile_msgbus.mk

include src/modules/Makefile_conf_applications.mk

include src/modules/Makefile_conf_display.mk

include src/modules/Makefile_conf_shelves.mk

include src/modules/Makefile_conf_bindings.mk

include src/modules/Makefile_conf_window_remembers.mk

include src/modules/Makefile_conf_window_manipulation.mk

include src/modules/Makefile_conf_menus.mk

include src/modules/Makefile_conf_dialogs.mk

include src/modules/Makefile_conf_performance.mk

include src/modules/Makefile_conf_paths.mk

include src/modules/Makefile_conf_interaction.mk

include src/modules/Makefile_conf_randr.mk

include src/modules/Makefile_gadman.mk

include src/modules/Makefile_mixer.mk

#include src/modules/Makefile_illume2.mk
#include src/modules/Makefile_illume-home.mk
#include src/modules/Makefile_illume-home-toggle.mk
#include src/modules/Makefile_illume-softkey.mk
#include src/modules/Makefile_illume-keyboard.mk
#include src/modules/Makefile_illume-indicator.mk
#include src/modules/Makefile_illume-kbd-toggle.mk
#include src/modules/Makefile_illume-mode-toggle.mk
#include src/modules/Makefile_illume-bluetooth.mk

include src/modules/Makefile_syscon.mk

include src/modules/Makefile_everything.mk

include src/modules/Makefile_systray.mk

include src/modules/Makefile_appmenu.mk

include src/modules/Makefile_quickaccess.mk

include src/modules/Makefile_teamwork.mk

include src/modules/Makefile_lokker.mk

include src/modules/Makefile_shot.mk

include src/modules/Makefile_backlight.mk

include src/modules/Makefile_tasks.mk

include src/modules/Makefile_xkbswitch.mk

include src/modules/Makefile_tiling.mk

#include src/modules/Makefile_access.mk

include src/modules/Makefile_music_control.mk

include src/modules/Makefile_packagekit.mk

include src/modules/Makefile_wl_drm.mk

include src/modules/Makefile_wl_desktop_shell.mk

include src/modules/Makefile_wl_x11.mk

include src/modules/Makefile_wl_fb.mk

#if HAVE_WAYLAND_SCREENSHOT
#include src/modules/Makefile_wl_screenshot.mk

include src/modules/Makefile_policy_mobile.mk
