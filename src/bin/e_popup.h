#ifdef E_TYPEDEFS

typedef struct _E_Popup E_Popup;
typedef Eina_Bool (*E_Popup_Key_Cb)(void *, Ecore_Event_Key *);

#else
#ifndef E_POPUP_H
#define E_POPUP_H

#define E_POPUP_TYPE 0xE0b0100e

struct _E_Popup
{
   E_Object             e_obj_inherit;

   int                  x, y, w, h, zx, zy;
   E_Comp_Canvas_Layer comp_layer;
   E_Layer layer;

   Evas                *evas;
   E_Comp_Win         *cw;
   E_Zone              *zone;
   Ecore_Evas         *ecore_evas;
   E_Container_Shape *shape;
   Evas_Object        *content;
   Eina_List          *objects;
   Eina_Stringshare  *name;
   E_Popup_Key_Cb    key_cb;
   void               *key_data;

   Eina_Bool           visible : 1;
   Eina_Bool           ignore_events : 1;
   Eina_Bool           autoclose : 1;
};

EINTERN int         e_popup_init(void);
EINTERN int         e_popup_shutdown(void);

EAPI E_Popup    *e_popup_new(E_Zone *zone, int x, int y, int w, int h);
EAPI void        e_popup_show(E_Popup *pop);
EAPI void        e_popup_hide(E_Popup *pop);
EAPI void        e_popup_move(E_Popup *pop, int x, int y);
EAPI void        e_popup_resize(E_Popup *pop, int w, int h);
EAPI void        e_popup_content_set(E_Popup *pop, Evas_Object *obj);
EAPI void        e_popup_move_resize(E_Popup *pop, int x, int y, int w, int h);
EAPI void        e_popup_ignore_events_set(E_Popup *pop, int ignore);
EAPI void        e_popup_layer_set(E_Popup *pop, E_Comp_Canvas_Layer comp_layer, E_Layer layer);
EAPI void        e_popup_name_set(E_Popup *pop, const char *name);
EAPI void        e_popup_object_add(E_Popup *pop, Evas_Object *obj);
EAPI void        e_popup_autoclose(E_Popup *pop, E_Popup_Key_Cb cb, const void *data);
#endif
#endif
