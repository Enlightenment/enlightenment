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

EAPI E_Manager *
e_manager_current_get(void)
{
   Eina_List *l;
   E_Manager *man;
   Evas_Coord x, y;

   if (!_managers) return NULL;

   /* get the current mouse position */
   ecore_wl_pointer_xy_get(&x, &y);

   /* loop the list of managers */
   EINA_LIST_FOREACH(_managers, l, man)
     {
        /* test if the mouse is inside this manager */
        if (E_INSIDE(x, y, man->x, man->y, man->w, man->h))
          return man;
     }

   /* return the current manager pointed to by the list */
   return eina_list_data_get(_managers);
}

EAPI void 
e_manager_show(E_Manager *man)
{
   /* check for valid manager */
   E_OBJECT_CHECK(man);
   E_OBJECT_TYPE_CHECK(man, E_MANAGER_TYPE);

   /* check for already visible */
   if (man->visible) return;

   /* TODO: show containers */

   man->visible = EINA_TRUE;
}

EAPI void 
e_manager_hide(E_Manager *man)
{
   /* check for valid manager */
   E_OBJECT_CHECK(man);
   E_OBJECT_TYPE_CHECK(man, E_MANAGER_TYPE);

   /* check for already invisible */
   if (!man->visible) return;

   /* TODO: hide containers */

   man->visible = EINA_FALSE;
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
