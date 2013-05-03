#include "e.h"

/* local function prototypes */
static void _e_manager_cb_free(E_Manager *man);

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

EAPI E_Manager *
e_manager_new(E_Output *output, int num)
{
   E_Manager *man;

   /* try to create new manager object */
   if (!(man = E_OBJECT_ALLOC(E_Manager, E_MANAGER_TYPE, _e_manager_cb_free)))
     return NULL;

   /* set manager properties */
   man->num = num;
   man->x = output->x;
   man->y = output->y;
   man->w = output->w;
   man->h = output->h;

   /* add this manager to the list */
   _managers = eina_list_append(_managers, man);

   return man;
}

/* local functions */
static void 
_e_manager_cb_free(E_Manager *man)
{
   /* remove this manager from the list */
   _managers = eina_list_remove(_managers, man);

   /* free the object */
   E_FREE(man);
}
