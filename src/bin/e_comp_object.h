#ifdef E_TYPEDEFS
typedef struct E_Comp_Object_Frame E_Comp_Object_Frame;
typedef struct E_Event_Comp_Object E_Event_Comp_Object;
typedef void (*E_Comp_Object_Autoclose_Cb)(void *, Evas_Object *);
typedef Eina_Bool (*E_Comp_Object_Key_Cb)(void *, Ecore_Event_Key *);
typedef Eina_Bool (*E_Comp_Object_Mover_Cb) (void *data, Evas_Object *comp_object, const char *signal);

typedef struct E_Comp_Object_Mover E_Comp_Object_Mover;

typedef enum
{
   E_COMP_OBJECT_TYPE_NONE,
   E_COMP_OBJECT_TYPE_MENU,
   E_COMP_OBJECT_TYPE_POPUP,
   E_COMP_OBJECT_TYPE_LAST,
} E_Comp_Object_Type;

#else
#ifndef E_COMP_OBJECT_H
#define E_COMP_OBJECT_H

#define E_COMP_OBJECT_FRAME_RESHADOW "COMP_RESHADOW"

struct E_Event_Comp_Object
{
   Evas_Object *comp_object;
};

struct E_Comp_Object_Frame
{
   int l, r, t, b;
   Eina_Bool calc : 1; // inset has been calculated
};


extern EAPI int E_EVENT_COMP_OBJECT_ADD;

EAPI void e_comp_object_zoomap_set(Evas_Object *obj, Eina_Bool enabled);
EAPI Evas_Object *e_comp_object_client_add(E_Client *ec);
EAPI Evas_Object *e_comp_object_util_mirror_add(Evas_Object *obj);
EAPI Evas_Object *e_comp_object_util_add(Evas_Object *obj, E_Comp_Object_Type type);
EAPI E_Client *e_comp_object_client_get(Evas_Object *obj);
EAPI E_Zone *e_comp_object_util_zone_get(Evas_Object *obj);
EAPI void e_comp_object_util_del_list_append(Evas_Object *obj, Evas_Object *to_del);
EAPI void e_comp_object_util_del_list_remove(Evas_Object *obj, Evas_Object *to_del);
EAPI void e_comp_object_util_autoclose(Evas_Object *obj, E_Comp_Object_Autoclose_Cb del_cb, E_Comp_Object_Key_Cb cb, const void *data);
EAPI void e_comp_object_util_center(Evas_Object *obj);
EAPI void e_comp_object_util_center_on(Evas_Object *obj, Evas_Object *on);
EAPI void e_comp_object_util_center_pos_get(Evas_Object *obj, int *x, int *y);
EAPI void e_comp_object_util_fullscreen(Evas_Object *obj);
EAPI E_Comp_Object_Mover *e_comp_object_effect_mover_add(int pri, const char *sig, E_Comp_Object_Mover_Cb provider, const void *data);
EAPI void e_comp_object_effect_mover_del(E_Comp_Object_Mover *prov);

#include "e_comp_object.eo.legacy.h"
#endif
#endif

