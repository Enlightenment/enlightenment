#ifdef E_TYPEDEFS
#else
# ifndef E_SURFACE_H
#  define E_SURFACE_H

EAPI Evas_Object *e_surface_add(Evas *evas);
EAPI void e_surface_input_set(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
EAPI void e_surface_damage_add(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
EAPI void e_surface_image_set(Evas_Object *obj, Evas_Coord w, Evas_Coord h, void *pixels);
EAPI void e_surface_border_input_set(Evas_Object *obj, E_Client *ec);

# endif
#endif
