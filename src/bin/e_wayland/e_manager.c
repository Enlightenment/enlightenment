#include "e.h"

/* local function prototypes */

/* local variables */
static Eina_List *_managers = NULL;

EINTERN int 
e_manager_init(void)
{
   return 1;
}

EINTERN int 
e_manager_shutdown(void)
{
   E_FREE_LIST(_managers, e_object_del);

   return 1;
}

EAPI Eina_List *
e_manager_list(void)
{
   return _managers;
}
