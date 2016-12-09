#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "config_descriptor.h"

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

void _ibar_config_update(Config_Item *ci);
void _config_ibar_module(Config_Item *ci);
extern Config *ibar_config;

/**
 * @addtogroup Optional_Gadgets
 * @{
 *
 * @defgroup Module_IBar IBar (Icon Launch Bar)
 *
 * Launches applications from an icon bar, usually placed on shelves.
 *
 * @}
 */

#endif
