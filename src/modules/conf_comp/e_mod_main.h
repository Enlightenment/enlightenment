#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "e_comp_cfdata.h"

typedef struct _Mod    Mod;

struct _Mod
{
   E_Module        *module;

   E_Comp_Config   *conf;

   E_Config_Dialog *config_dialog;
   E_Config_Dialog *match_config_dialog;
};

extern Mod *_comp_mod;

typedef struct _E_Demo_Style_Item
{
   Evas_Object *preview;
   Evas_Object *frame;
   Evas_Object *livethumb;
   Evas_Object *layout;
   Evas_Object *border;
   Evas_Object *client;
} E_Demo_Style_Item;


/**
 * @addtogroup Optional_Conf
 * @{
 *
 * @defgroup Module_Conf_Comp Conf_Comp (Composite Settings)
 *
 * Change settings for the internal compositor.
 *
 * @}
 */
EINTERN Evas_Object *_style_selector(Evas *evas, const char **source);
EINTERN E_Config_Dialog *e_int_config_comp_module(E_Container *con, const char *params EINA_UNUSED);
EINTERN E_Config_Dialog *e_int_config_comp_match(E_Container *con, const char *params EINA_UNUSED);

#endif
