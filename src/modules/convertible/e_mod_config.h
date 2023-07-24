//
// Created by raffaele on 01/08/19.
//
#include <e.h>
#include "e_mod_main.h"

#ifndef E_GADGET_CONVERTIBLE_E_MOD_CONFIG_H
#define E_GADGET_CONVERTIBLE_E_MOD_CONFIG_H

// Definition for a zone configuration
typedef struct _Convertible_Zone_Config Convertible_Zone_Config;

struct _Convertible_Zone_Config
{
   char *name;
   int follow_rotation;
};

// Definition of the data structure to hold the gadget configuration
typedef struct _Convertible_Config Convertible_Config;

struct _Convertible_Config
{
   int disable_keyboard_on_rotation;
//   Eina_List *rotatable_screen_configuration;
};

// As far as I understand, this structure should hold data useful for the configuration and a pointer to
// another structure holding the configuration options
struct _E_Config_Dialog_Data
{
   Convertible_Config *config;
   Evas_Object *zones;
   Evas_Object *inputs;
};

E_Config_Dialog* e_int_config_convertible_module(Evas_Object *comp, const char *p);
void econvertible_config_init();
void econvertible_config_shutdown();
//E_Config_Dialog* econvertible_config_init(Evas_Object *comp, const char*p);
void _menu_new(Instance *inst, Evas_Event_Mouse_Down *ev);
void _mouse_down_cb(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);

#endif //E_GADGET_CONVERTIBLE_E_MOD_CONFIG_H
