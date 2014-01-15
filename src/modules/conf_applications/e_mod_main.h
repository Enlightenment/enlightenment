#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

E_Config_Dialog *e_int_config_apps_favs(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_apps_add(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_apps_ibar(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_apps_ibar_other(E_Comp *comp, const char *path);
E_Config_Dialog *e_int_config_apps_startup(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_apps_restart(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_apps_desk_lock(E_Comp *comp, const char *params __UNUSED__);
E_Config_Dialog *e_int_config_apps_desk_unlock(E_Comp *comp, const char *params __UNUSED__);

E_Config_Dialog *e_int_config_defapps(E_Comp *comp, const char *params __UNUSED__);

E_Config_Dialog *e_int_config_apps_personal(E_Comp *comp, const char *params __UNUSED__);

E_Config_Dialog *e_int_config_deskenv(E_Comp *comp, const char *params __UNUSED__);

/**
 * @addtogroup Optional_Conf
 * @{
 *
 * @defgroup Module_Conf_Applications Applications Configuration
 *
 * Configure application icons (launchers ".desktop"), which
 * applications to use on start or restart of Enlightenment.
 *
 * @}
 */
#endif
