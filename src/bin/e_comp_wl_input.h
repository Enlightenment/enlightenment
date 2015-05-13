#ifdef E_TYPEDEFS
#else
# ifndef E_COMP_WL_INPUT_H
#  define E_COMP_WL_INPUT_H

EINTERN Eina_Bool e_comp_wl_input_init(E_Comp_Data *cdata);
EINTERN void e_comp_wl_input_shutdown(E_Comp_Data *cdata);
EINTERN Eina_Bool e_comp_wl_input_pointer_check(struct wl_resource *res);
EINTERN Eina_Bool e_comp_wl_input_keyboard_check(struct wl_resource *res);
EINTERN Eina_Bool e_comp_wl_input_touch_check(struct wl_resource *res);

EINTERN void e_comp_wl_input_keyboard_modifiers_serialize(E_Comp_Data *cdata);
EINTERN void e_comp_wl_input_keyboard_modifiers_update(E_Comp_Data *cdata);
EINTERN void e_comp_wl_input_keyboard_state_update(E_Comp_Data *cdata, uint32_t keycode, Eina_Bool pressed);
EINTERN void e_comp_wl_input_keyboard_enter_send(E_Client *client);

E_API void e_comp_wl_input_pointer_enabled_set(Eina_Bool enabled);
E_API void e_comp_wl_input_keyboard_enabled_set(Eina_Bool enabled);
E_API void e_comp_wl_input_touch_enabled_set(Eina_Bool enabled);

E_API void e_comp_wl_input_keymap_set(E_Comp_Data *cdata, const char *rules, const char *model, const char *layout);

# endif
#endif
