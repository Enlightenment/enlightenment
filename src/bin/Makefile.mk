DISTCLEANFILES += src/bin/e_fm_shared_types.h

E_CPPFLAGS = \
-I$(top_builddir) \
-I$(top_builddir)/src/bin \
-I$(top_srcdir) \
-I$(top_srcdir)/src/bin \
@e_cflags@ \
@cf_cflags@ \
@VALGRIND_CFLAGS@ \
@EDJE_DEF@ \
@WAYLAND_CFLAGS@ \
@WAYLAND_EGL_CFLAGS@ \
-DE_BINDIR=\"$(bindir)\" \
-DPACKAGE_BIN_DIR=\"@PACKAGE_BIN_DIR@\" \
-DPACKAGE_LIB_DIR=\"@PACKAGE_LIB_DIR@\" \
-DPACKAGE_DATA_DIR=\"@PACKAGE_DATA_DIR@\" \
-DLOCALE_DIR=\"@LOCALE_DIR@\" \
-DPACKAGE_SYSCONF_DIR=\"@PACKAGE_SYSCONF_DIR@\"

bin_PROGRAMS = \
src/bin/enlightenment \
src/bin/enlightenment_imc \
src/bin/enlightenment_start \
src/bin/enlightenment_filemanager \
src/bin/enlightenment_open

internal_bindir = $(libdir)/enlightenment/utils
internal_bin_PROGRAMS = \
src/bin/enlightenment_fm_op \
src/bin/enlightenment_sys \
src/bin/enlightenment_thumb \
src/bin/enlightenment_static_grabber

if ! HAVE_WAYLAND_ONLY
internal_bin_PROGRAMS += src/bin/enlightenment_alert
endif

if HAVE_EEZE
internal_bin_PROGRAMS += src/bin/enlightenment_backlight
endif

ENLIGHTENMENTHEADERS = \
src/bin/e_about.h \
src/bin/e_acpi.h \
src/bin/e_actions.h \
src/bin/e_alert.h \
src/bin/e_atoms.h \
src/bin/e_auth.h \
src/bin/e_backlight.h \
src/bin/e_bg.h \
src/bin/e_bindings.h \
src/bin/e_client.h \
src/bin/e_client.x \
src/bin/e_color_class.h \
src/bin/e_color_dialog.h  \
src/bin/e_color.h \
src/bin/e_comp.h \
src/bin/e_comp_canvas.h \
src/bin/e_comp_cfdata.h \
src/bin/e_comp_object.h \
src/bin/e_comp_x.h \
src/bin/e_config_data.h \
src/bin/e_config_dialog.h \
src/bin/e_config.h \
src/bin/e_configure.h \
src/bin/e_confirm_dialog.h \
src/bin/e_datastore.h \
src/bin/e_dbusmenu.h \
src/bin/e_desk.h \
src/bin/e_deskenv.h \
src/bin/e_desklock.h \
src/bin/e_deskmirror.h \
src/bin/e_dialog.h \
src/bin/e_dnd.h \
src/bin/e_dpms.h \
src/bin/e_desktop_editor.h \
src/bin/e_entry_dialog.h \
src/bin/e_env.h \
src/bin/e_error.h \
src/bin/e_exec.h \
src/bin/e_exehist.h \
src/bin/e_filereg.h \
src/bin/e_flowlayout.h \
src/bin/e_fm_custom.h \
src/bin/e_fm_device.h \
src/bin/e_fm.h \
src/bin/e_fm_mime.h \
src/bin/e_fm_op.h \
src/bin/e_fm_op_registry.h \
src/bin/e_fm_prop.h \
src/bin/e_fm_shared_codec.h \
src/bin/e_fm_shared_device.h \
src/bin/e_focus.h \
src/bin/e_font.h \
src/bin/e_gadcon.h \
src/bin/e_gadcon_popup.h \
src/bin/e_grabinput.h \
src/bin/e_grab_dialog.h \
src/bin/e.h \
src/bin/e_hints.h \
src/bin/e_icon.h \
src/bin/e_ilist.h \
src/bin/e_import_config_dialog.h \
src/bin/e_import_dialog.h \
src/bin/e_includes.h \
src/bin/e_init.h \
src/bin/e_int_client_locks.h \
src/bin/e_int_client_menu.h \
src/bin/e_int_client_prop.h \
src/bin/e_int_client_remember.h \
src/bin/e_int_config_modules.h \
src/bin/e_int_gadcon_config.h \
src/bin/e_intl_data.h \
src/bin/e_intl.h \
src/bin/e_int_menus.h \
src/bin/e_int_shelf_config.h \
src/bin/e_int_toolbar_config.h \
src/bin/e_ipc_codec.h \
src/bin/e_ipc.h \
src/bin/e_layout.h \
src/bin/e_livethumb.h \
src/bin/e_log.h \
src/bin/e_manager.h \
src/bin/e_maximize.h \
src/bin/e_menu.h \
src/bin/e_mmx.h \
src/bin/e_module.h \
src/bin/e_mouse.h \
src/bin/e_moveresize.h \
src/bin/e_msgbus.h \
src/bin/e_notification.h \
src/bin/e_msg.h \
src/bin/e_obj_dialog.h \
src/bin/e_object.h \
src/bin/e_order.h \
src/bin/e_pan.h \
src/bin/e_path.h \
src/bin/e_pixmap.h \
src/bin/e_place.h \
src/bin/e_pointer.h \
src/bin/e_powersave.h \
src/bin/e_prefix.h \
src/bin/e_randr2.h \
src/bin/e_remember.h \
src/bin/e_resist.h \
src/bin/e_scale.h \
src/bin/e_screensaver.h \
src/bin/e_scrollframe.h \
src/bin/e_sha1.h \
src/bin/e_shelf.h \
src/bin/e_signals.h \
src/bin/e_slidecore.h \
src/bin/e_slider.h \
src/bin/e_slidesel.h \
src/bin/e_spectrum.h \
src/bin/e_startup.h \
src/bin/e_sys.h \
src/bin/e_test.h \
src/bin/e_theme_about.h \
src/bin/e_theme.h \
src/bin/e_thumb.h \
src/bin/e_toolbar.h \
src/bin/e_update.h \
src/bin/e_user.h \
src/bin/e_utils.h \
src/bin/e_widget_aspect.h \
src/bin/e_widget_button.h \
src/bin/e_widget_check.h \
src/bin/e_widget_color_well.h \
src/bin/e_widget_config_list.h \
src/bin/e_widget_csel.h \
src/bin/e_widget_cslider.h \
src/bin/e_widget_bgpreview.h \
src/bin/e_widget_entry.h \
src/bin/e_widget_filepreview.h \
src/bin/e_widget_flist.h \
src/bin/e_widget_font_preview.h \
src/bin/e_widget_framelist.h \
src/bin/e_widget_frametable.h \
src/bin/e_widget_fsel.h \
src/bin/e_widget.h \
src/bin/e_widget_ilist.h \
src/bin/e_widget_image.h \
src/bin/e_widget_label.h \
src/bin/e_widget_list.h \
src/bin/e_widget_preview.h \
src/bin/e_widget_radio.h \
src/bin/e_widget_scrollframe.h \
src/bin/e_widget_slider.h \
src/bin/e_widget_spectrum.h \
src/bin/e_widget_table.h \
src/bin/e_widget_textblock.h \
src/bin/e_widget_toolbar.h \
src/bin/e_widget_toolbook.h \
src/bin/e_win.h \
src/bin/e_xinerama.h \
src/bin/e_xkb.h \
src/bin/e_xsettings.h \
src/bin/e_zoomap.h \
src/bin/e_zone.h

if HAVE_WAYLAND
ENLIGHTENMENTHEADERS += \
src/bin/e_uuid_store.h \
src/bin/e_comp_wl_data.h \
src/bin/e_comp_wl_input.h \
src/bin/e_comp_wl.h
endif


enlightenment_src = \
src/bin/e_about.c \
src/bin/e_acpi.c \
src/bin/e_actions.c \
src/bin/e_atoms.c \
src/bin/e_auth.c \
src/bin/e_backlight.c \
src/bin/e_bg.c \
src/bin/e_bindings.c \
src/bin/e_client.c \
src/bin/e_color.c \
src/bin/e_color_class.c \
src/bin/e_color_dialog.c \
src/bin/e_comp.c \
src/bin/e_comp_canvas.c \
src/bin/e_comp_cfdata.c \
src/bin/e_comp_object.c \
src/bin/e_config.c \
src/bin/e_config_data.c \
src/bin/e_config_dialog.c \
src/bin/e_configure.c \
src/bin/e_confirm_dialog.c \
src/bin/e_datastore.c \
src/bin/e_dbusmenu.c \
src/bin/e_desk.c \
src/bin/e_deskenv.c \
src/bin/e_desklock.c \
src/bin/e_deskmirror.c \
src/bin/e_dialog.c \
src/bin/e_dpms.c \
src/bin/e_desktop_editor.c \
src/bin/e_dnd.c \
src/bin/e_entry_dialog.c \
src/bin/e_env.c \
src/bin/e_error.c \
src/bin/e_exec.c \
src/bin/e_exehist.c \
src/bin/e_filereg.c \
src/bin/e_flowlayout.c \
src/bin/e_fm.c \
src/bin/e_fm_custom.c \
src/bin/e_fm_device.c \
src/bin/e_fm_mime.c \
src/bin/e_fm_op_registry.c \
src/bin/e_fm_prop.c \
src/bin/e_fm_shared_codec.c \
src/bin/e_fm_shared_device.c \
src/bin/e_focus.c \
src/bin/e_font.c \
src/bin/e_gadcon.c \
src/bin/e_gadcon_popup.c \
src/bin/e_grabinput.c \
src/bin/e_grab_dialog.c \
src/bin/e_hints.c \
src/bin/e_icon.c \
src/bin/e_ilist.c \
src/bin/e_import_config_dialog.c \
src/bin/e_import_dialog.c \
src/bin/e_init.c \
src/bin/e_int_client_locks.c \
src/bin/e_int_client_menu.c \
src/bin/e_int_client_prop.c \
src/bin/e_int_client_remember.c \
src/bin/e_int_config_modules.c \
src/bin/e_int_config_comp.c \
src/bin/e_int_config_comp_match.c \
src/bin/e_int_gadcon_config.c \
src/bin/e_intl.c \
src/bin/e_intl_data.c \
src/bin/e_int_menus.c \
src/bin/e_int_shelf_config.c \
src/bin/e_int_toolbar_config.c \
src/bin/e_ipc.c \
src/bin/e_ipc_codec.c \
src/bin/e_layout.c \
src/bin/e_livethumb.c \
src/bin/e_log.c \
src/bin/e_manager.c \
src/bin/e_maximize.c \
src/bin/e_menu.c \
src/bin/e_module.c \
src/bin/e_mouse.c \
src/bin/e_moveresize.c \
src/bin/e_msgbus.c \
src/bin/e_notification.c \
src/bin/e_msg.c \
src/bin/e_obj_dialog.c \
src/bin/e_object.c \
src/bin/e_order.c \
src/bin/e_pan.c \
src/bin/e_path.c \
src/bin/e_pixmap.c \
src/bin/e_place.c \
src/bin/e_pointer.c \
src/bin/e_powersave.c \
src/bin/e_prefix.c \
src/bin/e_remember.c \
src/bin/e_resist.c \
src/bin/e_scale.c \
src/bin/e_screensaver.c \
src/bin/e_scrollframe.c \
src/bin/e_sha1.c \
src/bin/e_shelf.c \
src/bin/e_signals.c \
src/bin/e_slidecore.c \
src/bin/e_slider.c \
src/bin/e_slidesel.c \
src/bin/e_spectrum.c \
src/bin/e_startup.c \
src/bin/e_sys.c \
src/bin/e_test.c \
src/bin/e_theme_about.c \
src/bin/e_theme.c \
src/bin/e_thumb.c \
src/bin/e_toolbar.c \
src/bin/e_update.c \
src/bin/e_user.c \
src/bin/e_utils.c \
src/bin/e_widget_aspect.c \
src/bin/e_widget_button.c \
src/bin/e_widget.c \
src/bin/e_widget_check.c \
src/bin/e_widget_color_well.c \
src/bin/e_widget_config_list.c \
src/bin/e_widget_csel.c \
src/bin/e_widget_cslider.c \
src/bin/e_widget_bgpreview.c \
src/bin/e_widget_entry.c \
src/bin/e_widget_filepreview.c \
src/bin/e_widget_flist.c \
src/bin/e_widget_font_preview.c \
src/bin/e_widget_framelist.c \
src/bin/e_widget_frametable.c \
src/bin/e_widget_fsel.c \
src/bin/e_widget_ilist.c \
src/bin/e_widget_image.c \
src/bin/e_widget_label.c \
src/bin/e_widget_list.c \
src/bin/e_widget_preview.c \
src/bin/e_widget_radio.c \
src/bin/e_widget_scrollframe.c \
src/bin/e_widget_slider.c \
src/bin/e_widget_spectrum.c \
src/bin/e_widget_table.c \
src/bin/e_widget_textblock.c \
src/bin/e_widget_toolbar.c \
src/bin/e_widget_toolbook.c \
src/bin/e_win.c \
src/bin/e_xkb.c \
src/bin/e_xinerama.c \
src/bin/e_zoomap.c \
src/bin/e_zone.c \
$(ENLIGHTENMENTHEADERS)

if ! HAVE_WAYLAND_ONLY
enlightenment_src += \
src/bin/e_comp_x.c \
src/bin/e_alert.c \
src/bin/e_randr2.c \
src/bin/e_xsettings.c
endif

if HAVE_WAYLAND
enlightenment_src += \
src/bin/e_uuid_store.c \
src/bin/e_comp_wl_data.c \
src/bin/e_comp_wl_input.c \
src/bin/e_comp_wl.c
endif

src_bin_enlightenment_CPPFLAGS = $(E_CPPFLAGS) -DEFL_BETA_API_SUPPORT -DEFL_EO_API_SUPPORT -DE_LOGGING=1 @WAYLAND_CFLAGS@ @WAYLAND_EGL_CFLAGS@ -DNEED_WL
if ! HAVE_WAYLAND_ONLY
src_bin_enlightenment_CPPFLAGS += @ECORE_X_CFLAGS@ -DNEED_X=1
endif
src_bin_enlightenment_SOURCES = \
src/bin/e_main.c \
$(enlightenment_src)

src_bin_enlightenment_LDFLAGS = -export-dynamic
src_bin_enlightenment_LDADD = @e_libs@ @dlopen_libs@ @cf_libs@ @VALGRIND_LIBS@ @WAYLAND_LIBS@ @WL_DRM_LIBS@ @WAYLAND_EGL_LIBS@ -lm @SHM_OPEN_LIBS@
if ! HAVE_WAYLAND_ONLY
src_bin_enlightenment_LDADD += @ECORE_X_LIBS@
endif

src_bin_enlightenment_imc_SOURCES = \
src/bin/e.h \
src/bin/e_config_data.c \
src/bin/e_imc_main.c \
src/bin/e_intl_data.c

src_bin_enlightenment_imc_LDADD = @E_IMC_LIBS@
src_bin_enlightenment_imc_CPPFLAGS = $(E_CPPFLAGS)

src_bin_enlightenment_start_SOURCES = \
src/bin/e_start_main.c
src_bin_enlightenment_start_CPPFLAGS = $(E_CPPFLAGS) @E_START_CFLAGS@
src_bin_enlightenment_start_LDADD = @dlopen_libs@ @E_START_LIBS@

src_bin_enlightenment_thumb_SOURCES = \
src/bin/e_sha1.c \
src/bin/e_thumb_main.c \
src/bin/e_user.c

src_bin_enlightenment_thumb_LDADD = @E_THUMB_LIBS@
src_bin_enlightenment_thumb_CPPFLAGS = $(E_CPPFLAGS)

src_bin_enlightenment_fm_op_SOURCES = \
src/bin/e_fm_op.c

src_bin_enlightenment_fm_op_LDADD = @E_FM_OP_LIBS@ -lm
src_bin_enlightenment_fm_op_CPPFLAGS = $(E_CPPFLAGS)

src_bin_enlightenment_sys_SOURCES = \
src/bin/e_sys_main.c \
src/bin/e_sys_l2ping.c

src_bin_enlightenment_sys_LDADD = @SUID_LDFLAGS@ @E_SYS_LIBS@ @BLUEZ_LIBS@
src_bin_enlightenment_sys_CPPFLAGS = @SUID_CFLAGS@ @E_SYS_CFLAGS@ @BLUEZ_CFLAGS@ -DPACKAGE_SYSCONF_DIR=\"@PACKAGE_SYSCONF_DIR@\"

if HAVE_EEZE
src_bin_enlightenment_backlight_SOURCES = \
src/bin/e_backlight_main.c

src_bin_enlightenment_backlight_CPPFLAGS = @SUID_CFLAGS@ @EEZE_CFLAGS@
src_bin_enlightenment_backlight_LDADD = @SUID_LDFLAGS@ @EEZE_LIBS@
endif

src_bin_enlightenment_alert_SOURCES = \
src/bin/e_alert_main.c

src_bin_enlightenment_alert_LDADD = @E_ALERT_LIBS@
src_bin_enlightenment_alert_CPPFLAGS = @E_ALERT_CFLAGS@

src_bin_enlightenment_filemanager_SOURCES = \
src/bin/e_fm_cmdline.c
src_bin_enlightenment_filemanager_LDADD = @E_FM_CMDLINE_LIBS@
src_bin_enlightenment_filemanager_CPPFLAGS = @E_FM_CMDLINE_CFLAGS@

src_bin_enlightenment_open_SOURCES = \
src/bin/e_open.c
src_bin_enlightenment_open_LDADD = @E_OPEN_LIBS@
src_bin_enlightenment_open_CPPFLAGS = @E_OPEN_CFLAGS@

src_bin_enlightenment_static_grabber_SOURCES = \
src/bin/e_static_grab.c
src_bin_enlightenment_static_grabber_LDADD = @E_GRABBER_LIBS@
src_bin_enlightenment_static_grabber_CPPFLAGS = @E_GRABBER_CFLAGS@

include src/bin/e_fm/Makefile.mk

# HACK! why install-data-hook? install-exec-hook is run after bin_PROGRAMS
# and before internal_bin_PROGRAMS are installed. install-data-hook is
# run after both
setuid_root_mode = a=rx,u+xs
if HAVE_EEZE
enlightenment-sys-install-data-hook:
	@chmod $(setuid_root_mode) $(DESTDIR)$(libdir)/enlightenment/utils/enlightenment_sys$(EXEEXT) || true
	@chmod $(setuid_root_mode) $(DESTDIR)$(libdir)/enlightenment/utils/enlightenment_backlight$(EXEEXT) || true
else
enlightenment-sys-install-data-hook:
	@chmod $(setuid_root_mode) $(DESTDIR)$(libdir)/enlightenment/utils/enlightenment_sys$(EXEEXT) || true
endif
installed_headersdir = $(prefix)/include/enlightenment
installed_headers_DATA = $(ENLIGHTENMENTHEADERS) src/bin/e_fm_shared_types.h
INSTALL_DATA_HOOKS += enlightenment-sys-install-data-hook

PHONIES += e enlightenment install-e install-enlightenment
e: $(bin_PROGRAMS)
enlightenment: e
install-e: install-binPROGRAMS
install-enlightenment: install-e 
