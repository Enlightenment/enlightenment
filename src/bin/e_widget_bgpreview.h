#ifndef E_WIDGET_DESK_PREVIEW_H
#define E_WIDGET_DESK_PREVIEW_H

EAPI Evas_Object *e_widget_bgpreview_add(Evas *evas, int nx, int ny);
EAPI void e_widget_bgpreview_num_desks_set(Evas_Object *obj, int nx, int ny);
EAPI Evas_Object *e_widget_bgpreview_desk_add(Evas *e, E_Zone *zone, int x, int y);
EAPI void e_widget_bgpreview_desk_configurable_set(Evas_Object *obj, Eina_Bool enable);

#endif
