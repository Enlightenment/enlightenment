#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "e_comp_cfdata.h"

typedef struct _Mod    Mod;

struct _Mod
{
   E_Module        *module;

   E_Config_DD     *conf_edd;
   E_Config_DD     *conf_match_edd;
   E_Comp_Config   *conf;

   E_Config_Dialog *config_dialog;
};

extern Mod *_comp_mod;


#define ENGINE_SW 1
#define ENGINE_GL 2


/**
 * @addtogroup Optional_Look
 * @{
 *
 * @defgroup Module_Comp Comp (Composite Manager)
 *
 * Implements the X11 Composite Manager to support alpha blend,
 * semi-transparent windows and drop shadow. Does support animations
 * and effects such as coloring unfocused windows.
 *
 * @}
 */

#endif
