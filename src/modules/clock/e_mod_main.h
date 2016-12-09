#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "config_descriptor.h"

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);


void e_int_config_clock_module(Evas_Object *parent, Config_Item *ci);
void e_int_clock_instances_redo(Eina_Bool all);

extern Config *clock_config;


/**
 * @addtogroup Optional_Gadgets
 * @{
 *
 * @defgroup Module_Clock Clock
 *
 * Shows current time and date.
 *
 * @}
 */

#endif
