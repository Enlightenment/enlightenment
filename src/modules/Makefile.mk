MDIR = $(libdir)/enlightenment/modules
MOD_LDFLAGS = -module -avoid-version
MOD_CPPFLAGS = -I. \
-I$(top_srcdir) \
-I$(top_srcdir)/src/bin \
-I$(top_builddir)/src/bin \
-I$(top_srcdir)/src/modules \
@e_cflags@ \
-DE_BINDIR=\"$(bindir)\"

MOD_LIBS = @e_libs@ @dlopen_libs@

if USE_MODULE_CONNMAN
include src/modules/Makefile_connman.mk
endif

if USE_MODULE_BLUEZ4
include src/modules/Makefile_bluez4.mk
endif

if USE_MODULE_IBAR
include src/modules/Makefile_ibar.mk
endif

if USE_MODULE_CLOCK
include src/modules/Makefile_clock.mk
endif

if USE_MODULE_PAGER
include src/modules/Makefile_pager.mk
endif

if USE_MODULE_BATTERY
include src/modules/Makefile_battery.mk
endif

if USE_MODULE_TEMPERATURE
include src/modules/Makefile_temperature.mk
endif

if USE_MODULE_NOTIFICATION
include src/modules/Makefile_notification.mk
endif

if USE_MODULE_CPUFREQ
include src/modules/Makefile_cpufreq.mk
endif

if USE_MODULE_IBOX
include src/modules/Makefile_ibox.mk
endif

if USE_MODULE_START
include src/modules/Makefile_start.mk
endif

if USE_MODULE_WINLIST
include src/modules/Makefile_winlist.mk
endif

if USE_MODULE_FILEMAN
include src/modules/Makefile_fileman.mk
endif

if USE_MODULE_FILEMAN_OPINFO
include src/modules/Makefile_fileman_opinfo.mk
endif

if USE_MODULE_WIZARD
include src/modules/Makefile_wizard.mk
endif

if USE_MODULE_CONF
include src/modules/Makefile_conf.mk
endif

if USE_MODULE_CONF_COMP
include src/modules/Makefile_conf_comp.mk
endif

if USE_MODULE_CONF_WALLPAPER2
include src/modules/Makefile_conf_wallpaper2.mk
endif

if USE_MODULE_CONF_THEME
include src/modules/Makefile_conf_theme.mk
endif

if USE_MODULE_CONF_INTL
include src/modules/Makefile_conf_intl.mk
endif

if USE_MODULE_MSGBUS
include src/modules/Makefile_msgbus.mk
endif

if USE_MODULE_CONF_APPLICATIONS
include src/modules/Makefile_conf_applications.mk
endif

if USE_MODULE_CONF_DISPLAY
include src/modules/Makefile_conf_display.mk
endif

if USE_MODULE_CONF_SHELVES
include src/modules/Makefile_conf_shelves.mk
endif

if USE_MODULE_CONF_BINDINGS
include src/modules/Makefile_conf_bindings.mk
endif

if USE_MODULE_CONF_WINDOW_REMEMBERS
include src/modules/Makefile_conf_window_remembers.mk
endif

if USE_MODULE_CONF_WINDOW_MANIPULATION
include src/modules/Makefile_conf_window_manipulation.mk
endif

if USE_MODULE_CONF_MENUS
include src/modules/Makefile_conf_menus.mk
endif

if USE_MODULE_CONF_DIALOGS
include src/modules/Makefile_conf_dialogs.mk
endif

if USE_MODULE_CONF_PERFORMANCE
include src/modules/Makefile_conf_performance.mk
endif

if USE_MODULE_CONF_PATHS
include src/modules/Makefile_conf_paths.mk
endif

if USE_MODULE_CONF_INTERACTION
include src/modules/Makefile_conf_interaction.mk
endif

if USE_MODULE_CONF_RANDR
include src/modules/Makefile_conf_randr.mk
endif

if USE_MODULE_GADMAN
include src/modules/Makefile_gadman.mk
endif

if USE_MODULE_MIXER
include src/modules/Makefile_mixer.mk
endif

if USE_MODULE_ILLUME2
include src/modules/Makefile_illume2.mk
include src/modules/Makefile_illume-home.mk
include src/modules/Makefile_illume-home-toggle.mk
include src/modules/Makefile_illume-softkey.mk
include src/modules/Makefile_illume-keyboard.mk
include src/modules/Makefile_illume-indicator.mk
include src/modules/Makefile_illume-kbd-toggle.mk
include src/modules/Makefile_illume-mode-toggle.mk
include src/modules/Makefile_illume-bluetooth.mk
endif

if USE_MODULE_SYSCON
include src/modules/Makefile_syscon.mk
endif

if USE_MODULE_EVERYTHING
include src/modules/Makefile_everything.mk
endif

if USE_MODULE_SYSTRAY
include src/modules/Makefile_systray.mk
endif

if USE_MODULE_APPMENU
include src/modules/Makefile_appmenu.mk
endif

if USE_MODULE_QUICKACCESS
include src/modules/Makefile_quickaccess.mk
endif

if USE_MODULE_TEAMWORK
include src/modules/Makefile_teamwork.mk
endif

if USE_MODULE_SHOT
include src/modules/Makefile_shot.mk
endif

if USE_MODULE_BACKLIGHT
include src/modules/Makefile_backlight.mk
endif

if USE_MODULE_TASKS
include src/modules/Makefile_tasks.mk
endif

if USE_MODULE_XKBSWITCH
include src/modules/Makefile_xkbswitch.mk
endif

if USE_MODULE_TILING
include src/modules/Makefile_tiling.mk
endif

if USE_MODULE_ACCESS
include src/modules/Makefile_access.mk
endif

if USE_MODULE_MUSIC_CONTROL
include src/modules/Makefile_music_control.mk
endif

if USE_MODULE_CONTACT
include src/modules/Makefile_contact.mk
endif

#if HAVE_WAYLAND_DRM
#include src/modules/Makefile_wl_drm.mk
#endif

if USE_MODULE_WL_DESKTOP_SHELL
include src/modules/Makefile_wl_desktop_shell.mk
endif

#if HAVE_WAYLAND_SCREENSHOT
#include src/modules/Makefile_wl_screenshot.mk
#endif
