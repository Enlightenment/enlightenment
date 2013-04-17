#ifdef E_TYPEDEFS
#else
#ifndef E_ZOOMAP_H
#define E_ZOOMAP_H

EAPI Evas_Object *e_zoomap_add            (Evas *evas);
EAPI void         e_zoomap_child_set      (Evas_Object *obj, Evas_Object *child);
EAPI Evas_Object *e_zoomap_child_get      (Evas_Object *obj);
EAPI void         e_zoomap_smooth_set     (Evas_Object *obj, Eina_Bool smooth);
EAPI Eina_Bool    e_zoomap_smooth_get     (Evas_Object *obj);
EAPI void         e_zoomap_solid_set      (Evas_Object *obj, Eina_Bool solid);
EAPI Eina_Bool    e_zoomap_solid_get      (Evas_Object *obj);
EAPI void         e_zoomap_always_set     (Evas_Object *obj, Eina_Bool always);
EAPI Eina_Bool    e_zoomap_always_get     (Evas_Object *obj);
EAPI void         e_zoomap_child_resize(Evas_Object *zoomap, int w, int h);
EAPI void         e_zoomap_child_edje_solid_setup(Evas_Object *obj);
#endif
#endif
