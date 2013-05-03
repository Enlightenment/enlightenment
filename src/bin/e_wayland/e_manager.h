#ifdef E_TYPEDEFS

typedef struct _E_Manager E_Manager;

#else
# ifndef E_MANAGER_H
#  define E_MANAGER_H

#  define E_MANAGER_TYPE 0xE0b01008

struct _E_Manager
{
   E_Object e_obj_inherit;

   int num;
   Evas_Coord x, y, w, h;
   Eina_Bool visible : 1;

   Eina_List *containers;

   E_Pointer *pointer;
};

EINTERN int e_manager_init(void);
EINTERN int e_manager_shutdown(void);

EAPI Eina_List *e_manager_list(void);
EAPI E_Manager *e_manager_new(E_Output *output, int num);

# endif
#endif
