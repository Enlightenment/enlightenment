#include "batman.h"

static void
_config_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;

   E_FREE_FUNC(inst->cfg->batman.configure, evas_object_del);
   E_FREE_FUNC(bc, free);
   e_config_save_queue();
}

static void
_update_suspend_percent(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;
   int value = elm_slider_value_get(obj);

   inst->cfg->batman.suspend_below = value;
   e_config_save_queue();
   _batman_config_updated(inst);
}

static void
_update_alert_time(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;
   int value = elm_slider_value_get(bc->alert_time);

   inst->cfg->batman.alert = value;
   e_config_save_queue();
   _batman_config_updated(inst);
}

static void
_update_alert_percent(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;
   int value = elm_slider_value_get(bc->alert_percent);

   inst->cfg->batman.alert_p = value;
   e_config_save_queue();
   _batman_config_updated(inst);
}

static void
_update_alert_timeout(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;
   int value = elm_slider_value_get(bc->alert_timeout);

   inst->cfg->batman.alert_timeout = value;
   e_config_save_queue();
   _batman_config_updated(inst);
}

static void
_check_desktop_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;

   inst->cfg->batman.desktop_notifications = elm_check_state_get(bc->alert_desktop);
   e_config_save_queue();
   _batman_config_updated(inst);
}

static void
_check_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;

   if (!elm_check_state_get(bc->alert_check))
     {
        elm_object_disabled_set(bc->alert_time, EINA_TRUE);
        elm_object_disabled_set(bc->alert_percent, EINA_TRUE);
        elm_object_disabled_set(bc->alert_desktop, EINA_TRUE);
        elm_object_disabled_set(bc->alert_timeout, EINA_TRUE);
        elm_slider_value_set(bc->alert_time, 0);
        elm_slider_value_set(bc->alert_percent, 0);
        _update_alert_time(bc, NULL, NULL);
        _update_alert_percent(bc, NULL, NULL);
     }
   else
     {
        elm_object_disabled_set(bc->alert_time, EINA_FALSE);
        elm_object_disabled_set(bc->alert_percent, EINA_FALSE);
        elm_object_disabled_set(bc->alert_desktop, EINA_FALSE);
        elm_object_disabled_set(bc->alert_timeout, EINA_FALSE);
     }
   e_config_save_queue();
   _batman_config_updated(inst);
}

static void
_suspend_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;
   int value = elm_radio_value_get(obj);

   switch (value)
     {
        case 0:
           inst->cfg->batman.suspend_method = SUSPEND;
           break;
        case 1:
           inst->cfg->batman.suspend_method = HIBERNATE;
           break;
        case 2:
           inst->cfg->batman.suspend_method = SHUTDOWN;
           break;
        default:
           inst->cfg->batman.suspend_method = SUSPEND;
     }

   e_config_save_queue();
   _batman_config_updated(inst);
}

static void
_poll_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Batman_Config *bc = data;
   Instance *inst = bc->inst;
   int value = elm_radio_value_get(obj);

   switch (value)
     {
        case 0:
          inst->cfg->batman.poll_interval = 4;
          break; 
        case 1:
          inst->cfg->batman.poll_interval = 8;
          break;
        case 2:
          inst->cfg->batman.poll_interval = 32;
          break;
        case 3:
          inst->cfg->batman.poll_interval = 64;
          break;
        case 4:
          inst->cfg->batman.poll_interval = 256;
          break;
        default:
          inst->cfg->batman.poll_interval = 32;
     }

   e_config_save_queue();
   _batman_config_updated(inst);
}

static void
_power_management_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;

   evas_object_del(popup);
   e_configure_registry_call("advanced/powermanagement", NULL, NULL);
}

static void
_icon_theme_file_set(Evas_Object *img, const char *icon)
{
   const char *path = NULL, *k = NULL;
   char buf[4096];
   int len = 0;

   if (!icon)
     path = NULL;
   else if (strncmp(icon, "/", 1) && !ecore_file_exists(icon))
     {
        path = efreet_icon_path_find(e_config->icon_theme, icon, 48);
        if (!path)
          {
             if (e_util_strcmp(e_config->icon_theme, "hicolor"))
             path = efreet_icon_path_find("hicolor", icon, 48);
          }
     }
   else if (ecore_file_exists(icon))
     {
        path = icon;
     }
   if (!path)
     {
        snprintf(buf, sizeof(buf), "e/icons/%s", icon);
        if (eina_list_count(e_theme_collection_items_find("base/theme/icons", buf)))
          {
             path = e_theme_edje_file_get("base/theme/icons", buf);
             k = buf;
          }
        else
          {
             path = e_theme_edje_file_get("base/theme/icons", "e/icons/unknown");
             k =  "e/icons/unknown";
          }
     }
   if (path && icon)
     {
        len = strlen(icon);
        if ((len > 4) && (!strcasecmp(icon + len - 4, ".edj")))
        k = "icon";
     }
   elm_image_file_set(img, path, k);
}

Evas_Object *
batman_configure(Instance *inst)
{
   Evas_Object *popup, *frame, *main_box, *box, *o, *group, *groupy, *lbl;
   Evas_Object *slider, *check, *but, *img;
   Eina_Bool show_alert;
   E_Zone *zone = e_zone_current_get();
   Batman_Config *bc = E_NEW(Batman_Config, 1);

   bc->inst = inst;

   if (inst->cfg->batman.alert > 0 || inst->cfg->batman.alert_p > 0)
     show_alert = EINA_TRUE;
   else
     show_alert = EINA_FALSE;

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
   elm_object_text_set(lbl, _("Batman Configuration"));
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
   evas_object_smart_callback_add(o, "changed", _poll_changed, bc);
   evas_object_show(o);
   group = o;

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Medium (8 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, bc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 2);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Normal (32 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, bc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 3);
   elm_radio_group_add(o, group);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Slow (64 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, bc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 4);
   elm_radio_group_add(o, group);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Very Slow (256 ticks)"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _poll_changed, bc);
   evas_object_show(o);

   switch(inst->cfg->batman.poll_interval)
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
   elm_object_text_set(frame, _("Alert"));
   E_EXPAND(frame);
   E_FILL(frame);
   elm_box_pack_end(main_box, frame);
   evas_object_show(frame);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_show(box);

   check = elm_check_add(box);
   elm_object_text_set(check, _("Show low battery alert"));
   elm_check_state_set(check, show_alert);
   evas_object_size_hint_align_set(check, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(check, "changed", _check_changed, bc);
   elm_box_pack_end(box, check);
   evas_object_show(check);
   bc->alert_check = check;

   check = elm_check_add(box);
   elm_object_text_set(check, _("Show alert as a desktop notification"));
   elm_check_state_set(check, inst->cfg->batman.desktop_notifications);
   evas_object_size_hint_align_set(check, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(check, "changed", _check_desktop_changed, bc);
   elm_object_disabled_set(check, !show_alert);
   elm_box_pack_end(box, check);
   evas_object_show(check);
   bc->alert_desktop = check;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Alert when time left is at:"));
   elm_slider_unit_format_set(slider, "%1.0f min");
   elm_slider_indicator_format_set(slider, "%1.0f min");
   elm_slider_min_max_set(slider, 0, 60);
   elm_slider_value_set(slider, inst->cfg->batman.alert);
   elm_slider_step_set(slider, 1);
   elm_slider_span_size_set(slider, 100);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(slider, "delay,changed", _update_alert_time, bc);
   elm_object_disabled_set(slider, !show_alert);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   bc->alert_time = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Alert when percent left is at:"));
   elm_slider_unit_format_set(slider, "%1.0f %%");
   elm_slider_indicator_format_set(slider, "%1.0f %%");
   elm_slider_min_max_set(slider, 0, 100);
   elm_slider_value_set(slider, inst->cfg->batman.alert_p);
   elm_slider_step_set(slider, 1);
   elm_slider_span_size_set(slider, 100);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(slider, "delay,changed", _update_alert_percent, bc);
   elm_object_disabled_set(slider, !show_alert);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   bc->alert_percent = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Alert timeout:"));
   elm_slider_unit_format_set(slider, "%1.0f s");
   elm_slider_indicator_format_set(slider, "%1.0f s");
   elm_slider_min_max_set(slider, 1, 300);
   elm_slider_value_set(slider, inst->cfg->batman.alert_timeout);
   elm_slider_step_set(slider, 1);
   elm_slider_span_size_set(slider, 100);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(slider, "delay,changed", _update_alert_timeout, bc);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   bc->alert_timeout = slider;

   elm_object_content_set(frame, box);

   frame = elm_frame_add(main_box);
   elm_object_text_set(frame, _("Power Management"));
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
   elm_object_text_set(o, _("Suspend when below:"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _suspend_changed, bc);
   evas_object_show(o);
   groupy = o;

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 1);
   elm_radio_group_add(o, groupy);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Hibernate when below:"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _suspend_changed, bc);
   evas_object_show(o);

   o = elm_radio_add(box);
   elm_radio_state_value_set(o, 2);
   elm_radio_group_add(o, groupy);
   E_EXPAND(o);
   E_ALIGN(o, 0, 0);
   elm_object_text_set(o, _("Shutdown when below:"));
   elm_box_pack_end(box, o);
   evas_object_smart_callback_add(o, "changed", _suspend_changed, bc);
   evas_object_show(o);

   switch(inst->cfg->batman.suspend_method)
     {
        case SUSPEND:
          elm_radio_value_set(groupy, 0);
          break;
        case HIBERNATE:
          elm_radio_value_set(groupy, 1);
          break;
        case SHUTDOWN:
          elm_radio_value_set(groupy, 2);
          break;
        default:
          elm_radio_value_set(groupy, 0);
     }

   slider = elm_slider_add(box);
   elm_slider_unit_format_set(slider, "%1.0f %%");
   elm_slider_indicator_format_set(slider, "%1.0f %%");
   elm_slider_min_max_set(slider, 0, 100);
   elm_slider_value_set(slider, inst->cfg->batman.suspend_below);
   elm_slider_step_set(slider, 1);
   elm_slider_span_size_set(slider, 100);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(slider, "delay,changed", _update_suspend_percent, bc);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);

   img = elm_icon_add(box);
   evas_object_size_hint_aspect_set(img, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   _icon_theme_file_set(img, "preferences-system-power-management");
   evas_object_show(img);

   but = elm_button_add(box);
   elm_object_part_content_set(but, "icon", img);
   elm_object_text_set(but, _("Power Management Timing"));
   E_EXPAND(but);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(but, "popup", popup);
   evas_object_smart_callback_add(but, "clicked", _power_management_cb, popup);
   elm_box_pack_end(box, but);
   evas_object_show(but);

   elm_object_content_set(frame, box);
   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center_on_zone(popup, zone);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, bc);

   return inst->cfg->batman.configure = popup;
}

