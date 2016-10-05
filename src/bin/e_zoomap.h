#ifdef E_TYPEDEFS
#else
#ifndef E_ZOOMAP_H
#define E_ZOOMAP_H

E_API Evas_Object *e_zoomap_add            (Evas *evas);
E_API void         e_zoomap_child_set      (Evas_Object *obj, Evas_Object *child);
E_API Evas_Object *e_zoomap_child_get      (Evas_Object *obj);
E_API void         e_zoomap_smooth_set     (Evas_Object *obj, Eina_Bool smooth);
E_API Eina_Bool    e_zoomap_smooth_get     (Evas_Object *obj);
E_API void         e_zoomap_solid_set      (Evas_Object *obj, Eina_Bool solid);
E_API Eina_Bool    e_zoomap_solid_get      (Evas_Object *obj);
E_API void         e_zoomap_always_set     (Evas_Object *obj, Eina_Bool always);
E_API Eina_Bool    e_zoomap_always_get     (Evas_Object *obj);
E_API void         e_zoomap_child_resize(Evas_Object *zoomap, int w, int h);
E_API void         e_zoomap_child_edje_solid_setup(Evas_Object *obj);
E_API void         e_zoomap_viewport_source_set(Evas_Object *obj, int x, int y, int w, int h);
E_API void         e_zoomap_viewport_destination_set(Evas_Object *obj, int w, int h);
E_API void         e_zoomap_viewport_set(Evas_Object *obj, Eina_Bool enabled);
E_API void         e_zoomap_scale_set(Evas_Object *obj, int32_t scale);
E_API void         e_zoomap_transform_set(Evas_Object *obj, uint32_t transform);
E_API void         e_zoomap_commit(Evas_Object *obj);

#endif
#endif
