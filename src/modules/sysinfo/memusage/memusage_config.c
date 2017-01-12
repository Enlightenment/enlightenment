#include "memusage.h"

static void
_config_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   E_FREE_FUNC(inst->cfg->memusage.configure, evas_object_del);
   e_config_save_queue();
}

static void
_config_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   int value = elm_radio_value_get(obj);

   switch(value)
     {
        case 0:
          inst->cfg->memusage.poll_interval = 4;
          break; 
        case 1:
          inst->cfg->memusage.poll_interval = 8;
          break;
        case 2:
          inst->cfg->memusage.poll_interval = 32;
          break;
        case 3:
          inst->cfg->memusage.poll_interval = 64;
          break;
        case 4:
          inst->cfg->memusage.poll_interval = 256;
          break;
        default:
           inst->cfg->memusage.poll_interval = 32;
     }
   e_config_save_queue();
   _memusage_config_updated(inst);
}
   
Evas_Object *
memusage_configure(Instance *inst)
{
   Evas_Object *popup, *frame, *box, *o, *group, *lbl;
   E_Zone *zone = e_zone_current_get();

   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   E_FILL(box);
   evas_object_show(box);
   elm_object_content_set(popup, box);

   lbl = elm_label_add(box);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(lbl, "marker");
   elm_object_text_set(lbl, _("MemUsage Configuration"));
   elm_box_pack_end(box, lbl);
   evas_object_show(lbl);

   frame = elm_frame_add(box);
   elm_object_text_set(frame, _("Update Poll Interval"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_box_pack_end(box, frame);
   evas_object_show(frame);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 0);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Fast (4 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _config_changed, inst);
   evas_object_show(o);
   group = o;

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Medium (8 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _config_changed, inst);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 2);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Normal (32 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _config_changed, inst);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 3);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Slow (64 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _config_changed, inst);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 4);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Very Slow (256 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _config_changed, inst);
   evas_object_show(o);

   switch(inst->cfg->memusage.poll_interval)
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
   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center_on_zone(popup, zone);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, inst);

   return inst->cfg->memusage.configure = popup;
}

