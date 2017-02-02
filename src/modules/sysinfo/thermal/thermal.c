#include "thermal.h"

static void
_thermal_thread_free(Tempthread *tth)
{
#if defined(HAVE_EEZE)
   const char *s;
#endif
   if (!tth) return;

   eina_stringshare_del(tth->sensor_name);
   eina_stringshare_del(tth->sensor_path);
#if defined(HAVE_EEZE)
   EINA_LIST_FREE(tth->tempdevs, s) eina_stringshare_del(s);
#endif
   free(tth->extn);
   free(tth);
}

static void
_thermal_face_level_set(Instance *inst, double level)
{
   Edje_Message_Float msg;

   if (level < 0.0) level = 0.0;
   else if (level > 1.0) level = 1.0;
   msg.val = level;
   edje_object_message_send(elm_layout_edje_get(inst->cfg->thermal.o_gadget), EDJE_MESSAGE_FLOAT, 1, &msg);
}

static void
_thermal_apply(Instance *inst, int temp)
{
   char buf[64];

   if (inst->cfg->thermal.temp == temp) return;
   inst->cfg->thermal.temp = temp;
   if (temp != -999)
     {
        if (inst->cfg->thermal.units == FAHRENHEIT) temp = (temp * 9.0 / 5.0) + 32;

        if (!inst->cfg->thermal.have_temp)
          {
             /* enable therm object */
             elm_layout_signal_emit(inst->cfg->thermal.o_gadget, "e,state,known", "");
             inst->cfg->thermal.have_temp = EINA_TRUE;
          }
        if (inst->cfg->thermal.units == FAHRENHEIT)
          snprintf(buf, sizeof(buf), "%i°F", temp);
        else
          snprintf(buf, sizeof(buf), "%i°C", temp);

        _thermal_face_level_set(inst,
                                    (double)(temp - inst->cfg->thermal.low) /
                                    (double)(inst->cfg->thermal.high - inst->cfg->thermal.low));
        elm_layout_text_set(inst->cfg->thermal.o_gadget, "e.text.reading", buf);
     }
   else
     {
        if (inst->cfg->thermal.have_temp)
          {
             /* disable therm object */
             elm_layout_signal_emit(inst->cfg->thermal.o_gadget, "e,state,unknown", "");
             elm_layout_text_set(inst->cfg->thermal.o_gadget, "e.text.reading", "N/A");
             _thermal_face_level_set(inst, 0.5);
             inst->cfg->thermal.have_temp = EINA_FALSE;
          }
     }
}

#if defined(HAVE_EEZE)
static Eina_Bool
_thermal_udev_poll(void *data)
{
   Tempthread *tth = data;
   int temp = thermal_udev_get(tth);

   _thermal_apply(tth->inst, temp);
   return EINA_TRUE;
}
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
static void
_thermal_check_sysctl(void *data, Ecore_Thread *th)
{
   Tempthread *tth = data;
   int ptemp = -500, temp;

   for (;;)
     {
        if (ecore_thread_check(th)) break;
        temp = thermal_sysctl_get(tth);
        if (ptemp != temp) ecore_thread_feedback(th, (void *)((long)temp));
        ptemp = temp;
        usleep((1000000.0 / 8.0) * (double)tth->poll_interval);
        if (ecore_thread_check(th)) break;
     }
}
#endif

#if !defined(HAVE_EEZE) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__DragonFly__)
static void
_thermal_check_fallback(void *data, Ecore_Thread *th)
{
   Tempthread *tth = data;
   int ptemp = -500, temp;
   for (;;)
     {
        if (ecore_thread_check(th)) break;
        temp = thermal_fallback_get(tth);
        if (ptemp != temp) ecore_thread_feedback(th, (void *)((long)temp));
        ptemp = temp;
        usleep((1000000.0 / 8.0) * (double)tth->poll_interval);
        if (ecore_thread_check(th)) break;
     }
}
#endif

#if !defined(HAVE_EEZE)
static void
_thermal_check_notify(void *data, Ecore_Thread *th, void *msg)
{
   Tempthread *tth  = data;
   Instance *inst = tth->inst;
   int temp = (int)((long)msg);
   if (th != inst->cfg->thermal.th) return;
   _thermal_apply(inst, temp);
}

static void
_thermal_check_done(void *data, Ecore_Thread *th EINA_UNUSED)
{
   _thermal_thread_free(data);
}
#endif

void
_thermal_config_updated(Instance *inst)
{
   Tempthread *tth;

   if (inst->cfg->thermal.th) ecore_thread_cancel(inst->cfg->thermal.th);

   tth = calloc(1, sizeof(Tempthread));
   tth->poll_interval = inst->cfg->thermal.poll_interval;
   tth->sensor_type = inst->cfg->thermal.sensor_type;
   tth->inst = inst;
   if (inst->cfg->thermal.sensor_name)
     tth->sensor_name = eina_stringshare_add(inst->cfg->thermal.sensor_name);

#if defined(HAVE_EEZE)
   _thermal_udev_poll(tth);
   inst->cfg->thermal.poller = ecore_poller_add(ECORE_POLLER_CORE, inst->cfg->thermal.poll_interval,
                                     _thermal_udev_poll, tth);
   inst->cfg->thermal.tth = tth;
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    inst->cfg->thermal.th = ecore_thread_feedback_run(_thermal_check_sysctl,
                                        _thermal_check_notify,
                                        _thermal_check_done,
                                        _thermal_check_done,
                                        tth, EINA_TRUE);  
#else 
   inst->cfg->thermal.th = ecore_thread_feedback_run(_thermal_check_main,
                                        _thermal_check_notify,
                                        _thermal_check_done,
                                        _thermal_check_done,
                                        tth, EINA_TRUE);
#endif
}

static void
_thermal_face_shutdown(Instance *inst)
{
   if (inst->cfg->thermal.th) ecore_thread_cancel(inst->cfg->thermal.th);
   if (inst->cfg->thermal.sensor_name) eina_stringshare_del(inst->cfg->thermal.sensor_name);
#if defined(HAVE_EEZE)
   if (inst->cfg->thermal.poller)
     {
        E_FREE_FUNC(inst->cfg->thermal.poller, ecore_poller_del);
        _thermal_thread_free(inst->cfg->thermal.tth);
     }
#endif
}

static void
_thermal_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->cfg->thermal.o_gadget), 0, 0, &w, &h);
   if (inst->cfg->esm == E_SYSINFO_MODULE_THERMAL)
     evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
   else
     evas_object_size_hint_aspect_set(inst->cfg->thermal.o_gadget, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_thermal_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;

   if (inst->o_main != event_data) return;

   _thermal_face_shutdown(inst);

#if defined(HAVE_EEZE)
   eeze_shutdown();
#endif

   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_thermal_remove, data);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_thermal_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   _thermal_face_shutdown(inst);

#if defined(HAVE_EEZE)
   eeze_shutdown();
#endif
}

static void
_thermal_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->cfg->thermal.temp = 900;
   inst->cfg->thermal.have_temp = EINA_FALSE;

   inst->cfg->thermal.o_gadget = elm_layout_add(inst->o_main);
   e_theme_edje_object_set(inst->cfg->thermal.o_gadget, "base/theme/modules/temperature",
                           "e/modules/temperature/main");
   E_EXPAND(inst->cfg->thermal.o_gadget);
   E_FILL(inst->cfg->thermal.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->thermal.o_gadget);
   evas_object_event_callback_add(inst->cfg->thermal.o_gadget, EVAS_CALLBACK_RESIZE, _thermal_resize_cb, inst);
   evas_object_show(inst->cfg->thermal.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _thermal_created_cb, data);
   _thermal_config_updated(inst);
}

Evas_Object *
sysinfo_thermal_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->thermal.temp = 900;
   inst->cfg->thermal.have_temp = EINA_FALSE;

   inst->cfg->thermal.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->thermal.o_gadget, "base/theme/modules/temperature",
                           "e/modules/temperature/main");
   E_EXPAND(inst->cfg->thermal.o_gadget);
   E_FILL(inst->cfg->thermal.o_gadget);
   evas_object_event_callback_add(inst->cfg->thermal.o_gadget, EVAS_CALLBACK_RESIZE, _thermal_resize_cb, inst);
   evas_object_show(inst->cfg->thermal.o_gadget);
   _thermal_config_updated(inst);

   return inst->cfg->thermal.o_gadget;
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(sysinfo_config->items, l, ci)
          if (*id == ci->id && ci->esm == E_SYSINFO_MODULE_THERMAL) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(sysinfo_config->items)+1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_THERMAL;
   ci->thermal.poll_interval = 128;
   ci->thermal.low = 30;
   ci->thermal.high = 80;
   ci->thermal.sensor_type = SENSOR_TYPE_NONE;
   ci->thermal.sensor_name = NULL;
   ci->thermal.units = CELSIUS;

   sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);

   return ci;
}

Evas_Object *
thermal_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_smart_callback_add(parent, "gadget_created", _thermal_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _thermal_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_thermal_remove, inst);
   evas_object_show(inst->o_main);

#if defined(HAVE_EEZE)
   eeze_init();
#endif

   if (inst->cfg->id < 0) return inst->o_main;
   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

