#include "e.h"

static E_Config_DD *_e_config_edd = NULL;
static E_Config_DD *_e_config_module_edd = NULL;
static E_Config_DD *_e_config_font_fallback_edd = NULL;
static E_Config_DD *_e_config_font_default_edd = NULL;
static E_Config_DD *_e_config_theme_edd = NULL;
static E_Config_DD *_e_config_path_append_edd = NULL;
static E_Config_DD *_e_config_desktop_bg_edd = NULL;
static E_Config_DD *_e_config_desklock_bg_edd = NULL;
static E_Config_DD *_e_config_desktop_name_edd = NULL;
static E_Config_DD *_e_config_menu_applications_edd = NULL;
static E_Config_DD *_e_config_color_class_edd = NULL;
static E_Config_DD *_e_config_gadcon_edd = NULL;
static E_Config_DD *_e_config_gadcon_client_edd = NULL;
static E_Config_DD *_e_config_shelf_edd = NULL;
static E_Config_DD *_e_config_shelf_desk_edd = NULL;
static E_Config_DD *_e_config_mime_icon_edd = NULL;
static E_Config_DD *_e_config_syscon_action_edd = NULL;
static E_Config_DD *_e_config_env_var_edd = NULL;
static E_Config_DD *_e_config_xkb_layout_edd = NULL;
static E_Config_DD *_e_config_xkb_option_edd = NULL;
static E_Config_DD *_e_config_binding_edd = NULL;
static E_Config_DD *_e_config_bindings_mouse_edd = NULL;
static E_Config_DD *_e_config_bindings_key_edd = NULL;
static E_Config_DD *_e_config_bindings_edge_edd = NULL;
static E_Config_DD *_e_config_bindings_signal_edd = NULL;
static E_Config_DD *_e_config_bindings_wheel_edd = NULL;
static E_Config_DD *_e_config_bindings_acpi_edd = NULL;


EINTERN void
e_config_descriptor_init(Eina_Bool old)
{
   _e_config_bindings_mouse_edd = E_CONFIG_DD_NEW("E_Config_Binding_Mouse",
                                                  E_Config_Binding_Mouse);
#undef T
#undef D
#define T E_Config_Binding_Mouse
#define D _e_config_bindings_mouse_edd
   E_CONFIG_VAL(D, T, context, INT);
   E_CONFIG_VAL(D, T, modifiers, INT);
   E_CONFIG_VAL(D, T, action, STR);
   E_CONFIG_VAL(D, T, params, STR);
   E_CONFIG_VAL(D, T, button, UCHAR);
   E_CONFIG_VAL(D, T, any_mod, UCHAR);

   _e_config_bindings_key_edd = E_CONFIG_DD_NEW("E_Config_Binding_Key",
                                                E_Config_Binding_Key);
#undef T
#undef D
#define T E_Config_Binding_Key
#define D _e_config_bindings_key_edd
   E_CONFIG_VAL(D, T, context, INT);
   E_CONFIG_VAL(D, T, modifiers, INT);
   E_CONFIG_VAL(D, T, key, STR);
   E_CONFIG_VAL(D, T, action, STR);
   E_CONFIG_VAL(D, T, params, STR);
   E_CONFIG_VAL(D, T, any_mod, UCHAR);

   _e_config_bindings_edge_edd = E_CONFIG_DD_NEW("E_Config_Binding_Edge",
                                                 E_Config_Binding_Edge);
#undef T
#undef D
#define T E_Config_Binding_Edge
#define D _e_config_bindings_edge_edd
   E_CONFIG_VAL(D, T, context, INT);
   E_CONFIG_VAL(D, T, modifiers, INT);
   E_CONFIG_VAL(D, T, action, STR);
   E_CONFIG_VAL(D, T, params, STR);
   E_CONFIG_VAL(D, T, edge, UCHAR);
   E_CONFIG_VAL(D, T, any_mod, UCHAR);
   E_CONFIG_VAL(D, T, drag_only, UCHAR);
   E_CONFIG_VAL(D, T, delay, FLOAT);

   _e_config_bindings_signal_edd = E_CONFIG_DD_NEW("E_Config_Binding_Signal",
                                                   E_Config_Binding_Signal);
#undef T
#undef D
#define T E_Config_Binding_Signal
#define D _e_config_bindings_signal_edd
   E_CONFIG_VAL(D, T, context, INT);
   E_CONFIG_VAL(D, T, signal, STR);
   E_CONFIG_VAL(D, T, source, STR);
   E_CONFIG_VAL(D, T, modifiers, INT);
   E_CONFIG_VAL(D, T, any_mod, UCHAR);
   E_CONFIG_VAL(D, T, action, STR);
   E_CONFIG_VAL(D, T, params, STR);

   _e_config_bindings_wheel_edd = E_CONFIG_DD_NEW("E_Config_Binding_Wheel",
                                                  E_Config_Binding_Wheel);
#undef T
#undef D
#define T E_Config_Binding_Wheel
#define D _e_config_bindings_wheel_edd
   E_CONFIG_VAL(D, T, context, INT);
   E_CONFIG_VAL(D, T, direction, INT);
   E_CONFIG_VAL(D, T, z, INT);
   E_CONFIG_VAL(D, T, modifiers, INT);
   E_CONFIG_VAL(D, T, any_mod, UCHAR);
   E_CONFIG_VAL(D, T, action, STR);
   E_CONFIG_VAL(D, T, params, STR);

   _e_config_bindings_acpi_edd = E_CONFIG_DD_NEW("E_Config_Binding_Acpi",
                                                 E_Config_Binding_Acpi);
#undef T
#undef D
#define T E_Config_Binding_Acpi
#define D _e_config_bindings_acpi_edd
   E_CONFIG_VAL(D, T, context, INT);
   E_CONFIG_VAL(D, T, type, INT);
   E_CONFIG_VAL(D, T, status, INT);
   E_CONFIG_VAL(D, T, action, STR);
   E_CONFIG_VAL(D, T, params, STR);



   _e_config_gadcon_client_edd = E_CONFIG_DD_NEW("E_Config_Gadcon_Client", E_Config_Gadcon_Client);
#undef T
#undef D
#define T E_Config_Gadcon_Client
#define D _e_config_gadcon_client_edd
   E_CONFIG_VAL(D, T, name, STR);
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, geom.pos, INT);
   E_CONFIG_VAL(D, T, geom.size, INT);
   E_CONFIG_VAL(D, T, geom.res, INT);
   E_CONFIG_VAL(D, T, geom.pos_x, DOUBLE);
   E_CONFIG_VAL(D, T, geom.pos_y, DOUBLE);
   E_CONFIG_VAL(D, T, geom.size_w, DOUBLE);
   E_CONFIG_VAL(D, T, geom.size_h, DOUBLE);
   E_CONFIG_VAL(D, T, state_info.seq, INT);
   E_CONFIG_VAL(D, T, state_info.flags, INT);
   E_CONFIG_VAL(D, T, style, STR);
   E_CONFIG_VAL(D, T, orient, INT);
   E_CONFIG_VAL(D, T, autoscroll, UCHAR);
   E_CONFIG_VAL(D, T, resizable, UCHAR);

   _e_config_gadcon_edd = E_CONFIG_DD_NEW("E_Config_Gadcon", E_Config_Gadcon);
#undef T
#undef D
#define T E_Config_Gadcon
#define D _e_config_gadcon_edd
   E_CONFIG_VAL(D, T, name, STR);
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, zone, UINT);
   E_CONFIG_LIST(D, T, clients, _e_config_gadcon_client_edd);

   _e_config_shelf_desk_edd = E_CONFIG_DD_NEW("E_Config_Shelf_Desk", E_Config_Shelf_Desk);
#undef T
#undef D
#define T E_Config_Shelf_Desk
#define D _e_config_shelf_desk_edd
   E_CONFIG_VAL(D, T, x, INT);
   E_CONFIG_VAL(D, T, y, INT);

   _e_config_shelf_edd = E_CONFIG_DD_NEW("E_Config_Shelf", E_Config_Shelf);
#undef T
#undef D
#define T E_Config_Shelf
#define D _e_config_shelf_edd
   E_CONFIG_VAL(D, T, name, STR);
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, zone, INT);
   E_CONFIG_VAL(D, T, layer, INT);
   E_CONFIG_VAL(D, T, popup, UCHAR);
   E_CONFIG_VAL(D, T, orient, INT);
   E_CONFIG_VAL(D, T, fit_along, UCHAR);
   E_CONFIG_VAL(D, T, fit_size, UCHAR);
   E_CONFIG_VAL(D, T, style, STR);
   E_CONFIG_VAL(D, T, size, INT);
   E_CONFIG_VAL(D, T, overlap, INT);
   E_CONFIG_VAL(D, T, autohide, INT);
   E_CONFIG_VAL(D, T, autohide_show_action, INT);
   E_CONFIG_VAL(D, T, hide_timeout, FLOAT);
   E_CONFIG_VAL(D, T, hide_duration, FLOAT);
   E_CONFIG_VAL(D, T, desk_show_mode, INT);
   E_CONFIG_LIST(D, T, desk_list, _e_config_shelf_desk_edd);

   _e_config_desklock_bg_edd = E_CONFIG_DD_NEW("E_Config_Desklock_Background", E_Config_Desklock_Background);
#undef T
#undef D
#define T E_Config_Desklock_Background
#define D _e_config_desklock_bg_edd
   E_CONFIG_VAL(D, T, file, STR);
   E_CONFIG_VAL(D, T, hide_logo, UCHAR);

   _e_config_desktop_bg_edd = E_CONFIG_DD_NEW("E_Config_Desktop_Background", E_Config_Desktop_Background);
#undef T
#undef D
#define T E_Config_Desktop_Background
#define D _e_config_desktop_bg_edd
   E_CONFIG_VAL(D, T, zone, INT);
   E_CONFIG_VAL(D, T, desk_x, INT);
   E_CONFIG_VAL(D, T, desk_y, INT);
   E_CONFIG_VAL(D, T, file, STR);

   _e_config_desktop_name_edd = E_CONFIG_DD_NEW("E_Config_Desktop_Name", E_Config_Desktop_Name);
#undef T
#undef D
#define T E_Config_Desktop_Name
#define D _e_config_desktop_name_edd
   E_CONFIG_VAL(D, T, zone, INT);
   E_CONFIG_VAL(D, T, desk_x, INT);
   E_CONFIG_VAL(D, T, desk_y, INT);
   E_CONFIG_VAL(D, T, name, STR);

   _e_config_path_append_edd = E_CONFIG_DD_NEW("E_Path_Dir", E_Path_Dir);
#undef T
#undef D
#define T E_Path_Dir
#define D _e_config_path_append_edd
   E_CONFIG_VAL(D, T, dir, STR);

   _e_config_module_edd = E_CONFIG_DD_NEW("E_Config_Module", E_Config_Module);
#undef T
#undef D
#define T E_Config_Module
#define D _e_config_module_edd
   E_CONFIG_VAL(D, T, name, STR);
   E_CONFIG_VAL(D, T, enabled, UCHAR);
   E_CONFIG_VAL(D, T, delayed, UCHAR);
   E_CONFIG_VAL(D, T, priority, INT);

   _e_config_font_default_edd = E_CONFIG_DD_NEW("E_Font_Default",
                                                E_Font_Default);
#undef T
#undef D
#define T E_Font_Default
#define D _e_config_font_default_edd
   E_CONFIG_VAL(D, T, text_class, STR);
   E_CONFIG_VAL(D, T, font, STR);
   E_CONFIG_VAL(D, T, size, INT);

   _e_config_font_fallback_edd = E_CONFIG_DD_NEW("E_Font_Fallback",
                                                 E_Font_Fallback);
#undef T
#undef D
#define T E_Font_Fallback
#define D _e_config_font_fallback_edd
   E_CONFIG_VAL(D, T, name, STR);

   _e_config_menu_applications_edd = E_CONFIG_DD_NEW("E_Int_Menu_Applications",
                                                     E_Int_Menu_Applications);
#undef T
#undef D
#define T E_Int_Menu_Applications
#define D _e_config_menu_applications_edd
   E_CONFIG_VAL(D, T, orig_path, STR);
   E_CONFIG_VAL(D, T, try_exec, STR);
   E_CONFIG_VAL(D, T, exec, STR);
   E_CONFIG_VAL(D, T, load_time, LL);
   E_CONFIG_VAL(D, T, exec_valid, INT);

   e_remember_edd = E_CONFIG_DD_NEW("E_Remember", E_Remember);
#undef T
#undef D
#define T E_Remember
#define D e_remember_edd
   E_CONFIG_VAL(D, T, version, UINT);
   E_CONFIG_VAL(D, T, match, INT);
   E_CONFIG_VAL(D, T, no_reopen, INT);
   E_CONFIG_VAL(D, T, apply_first_only, UCHAR);
   E_CONFIG_VAL(D, T, keep_settings, UCHAR);
   E_CONFIG_VAL(D, T, name, STR);
   E_CONFIG_VAL(D, T, class, STR);
   E_CONFIG_VAL(D, T, title, STR);
   E_CONFIG_VAL(D, T, role, STR);
   E_CONFIG_VAL(D, T, type, INT);
   E_CONFIG_VAL(D, T, transient, UCHAR);
   E_CONFIG_VAL(D, T, apply, INT);
   E_CONFIG_VAL(D, T, max_score, INT);
   E_CONFIG_VAL(D, T, prop.pos_x, INT);
   E_CONFIG_VAL(D, T, prop.pos_y, INT);
   E_CONFIG_VAL(D, T, prop.res_x, INT);
   E_CONFIG_VAL(D, T, prop.res_y, INT);
   E_CONFIG_VAL(D, T, prop.pos_w, INT);
   E_CONFIG_VAL(D, T, prop.pos_h, INT);
   E_CONFIG_VAL(D, T, prop.w, INT);
   E_CONFIG_VAL(D, T, prop.h, INT);
   E_CONFIG_VAL(D, T, prop.layer, INT);
   E_CONFIG_VAL(D, T, prop.maximize, UINT);
   E_CONFIG_VAL(D, T, prop.lock_user_location, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_location, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_user_size, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_size, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_user_stacking, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_stacking, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_user_iconify, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_iconify, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_user_desk, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_desk, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_user_sticky, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_sticky, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_user_shade, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_shade, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_user_maximize, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_maximize, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_user_fullscreen, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_client_fullscreen, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_border, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_close, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_focus_in, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_focus_out, UCHAR);
   E_CONFIG_VAL(D, T, prop.lock_life, UCHAR);
   E_CONFIG_VAL(D, T, prop.border, STR);
   E_CONFIG_VAL(D, T, prop.sticky, UCHAR);
   E_CONFIG_VAL(D, T, prop.shaded, UCHAR);
   E_CONFIG_VAL(D, T, prop.skip_winlist, UCHAR);
   E_CONFIG_VAL(D, T, prop.skip_pager, UCHAR);
   E_CONFIG_VAL(D, T, prop.skip_taskbar, UCHAR);
   E_CONFIG_VAL(D, T, prop.fullscreen, UCHAR);
   E_CONFIG_VAL(D, T, prop.desk_x, INT);
   E_CONFIG_VAL(D, T, prop.desk_y, INT);
   E_CONFIG_VAL(D, T, prop.zone, INT);
   E_CONFIG_VAL(D, T, prop.command, STR);
   E_CONFIG_VAL(D, T, prop.icon_preference, UCHAR);
   E_CONFIG_VAL(D, T, prop.desktop_file, STR);
   E_CONFIG_VAL(D, T, prop.offer_resistance, UCHAR);
   E_CONFIG_VAL(D, T, prop.opacity, UCHAR);
   E_CONFIG_VAL(D, T, prop.volume, INT);
   E_CONFIG_VAL(D, T, prop.volume_min, INT);
   E_CONFIG_VAL(D, T, prop.volume_max, INT);
   E_CONFIG_VAL(D, T, prop.mute, UCHAR);
   E_CONFIG_VAL(D, T, uuid, STR);
   E_CONFIG_VAL(D, T, pid, INT);

   _e_config_color_class_edd = E_CONFIG_DD_NEW("E_Color_Class", E_Color_Class);
#undef T
#undef D
#define T E_Color_Class
#define D _e_config_color_class_edd
   E_CONFIG_VAL(D, T, name, STR);
   E_CONFIG_VAL(D, T, r, INT);
   E_CONFIG_VAL(D, T, g, INT);
   E_CONFIG_VAL(D, T, b, INT);
   E_CONFIG_VAL(D, T, a, INT);
   E_CONFIG_VAL(D, T, r2, INT);
   E_CONFIG_VAL(D, T, g2, INT);
   E_CONFIG_VAL(D, T, b2, INT);
   E_CONFIG_VAL(D, T, a2, INT);
   E_CONFIG_VAL(D, T, r3, INT);
   E_CONFIG_VAL(D, T, g3, INT);
   E_CONFIG_VAL(D, T, b3, INT);
   E_CONFIG_VAL(D, T, a3, INT);

   _e_config_mime_icon_edd = E_CONFIG_DD_NEW("E_Config_Mime_Icon",
                                             E_Config_Mime_Icon);
#undef T
#undef D
#define T E_Config_Mime_Icon
#define D _e_config_mime_icon_edd
   E_CONFIG_VAL(D, T, mime, STR);
   E_CONFIG_VAL(D, T, icon, STR);

   _e_config_syscon_action_edd = E_CONFIG_DD_NEW("E_Config_Syscon_Action",
                                                 E_Config_Syscon_Action);
#undef T
#undef D
#define T E_Config_Syscon_Action
#define D _e_config_syscon_action_edd
   E_CONFIG_VAL(D, T, action, STR);
   E_CONFIG_VAL(D, T, params, STR);
   E_CONFIG_VAL(D, T, button, STR);
   E_CONFIG_VAL(D, T, icon, STR);
   E_CONFIG_VAL(D, T, is_main, INT);

   _e_config_env_var_edd = E_CONFIG_DD_NEW("E_Config_Env_Var",
                                           E_Config_Env_Var);
#undef T
#undef D
#define T E_Config_Env_Var
#define D _e_config_env_var_edd
   E_CONFIG_VAL(D, T, var, STR);
   E_CONFIG_VAL(D, T, val, STR);
   E_CONFIG_VAL(D, T, unset, UCHAR);

   _e_config_xkb_layout_edd = E_CONFIG_DD_NEW("E_Config_XKB_Layout",
                                              E_Config_XKB_Layout);
#undef T
#undef D
#define T E_Config_XKB_Layout
#define D _e_config_xkb_layout_edd
   E_CONFIG_VAL(D, T, name, STR);
   E_CONFIG_VAL(D, T, model, STR);
   E_CONFIG_VAL(D, T, variant, STR);

   _e_config_xkb_option_edd = E_CONFIG_DD_NEW("E_Config_XKB_Option",
                                              E_Config_XKB_Option);
#undef T
#undef D
#define T E_Config_XKB_Option
#define D _e_config_xkb_option_edd
   E_CONFIG_VAL(D, T, name, STR);

   _e_config_edd = E_CONFIG_DD_NEW("E_Config", E_Config);
#undef T
#undef D
#define T E_Config
#define D _e_config_edd
   /**/ /* == already configurable via ipc */
   E_CONFIG_VAL(D, T, config_version, INT); /**/
   E_CONFIG_VAL(D, T, config_type, UINT); /**/
   E_CONFIG_VAL(D, T, show_splash, INT); /**/
   E_CONFIG_VAL(D, T, desktop_default_background, STR); /**/
   E_CONFIG_VAL(D, T, desktop_default_name, STR); /**/
   E_CONFIG_LIST(D, T, desktop_backgrounds, _e_config_desktop_bg_edd); /**/
   E_CONFIG_LIST(D, T, desktop_names, _e_config_desktop_name_edd); /**/
   E_CONFIG_VAL(D, T, menus_scroll_speed, DOUBLE); /**/
   E_CONFIG_VAL(D, T, menus_fast_mouse_move_threshhold, DOUBLE); /**/
   E_CONFIG_VAL(D, T, menus_click_drag_timeout, DOUBLE); /**/
   E_CONFIG_VAL(D, T, window_maximize_animate, INT); /**/
   E_CONFIG_VAL(D, T, window_maximize_transition, INT); /**/
   E_CONFIG_VAL(D, T, window_maximize_time, DOUBLE); /**/
   E_CONFIG_VAL(D, T, border_shade_animate, INT); /**/
   E_CONFIG_VAL(D, T, border_shade_transition, INT); /**/
   E_CONFIG_VAL(D, T, border_shade_speed, DOUBLE); /**/
   E_CONFIG_VAL(D, T, framerate, DOUBLE); /**/
   E_CONFIG_VAL(D, T, priority, INT); /**/
   E_CONFIG_VAL(D, T, zone_desks_x_count, INT); /**/
   E_CONFIG_VAL(D, T, zone_desks_y_count, INT); /**/
   E_CONFIG_VAL(D, T, show_desktop_icons, INT); /**/
   E_CONFIG_VAL(D, T, edge_flip_dragging, INT); /**/
   E_CONFIG_VAL(D, T, language, STR); /**/
   E_CONFIG_VAL(D, T, no_module_delay, INT); /**/
   E_CONFIG_VAL(D, T, desklock_language, STR); /**/
   E_CONFIG_LIST(D, T, modules, _e_config_module_edd); /**/
   EET_DATA_DESCRIPTOR_ADD_LIST_STRING(D, T, "bad_modules", bad_modules);
   E_CONFIG_LIST(D, T, font_fallbacks, _e_config_font_fallback_edd); /**/
   E_CONFIG_LIST(D, T, font_defaults, _e_config_font_default_edd); /**/
   E_CONFIG_LIST(D, T, mouse_bindings, _e_config_bindings_mouse_edd); /**/
   E_CONFIG_LIST(D, T, key_bindings, _e_config_bindings_key_edd); /**/
   E_CONFIG_LIST(D, T, edge_bindings, _e_config_bindings_edge_edd); /**/
   E_CONFIG_LIST(D, T, signal_bindings, _e_config_bindings_signal_edd); /**/
   E_CONFIG_LIST(D, T, wheel_bindings, _e_config_bindings_wheel_edd); /**/
   E_CONFIG_LIST(D, T, acpi_bindings, _e_config_bindings_acpi_edd); /**/
   E_CONFIG_LIST(D, T, path_append_data, _e_config_path_append_edd); /**/
   E_CONFIG_LIST(D, T, path_append_images, _e_config_path_append_edd); /**/
   E_CONFIG_LIST(D, T, path_append_fonts, _e_config_path_append_edd); /**/
   E_CONFIG_LIST(D, T, path_append_init, _e_config_path_append_edd); /**/
   E_CONFIG_LIST(D, T, path_append_icons, _e_config_path_append_edd); /**/
   E_CONFIG_LIST(D, T, path_append_modules, _e_config_path_append_edd); /**/
   E_CONFIG_LIST(D, T, path_append_backgrounds, _e_config_path_append_edd); /**/
   E_CONFIG_VAL(D, T, window_placement_policy, INT); /**/
   E_CONFIG_VAL(D, T, window_grouping, INT); /**/
   E_CONFIG_VAL(D, T, focus_policy, INT); /**/
   E_CONFIG_VAL(D, T, focus_setting, INT); /**/
   E_CONFIG_VAL(D, T, pass_click_on, INT); /**/
   E_CONFIG_VAL(D, T, window_activehint_policy, INT); /**/
   E_CONFIG_VAL(D, T, always_click_to_raise, INT); /**/
   E_CONFIG_VAL(D, T, always_click_to_focus, INT); /**/
   E_CONFIG_VAL(D, T, use_auto_raise, INT); /**/
   E_CONFIG_VAL(D, T, auto_raise_delay, DOUBLE); /**/
   E_CONFIG_VAL(D, T, use_resist, INT); /**/
   E_CONFIG_VAL(D, T, drag_resist, INT); /**/
   E_CONFIG_VAL(D, T, desk_resist, INT); /**/
   E_CONFIG_VAL(D, T, window_resist, INT); /**/
   E_CONFIG_VAL(D, T, gadget_resist, INT); /**/
   E_CONFIG_VAL(D, T, geometry_auto_resize_limit, INT); /**/
   E_CONFIG_VAL(D, T, geometry_auto_move, INT); /**/
   E_CONFIG_VAL(D, T, winlist_warp_while_selecting, INT); /**/
   E_CONFIG_VAL(D, T, winlist_warp_at_end, INT); /**/
   E_CONFIG_VAL(D, T, winlist_no_warp_on_direction, INT); /**/
   E_CONFIG_VAL(D, T, winlist_warp_speed, DOUBLE); /**/
   E_CONFIG_VAL(D, T, winlist_scroll_animate, INT); /**/
   E_CONFIG_VAL(D, T, winlist_scroll_speed, DOUBLE); /**/
   E_CONFIG_VAL(D, T, winlist_list_show_iconified, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_show_other_desk_iconified, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_show_other_screen_iconified, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_show_other_desk_windows, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_show_other_screen_windows, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_uncover_while_selecting, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_jump_desk_while_selecting, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_focus_while_selecting, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_raise_while_selecting, INT); /**/
   E_CONFIG_VAL(D, T, winlist_list_move_after_select, INT); /**/
   E_CONFIG_VAL(D, T, winlist_pos_align_x, DOUBLE); /**/
   E_CONFIG_VAL(D, T, winlist_pos_align_y, DOUBLE); /**/
   E_CONFIG_VAL(D, T, winlist_pos_size_w, DOUBLE); /**/
   E_CONFIG_VAL(D, T, winlist_pos_size_h, DOUBLE); /**/
   E_CONFIG_VAL(D, T, winlist_pos_min_w, INT); /**/
   E_CONFIG_VAL(D, T, winlist_pos_min_h, INT); /**/
   E_CONFIG_VAL(D, T, winlist_pos_max_w, INT); /**/
   E_CONFIG_VAL(D, T, winlist_pos_max_h, INT); /**/
   E_CONFIG_VAL(D, T, maximize_policy, INT); /**/
   E_CONFIG_VAL(D, T, allow_manip, INT); /**/
   E_CONFIG_VAL(D, T, border_fix_on_shelf_toggle, INT); /**/
   E_CONFIG_VAL(D, T, allow_above_fullscreen, INT); /**/
   E_CONFIG_VAL(D, T, kill_if_close_not_possible, INT); /**/
   E_CONFIG_VAL(D, T, kill_process, INT); /**/
   E_CONFIG_VAL(D, T, kill_timer_wait, DOUBLE); /**/
   E_CONFIG_VAL(D, T, ping_clients, INT); /**/
   E_CONFIG_VAL(D, T, transition_start, STR); /**/
   E_CONFIG_VAL(D, T, transition_desk, STR); /**/
   E_CONFIG_VAL(D, T, transition_change, STR); /**/
   E_CONFIG_LIST(D, T, remembers, e_remember_edd);
   E_CONFIG_LIST(D, T, menu_applications, _e_config_menu_applications_edd);
   E_CONFIG_VAL(D, T, remember_internal_windows, INT);
   E_CONFIG_VAL(D, T, remember_internal_fm_windows, UCHAR);
   E_CONFIG_VAL(D, T, remember_internal_fm_windows_globally, UCHAR);
   E_CONFIG_VAL(D, T, move_info_follows, INT); /**/
   E_CONFIG_VAL(D, T, resize_info_follows, INT); /**/
   E_CONFIG_VAL(D, T, move_info_visible, INT); /**/
   E_CONFIG_VAL(D, T, resize_info_visible, INT); /**/
   E_CONFIG_VAL(D, T, focus_last_focused_per_desktop, INT); /**/
   E_CONFIG_VAL(D, T, focus_revert_on_hide_or_close, INT); /**/
   E_CONFIG_VAL(D, T, focus_revert_allow_sticky, INT); /**/
   E_CONFIG_VAL(D, T, pointer_slide, INT); /**/
   E_CONFIG_VAL(D, T, disable_all_pointer_warps, INT); /**/
   E_CONFIG_VAL(D, T, pointer_warp_speed, DOUBLE); /**/
   E_CONFIG_VAL(D, T, use_e_cursor, INT); /**/
   E_CONFIG_VAL(D, T, cursor_size, INT); /**/
   E_CONFIG_VAL(D, T, menu_autoscroll_margin, INT); /**/
   E_CONFIG_VAL(D, T, menu_autoscroll_cursor_margin, INT); /**/
   E_CONFIG_VAL(D, T, transient.move, INT); /* FIXME: implement */
   E_CONFIG_VAL(D, T, transient.resize, INT); /* FIXME: implement */
   E_CONFIG_VAL(D, T, transient.raise, INT); /**/
   E_CONFIG_VAL(D, T, transient.lower, INT); /**/
   E_CONFIG_VAL(D, T, transient.layer, INT); /**/
   E_CONFIG_VAL(D, T, transient.desktop, INT); /**/
   E_CONFIG_VAL(D, T, transient.iconify, INT); /**/
   E_CONFIG_VAL(D, T, menu_eap_name_show, INT); /**/
   E_CONFIG_VAL(D, T, menu_eap_generic_show, INT); /**/
   E_CONFIG_VAL(D, T, menu_eap_comment_show, INT); /**/
   E_CONFIG_VAL(D, T, fullscreen_policy, INT); /**/
   E_CONFIG_VAL(D, T, input_method, STR); /**/
   E_CONFIG_LIST(D, T, path_append_messages, _e_config_path_append_edd); /**/
   E_CONFIG_VAL(D, T, exebuf_term_cmd, STR);
   E_CONFIG_LIST(D, T, color_classes, _e_config_color_class_edd);
   E_CONFIG_VAL(D, T, use_app_icon, INT);
   E_CONFIG_VAL(D, T, cnfmdlg_disabled, INT); /**/
   E_CONFIG_VAL(D, T, cfgdlg_auto_apply, INT); /**/
   E_CONFIG_VAL(D, T, cfgdlg_default_mode, INT); /**/
   E_CONFIG_LIST(D, T, gadcons, _e_config_gadcon_edd);
   E_CONFIG_LIST(D, T, shelves, _e_config_shelf_edd);
   E_CONFIG_VAL(D, T, font_hinting, INT); /**/
   E_CONFIG_VAL(D, T, desklock_passwd, INT);
   E_CONFIG_VAL(D, T, desklock_pin, INT);
   E_CONFIG_LIST(D, T, desklock_backgrounds, _e_config_desklock_bg_edd); /**/
   E_CONFIG_VAL(D, T, desklock_auth_method, INT);
   E_CONFIG_VAL(D, T, desklock_login_box_zone, INT);
   E_CONFIG_VAL(D, T, desklock_start_locked, INT);
   E_CONFIG_VAL(D, T, desklock_on_suspend, INT);
   E_CONFIG_VAL(D, T, desklock_autolock_screensaver, INT);
   E_CONFIG_VAL(D, T, desklock_post_screensaver_time, DOUBLE);
   E_CONFIG_VAL(D, T, desklock_autolock_idle, INT);
   E_CONFIG_VAL(D, T, desklock_autolock_idle_timeout, DOUBLE);
   E_CONFIG_VAL(D, T, desklock_use_custom_desklock, INT);
   E_CONFIG_VAL(D, T, desklock_custom_desklock_cmd, STR);
   E_CONFIG_VAL(D, T, desklock_ask_presentation, UCHAR);
   E_CONFIG_VAL(D, T, desklock_ask_presentation_timeout, DOUBLE);

   E_CONFIG_VAL(D, T, screensaver_enable, INT);
   E_CONFIG_VAL(D, T, screensaver_timeout, INT);
   E_CONFIG_VAL(D, T, screensaver_interval, INT);
   E_CONFIG_VAL(D, T, screensaver_blanking, INT);
   E_CONFIG_VAL(D, T, screensaver_expose, INT);
   E_CONFIG_VAL(D, T, screensaver_ask_presentation, UCHAR);
   E_CONFIG_VAL(D, T, screensaver_ask_presentation_timeout, DOUBLE);

   E_CONFIG_VAL(D, T, screensaver_wake_on_notify, INT);
   E_CONFIG_VAL(D, T, screensaver_wake_on_urgent, INT);

   E_CONFIG_VAL(D, T, screensaver_suspend, UCHAR);
   E_CONFIG_VAL(D, T, screensaver_suspend_on_ac, UCHAR);
   E_CONFIG_VAL(D, T, screensaver_suspend_delay, DOUBLE);

   E_CONFIG_VAL(D, T, dpms_enable, INT);
   E_CONFIG_VAL(D, T, dpms_standby_enable, INT);
   E_CONFIG_VAL(D, T, dpms_suspend_enable, INT);
   E_CONFIG_VAL(D, T, dpms_off_enable, INT);
   E_CONFIG_VAL(D, T, dpms_standby_timeout, INT);
   E_CONFIG_VAL(D, T, dpms_suspend_timeout, INT);
   E_CONFIG_VAL(D, T, dpms_off_timeout, INT);
   E_CONFIG_VAL(D, T, no_dpms_on_fullscreen, UCHAR);

   E_CONFIG_VAL(D, T, clientlist_group_by, INT);
   E_CONFIG_VAL(D, T, clientlist_include_all_zones, INT);
   E_CONFIG_VAL(D, T, clientlist_separate_with, INT);
   E_CONFIG_VAL(D, T, clientlist_sort_by, INT);
   E_CONFIG_VAL(D, T, clientlist_separate_iconified_apps, INT);
   E_CONFIG_VAL(D, T, clientlist_warp_to_iconified_desktop, INT);
   E_CONFIG_VAL(D, T, clientlist_limit_caption_len, INT);
   E_CONFIG_VAL(D, T, clientlist_max_caption_len, INT);

   E_CONFIG_VAL(D, T, mouse_hand, INT);
   E_CONFIG_VAL(D, T, mouse_accel_numerator, INT);
   E_CONFIG_VAL(D, T, mouse_accel_denominator, INT);
   E_CONFIG_VAL(D, T, mouse_accel_threshold, INT);

   E_CONFIG_VAL(D, T, border_raise_on_mouse_action, INT);
   E_CONFIG_VAL(D, T, border_raise_on_focus, INT);
   E_CONFIG_VAL(D, T, raise_on_revert_focus, INT);
   E_CONFIG_VAL(D, T, desk_flip_wrap, INT);
   E_CONFIG_VAL(D, T, fullscreen_flip, INT);
   E_CONFIG_VAL(D, T, multiscreen_flip, INT);

   E_CONFIG_VAL(D, T, icon_theme, STR);
   E_CONFIG_VAL(D, T, icon_theme_overrides, UCHAR);
   E_CONFIG_VAL(D, T, desktop_environment, STR);

   E_CONFIG_VAL(D, T, desk_flip_animate_mode, INT);
   E_CONFIG_VAL(D, T, desk_flip_animate_type, STR);
   E_CONFIG_VAL(D, T, desk_flip_animate_interpolation, INT);

   E_CONFIG_VAL(D, T, wallpaper_import_last_dev, STR);
   E_CONFIG_VAL(D, T, wallpaper_import_last_path, STR);

   E_CONFIG_VAL(D, T, theme_default_border_style, STR);

   E_CONFIG_LIST(D, T, mime_icons, _e_config_mime_icon_edd); /**/

   E_CONFIG_VAL(D, T, desk_auto_switch, INT);

   E_CONFIG_VAL(D, T, screen_limits, INT);

   E_CONFIG_VAL(D, T, thumb_nice, INT);

   E_CONFIG_VAL(D, T, menu_icons_hide, UCHAR);
   E_CONFIG_VAL(D, T, menu_favorites_show, INT);
   E_CONFIG_VAL(D, T, menu_apps_show, INT);
   E_CONFIG_VAL(D, T, menu_gadcon_client_toplevel, INT);

   E_CONFIG_VAL(D, T, ping_clients_interval, INT);

   E_CONFIG_VAL(D, T, thumbscroll_enable, INT);
   E_CONFIG_VAL(D, T, thumbscroll_threshhold, INT);
   E_CONFIG_VAL(D, T, thumbscroll_momentum_threshhold, DOUBLE);
   E_CONFIG_VAL(D, T, thumbscroll_friction, DOUBLE);

   E_CONFIG_VAL(D, T, filemanager_single_click, UCHAR);
   E_CONFIG_VAL(D, T, device_desktop, INT);
   E_CONFIG_VAL(D, T, device_auto_mount, INT);
   E_CONFIG_VAL(D, T, device_auto_open, INT);
   E_CONFIG_VAL(D, T, filemanager_copy, UCHAR);
   E_CONFIG_VAL(D, T, filemanager_secure_rm, UCHAR);

   E_CONFIG_VAL(D, T, border_keyboard.timeout, DOUBLE);
   E_CONFIG_VAL(D, T, border_keyboard.move.dx, UCHAR);
   E_CONFIG_VAL(D, T, border_keyboard.move.dy, UCHAR);
   E_CONFIG_VAL(D, T, border_keyboard.resize.dx, UCHAR);
   E_CONFIG_VAL(D, T, border_keyboard.resize.dy, UCHAR);

   E_CONFIG_VAL(D, T, scale.min, DOUBLE);
   E_CONFIG_VAL(D, T, scale.max, DOUBLE);
   E_CONFIG_VAL(D, T, scale.factor, DOUBLE);
   E_CONFIG_VAL(D, T, scale.base_dpi, INT);
   E_CONFIG_VAL(D, T, scale.use_dpi, UCHAR);
   E_CONFIG_VAL(D, T, scale.use_custom, UCHAR);

   E_CONFIG_VAL(D, T, show_cursor, UCHAR);
   E_CONFIG_VAL(D, T, idle_cursor, UCHAR);

   E_CONFIG_VAL(D, T, default_system_menu, STR);

   E_CONFIG_VAL(D, T, cfgdlg_normal_wins, UCHAR);

   E_CONFIG_VAL(D, T, syscon.main.icon_size, INT);
   E_CONFIG_VAL(D, T, syscon.secondary.icon_size, INT);
   E_CONFIG_VAL(D, T, syscon.extra.icon_size, INT);
   E_CONFIG_VAL(D, T, syscon.timeout, DOUBLE);
   E_CONFIG_VAL(D, T, syscon.do_input, UCHAR);
   E_CONFIG_LIST(D, T, syscon.actions, _e_config_syscon_action_edd);

   E_CONFIG_VAL(D, T, mode.presentation, UCHAR);
   E_CONFIG_VAL(D, T, mode.offline, UCHAR);

   E_CONFIG_VAL(D, T, exec.expire_timeout, DOUBLE);
   E_CONFIG_VAL(D, T, exec.show_run_dialog, UCHAR);
   E_CONFIG_VAL(D, T, exec.show_exit_dialog, UCHAR);

   E_CONFIG_VAL(D, T, null_container_win, UCHAR);

   E_CONFIG_LIST(D, T, env_vars, _e_config_env_var_edd);

   E_CONFIG_VAL(D, T, backlight.normal, DOUBLE);
   E_CONFIG_VAL(D, T, backlight.dim, DOUBLE);
   E_CONFIG_VAL(D, T, backlight.transition, DOUBLE);
   E_CONFIG_VAL(D, T, backlight.timer, DOUBLE);
   E_CONFIG_VAL(D, T, backlight.sysdev, STR);
   E_CONFIG_VAL(D, T, backlight.idle_dim, UCHAR);

   E_CONFIG_VAL(D, T, deskenv.load_xrdb, UCHAR);
   E_CONFIG_VAL(D, T, deskenv.load_xmodmap, UCHAR);
   E_CONFIG_VAL(D, T, deskenv.load_gnome, UCHAR);
   E_CONFIG_VAL(D, T, deskenv.load_kde, UCHAR);

   E_CONFIG_VAL(D, T, powersave.none, DOUBLE);
   E_CONFIG_VAL(D, T, powersave.low, DOUBLE);
   E_CONFIG_VAL(D, T, powersave.medium, DOUBLE);
   E_CONFIG_VAL(D, T, powersave.high, DOUBLE);
   E_CONFIG_VAL(D, T, powersave.extreme, DOUBLE);
   E_CONFIG_VAL(D, T, powersave.min, INT);
   E_CONFIG_VAL(D, T, powersave.max, INT);

   E_CONFIG_VAL(D, T, xsettings.enabled, UCHAR);
   E_CONFIG_VAL(D, T, xsettings.match_e17_theme, UCHAR);
   E_CONFIG_VAL(D, T, xsettings.match_e17_icon_theme, UCHAR);
   E_CONFIG_VAL(D, T, xsettings.xft_antialias, INT);
   E_CONFIG_VAL(D, T, xsettings.xft_hinting, INT);
   E_CONFIG_VAL(D, T, xsettings.xft_hint_style, STR);
   E_CONFIG_VAL(D, T, xsettings.xft_rgba, STR);
   E_CONFIG_VAL(D, T, xsettings.net_theme_name, STR);
   E_CONFIG_VAL(D, T, xsettings.net_icon_theme_name, STR);
   E_CONFIG_VAL(D, T, xsettings.gtk_font_name, STR);

   E_CONFIG_VAL(D, T, update.check, UCHAR);
   E_CONFIG_VAL(D, T, update.later, UCHAR);

   E_CONFIG_LIST(D, T, xkb.used_layouts, _e_config_xkb_layout_edd);
   E_CONFIG_LIST(D, T, xkb.used_options, _e_config_xkb_option_edd);
   E_CONFIG_VAL(D, T, xkb.only_label, INT);
   E_CONFIG_VAL(D, T, xkb.dont_touch_my_damn_keyboard, UCHAR);
   E_CONFIG_VAL(D, T, xkb.default_model, STR);
   E_CONFIG_VAL(D, T, xkb.use_cache, UCHAR);

   E_CONFIG_VAL(D, T, keyboard.repeat_delay, INT);
   E_CONFIG_VAL(D, T, keyboard.repeat_rate, INT);

   if (old)
     {
        E_CONFIG_SUB(D, T, xkb.current_layout, _e_config_xkb_option_edd);
        E_CONFIG_SUB(D, T, xkb.sel_layout, _e_config_xkb_option_edd);
        E_CONFIG_SUB(D, T, xkb.lock_layout, _e_config_xkb_option_edd);
     }
   else
     {
        E_CONFIG_SUB(D, T, xkb.current_layout, _e_config_xkb_layout_edd);
        E_CONFIG_SUB(D, T, xkb.sel_layout, _e_config_xkb_layout_edd);
        E_CONFIG_SUB(D, T, xkb.lock_layout, _e_config_xkb_layout_edd);
     }
   E_CONFIG_VAL(D, T, xkb.selected_layout, STR);
   E_CONFIG_VAL(D, T, xkb.cur_layout, STR);
   E_CONFIG_VAL(D, T, xkb.desklock_layout, STR);
   //E_CONFIG_VAL(D, T, xkb.cur_group, INT);

   E_CONFIG_VAL(D, T, exe_always_single_instance, UCHAR);

   _e_config_binding_edd = E_CONFIG_DD_NEW("E_Config_Bindings", E_Config_Bindings);
#undef T
#undef D
#define T E_Config_Bindings
#define D _e_config_binding_edd
   E_CONFIG_VAL(D, T, config_version, UINT); /**/
   E_CONFIG_LIST(D, T, mouse_bindings, _e_config_bindings_mouse_edd); /**/
   E_CONFIG_LIST(D, T, key_bindings, _e_config_bindings_key_edd); /**/
   E_CONFIG_LIST(D, T, edge_bindings, _e_config_bindings_edge_edd); /**/
   E_CONFIG_LIST(D, T, signal_bindings, _e_config_bindings_signal_edd); /**/
   E_CONFIG_LIST(D, T, wheel_bindings, _e_config_bindings_wheel_edd); /**/
   E_CONFIG_LIST(D, T, acpi_bindings, _e_config_bindings_acpi_edd); /**/


}

EINTERN void
e_config_descriptor_shutdown(void)
{
   E_CONFIG_DD_FREE(_e_config_edd);
   E_CONFIG_DD_FREE(_e_config_module_edd);
   E_CONFIG_DD_FREE(_e_config_font_default_edd);
   E_CONFIG_DD_FREE(_e_config_font_fallback_edd);
   E_CONFIG_DD_FREE(_e_config_theme_edd);
   E_CONFIG_DD_FREE(_e_config_path_append_edd);
   E_CONFIG_DD_FREE(_e_config_desktop_bg_edd);
   E_CONFIG_DD_FREE(_e_config_desklock_bg_edd);
   E_CONFIG_DD_FREE(_e_config_desktop_name_edd);
   E_CONFIG_DD_FREE(e_remember_edd);
   E_CONFIG_DD_FREE(_e_config_menu_applications_edd);
   E_CONFIG_DD_FREE(_e_config_gadcon_edd);
   E_CONFIG_DD_FREE(_e_config_gadcon_client_edd);
   E_CONFIG_DD_FREE(_e_config_shelf_edd);
   E_CONFIG_DD_FREE(_e_config_shelf_desk_edd);
   E_CONFIG_DD_FREE(_e_config_mime_icon_edd);
   E_CONFIG_DD_FREE(_e_config_syscon_action_edd);
   E_CONFIG_DD_FREE(_e_config_env_var_edd);
   E_CONFIG_DD_FREE(_e_config_xkb_layout_edd);
   E_CONFIG_DD_FREE(_e_config_xkb_option_edd);
   E_CONFIG_DD_FREE(_e_config_binding_edd);
   E_CONFIG_DD_FREE(_e_config_bindings_mouse_edd);
   E_CONFIG_DD_FREE(_e_config_bindings_key_edd);
   E_CONFIG_DD_FREE(_e_config_bindings_edge_edd);
   E_CONFIG_DD_FREE(_e_config_bindings_signal_edd);
   E_CONFIG_DD_FREE(_e_config_bindings_wheel_edd);
   E_CONFIG_DD_FREE(_e_config_bindings_acpi_edd);
}

EINTERN E_Config_DD *
e_config_descriptor_get(void)
{
   return _e_config_edd;
}

EINTERN E_Config_DD *
e_config_binding_descriptor_get(void)
{
   return _e_config_binding_edd;
}
