#include "cpuclock.h"

static void
_config_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;
   Instance *inst = cc->inst;

   E_FREE_FUNC(inst->cfg->cpuclock.configure, evas_object_del);
   E_FREE_FUNC(cc->powersaves, eina_list_free);
   E_FREE(cc);
   e_config_save_queue();
}

static void
_config_show_general(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;

   evas_object_hide(cc->policy);
   evas_object_hide(cc->saving);
   if (cc->pstate)
     evas_object_hide(cc->ps);
   if (cc->frequencies)
     evas_object_hide(cc->freq);
   evas_object_show(cc->general);
}

static void
_config_show_policy(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;

   evas_object_hide(cc->general);
   evas_object_hide(cc->saving);
   if (cc->pstate)
     evas_object_hide(cc->ps);
   if (cc->frequencies)
     evas_object_hide(cc->freq);
   evas_object_show(cc->policy);
}

static void
_config_show_saving(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;

   evas_object_hide(cc->general);
   evas_object_hide(cc->policy);
   if (cc->pstate)
     evas_object_hide(cc->ps);
   if (cc->frequencies)
     evas_object_hide(cc->freq);
   evas_object_show(cc->saving);
}

static void
_config_show_frequencies(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;

   evas_object_hide(cc->general);
   evas_object_hide(cc->policy);
   evas_object_hide(cc->saving);
   if (cc->pstate)
     evas_object_hide(cc->ps);
   evas_object_show(cc->freq);
}

static void
_config_show_pstate(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;

   evas_object_hide(cc->general);
   evas_object_hide(cc->policy);
   evas_object_hide(cc->saving);
   if (cc->frequencies)
     evas_object_hide(cc->freq);
   evas_object_show(cc->ps);
}

static void
_update_max_power(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;
   Instance *inst = cc->inst;
   int value = elm_slider_value_get(cc->max);

   inst->cfg->cpuclock.pstate_max = value;
   _cpuclock_set_pstate(inst->cfg->cpuclock.pstate_min - 1,
                        inst->cfg->cpuclock.pstate_max - 1, inst->cfg->cpuclock.status->pstate_turbo);
   e_config_save_queue();
   _cpuclock_config_updated(inst);
}

static void
_update_min_power(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;
   Instance *inst = cc->inst;
   int value = elm_slider_value_get(cc->min);

   inst->cfg->cpuclock.pstate_min = value;
   _cpuclock_set_pstate(inst->cfg->cpuclock.pstate_min - 1,
                        inst->cfg->cpuclock.pstate_max - 1, inst->cfg->cpuclock.status->pstate_turbo);
   e_config_save_queue();
   _cpuclock_config_updated(inst);
}

static void
_auto_powersave(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;
   Instance *inst = cc->inst;
   Eina_List *l = NULL;
   Evas_Object *o = NULL;

   inst->cfg->cpuclock.auto_powersave = elm_check_state_get(obj);
   EINA_LIST_FOREACH(cc->powersaves, l, o)
     elm_object_disabled_set(o, inst->cfg->cpuclock.auto_powersave);
   e_config_save_queue();
   _cpuclock_config_updated(inst);
}

static void
_restore_governor(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;
   Instance *inst = cc->inst;

   inst->cfg->cpuclock.restore_governor = elm_check_state_get(obj);
   if ((!inst->cfg->cpuclock.governor) ||
       (strcmp(inst->cfg->cpuclock.status->cur_governor, inst->cfg->cpuclock.governor)))
     {
        eina_stringshare_replace(&inst->cfg->cpuclock.governor, inst->cfg->cpuclock.status->cur_governor);
     }
   e_config_save_queue();
   _cpuclock_config_updated(inst);
}

static void
_frequency_changed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   const char *value = elm_object_text_get(obj);
   int frequency = atol(value);

   if (frequency > 0)
     _cpuclock_set_frequency(frequency);
}

static void
_powersave_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;
   Instance *inst = cc->inst;
   const char *value = evas_object_data_get(obj, "governor");

   if (value)
     eina_stringshare_replace(&inst->cfg->cpuclock.powersave_governor, value);
   e_config_save_queue();
   _cpuclock_config_updated(inst);
}

static void
_governor_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;
   Instance *inst = cc->inst;
   const char *value = evas_object_data_get(obj, "governor");

   if (value)
     {
        eina_stringshare_replace(&inst->cfg->cpuclock.governor, value);
        _cpuclock_set_governor(inst->cfg->cpuclock.governor);
     }

   e_config_save_queue();
   _cpuclock_config_updated(inst);
}

static void
_poll_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Cpuclock_Config *cc = data;
   Instance *inst = cc->inst;
   int value = elm_radio_value_get(obj);

   switch (value)
     {
      case 0:
        inst->cfg->cpuclock.poll_interval = 4;
        break;

      case 1:
        inst->cfg->cpuclock.poll_interval = 8;
        break;

      case 2:
        inst->cfg->cpuclock.poll_interval = 32;
        break;

      case 3:
        inst->cfg->cpuclock.poll_interval = 64;
        break;

      case 4:
        inst->cfg->cpuclock.poll_interval = 256;
        break;

      default:
        inst->cfg->cpuclock.poll_interval = 32;
     }

   e_config_save_queue();
   _cpuclock_config_updated(inst);
}

Evas_Object *
cpuclock_configure(Instance *inst)
{
   Evas_Object *popup, *tb, *frame, *box, *o, *group, *lbl, *slider, *list, *check;
   Evas_Object *groupg = NULL, *groups = NULL, *groupf = NULL;
   Elm_Object_Item *it;
   E_Zone *zone = e_zone_current_get();
   Eina_List *l = NULL;
   Cpuclock_Config *cc = E_NEW(Cpuclock_Config, 1);
   int i = 0, value = 0;

   cc->inst = inst;
   cc->powersaves = NULL;
   cc->frequencies = EINA_FALSE;
   cc->pstate = EINA_FALSE;

   if ((inst->cfg->cpuclock.status->frequencies) &&
       (inst->cfg->cpuclock.status->can_set_frequency) &&
       (!inst->cfg->cpuclock.status->pstate))
     {
        cc->frequencies = EINA_TRUE;
     }
   if ((inst->cfg->cpuclock.status) && (inst->cfg->cpuclock.status->pstate))
     {
        cc->pstate = EINA_TRUE;
     }

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
   elm_object_text_set(lbl, _("CpuClock Configuration"));
   elm_table_pack(tb, lbl, 0, 0, 2, 1);
   evas_object_show(lbl);

   list = elm_list_add(tb);
   E_ALIGN(list, 0, EVAS_HINT_FILL);
   E_WEIGHT(list, 0, EVAS_HINT_EXPAND);
   elm_table_pack(tb, list, 0, 1, 1, 1);
   elm_list_select_mode_set(list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_scroller_content_min_limit(list, 1, 1);
   it = elm_list_item_append(list, _("General"), NULL, NULL,
                             _config_show_general, cc);
   elm_list_item_selected_set(it, 1);
   it = elm_list_item_append(list, _("Power Policy"), NULL, NULL,
                             _config_show_policy, cc);
   it = elm_list_item_append(list, _("Power Saving"), NULL, NULL,
                             _config_show_saving, cc);
   if (cc->pstate)
     it = elm_list_item_append(list, _("Power State"), NULL, NULL,
                               _config_show_pstate, cc);
   if (cc->frequencies)
     it = elm_list_item_append(list, _("Frequencies"), NULL, NULL,
                               _config_show_frequencies, cc);
   elm_list_go(list);
   evas_object_show(list);

   frame = elm_frame_add(tb);
   elm_object_text_set(frame, _("General"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_table_pack(tb, frame, 1, 1, 1, 1);
   evas_object_show(frame);
   cc->general = frame;

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   lbl = elm_label_add(tb);
   E_ALIGN(lbl, 0.0, 0.0);
   E_WEIGHT(lbl, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(lbl, _("Update Poll Interval"));
   elm_box_pack_end(box, lbl);
   evas_object_show(lbl);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 0);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Fast (4 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, cc);
   evas_object_show(o);
   group = o;

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Medium (8 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, cc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 2);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Normal (32 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, cc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 3);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Slow (64 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, cc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 4);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0.0, 0.0);
   E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(o, _("Very Slow (256 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, cc);
   evas_object_show(o);

   switch (inst->cfg->cpuclock.poll_interval)
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
   elm_object_text_set(frame, _("Power Policy"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_table_pack(tb, frame, 1, 1, 1, 1);
   evas_object_show(frame);
   cc->policy = frame;

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   i = 0;
   for (l = inst->cfg->cpuclock.status->governors; l; l = l->next)
     {
        o = elm_radio_add(box);
        elm_radio_state_value_set(o, i);
        E_ALIGN(o, 0.0, 0.0);
        E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
        if (!strcmp(l->data, "ondemand"))
          elm_object_text_set(o, _("Automatic"));
        else if (!strcmp(l->data, "conservative"))
          elm_object_text_set(o, _("Lower Power Automatic"));
        else if (!strcmp(l->data, "interactive"))
          elm_object_text_set(o, _("Automatic Interactive"));
        else if (!strcmp(l->data, "powersave"))
          elm_object_text_set(o, _("Minimum Speed"));
        else if (!strcmp(l->data, "performance"))
          elm_object_text_set(o, _("Maximum Speed"));
        else
          elm_object_text_set(o, l->data);
        evas_object_data_set(o, "governor", strdup(l->data));
        elm_box_pack_end(box, o);
        evas_object_smart_callback_add(o, "changed", _governor_changed, cc);
        evas_object_show(o);

        if (!strcmp(inst->cfg->cpuclock.status->cur_governor, l->data))
          value = i;

        if (!groupg)
          groupg = o;
        else
          elm_radio_group_add(o, groupg);
        i++;
     }
   if (groupg)
     elm_radio_value_set(groupg, value);

   check = elm_check_add(box);
   elm_object_text_set(check, _("Restore CPU Power Policy"));
   elm_check_state_set(check, inst->cfg->cpuclock.restore_governor);
   E_ALIGN(check, 0.0, 0.0);
   E_WEIGHT(check, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(check, "changed", _restore_governor, cc);
   elm_box_pack_end(box, check);
   evas_object_show(check);

   elm_object_content_set(frame, box);

   frame = elm_frame_add(tb);
   elm_object_text_set(frame, _("Power Saving"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_table_pack(tb, frame, 1, 1, 1, 1);
   evas_object_show(frame);
   cc->saving = frame;

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   check = elm_check_add(box);
   elm_object_text_set(check, _("Automatic Powersaving"));
   elm_check_state_set(check, inst->cfg->cpuclock.auto_powersave);
   E_ALIGN(check, 0.0, 0.0);
   E_WEIGHT(check, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(check, "changed", _auto_powersave, cc);
   elm_box_pack_end(box, check);
   evas_object_show(check);

   i = 0;
   for (l = inst->cfg->cpuclock.status->governors; l; l = l->next)
     {
        o = elm_radio_add(box);
        elm_radio_state_value_set(o, i);
        E_ALIGN(o, 0.0, 0.0);
        E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
        if (!strcmp(l->data, "ondemand"))
          elm_object_text_set(o, _("Automatic"));
        else if (!strcmp(l->data, "conservative"))
          elm_object_text_set(o, _("Lower Power Automatic"));
        else if (!strcmp(l->data, "interactive"))
          elm_object_text_set(o, _("Automatic Interactive"));
        else if (!strcmp(l->data, "powersave"))
          elm_object_text_set(o, _("Minimum Speed"));
        else if (!strcmp(l->data, "performance"))
          elm_object_text_set(o, _("Maximum Speed"));
        else
          elm_object_text_set(o, l->data);
        elm_object_disabled_set(o, inst->cfg->cpuclock.auto_powersave);
        evas_object_data_set(o, "governor", strdup(l->data));
        elm_box_pack_end(box, o);
        evas_object_smart_callback_add(o, "changed", _powersave_changed, cc);
        evas_object_show(o);
        cc->powersaves = eina_list_append(cc->powersaves, o);

        if (inst->cfg->cpuclock.powersave_governor && !strcmp(inst->cfg->cpuclock.powersave_governor, l->data))
          value = i;

        if (!groups)
          groups = o;
        else
          elm_radio_group_add(o, groups);
        i++;
     }
   if (groups)
     elm_radio_value_set(groups, value);

   elm_object_content_set(frame, box);

   if (cc->pstate)
     {
        frame = elm_frame_add(tb);
        elm_object_text_set(frame, _("Power State"));
        E_EXPAND(frame);
        E_FILL(frame);
        elm_table_pack(tb, frame, 1, 1, 1, 1);
        evas_object_show(frame);
        cc->ps = frame;

        box = elm_box_add(frame);
        elm_box_horizontal_set(box, EINA_FALSE);
        E_EXPAND(box);
        evas_object_show(box);

        slider = elm_slider_add(box);
        elm_object_text_set(slider, _("Maximum Power State:"));
        elm_slider_unit_format_set(slider, "%3.0f");
        elm_slider_indicator_format_set(slider, "%3.0f");
        elm_slider_min_max_set(slider, 2, 100);
        elm_slider_value_set(slider, inst->cfg->cpuclock.pstate_max);
        elm_slider_step_set(slider, 1);
        elm_slider_span_size_set(slider, 100);
        evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
        evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
        evas_object_smart_callback_add(slider, "delay,changed", _update_max_power, cc);
        elm_box_pack_end(box, slider);
        evas_object_show(slider);
        cc->max = slider;

        slider = elm_slider_add(box);
        elm_object_text_set(slider, _("Minimum Power State:"));
        elm_slider_unit_format_set(slider, "%3.0f");
        elm_slider_indicator_format_set(slider, "%3.0f");
        elm_slider_min_max_set(slider, 0, 100);
        elm_slider_value_set(slider, inst->cfg->cpuclock.pstate_min);
        elm_slider_step_set(slider, 1);
        elm_slider_span_size_set(slider, 100);
        evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
        evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
        evas_object_smart_callback_add(slider, "delay,changed", _update_min_power, cc);
        elm_box_pack_end(box, slider);
        evas_object_show(slider);
        cc->min = slider;

        elm_object_content_set(frame, box);
     }
   if (cc->frequencies)
     {
        frame = elm_frame_add(tb);
        elm_object_text_set(frame, _("Frequencies"));
        E_EXPAND(frame);
        E_FILL(frame);
        elm_table_pack(tb, frame, 1, 1, 1, 1);
        evas_object_show(frame);
        cc->freq = frame;

        box = elm_box_add(frame);
        elm_box_horizontal_set(box, EINA_FALSE);
        E_EXPAND(box);
        evas_object_show(box);

        i = 0;
        for (l = inst->cfg->cpuclock.status->frequencies; l; l = l->next)
          {
             char buf[4096];
             int frequency;

             frequency = (long)l->data;

             o = elm_radio_add(box);
             elm_radio_state_value_set(o, i);
             E_ALIGN(o, 0.0, 0.0);
             E_WEIGHT(o, EVAS_HINT_EXPAND, 0);
#if defined(__OpenBSD__)
             snprintf(buf, sizeof(buf), "%i %%", frequency);
#else
             if (frequency < 1000000)
               snprintf(buf, sizeof(buf), _("%i MHz"), frequency / 1000);
             else
               snprintf(buf, sizeof(buf), _("%'.1f GHz"),
                        frequency / 1000000.);
#endif
             elm_object_text_set(o, buf);
             elm_box_pack_end(box, o);
             evas_object_smart_callback_add(o, "changed", _frequency_changed, cc);
             evas_object_show(o);

#if defined(__OpenBSD__)
             if (inst->cfg->cpuclock.status->cur_percent == frequency)
               value = i;
#else
             if (inst->cfg->cpuclock.status->cur_frequency == frequency)
               value = i;
#endif
             if (!groupf)
               groupf = o;
             else
               elm_radio_group_add(o, groupf);
             i++;
          }
        if (groupf)
          elm_radio_value_set(groupf, value);

        elm_object_content_set(frame, box);
        evas_object_hide(cc->freq);
     }
   if (cc->pstate)
     evas_object_hide(cc->ps);
   evas_object_hide(cc->policy);
   evas_object_hide(cc->saving);
   evas_object_show(cc->general);

   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center_on_zone(popup, zone);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, cc);

   return inst->cfg->cpuclock.configure = popup;
}

