#ifndef E_WIDGET_DESKMIRROR_H
#define E_WIDGET_DESKMIRROR_H

EAPI Evas_Object *e_deskmirror_add(E_Desk *desk, Eina_Bool pager, Eina_Bool taskbar);
EAPI Evas_Object *e_deskmirror_mirror_find(Evas_Object *deskmirror, Evas_Object *comp_object);
EAPI Eina_List *e_deskmirror_mirror_list(Evas_Object *deskmirror);
EAPI Evas_Object *e_deskmirror_mirror_copy(Evas_Object *obj);
EAPI void e_deskmirror_coord_canvas_to_virtual(Evas_Object *obj, Evas_Coord cx, Evas_Coord cy, Evas_Coord *vx, Evas_Coord *vy);
EAPI void e_deskmirror_coord_virtual_to_canvas(Evas_Object *obj, Evas_Coord vx, Evas_Coord vy, Evas_Coord *cx, Evas_Coord *cy);
EAPI E_Desk *e_deskmirror_desk_get(Evas_Object *obj);
EAPI void e_deskmirror_util_wins_print(Evas_Object *obj);
//#define DESKMIRROR_TEST

#endif
