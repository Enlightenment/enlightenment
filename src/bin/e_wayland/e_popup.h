#ifdef E_TYPEDEFS

typedef struct _E_Popup E_Popup;

#else
# ifndef E_POPUP_H
#  define E_POPUP_H

#  define E_POPUP_TYPE 0xE0b0100e

struct _E_Popup
{
   E_Object e_obj_inherit;

   int x, y, w, h;
   int layer;
   const char *name;

   Ecore_Evas *ee;
   Evas *evas;
   E_Zone *zone;

   Eina_Bool visible : 1;
   Eina_Bool shaped : 1;
};

EINTERN int e_popup_init(void);
EINTERN int e_popup_shutdown(void);

EAPI E_Popup *e_popup_new(E_Zone *zone, int x, int y, int w, int h);
EAPI void e_popup_show(E_Popup *pop);
EAPI void e_popup_hide(E_Popup *pop);
EAPI void e_popup_move(E_Popup *pop, int x, int y);
EAPI void e_popup_resize(E_Popup *pop, int w, int h);
EAPI void e_popup_move_resize(E_Popup *pop, int x, int y, int w, int h);
EAPI void e_popup_layer_set(E_Popup *pop, int layer);
EAPI void e_popup_name_set(E_Popup *pop, const char *name);
EAPI void e_popup_ignore_events_set(E_Popup *pop, Eina_Bool ignore);
EAPI void e_popup_edje_bg_object_set(E_Popup *pop, Evas_Object *obj);
EAPI void e_popup_idler_before(void);
EAPI E_Popup *e_popup_find_by_window(Ecore_Wl_Window *win);

# endif
#endif
