#ifdef E_TYPEDEFS

typedef struct _E_Container E_Container;

#else
# ifndef E_CONTAINER_H
#  define E_CONTAINER_H

#  define E_CONTAINER_TYPE 0xE0b01003

struct _E_Container
{
   E_Object e_obj_inherit;

   unsigned int num;
   Evas_Coord x, y, w, h;
   Eina_Bool visible : 1;

   Ecore_Wl_Window *win;
   E_Manager *man;
   Ecore_Evas *bg_ee;
   Evas *bg_evas;
   E_Pointer *ptr;

   Eina_List *zones;
};

EINTERN int e_container_init(void);
EINTERN int e_container_shutdown(void);

EAPI E_Container *e_container_new(E_Manager *man);
EAPI void e_container_show(E_Container *con);
EAPI void e_container_hide(E_Container *con);
EAPI void e_container_all_freeze(void);
EAPI void e_container_all_thaw(void);

# endif
#endif
