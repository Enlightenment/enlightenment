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
  e_plane.h \
  e_buffer.h \
  e_region.h \
  e_surface.h \
  e_shell_surface.h \
  e_module.h \
  e_comp.h \
  e_input.h \
  e_output.h \
  e_shader.h \
  e_renderer.h \
  e_shell.h \
  e_scale.h \
  e_theme.h \
  e_pointer.h \
  e_canvas.h \
  e_manager.h \
  e_container.h \
  e_zone.h \
  e_desk.h \
  e_bg.h \
  e_box.h \
  e_icon.h \
  e_menu.h \
  e_int_menus.h \
  e_win.h \
  e_dialog.h \
  e_about.h \
  e_theme_about.h \
  e_actions.h \
  e_acpi.h \
  e_exec.h \
  e_configure.h \
  e_config_dialog.h \
  e_thumb.h \
  e_shelf.h \
  e_gadcon.h \
  e_popup.h \
  e_obj_dialog.h \
  e_border.h \
  e_sys.h \
  e_bindings.h \
  e_ipc.h \
  e_ipc_codec.h \
  e_exehist.h \
  e_alert.h

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
  e_plane.c \
  e_buffer.c \
  e_region.c \
  e_surface.c \
  e_shell_surface.c \
  e_module.c \
  e_comp.c \
  e_input.c \
  e_output.c \
  e_renderer.c \
  e_shell.c \
  e_scale.c \
  e_theme.c \
  e_pointer.c \
  e_canvas.c \
  e_manager.c \
  e_container.c \
  e_zone.c \
  e_desk.c \
  e_bg.c \
  e_box.c \
  e_icon.c \
  e_menu.c \
  e_int_menus.c \
  e_win.c \
  e_dialog.c \
  e_about.c \
  e_theme_about.c \
  e_actions.c \
  e_acpi.c \
  e_exec.c \
  e_configure.c \
  e_config_dialog.c \
  e_thumb.c \
  e_shelf.c \
  e_gadcon.c \
  e_popup.c \
  e_obj_dialog.c \
  e_border.c \
  e_sys.c \
  e_bindings.c \
  e_ipc.c \
  e_ipc_codec.c \
  e_exehist.c \
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
