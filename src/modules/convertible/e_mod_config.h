//
// Created by raffaele on 01/08/19.
//
#include <e.h>
#include "e_mod_main.h"

#ifndef E_GADGET_CONVERTIBLE_E_MOD_CONFIG_H
#define E_GADGET_CONVERTIBLE_E_MOD_CONFIG_H

/* Increment for Major Changes */
#define MOD_CONFIG_FILE_EPOCH      1
/* Increment for Minor Changes (ie: user doesn't need a new config) */
#define MOD_CONFIG_FILE_GENERATION 0
#define MOD_CONFIG_FILE_VERSION    ((MOD_CONFIG_FILE_EPOCH * 1000000) + MOD_CONFIG_FILE_GENERATION)

// Definition of the data structure to hold the gadget configuration
typedef struct _Convertible_Config Convertible_Config;
struct _Convertible_Config
{
   int version;
   E_Module *module;
   int disable_keyboard_on_rotation;
};

// As far as I understand, this structure should hold data useful for the configuration and a pointer to
// another structure holding the configuration options
struct _E_Config_Dialog_Data
{
   Convertible_Config *config;
   Evas_Object *zones;
   Evas_Object *inputs;
};

E_Config_Dialog*
e_int_config_convertible_module(Evas_Object *comp, const char *p);
void
econvertible_config_init(void);
void
econvertible_config_shutdown(void);

#endif //E_GADGET_CONVERTIBLE_E_MOD_CONFIG_H
