#include "e.h"

typedef struct _E_Widget_Data E_Widget_Data;
struct _E_Widget_Data
{
   Evas_Object *obj;
   Evas_Object *o_frame, *o_table;
};

static void _e_wid_del_hook(Evas_Object *obj);
static void _e_wid_disable_hook(Evas_Object *obj);
static void
_size_hint(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   int w, h;
   E_Widget_Data *wd = data;

   edje_object_size_min_calc(wd->o_frame, &w, &h);
   e_widget_size_min_set(wd->obj, w, h);
}
/* local subsystem functions */

/* externally accessible functions */
EAPI Evas_Object *
e_widget_frametable_add(Evas *evas, const char *label, int homogenous)
{
   Evas_Object *obj, *o;
   E_Widget_Data *wd;
   Evas_Coord mw, mh;

   obj = e_widget_add(evas);

   e_widget_del_hook_set(obj, _e_wid_del_hook);
   e_widget_disable_hook_set(obj, _e_wid_disable_hook);
   wd = calloc(1, sizeof(E_Widget_Data));
   e_widget_data_set(obj, wd);
   wd->obj = obj;

   o = edje_object_add(e_win_evas_win_get(evas));
   wd->o_frame = o;
   e_theme_edje_object_set(o, "base/theme/widgets",
                           "e/widgets/frame");
   edje_object_part_text_set(o, "e.text.label", label);
   evas_object_show(o);
   e_widget_sub_object_add(obj, o);
   e_widget_resize_object_set(obj, o);

   o = elm_table_add(e_win_evas_win_get(evas));
   evas_object_event_callback_add(o, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _size_hint, wd);
   wd->o_table = o;
   elm_table_homogeneous_set(o, homogenous);
   edje_object_part_swallow(wd->o_frame, "e.swallow.content", o);
   e_widget_sub_object_add(obj, o);
   evas_object_show(o);

   edje_object_size_min_calc(wd->o_frame, &mw, &mh);
   e_widget_size_min_set(obj, mw, mh);

   return obj;
}

EAPI void
e_widget_frametable_object_append(Evas_Object *obj, Evas_Object *sobj, int col, int row, int colspan, int rowspan, int fill_w, int fill_h, int expand_w, int expand_h)
{
   e_widget_frametable_object_append_full(obj, sobj, col, row, colspan, rowspan, fill_w, fill_h, expand_w, expand_h, 0.5, 0.5, -1, -1, -1, -1);
}

EAPI void
e_widget_frametable_object_append_full(Evas_Object *obj, Evas_Object *sobj, int col, int row, int colspan, int rowspan, int fill_w, int fill_h, int expand_w, int expand_h, double align_x, double align_y, Evas_Coord min_w, Evas_Coord min_h, Evas_Coord max_w, Evas_Coord max_h)
{
   E_Widget_Data *wd = e_widget_data_get(obj);
   Evas_Coord mw = 0, mh = 0;

   if ((min_w > 0) || (min_h > 0))
     evas_object_size_hint_min_set(sobj, min_w, min_h);
   if ((max_w > 0) || (max_h > 0))
     evas_object_size_hint_max_set(sobj, max_w, max_h);
   if (fill_w) align_x = -1;
   if (fill_h) align_y = -1;
   E_ALIGN(sobj, align_x, align_y);
   E_WEIGHT(sobj, expand_w, expand_h);
   elm_table_pack(wd->o_table, sobj, col, row, colspan, rowspan);
   evas_object_smart_need_recalculate_set(wd->o_table, 1);
   evas_object_smart_calculate(wd->o_table);
   edje_object_part_swallow(wd->o_frame, "e.swallow.content", wd->o_table);
   edje_object_size_min_calc(wd->o_frame, &mw, &mh);
   e_widget_size_min_set(obj, mw, mh);
   e_widget_sub_object_add(obj, sobj);
   evas_object_show(sobj);
}

EAPI void
e_widget_frametable_object_repack(Evas_Object *obj, Evas_Object *sobj, int col, int row, int colspan, int rowspan, int fill_w, int fill_h, int expand_w, int expand_h)
{
   E_Widget_Data *wd;
   Evas_Coord mw = 0, mh = 0;

   wd = e_widget_data_get(obj);

   if (fill_w || fill_h)
     E_ALIGN(sobj, fill_w ? -1 : 0.5, fill_h ? -1 : 0.5);
   E_WEIGHT(sobj, expand_w, expand_h);
   elm_table_pack_set(sobj, col, row, colspan, rowspan);
   evas_object_smart_need_recalculate_set(wd->o_table, 1);
   evas_object_smart_calculate(wd->o_table);
   edje_object_size_min_calc(wd->o_frame, &mw, &mh);
   e_widget_size_min_set(obj, mw, mh);
}

EAPI void
e_widget_frametable_content_align_set(Evas_Object *obj, double halign, double valign)
{
   E_Widget_Data *wd;

   wd = e_widget_data_get(obj);
   elm_table_align_set(wd->o_table, halign, valign);
}

EAPI void
e_widget_frametable_label_set(Evas_Object *obj, const char *label)
{
   E_Widget_Data *wd;

   wd = e_widget_data_get(obj);
   edje_object_part_text_set(wd->o_frame, "e.text.label", label);
}

static void
_e_wid_del_hook(Evas_Object *obj)
{
   E_Widget_Data *wd;

   wd = e_widget_data_get(obj);
   free(wd);
}

static void
_e_wid_disable_hook(Evas_Object *obj)
{
   E_Widget_Data *wd;

   wd = e_widget_data_get(obj);
   if (e_widget_disabled_get(obj))
     edje_object_signal_emit(wd->o_frame, "e,state,disabled", "e");
   else
     edje_object_signal_emit(wd->o_frame, "e,state,enabled", "e");
}

