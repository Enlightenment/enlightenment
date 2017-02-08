#include "netstatus.h"

static void
_config_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Netstatus_Config *nc = data;
   Instance *inst = nc->inst;

   E_FREE_FUNC(inst->cfg->netstatus.configure, evas_object_del);
   E_FREE_FUNC(nc, free);
   e_config_save_queue();
}

static void
_poll_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Netstatus_Config *nc = data;
   Instance *inst = nc->inst;
   int value = elm_radio_value_get(obj);

   switch(value)
     {
        case 0:
          inst->cfg->netstatus.poll_interval = 4;
          break; 
        case 1:
          inst->cfg->netstatus.poll_interval = 8;
          break;
        case 2:
          inst->cfg->netstatus.poll_interval = 32;
          break;
        case 3:
          inst->cfg->netstatus.poll_interval = 64;
          break;
        case 4:
          inst->cfg->netstatus.poll_interval = 256;
          break;
        default:
          inst->cfg->netstatus.poll_interval = 32;
     }

   e_config_save_queue();
   _netstatus_config_updated(inst);
}

static void
_update_receive_maximums(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Netstatus_Config *nc = data;
   Instance *inst = nc->inst;
   int value = elm_slider_value_get(nc->receive_max);

   inst->cfg->netstatus.inmax = value * nc->receive_unit_adjust;
   e_config_save_queue();
   _netstatus_config_updated(inst);
}

static void
_update_send_maximums(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Netstatus_Config *nc = data;
   Instance *inst = nc->inst;
   int value = elm_slider_value_get(nc->send_max);

   inst->cfg->netstatus.outmax = value * nc->send_unit_adjust;
   e_config_save_queue();
   _netstatus_config_updated(inst);
}

static void
_receive_hover_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Netstatus_Config *nc = data;
   Instance *inst = nc->inst;
   const char *txt = elm_object_item_text_get(event_info);

   if (!strcmp(txt, _("Bytes")))
     {
        inst->cfg->netstatus.receive_units = NETSTATUS_UNIT_BYTES;
        nc->receive_unit_adjust = 1;
     }
   if (!strcmp(txt, _("KB")))
     {
        inst->cfg->netstatus.receive_units = NETSTATUS_UNIT_KB;
        nc->receive_unit_adjust = 1024;
     }
   if (!strcmp(txt, _("MB")))
     {
        inst->cfg->netstatus.receive_units = NETSTATUS_UNIT_MB;
        nc->receive_unit_adjust = 2048;
     }
   if (!strcmp(txt, _("GB")))
     {
        inst->cfg->netstatus.receive_units = NETSTATUS_UNIT_GB;
        nc->receive_unit_adjust = 3072;
     }
   _update_receive_maximums(nc, NULL, NULL);
}

static void
_send_hover_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Netstatus_Config *nc = data;
   Instance *inst = nc->inst;
   const char *txt = elm_object_item_text_get(event_info);

   if (!strcmp(txt, _("Bytes")))
     {
        inst->cfg->netstatus.send_units = NETSTATUS_UNIT_BYTES;
        nc->send_unit_adjust = 1;
     }
   if (!strcmp(txt, _("KB")))
     {
        inst->cfg->netstatus.send_units = NETSTATUS_UNIT_KB;
        nc->send_unit_adjust = 1024;
     }
   if (!strcmp(txt, _("MB")))
     {
        inst->cfg->netstatus.send_units = NETSTATUS_UNIT_MB;
        nc->send_unit_adjust = 2048;
     }
   if (!strcmp(txt, _("GB")))
     {
        inst->cfg->netstatus.send_units = NETSTATUS_UNIT_GB;
        nc->send_unit_adjust = 3072;
     }
   _update_send_maximums(nc, NULL, NULL);
}

static void
_check_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Netstatus_Config *nc = data;
   Instance *inst = nc->inst;

   if (elm_check_state_get(nc->transfer_check))
     {
        elm_object_disabled_set(nc->receive_max, EINA_TRUE);
        elm_object_disabled_set(nc->receive_units, EINA_TRUE);
        elm_object_disabled_set(nc->send_max, EINA_TRUE);
        elm_object_disabled_set(nc->send_units, EINA_TRUE);
        inst->cfg->netstatus.automax = EINA_TRUE;
     }
   else
     {
        elm_object_disabled_set(nc->receive_max, EINA_FALSE);
        elm_object_disabled_set(nc->receive_units, EINA_FALSE);
        elm_object_disabled_set(nc->send_max, EINA_FALSE);
        elm_object_disabled_set(nc->send_units, EINA_FALSE);
        inst->cfg->netstatus.automax = EINA_FALSE;
        _update_receive_maximums(nc, NULL, NULL);
        _update_send_maximums(nc, NULL, NULL);
     }
}

Evas_Object *
netstatus_configure(Instance *inst)
{
   Evas_Object *popup, *frame, *main_box, *box, *o, *group, *lbl;
   Evas_Object *hover, *slider, *check;
   E_Zone *zone = e_zone_current_get();
   Netstatus_Config *nc = E_NEW(Netstatus_Config, 1);

   nc->inst = inst;

   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   main_box = elm_box_add(popup);
   elm_box_horizontal_set(main_box, EINA_FALSE);
   E_EXPAND(main_box);
   E_FILL(main_box);
   evas_object_show(main_box);
   elm_object_content_set(popup, main_box);

   lbl = elm_label_add(main_box);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(lbl, "marker");
   elm_object_text_set(lbl, _("NetStatus Configuration"));
   elm_box_pack_end(main_box, lbl);
   evas_object_show(lbl);

   frame = elm_frame_add(main_box);
   elm_object_text_set(frame, _("Update Poll Interval"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_box_pack_end(main_box, frame);
   evas_object_show(frame);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 0);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Fast (4 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, nc);
   evas_object_show(o);
   group = o;

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Medium (8 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, nc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 2);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Normal (32 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, nc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 3);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Slow (64 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, nc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 4);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Very Slow (256 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, nc);
   evas_object_show(o);

   switch(inst->cfg->netstatus.poll_interval)
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

   frame = elm_frame_add(main_box);
   elm_object_text_set(frame, _("Maximum Transfer Speed"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_box_pack_end(main_box, frame);
   evas_object_show(frame);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   check = elm_check_add(box);
   elm_object_text_set(check, _("Use Automatic Maximums"));
   elm_check_state_set(check, inst->cfg->netstatus.automax);
   evas_object_size_hint_align_set(check, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(check, "changed", _check_changed, nc);
   elm_box_pack_end(box, check);
   evas_object_show(check);
   nc->transfer_check = check;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Receive:"));
   elm_slider_unit_format_set(slider, "%1.0f");
   elm_slider_indicator_format_set(slider, "%1.0f");
   elm_slider_min_max_set(slider, 0, 1000);
   elm_slider_value_set(slider, inst->cfg->netstatus.inmax);
   elm_slider_step_set(slider, 10);
   elm_slider_span_size_set(slider, 100);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(slider, "delay,changed", _update_receive_maximums, nc);
   if (inst->cfg->netstatus.automax) elm_object_disabled_set(slider, EINA_TRUE);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   nc->receive_max = slider;

   hover = elm_hoversel_add(box);
   elm_hoversel_auto_update_set(hover, EINA_TRUE);
   elm_hoversel_hover_parent_set(hover, popup);
   elm_hoversel_item_add(hover, _("Bytes"), NULL, ELM_ICON_NONE, NULL, NULL);
   if (inst->cfg->netstatus.receive_units == NETSTATUS_UNIT_BYTES)
     {
        elm_object_text_set(hover, _("Bytes"));
        nc->receive_unit_adjust = 1;
     }
   elm_hoversel_item_add(hover, _("KB"), NULL, ELM_ICON_NONE, NULL, NULL);
   if (inst->cfg->netstatus.receive_units == NETSTATUS_UNIT_KB)
     {
        elm_object_text_set(hover, _("KB"));
        nc->receive_unit_adjust = 1024;
     }
   elm_hoversel_item_add(hover, _("MB"), NULL, ELM_ICON_NONE, NULL, NULL);
   if (inst->cfg->netstatus.receive_units == NETSTATUS_UNIT_MB)
     {
        elm_object_text_set(hover, _("MB"));
        nc->receive_unit_adjust = 2048;
     }
   elm_hoversel_item_add(hover, _("GB"), NULL, ELM_ICON_NONE, NULL, NULL);
   if (inst->cfg->netstatus.receive_units == NETSTATUS_UNIT_GB)
     {
        elm_object_text_set(hover, _("GB"));
        nc->receive_unit_adjust = 3072;
     }
   evas_object_size_hint_align_set(hover, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(hover, EVAS_HINT_EXPAND, 0.0);
   if (inst->cfg->netstatus.automax) elm_object_disabled_set(hover, EINA_TRUE);
   evas_object_smart_callback_add(hover, "selected", _receive_hover_changed, nc);
   elm_box_pack_end(box, hover);
   evas_object_show(hover);
   nc->receive_units = hover;

   elm_slider_value_set(nc->receive_max, ceil((double)inst->cfg->netstatus.inmax / (double)nc->receive_unit_adjust));

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Send:"));
   elm_slider_unit_format_set(slider, "%1.0f");
   elm_slider_indicator_format_set(slider, "%1.0f");
   elm_slider_min_max_set(slider, 0, 1000);
   elm_slider_value_set(slider, inst->cfg->netstatus.outmax);
   elm_slider_step_set(slider, 10);
   elm_slider_span_size_set(slider, 100);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(slider, "delay,changed", _update_send_maximums, nc);
   if (inst->cfg->netstatus.automax) elm_object_disabled_set(slider, EINA_TRUE);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   nc->send_max = slider;

   hover = elm_hoversel_add(box);
   elm_hoversel_auto_update_set(hover, EINA_TRUE);
   elm_hoversel_hover_parent_set(hover, popup);
   elm_hoversel_item_add(hover, _("Bytes"), NULL, ELM_ICON_NONE, NULL, nc);
   if (inst->cfg->netstatus.send_units == NETSTATUS_UNIT_BYTES)
     {
        elm_object_text_set(hover, _("Bytes"));
        nc->send_unit_adjust = 1;
     }
   elm_hoversel_item_add(hover, _("KB"), NULL, ELM_ICON_NONE, NULL, nc);
   if (inst->cfg->netstatus.send_units == NETSTATUS_UNIT_KB)
     {
        elm_object_text_set(hover, _("KB"));
        nc->send_unit_adjust = 1024;
     }
   elm_hoversel_item_add(hover, _("MB"), NULL, ELM_ICON_NONE, NULL, nc);
   if (inst->cfg->netstatus.send_units == NETSTATUS_UNIT_MB)
     {
        elm_object_text_set(hover, _("MB"));
        nc->send_unit_adjust = 2048;
     }
   elm_hoversel_item_add(hover, _("GB"), NULL, ELM_ICON_NONE, NULL, nc);
   if (inst->cfg->netstatus.send_units == NETSTATUS_UNIT_GB)
     {
        elm_object_text_set(hover, _("GB"));
        nc->send_unit_adjust = 3072;
     }
   evas_object_size_hint_align_set(hover, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(hover, EVAS_HINT_EXPAND, 0.0);
   if (inst->cfg->netstatus.automax) elm_object_disabled_set(hover, EINA_TRUE);
   evas_object_smart_callback_add(hover, "selected", _send_hover_changed, nc);
   elm_box_pack_end(box, hover);
   evas_object_show(hover);
   nc->send_units = hover;

   elm_slider_value_set(nc->send_max, ceil((double)inst->cfg->netstatus.outmax / (double)nc->send_unit_adjust));

   elm_object_content_set(frame, box);
   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center_on_zone(popup, zone);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, nc);

   return inst->cfg->netstatus.configure = popup;
}

