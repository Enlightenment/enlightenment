#include "thermal.h"

#define FAR_2_CEL(x) ((x - 32) / 9.0) * 5.0
#define CEL_2_FAR(x) (x * 9.0 / 5.0) + 32

static void
_config_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Thermal_Config *tc = data;
   Instance *inst = tc->inst;

   E_FREE_FUNC(inst->cfg->thermal.configure, evas_object_del);
   E_FREE(tc);
   e_config_save_queue();
}

static void
_update_high_temperature(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Thermal_Config *tc = data;
   Instance *inst = tc->inst;
   int value = (int)elm_slider_value_get(tc->high);

   inst->cfg->thermal.high = value;
   e_config_save_queue();
   _thermal_config_updated(inst);
}

static void
_update_low_temperature(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Thermal_Config *tc = data;
   Instance *inst = tc->inst;
   int value = (int)elm_slider_value_get(tc->low);

   inst->cfg->thermal.low = value;
   e_config_save_queue();
   _thermal_config_updated(inst);
}

static void
_units_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Thermal_Config *tc = data;
   Instance *inst = tc->inst;
   int value = elm_radio_value_get(obj), val;

   switch (value)
     {
      case 0:
        inst->cfg->thermal.units = CELSIUS;
        break;

      case 1:
        inst->cfg->thermal.units = FAHRENHEIT;
        break;

      default:
        inst->cfg->thermal.units = CELSIUS;
     }
   if (inst->cfg->thermal.units == FAHRENHEIT)
     {
        elm_slider_min_max_set(tc->low, 0, 200);
        elm_slider_min_max_set(tc->high, 0, 230);
        val = (int)elm_slider_value_get(tc->low);
        elm_slider_value_set(tc->low, ceil(CEL_2_FAR(val)));
        val = (int)elm_slider_value_get(tc->high);
        elm_slider_value_set(tc->high, ceil(CEL_2_FAR(val)));
        elm_slider_unit_format_set(tc->high, "%1.0f F");
        elm_slider_indicator_format_set(tc->high, "%1.0f F");
        elm_slider_unit_format_set(tc->low, "%1.0f F");
        elm_slider_indicator_format_set(tc->low, "%1.0f F");
     }
   else
     {
        val = (int)elm_slider_value_get(tc->low);
        elm_slider_value_set(tc->low, ceil(FAR_2_CEL(val)));
        val = (int)elm_slider_value_get(tc->high);
        elm_slider_value_set(tc->high, ceil(FAR_2_CEL(val)));
        elm_slider_unit_format_set(tc->low, "%1.0f C");
        elm_slider_indicator_format_set(tc->low, "%1.0f C");
        elm_slider_unit_format_set(tc->high, "%1.0f C");
        elm_slider_indicator_format_set(tc->high, "%1.0f C");
        elm_slider_min_max_set(tc->low, 0, 95);
        elm_slider_min_max_set(tc->high, 0, 110);
     }
   val = (int)elm_slider_value_get(tc->high);
   inst->cfg->thermal.high = val;
   val = (int)elm_slider_value_get(tc->low);
   inst->cfg->thermal.low = val;
   e_config_save_queue();
   _thermal_config_updated(inst);
}

static void
_poll_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Thermal_Config *tc = data;
   Instance *inst = tc->inst;
   int value = elm_radio_value_get(obj);

   switch (value)
     {
      case 0:
        inst->cfg->thermal.poll_interval = 4;
        break;

      case 1:
        inst->cfg->thermal.poll_interval = 8;
        break;

      case 2:
        inst->cfg->thermal.poll_interval = 32;
        break;

      case 3:
        inst->cfg->thermal.poll_interval = 64;
        break;

      case 4:
        inst->cfg->thermal.poll_interval = 256;
        break;

      default:
        inst->cfg->thermal.poll_interval = 32;
     }

   e_config_save_queue();
   _thermal_config_updated(inst);
}

Evas_Object *
thermal_configure(Instance *inst)
{
   Evas_Object *popup, *tb, *frame, *box, *o, *group, *groupu, *lbl, *slider;
   E_Zone *zone = e_zone_current_get();
   Thermal_Config *tc = E_NEW(Thermal_Config, 1);

   tc->inst = inst;

   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   lbl = elm_label_add(tb);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(lbl, "marker");
   elm_object_text_set(lbl, _("Thermal Configuration"));
   elm_table_pack(tb, lbl, 0, 0, 1, 1);
   evas_object_show(lbl);

   frame = elm_frame_add(tb);
   elm_object_text_set(frame, _("Temperature Units"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_table_pack(tb, frame, 0, 1, 1, 1);
   evas_object_show(frame);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 0);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Celsius"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _units_changed, tc);
   evas_object_show(o);
   groupu = o;

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, groupu);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Fahrenheit"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _units_changed, tc);
   evas_object_show(o);

   switch (inst->cfg->thermal.units)
     {
      case CELSIUS:
        elm_radio_value_set(groupu, 0);
        break;

      case FAHRENHEIT:
        elm_radio_value_set(groupu, 1);
        break;

      default:
        elm_radio_value_set(groupu, 0);
     }

   elm_object_content_set(frame, box);

   frame = elm_frame_add(tb);
   elm_object_text_set(frame, _("Update Poll Interval"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_table_pack(tb, frame, 0, 2, 1, 1);
   evas_object_show(frame);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 0);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Fast (4 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, tc);
   evas_object_show(o);
   group = o;

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Medium (8 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, tc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 2);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Normal (32 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, tc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 3);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Slow (64 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, tc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 4);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Very Slow (256 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, tc);
   evas_object_show(o);

   switch (inst->cfg->thermal.poll_interval)
     {
      case 4:
        elm_radio_value_set(group, 0);
        break;

      case 8:
        elm_radio_value_set(group, 1);
        break;

      case 32:
        elm_radio_value_set(group, 2);
        break;

      case 64:
        elm_radio_value_set(group, 3);
        break;

      case 256:
        elm_radio_value_set(group, 4);
        break;

      default:
        elm_radio_value_set(group, 2);
     }

   elm_object_content_set(frame, box);

   frame = elm_frame_add(tb);
   elm_object_text_set(frame, _("Temperature Limits"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_table_pack(tb, frame, 0, 3, 1, 1);
   evas_object_show(frame);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("High Temperature:"));
   if (inst->cfg->thermal.units == FAHRENHEIT)
     {
        elm_slider_unit_format_set(slider, "%1.0f F");
        elm_slider_indicator_format_set(slider, "%1.0f F");
        elm_slider_min_max_set(slider, 0, 230);
     }
   else
     {
        elm_slider_unit_format_set(slider, "%1.0f C");
        elm_slider_indicator_format_set(slider, "%1.0f C");
        elm_slider_min_max_set(slider, 0, 110);
     }
   elm_slider_value_set(slider, inst->cfg->thermal.high);
   elm_slider_step_set(slider, 5);
   elm_slider_span_size_set(slider, 150);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(slider, "delay,changed", _update_high_temperature, tc);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   tc->high = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Low Temperature:"));
   if (inst->cfg->thermal.units == FAHRENHEIT)
     {
        elm_slider_unit_format_set(slider, "%1.0f F");
        elm_slider_indicator_format_set(slider, "%1.0f F");
        elm_slider_min_max_set(slider, 0, 200);
     }
   else
     {
        elm_slider_unit_format_set(slider, "%1.0f C");
        elm_slider_indicator_format_set(slider, "%1.0f C");
        elm_slider_min_max_set(slider, 0, 95);
     }
   elm_slider_value_set(slider, inst->cfg->thermal.low);
   elm_slider_step_set(slider, 5);
   elm_slider_span_size_set(slider, 150);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(slider, "delay,changed", _update_low_temperature, tc);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   tc->low = slider;

   elm_object_content_set(frame, box);

   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center_on_zone(popup, zone);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, tc);

   return inst->cfg->thermal.configure = popup;
}

