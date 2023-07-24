//
// Created by raffaele on 01/08/19.
//
#include "convertible_logging.h"
#include "e.h"
#include "e_mod_config.h"

static Convertible_Config *_config = NULL;
E_Config_DD *edd = NULL;
EINTERN Convertible_Config *convertible_config;

/**
 * Create the config structure
 * */
static void
_econvertible_config_dd_new(void)
{
//   c_zone = E_CONFIG_DD_NEW("Econvertible_Zone_Config", Convertible_Zone_Config);
//   E_CONFIG_VAL(c_zone, Convertible_Zone_Config, name, STR);
//   E_CONFIG_VAL(c_zone, Convertible_Zone_Config, follow_rotation, INT);

   // TODO Not sure what his line does. Apparently, it is needed to specify the type of the configuration data structure
   edd = E_CONFIG_DD_NEW("Convertible_Config", Convertible_Config);

   E_CONFIG_VAL(edd, Convertible_Config, disable_keyboard_on_rotation, INT);
//   E_CONFIG_LIST(edd, Convertible_Config, rotatable_screen_configuration, c_zone);
}

/**
 * Update the *_config data structure based on the settings coming from the dialog panel
 * @param config The config coming from the Dialog Panel (E_Config_Dialog_data)
 */
static void
_config_set(Convertible_Config *config)
{
   DBG("config_set disable_keyboard_on_rotation %d", config->disable_keyboard_on_rotation);
   _config->disable_keyboard_on_rotation = config->disable_keyboard_on_rotation;
   e_config_domain_save("module.convertible", edd, config);
}

/**
 * Initialise the configuration object
 *
 * @param cfg
 * @return
 */
static void*
_create_data(E_Config_Dialog *cfg EINA_UNUSED)
{
   E_Config_Dialog_Data *dialog_data;

   dialog_data = E_NEW(E_Config_Dialog_Data, 1);
   dialog_data->config = malloc(sizeof(Convertible_Config));
   dialog_data->config->disable_keyboard_on_rotation = _config->disable_keyboard_on_rotation;
//   dialog_data->config->rotatable_screen_configuration = _config->rotatable_screen_configuration;

   DBG("disable_keyboard_on_rotation %d", dialog_data->config->disable_keyboard_on_rotation);
   return dialog_data;
}

/**
 * Release memory for the data structure holding the configuration
 *
 * @param c
 * @param dialog_data
 */
static void
_free_data(E_Config_Dialog *c EINA_UNUSED, E_Config_Dialog_Data *dialog_data)
{
   free(dialog_data->config);
   free(dialog_data);
}

/**
 * This function should store the modified settings into the data structure referred by the pointer _config
 * @param cfd
 * @param cfdata
 * @return
 */
static int
_basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   DBG("_basic_apply_data");
   _config_set(cfdata->config);
   return 1;
}

/**
 * Create the panel by adding all the items like labels, checkbox and lists
 *
 * @param cfd
 * @param evas
 * @param cfdata
 * @return
 */
static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd EINA_UNUSED, Evas *evas,
                      E_Config_Dialog_Data *cfdata)
{
   Evas_Object *o, *evas_option_input_disable; //, *evas_label_section_screens; // *list_item_screen,
//   Evas_Object *screen_list = NULL;

   o = e_widget_list_add(evas, 0, 0);

   evas_option_input_disable = e_widget_check_add(evas, "Disable input when rotated", &(cfdata->config->disable_keyboard_on_rotation));
   e_widget_list_object_append(o, evas_option_input_disable, 0, 0, 0);

   DBG("After basic_create_widget");
   return o;
   }

/**
 * This function initialise the config dialog for the module
 * @param comp
 * @param p
 * @return
 */
E_Config_Dialog*
e_int_config_convertible_module(Evas_Object *comp, const char *p EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   if (e_config_dialog_find("E", "windows/convertible"))
      return NULL;

   v = E_NEW(E_Config_Dialog_View, 1);
   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply_data;
   v->basic.create_widgets = _basic_create_widgets;

   cfd = e_config_dialog_new(comp,
                             "Convertible Configuration",
                             "E", "windows/convertible",
                             NULL,
                             0, v, NULL);
   return cfd;
}

void
econvertible_config_init()
{
   _econvertible_config_dd_new();
   _config = e_config_domain_load("module.econvertible", edd);
   if (!_config)
   {
      _config = E_NEW(Convertible_Config, 1);
      _config->disable_keyboard_on_rotation = 1;
//      _config->rotatable_screen_configuration = NULL;
   }

   DBG("Config loaded");
}

void econvertible_config_shutdown()
{
   E_CONFIG_DD_FREE(edd);
   E_FREE(convertible_config);
}