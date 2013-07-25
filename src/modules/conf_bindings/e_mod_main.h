#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

E_Config_Dialog *e_int_config_acpibindings(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_keybindings(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_mousebindings(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_edgebindings(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_signalbindings(E_Comp *comp, const char *params);

/**
 * @addtogroup Optional_Conf
 * @{
 *
 * @defgroup Module_Conf_KeyBinding Key Bindings (Shortcuts) Configuration
 *
 * Configure global keyboard shortcuts.
 *
 * @see Module_Conf_MouseBinding
 * @see Module_Conf_EdgeBinding
 * @}
 */
#endif
