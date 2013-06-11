#ifdef E_TYPEDEFS

typedef struct _E_Shelf E_Shelf;

#else
# ifndef E_SHELF_H
#  define E_SHELF_H

#  define E_SHELF_TYPE 0xE0b0101e

struct _E_Shelf
{
   E_Object e_obj_inherit;

   int id;
   int x, y, w, h;
   int layer, size;

   const char *name, *style;

   E_Popup *popup;
   E_Zone *zone;
   E_Gadcon *gadcon;

   Ecore_Evas *ee;
   Evas *evas;
   Evas_Object *o_base, *o_event;

   E_Config_Shelf *cfg;

   Eina_Bool fit_along : 1;
   Eina_Bool fit_size : 1;
   Eina_Bool hidden : 1;
   Eina_Bool toggle : 1;
   Eina_Bool edge : 1;
   Eina_Bool urgent_show : 1;
   Eina_Bool locked : 1;
};

EINTERN int e_shelf_init(void);
EINTERN int e_shelf_shutdown(void);

EAPI void e_shelf_config_update(void);
EAPI E_Shelf *e_shelf_config_new(E_Zone *zone, E_Config_Shelf *cfg);
EAPI E_Shelf *e_shelf_zone_new(E_Zone *zone, const char *name, const char *style, int popup, int layer, int id);
EAPI void e_shelf_show(E_Shelf *es);
EAPI void e_shelf_hide(E_Shelf *es);
EAPI void e_shelf_toggle(E_Shelf *es, Eina_Bool show);
EAPI void e_shelf_populate(E_Shelf *es);
EAPI void e_shelf_position_calc(E_Shelf *es);
EAPI void e_shelf_move_resize(E_Shelf *es, int x, int y, int w, int h);
EAPI Eina_List *e_shelf_list(void);
EAPI const char *e_shelf_orient_string_get(E_Shelf *es);
EAPI void e_shelf_orient(E_Shelf *es, E_Gadcon_Orient orient);

# endif
#endif
