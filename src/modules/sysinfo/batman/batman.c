#include "batman.h"

Eina_Bool upower;
Eina_List *batman_device_batteries;
Eina_List *batman_device_ac_adapters;
double batman_init_time;

static Eina_Bool _batman_cb_warning_popup_timeout(void *data);
static void      _batman_cb_warning_popup_hide(void *data, Evas *e, Evas_Object *obj, void *event);
static void      _batman_warning_popup_destroy(Instance *inst);
static void      _batman_warning_popup(Instance *inst, int time, double percent);

Eina_List *
_batman_battery_find(const char *udi)
{
   Eina_List *l;
   Battery *bat;
   Eina_List *batteries = NULL;
   EINA_LIST_FOREACH(batman_device_batteries, l, bat)
     { /* these are always stringshared */
       if (udi == bat->udi) batteries = eina_list_append(batteries, bat);
     }

   return batteries;
}

Eina_List *
_batman_ac_adapter_find(const char *udi)
{
   Eina_List *l;
   Ac_Adapter *ac;
   Eina_List *adapters = NULL;
   EINA_LIST_FOREACH(batman_device_ac_adapters, l, ac)
     { /* these are always stringshared */
       if (udi == ac->udi) adapters = eina_list_append(adapters, ac);
     }

   return adapters;
}

static void
_batman_face_level_set(Evas_Object *battery, double level)
{
   Edje_Message_Float msg;
   char buf[256];

   snprintf(buf, sizeof(buf), "%i", (int)(level * 100.0));
   elm_layout_text_set(battery, "e.text.reading", buf);
   if (level < 0.0) level = 0.0;
   else if (level > 1.0)
     level = 1.0;
   msg.val = level;
   edje_object_message_send(elm_layout_edje_get(battery), EDJE_MESSAGE_FLOAT, 1, &msg);
}

static void
_batman_face_time_set(Evas_Object *battery, int t)
{
   char buf[256];
   int hrs, mins;

   if (t < 0) return;

   hrs = (t / 3600);
   mins = ((t) / 60 - (hrs * 60));
   if (mins < 0) mins = 0;
   snprintf(buf, sizeof(buf), "%i:%02i", hrs, mins); 
   elm_layout_text_set(battery, "e.text.time", buf);
}

void
_batman_update(Instance *inst, int full, int time_left, int time_full, Eina_Bool have_battery, Eina_Bool have_power)
{
   static double debounce_time = 0.0;

   if (!inst) return;
   if (!inst->cfg) return;
   if (inst->cfg->esm != E_SYSINFO_MODULE_BATMAN && inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;

   if (have_power != inst->cfg->batman.have_power)
     {
        if (have_power && (full < 100))
          elm_layout_signal_emit(inst->cfg->batman.o_gadget,
                                  "e,state,charging",
                                  "e");
        else
          {
             elm_layout_signal_emit(inst->cfg->batman.o_gadget,
                                     "e,state,discharging",
                                     "e");
             if (inst->popup_battery)
               elm_layout_signal_emit(inst->popup_battery,
                                       "e,state,discharging", "e");
          }
     }
   if (have_battery)
     {
        if (inst->cfg->batman.full != full)
          {
             double val;

             if (full >= 100) val = 1.0;
             else val = (double)full / 100.0;
             _batman_face_level_set(inst->cfg->batman.o_gadget, val);
             if (inst->popup_battery)
               _batman_face_level_set(inst->popup_battery, val);
          }
     }
   else
     {
        _batman_face_level_set(inst->cfg->batman.o_gadget, 0.0);
        elm_layout_text_set(inst->cfg->batman.o_gadget,
                                  "e.text.reading",
                                  _("N/A"));
     }

   if ((time_full < 0) && (time_left != inst->cfg->batman.time_left))
     {
        _batman_face_time_set(inst->cfg->batman.o_gadget, time_left);
        if (inst->popup_battery)
          _batman_face_time_set(inst->popup_battery,
                                 time_left);
     }
   else if ((time_left < 0) && (time_full != inst->cfg->batman.time_full))
     {
        _batman_face_time_set(inst->cfg->batman.o_gadget, time_full);
        if (inst->popup_battery)
          _batman_face_time_set(inst->popup_battery,
                                  time_full);
     }
   if (have_battery &&
       (!have_power) &&
       (full < 100) &&
       (
         (
           (time_left > 0) &&
           inst->cfg->batman.alert &&
           ((time_left / 60) <= inst->cfg->batman.alert)
         ) ||
         (
           inst->cfg->batman.alert_p &&
           (full <= inst->cfg->batman.alert_p)
         )
       )
       )
     {
        double t;

        printf("-------------------------------------- bat warn .. why below\n");
        printf("have_battery = %i\n", (int)have_battery);
        printf("have_power = %i\n", (int)have_power);
        printf("full = %i\n", (int)full);
        printf("time_left = %i\n", (int)time_left);
        printf("inst->cfg->batman.alert = %i\n", (int)inst->cfg->batman.alert);
        printf("inst->cfg->batman.alert_p = %i\n", (int)inst->cfg->batman.alert_p);
        t = ecore_time_get();
        if ((t - debounce_time) > 30.0)
          {
             printf("t-debounce = %3.3f\n", (t - debounce_time));
             debounce_time = t;
             if ((t - batman_init_time) > 5.0)
               _batman_warning_popup(inst, time_left, (double)full / 100.0);
          }
     }
   else if (have_power || ((time_left / 60) > inst->cfg->batman.alert))
     _batman_warning_popup_destroy(inst);
   if ((have_battery) && (!have_power) && (full >= 0) &&
       (inst->cfg->batman.suspend_below > 0) &&
       (full < inst->cfg->batman.suspend_below))
     {
        if (inst->cfg->batman.suspend_method == SUSPEND)
          e_sys_action_do(E_SYS_SUSPEND, NULL);
        else if (inst->cfg->batman.suspend_method == HIBERNATE)
          e_sys_action_do(E_SYS_HIBERNATE, NULL);
        else if (inst->cfg->batman.suspend_method == SHUTDOWN)
          e_sys_action_do(E_SYS_HALT, NULL);
     }
   inst->cfg->batman.full = full;
   inst->cfg->batman.time_left = time_left;
   inst->cfg->batman.have_battery = have_battery;
   inst->cfg->batman.have_power = have_power;

   if (!have_battery)
     e_powersave_mode_set(E_POWERSAVE_MODE_LOW);
   else
     {
        if (have_power)
          e_powersave_mode_set(E_POWERSAVE_MODE_LOW);
        else if (full > 95)
          e_powersave_mode_set(E_POWERSAVE_MODE_MEDIUM);
        else if (full > 30)
          e_powersave_mode_set(E_POWERSAVE_MODE_HIGH);
        else
          e_powersave_mode_set(E_POWERSAVE_MODE_EXTREME);
     }
}

void
_batman_device_update(Instance *inst)
{
   Eina_List *l;
   Battery *bat;
   Ac_Adapter *ac;
   int full = -1;
   int time_left = -1;
   int time_full = -1;
   int have_battery = 0;
   int have_power = 0;
   int charging = 0;

   int batnum = 0;
   int acnum = 0;

   EINA_LIST_FOREACH(batman_device_ac_adapters, l, ac)
     if (ac->present) acnum++;

   EINA_LIST_FOREACH(batman_device_batteries, l, bat)
     {
        if (!bat->got_prop)
          continue;
        have_battery = 1;
        batnum++;
        if (bat->charging == 1) have_power = 1;
        if (full == -1) full = 0;
        if (bat->percent >= 0)
          full += bat->percent;
        else if (bat->last_full_charge > 0)
          full += (bat->current_charge * 100) / bat->last_full_charge;
        else if (bat->design_charge > 0)
          full += (bat->current_charge * 100) / bat->design_charge;
        if (bat->time_left > 0)
          {
             if (time_left < 0) time_left = bat->time_left;
             else time_left += bat->time_left;
          }
        if (bat->time_full > 0)
          {
             if (time_full < 0) time_full = bat->time_full;
             else time_full += bat->time_full;
          }
        charging += bat->charging;
     }

   if ((batman_device_batteries) && (batnum == 0))
     return;  /* not ready yet, no properties received for any battery */

   if (batnum > 0) full /= batnum;
   if ((full == 100) && have_power)
     {
        time_left = -1;
        time_full = -1;
     }
   if (time_left < 1) time_left = -1;
   if (time_full < 1) time_full = -1;

   _batman_update(inst, full, time_left, time_full, have_battery, have_power);
}

void
_batman_config_updated(Instance *inst)
{
   int ok = 0;

   if (!inst->cfg) return;

   if ((inst->cfg->batman.force_mode == UNKNOWN) ||
       (inst->cfg->batman.force_mode == SUBSYSTEM))
     {
#ifdef HAVE_EEZE
        ok = _batman_udev_start(inst);
#elif defined __OpenBSD__ || defined __DragonFly__ || defined __FreeBSD__ || defined __NetBSD__
        ok = _batman_sysctl_start(inst);
#else
        ok = _batman_upower_start();
        if (ok)
          upower = EINA_TRUE;
#endif
     }
   if (ok) return;

   if ((inst->cfg->batman.force_mode == UNKNOWN) ||
       (inst->cfg->batman.force_mode == NOSUBSYSTEM))
     {
        ok = _batman_fallback_start(inst);
     }
}

static void _warning_popup_dismissed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   E_FREE_FUNC(inst->popup_battery, evas_object_del);
   E_FREE_FUNC(inst->warning, evas_object_del);
}


static Eina_Bool
_batman_cb_warning_popup_timeout(void *data)
{
   Instance *inst;

   inst = data;
   elm_ctxpopup_dismiss(inst->warning);
   inst->cfg->batman.alert_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_batman_cb_warning_popup_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Instance *inst = NULL;

   inst = (Instance *)data;
   if ((!inst) || (!inst->warning)) return;
   elm_ctxpopup_dismiss(inst->warning);
}

static void
_batman_warning_popup_destroy(Instance *inst)
{
   if (inst->cfg->batman.alert_timer)
     {
        ecore_timer_del(inst->cfg->batman.alert_timer);
        inst->cfg->batman.alert_timer = NULL;
     }
   if (!inst->warning) return;
   elm_ctxpopup_dismiss(inst->warning);
}

static void
_batman_warning_popup_cb(void *data, unsigned int id)
{
   Instance *inst = data;

   inst->notification_id = id;
}

static void
_batman_warning_popup(Instance *inst, int t, double percent)
{
   Evas_Object *popup_bg = NULL;
   int x, y, w, h;

   if ((!inst) || (inst->warning)) return;

   if (inst->cfg->batman.desktop_notifications)
     {
        E_Notification_Notify n;
        memset(&n, 0, sizeof(E_Notification_Notify));
        n.app_name = _("Battery");
        n.replaces_id = 0;
        n.icon.icon = "battery-low";
        n.summary = _("Your battery is low!");
        n.body = _("AC power is recommended.");
        n.timeout = inst->cfg->batman.alert_timeout * 1000;
        e_notification_client_send(&n, _batman_warning_popup_cb, inst);
        return;
     }

   inst->warning = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(inst->warning, "noblock");
   evas_object_smart_callback_add(inst->warning, "dismissed", _warning_popup_dismissed, inst);
   if (!inst->warning) return;

   popup_bg = elm_layout_add(inst->warning);
   inst->popup_battery = elm_layout_add(popup_bg);

   if ((!popup_bg) || (!inst->popup_battery))
     {
        elm_ctxpopup_dismiss(inst->warning);
        inst->warning = NULL;
        return;
     }

   e_theme_edje_object_set(popup_bg, "base/theme/modules/battery/popup",
                           "e/modules/battery/popup");
   e_theme_edje_object_set(inst->popup_battery, "base/theme/modules/battery",
                           "e/modules/battery/main");
   if (edje_object_part_exists(elm_layout_edje_get(popup_bg), "e.swallow.battery"))
     elm_layout_content_set(popup_bg, "e.swallow.battery", inst->popup_battery);
   else
     elm_layout_content_set(popup_bg, "battery", inst->popup_battery);

   elm_layout_text_set(popup_bg, "e.text.title",
                             _("Your battery is low!"));
   elm_layout_text_set(popup_bg, "e.text.label",
                             _("AC power is recommended."));
   evas_object_show(inst->popup_battery);
   evas_object_show(popup_bg);

   elm_object_content_set(inst->warning, popup_bg);
   e_gadget_util_ctxpopup_place(inst->o_main, inst->warning, inst->cfg->batman.o_gadget);
   evas_object_layer_set(inst->warning, E_LAYER_POPUP);
   evas_object_show(inst->warning);

   evas_object_geometry_get(inst->warning, &x, &y, &w, &h);
   evas_object_event_callback_add(inst->warning, EVAS_CALLBACK_MOUSE_DOWN,
                               _batman_cb_warning_popup_hide, inst);

   _batman_face_time_set(inst->popup_battery, t);
   _batman_face_level_set(inst->popup_battery, percent);
   edje_object_signal_emit(inst->popup_battery, "e,state,discharging", "e");

   if ((inst->cfg->batman.alert_timeout > 0) &&
       (!inst->cfg->batman.alert_timer))
     {
        inst->cfg->batman.alert_timer =
          ecore_timer_add(inst->cfg->batman.alert_timeout,
                          _batman_cb_warning_popup_timeout, inst);
     }
}

static void
_batman_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->cfg->batman.o_gadget), 0, 0, &w, &h);
   if (inst->cfg->esm == E_SYSINFO_MODULE_BATMAN)
     evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
   else
     evas_object_size_hint_aspect_set(inst->cfg->batman.o_gadget, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_batman_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;

   if (inst->o_main != event_data) return;

#ifdef HAVE_EEZE
   _batman_udev_stop(inst);
#elif defined __OpenBSD__ || defined __DragonFly__ || defined __FreeBSD__ || defined __NetBSD__
   _batman_sysctl_stop();
#elif defined(upower)
   _batman_upower_stop();
#else
   _batman_fallback_stop();
#endif

#ifdef HAVE_EEZE
   eeze_shutdown();
#endif

   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_batman_remove, data);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_batman_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

#ifdef HAVE_EEZE
   _batman_udev_stop(inst);
#elif defined __OpenBSD__ || defined __DragonFly__ || defined __FreeBSD__ || defined __NetBSD__
   _batman_sysctl_stop();
#elif defined(upower)
   _batman_upower_stop();
#else
   _batman_fallback_stop();
#endif

#ifdef HAVE_EEZE
   eeze_shutdown();
#endif
}

static void
_batman_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->cfg->batman.full = -2;
   inst->cfg->batman.time_left = -2;
   inst->cfg->batman.time_full = -2;
   inst->cfg->batman.have_battery = -2;
   inst->cfg->batman.have_power = -2;

   inst->cfg->batman.o_gadget = elm_layout_add(inst->o_main);
   e_theme_edje_object_set(inst->cfg->batman.o_gadget, "base/theme/modules/battery",
                           "e/modules/battery/main");
   E_EXPAND(inst->cfg->batman.o_gadget);
   E_FILL(inst->cfg->batman.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->batman.o_gadget);
   evas_object_event_callback_add(inst->cfg->batman.o_gadget, EVAS_CALLBACK_RESIZE, _batman_resize_cb, inst);
   evas_object_show(inst->cfg->batman.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _batman_created_cb, data);
   _batman_config_updated(inst);
}

Evas_Object *
sysinfo_batman_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->batman.full = -2;
   inst->cfg->batman.time_left = -2;
   inst->cfg->batman.time_full = -2;
   inst->cfg->batman.have_battery = -2;
   inst->cfg->batman.have_power = -2;

   inst->cfg->batman.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->batman.o_gadget, "base/theme/modules/battery",
                           "e/modules/battery/main");
   E_EXPAND(inst->cfg->batman.o_gadget);
   E_FILL(inst->cfg->batman.o_gadget);
   evas_object_event_callback_add(inst->cfg->batman.o_gadget, EVAS_CALLBACK_RESIZE, _batman_resize_cb, inst);
   evas_object_show(inst->cfg->batman.o_gadget);
   _batman_config_updated(inst);

   return inst->cfg->batman.o_gadget; 
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(sysinfo_config->items, l, ci)
          if (*id == ci->id && ci->esm == E_SYSINFO_MODULE_BATMAN) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(sysinfo_config->items)+1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_BATMAN;
   ci->batman.poll_interval = 512;
   ci->batman.alert = 30;
   ci->batman.alert_p = 10;
   ci->batman.alert_timeout = 0;
   ci->batman.suspend_below = 0;
   ci->batman.force_mode = 0;
   ci->batman.full = -2;
   ci->batman.time_left = -2;
   ci->batman.time_full = -2;
   ci->batman.have_battery = -2;
   ci->batman.have_power = -2;
#if defined HAVE_EEZE || defined __OpenBSD__ || defined __NetBSD__
   ci->batman.fuzzy = 0;
#endif
   ci->batman.desktop_notifications = 0;

   sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);

   return ci;
}

Evas_Object *
batman_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_smart_callback_add(parent, "gadget_created", _batman_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _batman_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_batman_remove, inst);
   evas_object_show(inst->o_main);

#ifdef HAVE_EEZE
   eeze_init();
#endif

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}
