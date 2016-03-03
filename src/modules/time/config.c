#include "clock.h"
#include <time.h>

static const char *datecfg[] =
{
   N_("None"),
   N_("Full"),
   N_("Numeric"),
   N_("Date-only"),
   N_("ISO 8601"),
   N_("Custom"),
};


static void
_config_rect_click(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Up *ev = event_info;
   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   evas_object_del(data);
}

static Evas_Object *
_config_autoclose_rect_add(Evas_Object *obj)
{
   Evas_Object *rect;

   rect = evas_object_rectangle_add(e_comp->evas);
   e_comp_object_util_fullscreen(rect);
   evas_object_color_set(rect, 0, 0, 0, 0);
   evas_object_layer_set(rect, E_LAYER_MENU - 1);
   evas_object_show(rect);
   evas_object_event_callback_add(rect, EVAS_CALLBACK_MOUSE_UP, _config_rect_click, obj);
   return rect;
}

static void
_config_close(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   time_config->config_dialog = NULL;
}

static void
_config_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   time_config_update(data);
}

static void
_clock_color_dismissed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_del(obj);
}

static void
_config_color_reset(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   Evas_Object *cs;
   int num;

   num = !evas_object_data_get(obj, "bg_color");
   if (ci->colorclass[num])
     {
        elm_config_color_overlay_unset(ci->colorclass[num]);
        edje_color_class_del(ci->colorclass[num]);
     }
   eina_stringshare_replace(&ci->colorclass[num], NULL);
   cs = evas_object_data_get(obj, "colorselector");
   elm_colorselector_color_set(cs, 0, 0, 0, 0);
   elm_colorselector_palette_item_color_set(evas_object_data_get(cs, "colorselector_it"),
     0, 0, 0, 0);
   time_config_update(data);
}

static void
_config_color_change(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   int r, g, b, a;
   int num;
   char buf[1024];

   num = !evas_object_data_get(obj, "bg_color");
   elm_colorselector_color_get(obj, &r, &g, &b, &a);

   if (!ci->colorclass[num])
     {
        snprintf(buf, sizeof(buf), "e.clock_color_%s.%d", num ? "fg" : "bg", ci->id);
        eina_stringshare_replace(&ci->colorclass[num], buf);
     }
   elm_config_color_overlay_set(ci->colorclass[num], r, g, b, a, 0, 0, 0, 0, 0, 0, 0, 0);
   edje_color_class_set(ci->colorclass[num], r, g, b, a, 0, 0, 0, 0, 0, 0, 0, 0);
   elm_colorselector_palette_item_color_set(evas_object_data_get(obj, "colorselector_it"), r, g, b, a);
   time_config_update(data);
}

static void
_config_color_setup(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   Evas_Object *cs, *ctx, *bx, *bt, *rect;
   int r, g, b, a, x, y;
   Eina_Bool bg;
   const char *ccname, *ccnames[] =
   {
      "e.clock_color_bg",
      "e.clock_color_fg",
   };

   bg = !!evas_object_data_get(obj, "bg_color");
   ccname = ci->colorclass[!bg];
   if (!ccname) ccname = ccnames[!bg];
   edje_color_class_get(ccname, &r, &g, &b, &a,
     NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

   bx = elm_box_add(obj);
   E_FILL(bx);
   evas_object_show(bx);

   cs = elm_colorselector_add(obj);
   evas_object_data_set(cs, "colorselector_bt", evas_object_data_get(obj, "colorselector_tt"));
   evas_object_data_set(cs, "bg_color", (void*)(long)bg);
   evas_object_smart_callback_add(cs, "changed,user", _config_color_change, ci);
   elm_colorselector_mode_set(cs, ELM_COLORSELECTOR_COMPONENTS);
   elm_colorselector_color_set(cs, r, g, b, a);
   E_FILL(cs);
   elm_box_pack_end(bx, cs);
   evas_object_show(cs);

   bt = elm_button_add(bx);
   evas_object_data_set(bt, "colorselector", cs);
   evas_object_data_set(bt, "bg_color", (void*)(long)bg);
   elm_object_text_set(bt, _("Reset"));
   evas_object_smart_callback_add(bt, "clicked", _config_color_reset, ci);
   evas_object_show(bt);
   elm_box_pack_end(bx, bt);

   /* size hints: the final frontier */
   rect = evas_object_rectangle_add(e_comp->elm);
   evas_object_geometry_get(time_config->config_dialog, NULL, NULL, &x, &y);
   evas_object_size_hint_min_set(rect, x - 10, 1);
   e_comp_object_util_del_list_append(bx, rect);
   elm_box_pack_end(bx, rect);

   ctx = elm_ctxpopup_add(obj);
   elm_ctxpopup_hover_parent_set(ctx, e_comp->elm);
   evas_object_layer_set(ctx, E_LAYER_MENU);
   elm_object_style_set(ctx, "noblock");
   e_comp_object_util_del_list_append(ctx, _config_autoclose_rect_add(ctx));
   evas_object_smart_callback_add(ctx, "dismissed", _clock_color_dismissed, NULL);
   elm_object_content_set(ctx, bx);
   evas_pointer_canvas_xy_get(e_comp->evas, &x, &y);
   evas_object_move(ctx, x, y);
   evas_object_show(ctx);
}

static void
_config_digital_timestr_update(Config_Item *ci, Evas_Object *obj, int idx)
{
   const char *str, *p;
   char seconds[] =
   {
    'S',
    's',
    'r',
    'T',
   };
   unsigned int i;

   str = elm_entry_entry_get(obj);
   eina_stringshare_replace(&ci->time_str[idx], str);
   ci->show_seconds = 0;
   for (p = strchr(str, '%'); p; p = strchr(p + 1, '%'))
     {
        for (i = 0; i < EINA_C_ARRAY_LENGTH(seconds); i++)
          if (p[1] == seconds[i])
            {
               ci->show_seconds = 1;
               time_config_update(ci);
               return;
            }
     }

   time_config_update(ci);
}

static void
_config_digital_datestr_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   _config_digital_timestr_update(data, obj, 1);
}

static Evas_Object *
_config_date_custom(Config_Item *ci, Evas_Object *bx)
{
   Evas_Object *o;

   o = elm_entry_add(bx);
   elm_entry_single_line_set(o, 1);
   elm_object_tooltip_text_set(o, _("strftime() format string"));
   elm_entry_entry_set(o, ci->time_str[1]);
   evas_object_smart_callback_add(o, "changed,user", _config_digital_datestr_changed, ci);
   E_FILL(o);
   E_EXPAND(o);
   evas_object_show(o);
   elm_box_pack_end(bx, o);
   return o;
}

static void
_config_date_changed(void *data, Evas_Object *obj, void *event_info)
{
   Config_Item *ci = data;
   Evas_Object *bx = elm_object_parent_widget_get(obj);
   Eina_Bool custom;

   custom = ci->show_date == CLOCK_DATE_DISPLAY_CUSTOM;
   ci->show_date = (intptr_t)elm_object_item_data_get(event_info);
   if (custom)
     {
        elm_box_unpack(bx, obj);
        elm_box_clear(bx);
        E_FILL(obj);
        E_EXPAND(obj);
        elm_box_pack_end(bx, obj);
     }
   else if (ci->show_date == CLOCK_DATE_DISPLAY_CUSTOM)
     {
        E_WEIGHT(obj, 0, 0);
        E_ALIGN(obj, 0, 0.5);
        elm_object_focus_set(_config_date_custom(ci, bx), 1);
     }
   time_config_update(ci);
}

static void
_config_weekend_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Config_Item *ci = data;

   ci->weekend.start = (intptr_t)elm_object_item_data_get(event_info);
   time_config_update(ci);
}

static void
_config_weekend_end_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Config_Item *ci = data;
   int end;

   end = (intptr_t)elm_object_item_data_get(event_info);
   if (end < ci->weekend.start) end += 7;
   ci->weekend.len = end - ci->weekend.start + 1;
   time_config_update(ci);
}

static void
_config_date_populate(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   unsigned int i;

   elm_hoversel_clear(obj);
   for (i = 0; i <= 5; i++)
     if (ci->show_date != i)
       elm_hoversel_item_add(obj, datecfg[i], NULL, ELM_ICON_NONE, NULL, (uintptr_t*)(unsigned long)i);
}

static void
_config_weekend_populate(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   char daynames[7][64];
   struct tm tm;
   int i;

   memset(&tm, 0, sizeof(struct tm));
   for (i = 0; i < 7; i++)
     {
        tm.tm_wday = i;
        strftime(daynames[i], sizeof(daynames[i]), "%A", &tm);
     }

   elm_hoversel_clear(obj);
   for (i = ci->weekend.start + 1; i <= 6; i++)
     if (ci->weekend.start != i)
       elm_hoversel_item_add(obj, daynames[i], NULL, ELM_ICON_NONE, NULL, (intptr_t*)(long)i);
   for (i = 0; i < ci->weekend.start; i++)
     elm_hoversel_item_add(obj, daynames[i], NULL, ELM_ICON_NONE, NULL, (intptr_t*)(long)i);
}

static void
_config_weekend_end_populate(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   char daynames[7][64];
   struct tm tm;
   int i, end;

   memset(&tm, 0, sizeof(struct tm));
   for (i = 0; i < 7; i++)
     {
        tm.tm_wday = i;
        strftime(daynames[i], sizeof(daynames[i]), "%A", &tm);
     }

   elm_hoversel_clear(obj);
   end = (ci->weekend.start + ci->weekend.len - 1) % 7;
   for (i = end + 1; i <= 6; i++)
     if (end != i)
       elm_hoversel_item_add(obj, daynames[i], NULL, ELM_ICON_NONE, NULL, (intptr_t*)(long)i);
   for (i = 0; i < end; i++)
     elm_hoversel_item_add(obj, daynames[i], NULL, ELM_ICON_NONE, NULL, (intptr_t*)(long)i);
}

static void
_config_timezone_setup(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evas_Object *hover, *gl;

   hover = elm_hover_add(e_comp->elm);
   evas_object_layer_set(hover, E_LAYER_MENU);
   elm_hover_parent_set(hover, elm_object_parent_widget_get(obj));
   elm_hover_target_set(hover, elm_object_parent_widget_get(obj));

   gl = elm_genlist_add(hover);
   evas_object_layer_set(gl, E_LAYER_MENU);
   evas_object_data_set(gl, "config_item", data);
   evas_object_data_set(gl, "button", obj);
   elm_genlist_mode_set(gl, ELM_LIST_COMPRESS);
   elm_genlist_homogeneous_set(gl, 1);
   elm_scroller_bounce_set(gl, 0, 0);
   evas_object_show(gl);
   elm_object_part_content_set(hover, "middle", gl);
   time_zoneinfo_scan(gl);
   e_comp_object_util_del_list_append(gl, hover);
   e_comp_object_util_del_list_append(gl, _config_autoclose_rect_add(gl));
   evas_object_show(hover);
}

static void
_config_digital_timestr_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   _config_digital_timestr_update(data, obj, 0);
}

static void
_config_label_add(Evas_Object *tb, const char *txt, int row)
{
   Evas_Object *o;

   o = elm_label_add(tb);
   E_ALIGN(o, 0, 0.5);
   elm_object_text_set(o, txt);
   evas_object_show(o);
   elm_table_pack(tb, o, 0, row, 1, 1);
}

static void
_config_digital_rows_setup(Config_Item *ci, Evas_Object *tb)
{
   int row = 1;
   Evas_Object *o;

   evas_object_del(elm_table_child_get(tb, 0, 1));
   evas_object_del(elm_table_child_get(tb, 1, 1));
   evas_object_del(elm_table_child_get(tb, 0, 2));
   evas_object_del(elm_table_child_get(tb, 1, 2));
   if (ci->advanced)
     {
        _config_label_add(tb, _("Time string:"), row);
        o = elm_entry_add(tb);
        E_FILL(o);
        evas_object_show(o);
        elm_entry_single_line_set(o, 1);
        elm_entry_entry_set(o, ci->time_str[0]);
        elm_object_focus_set(o, 1);
        evas_object_smart_callback_add(o, "changed,user", _config_digital_timestr_changed, ci);
        elm_table_pack(tb, o, 1, row++, 1, 1);

        o = elm_separator_add(tb);
        E_EXPAND(o);
        E_FILL(o);
        elm_separator_horizontal_set(o, 1);
        evas_object_show(o);
        elm_table_pack(tb, o, 0, row++, 2, 1);
        return;
     }
   if (ci->digital_clock)
     {
        _config_label_add(tb, _("24-hour Display:"), row);
        o = elm_check_add(tb);
        E_FILL(o);
        evas_object_show(o);
        elm_object_style_set(o, "toggle");
        elm_object_part_text_set(o, "on", "On");
        elm_object_part_text_set(o, "off", "Off");
        elm_check_state_pointer_set(o, &ci->digital_24h);
        evas_object_smart_callback_add(o, "changed", _config_changed, ci);
        elm_table_pack(tb, o, 1, row++, 1, 1);
     }
     
   _config_label_add(tb, _("Show Seconds:"), row);
   o = elm_check_add(tb);
   E_FILL(o);
   evas_object_show(o);
   elm_object_style_set(o, "toggle");
   elm_object_part_text_set(o, "on", _("On"));
   elm_object_part_text_set(o, "off", _("Off"));
   elm_check_state_pointer_set(o, &ci->show_seconds);
   evas_object_smart_callback_add(o, "changed", _config_changed, ci);
   elm_table_pack(tb, o, 1, row++, 1, 1);
}

static void
_config_advanced_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;

   _config_digital_rows_setup(data, evas_object_data_get(obj, "table"));
   time_config_update(ci);
}

EINTERN Evas_Object *
config_clock(Config_Item *ci)
{
   Evas_Object *popup, *tb, *o, *bx;
   int i, row = 0;
   char daynames[7][64];
   struct tm tm;

   memset(&tm, 0, sizeof(struct tm));
   for (i = 0; i < 7; i++)
     {
        tm.tm_wday = i;
        strftime(daynames[i], sizeof(daynames[i]), "%A", &tm);
     }
   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   if (ci->digital_clock)
     {
        _config_label_add(tb, _("Mode"), row);
        o = elm_check_add(tb);
        E_FILL(o);
        evas_object_show(o);
        elm_object_style_set(o, "toggle");
        elm_object_part_text_set(o, "on", _("Advanced"));
        elm_object_part_text_set(o, "off", _("Simple"));
        elm_check_state_pointer_set(o, &ci->advanced);
        evas_object_smart_callback_add(o, "changed", _config_advanced_changed, ci);
        evas_object_data_set(o, "table", tb);
        elm_table_pack(tb, o, 1, row++, 1, 1);
     }
   _config_digital_rows_setup(ci, tb);
   row = 3;

   _config_label_add(tb, _("Date Display:"), row);
   bx = elm_box_add(tb);
   elm_box_horizontal_set(bx, 1);
   evas_object_show(bx);
   elm_table_pack(tb, bx, 1, row++, 1, 1);
   E_FILL(bx);
   E_EXPAND(bx);
   o = elm_hoversel_add(tb);
   elm_box_pack_end(bx, o);
   elm_hoversel_hover_parent_set(o, popup);
   elm_hoversel_auto_update_set(o, 1);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "clicked", _config_date_populate, ci);
   evas_object_smart_callback_add(o, "selected", _config_date_changed, ci);
   elm_object_text_set(o, datecfg[ci->show_date]);
   if (ci->show_date == CLOCK_DATE_DISPLAY_CUSTOM)
     {
        E_ALIGN(o, 0, 0.5);
        E_WEIGHT(o, 0, 0);
        o = _config_date_custom(ci, bx);
     }
   else
     {
        E_FILL(o);
        E_EXPAND(o);
     }

   _config_label_add(tb, _("Weekend Start:"), row);
   o = elm_hoversel_add(tb);
   E_FILL(o);
   elm_hoversel_hover_parent_set(o, popup);
   elm_hoversel_auto_update_set(o, 1);
   evas_object_show(o);
   elm_table_pack(tb, o, 1, row++, 1, 1);
   elm_object_text_set(o, daynames[ci->weekend.start]);
   evas_object_smart_callback_add(o, "clicked", _config_weekend_populate, ci);
   evas_object_smart_callback_add(o, "selected", _config_weekend_changed, ci);

   _config_label_add(tb, _("Weekend End:"), row);
   o = elm_hoversel_add(tb);
   E_FILL(o);
   elm_hoversel_hover_parent_set(o, popup);
   elm_hoversel_auto_update_set(o, 1);
   evas_object_show(o);
   elm_table_pack(tb, o, 1, row++, 1, 1);
   elm_object_text_set(o, daynames[(ci->weekend.start + ci->weekend.len - 1) % 7]);
   evas_object_smart_callback_add(o, "clicked", _config_weekend_end_populate, ci);
   evas_object_smart_callback_add(o, "selected", _config_weekend_end_changed, ci);

   _config_label_add(tb, _("Timezone:"), row);
   o = elm_button_add(tb);
   E_FILL(o);
   elm_object_text_set(o, ci->timezone ?: _("System"));
   evas_object_show(o);
   evas_object_smart_callback_add(o, "clicked", _config_timezone_setup, ci);
   elm_table_pack(tb, o, 1, row++, 1, 1);

   for (i = 0; i <= 1; i++)
     {
        const char *ccname, *names[] =
        {
           N_("Background"),
           N_("Foreground"),
        };
        const char *ccnames[] =
        {
           "e.clock_color_bg",
           "e.clock_color_fg",
        };
        Evas_Object *cs;
        Elm_Object_Item *it;
        int r, g, b, a;

        cs = elm_colorselector_add(tb);
        elm_colorselector_mode_set(cs, ELM_COLORSELECTOR_PALETTE);
        ccname = ci->colorclass[i];
        if (!ccname) ccname = ccnames[i];
        edje_color_class_get(ccname, &r, &g, &b, &a,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
          
        it = elm_colorselector_palette_color_add(cs, r, g, b, a);
        o = elm_button_add(tb);
        elm_object_text_set(o, names[i]);
        elm_object_content_set(o, cs);
        E_FILL(o);
        if (!i)
          evas_object_data_set(o, "bg_color", (void*)1L);
        evas_object_data_set(o, "colorselector_it", it);
        evas_object_smart_callback_add(o, "clicked", _config_color_setup, ci);
        evas_object_show(o);
        elm_table_pack(tb, o, i, row, 1, 1);
     }

   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_resize(popup, e_zone_current_get()->w / 4, e_zone_current_get()->h / 3);
   e_comp_object_util_center(popup);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, NULL);

   return time_config->config_dialog = popup;
}


static char *
_config_timezone_text_get(const char *str, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   return strdup(str);
}

static void
_config_timezone_text_del(void *d, Evas_Object *obj EINA_UNUSED)
{
   free(d);
}

static int
_config_timezone_sort(void *ita, void *itb)
{
   const char *a, *b;
   a = elm_object_item_data_get(ita);
   b = elm_object_item_data_get(itb);
   return strcmp(a, b);
}

static void
_config_timezone_set(void *data EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   char *tz = elm_object_item_data_get(event_info);
   Config_Item *ci;
   Evas_Object *bt;

   ci = evas_object_data_get(obj, "config_item");
   bt = evas_object_data_get(obj, "button");
   eina_stringshare_replace(&ci->timezone, tz);
   elm_object_text_set(bt, tz);
   time_config_update(ci);
   evas_object_del(obj);
}

EINTERN void
config_timezone_populate(Evas_Object *obj, const char *name)
{
   static const Elm_Genlist_Item_Class itc =
   {
      .item_style = "default",
      .func = {
           .text_get = (Elm_Genlist_Item_Text_Get_Cb)_config_timezone_text_get,
           .del = _config_timezone_text_del,
      },
      .version = ELM_GENLIST_ITEM_CLASS_VERSION
   };
   Config_Item *ci;
   Elm_Object_Item *it;

   it = elm_genlist_item_sorted_insert(obj, &itc, strdup(name), NULL, 0, (Eina_Compare_Cb)_config_timezone_sort, _config_timezone_set, NULL);
   ci = evas_object_data_get(obj, "config_item");
   if (eina_streq(name, ci->timezone))
     elm_genlist_item_bring_in(it, ELM_GENLIST_ITEM_SCROLLTO_MIDDLE);
}
