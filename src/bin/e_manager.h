#ifdef E_TYPEDEFS

typedef struct _E_Manager             E_Manager;

#else
#ifndef E_MANAGER_H
#define E_MANAGER_H

#define E_MANAGER_TYPE (int) 0xE0b01008

extern E_API int E_EVENT_MANAGER_KEYS_GRAB;

struct _E_Manager
{
   E_Object             e_obj_inherit;

   int                  num;
   int                  x, y, w, h;
   Ecore_Window        root;
   Eina_List           *handlers;

   E_Comp             *comp;

   Eina_Bool            visible : 1;
};

EINTERN int        e_manager_init(void);
EINTERN int        e_manager_shutdown(void);
E_API Eina_List *e_manager_list(void);

E_API E_Manager      *e_manager_new(Ecore_Window root, E_Comp *c, int w, int h);
E_API void            e_manager_manage_windows(E_Manager *man);
E_API void            e_manager_resize(E_Manager *man, int w, int h);
E_API E_Manager      *e_manager_current_get(void);
E_API E_Manager      *e_manager_number_get(int num);

E_API void            e_managers_keys_grab(void);
E_API void            e_managers_keys_ungrab(void);

static inline E_Manager *
e_manager_find_by_root(Ecore_Window root)
{
   Eina_List *l;
   E_Manager *man;

   EINA_LIST_FOREACH(e_manager_list(), l, man)
     if (root == man->root) return man;
   return NULL;
}

#endif
#endif
