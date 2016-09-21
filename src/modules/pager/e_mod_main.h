#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H
#include "gadget/pager.h"

typedef struct _Config Config;
typedef struct _Config_Item Config_Item;

#define PAGER_RESIZE_NONE 0
#define PAGER_RESIZE_HORZ 1
#define PAGER_RESIZE_VERT 2
#define PAGER_RESIZE_BOTH 3

#define PAGER_DESKNAME_NONE 0
#define PAGER_DESKNAME_TOP 1
#define PAGER_DESKNAME_BOTTOM 2
#define PAGER_DESKNAME_LEFT 3
#define PAGER_DESKNAME_RIGHT 4

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init(E_Module *m);
E_API int e_modapi_shutdown(E_Module *m);
E_API int e_modapi_save(E_Module *m);

EINTERN void _config_pager_module(Config_Item *ci);

extern E_Module *module;
extern E_Config_Dialog *config_dialog;
extern Eina_List *instances, *shandlers;

/**
 * @addtogroup Optional_Gadgets
 * @{
 *
 * @defgroup Module_Pager Virtual Desktop Pager
 *
 * Shows the grid of virtual desktops and allows changing between
 * them.
 *
 * @}
 */
#endif
