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
   EINA_LIST_FREE(tth->tempdevs, s)
     eina_stringshare_del(s);
#endif
   e_powersave_sleeper_free(tth->sleeper);
   E_FREE(tth->extn);
   E_FREE(tth);
}

static void
_thermal_face_level_set(Instance *inst, double level)
{
   Edje_Message_Float msg;

   if (level < 0.0) level = 0.0;
   else if (level > 1.0)
     level = 1.0;
   inst->cfg->thermal.percent = level * 100;
   msg.val = level;
   edje_object_message_send(elm_layout_edje_get(inst->cfg->thermal.o_gadget), EDJE_MESSAGE_FLOAT, 1, &msg);
}

static void
_thermal_apply(Instance *inst, int temp)
{
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

        _thermal_face_level_set(inst,
                                (double)(temp - inst->cfg->thermal.low) /
                                (double)(inst->cfg->thermal.high - inst->cfg->thermal.low));
     }
   else
     {
        if (inst->cfg->thermal.have_temp)
          {
             /* disable therm object */
             elm_layout_signal_emit(inst->cfg->thermal.o_gadget, "e,state,unknown", "");
             _thermal_face_level_set(inst, 0.5);
             inst->cfg->thermal.have_temp = EINA_FALSE;
          }
     }
   if (inst->cfg->thermal.popup)
     {
        char buf[4096];

        if (inst->cfg->thermal.units == FAHRENHEIT)
          snprintf(buf, sizeof(buf), "%d F (%d %%)",
                   (int)((inst->cfg->thermal.temp * 9.0 / 5.0) + 32),
                   inst->cfg->thermal.percent);
        else
          snprintf(buf, sizeof(buf), "%d C (%d %%)",
                   (int)inst->cfg->thermal.temp,
                   inst->cfg->thermal.percent);
        elm_progressbar_value_set(inst->cfg->thermal.popup_pbar,
                                  (float)inst->cfg->thermal.percent / 100);
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

   for (;; )
     {
        if (ecore_thread_check(th)) break;
        temp = thermal_sysctl_get(tth);
        if (ptemp != temp) ecore_thread_feedback(th, (void *)((long)temp));
        ptemp = temp;
        e_powersave_sleeper_sleep(tth->sleeper, tth->poll_interval);
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
   for (;; )
     {
        if (ecore_thread_check(th)) break;
        temp = thermal_fallback_get(tth);
        if (ptemp != temp) ecore_thread_feedback(th, (void *)((long)temp));
        ptemp = temp;
        e_powersave_sleeper_sleep(tth->sleeper, tth->poll_interval);
        if (ecore_thread_check(th)) break;
     }
}

#endif

#if !defined(HAVE_EEZE)
static void
_thermal_check_notify(void *data, Ecore_Thread *th, void *msg)
{
   Tempthread *tth = data;
   int temp = (int)((long)msg);
   if (th != tth->inst->cfg->thermal.th) return;
   _thermal_apply(tth->inst, temp);
}

static void
_thermal_check_done(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Tempthread *tth = data;
   _thermal_thread_free(tth);
}

#endif

static Evas_Object *
_thermal_configure_cb(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "Instance");

   if (!sysinfo_config) return NULL;
   if (inst->cfg->thermal.popup) return NULL;
   return thermal_configure(inst);
}

static void
_thermal_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);

   inst->cfg->thermal.popup = NULL;
   inst->cfg->thermal.popup_pbar = NULL;
}

static void
_thermal_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->cfg->thermal.popup = NULL;
}

static Evas_Object *
_thermal_popup_create(Instance *inst)
{
   Evas_Object *popup, *table, *label, *pbar;
   char text[4096], buf[100];

   popup = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(popup, "noblock");
   evas_object_smart_callback_add(popup, "dismissed",
                                  _thermal_popup_dismissed, inst);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL,
                                  _thermal_popup_deleted, inst);

   table = elm_table_add(popup);
   E_EXPAND(table);
   E_FILL(table);
   elm_object_content_set(popup, table);
   evas_object_show(table);

   snprintf(text, sizeof(text), "<big><b>%s</b></big>", _("Temperature"));

   label = elm_label_add(table);
   E_EXPAND(label); E_ALIGN(label, 0.5, 0.5);
   elm_object_text_set(label, text);
   elm_table_pack(table, label, 0, 0, 2, 1);
   evas_object_show(label);

   if (inst->cfg->thermal.units == FAHRENHEIT)
     snprintf(buf, sizeof(buf), "%d F (%d %%)",
              (int)((inst->cfg->thermal.temp * 9.0 / 5.0) + 32),
              inst->cfg->thermal.percent);
   else
     snprintf(buf, sizeof(buf), "%d C (%d %%)",
              (int)inst->cfg->thermal.temp,
              inst->cfg->thermal.percent);

   pbar = elm_progressbar_add(table);
   E_EXPAND(pbar); E_FILL(pbar);
   elm_progressbar_span_size_set(pbar, 200 * e_scale);
   elm_progressbar_value_set(pbar, (float)inst->cfg->thermal.percent / 100);
   elm_progressbar_unit_format_set(pbar, buf);
   elm_table_pack(table, pbar, 0, 1, 2, 1);
   evas_object_show(pbar);
   inst->cfg->thermal.popup_pbar = pbar;

   e_gadget_util_ctxpopup_place(inst->o_main, popup,
                                inst->cfg->thermal.o_gadget);
   evas_object_show(popup);

   return popup;
}

static void
_thermal_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Evas_Event_Mouse_Down *ev = event_data;
   Instance *inst = data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 3)
     {
        if (inst->cfg->thermal.popup)
          elm_ctxpopup_dismiss(inst->cfg->thermal.popup);
        else
          inst->cfg->thermal.popup = _thermal_popup_create(inst);
     }
   else
     {
        if (inst->cfg->thermal.popup)
          elm_ctxpopup_dismiss(inst->cfg->thermal.popup);
        if (!sysinfo_config) return;
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        if (inst->cfg->esm != E_SYSINFO_MODULE_THERMAL)
          thermal_configure(inst);
        else
          e_gadget_configure(inst->o_main);
     }
}

static Eina_Bool
_screensaver_on(void *data)
{
   Instance *inst = data;

   if (inst->cfg->thermal.th)
     {
        ecore_thread_cancel(inst->cfg->thermal.th);
        inst->cfg->thermal.th = NULL;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_screensaver_off(void *data)
{
   Instance *inst = data;

   _thermal_config_updated(inst);

   return ECORE_CALLBACK_RENEW;
}

void
_thermal_config_updated(Instance *inst)
{
   Tempthread *tth;

   if (inst->cfg->id == -1)
     {
        _thermal_face_level_set(inst, .60);
        return;
     }
   if (inst->cfg->thermal.th) ecore_thread_cancel(inst->cfg->thermal.th);

   tth = calloc(1, sizeof(Tempthread));
   tth->poll_interval = inst->cfg->thermal.poll_interval;
   tth->sensor_type = inst->cfg->thermal.sensor_type;
   tth->inst = inst;
   tth->sleeper = e_powersave_sleeper_new();
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
   inst->cfg->thermal.th = ecore_thread_feedback_run(_thermal_check_fallback,
                                                     _thermal_check_notify,
                                                     _thermal_check_done,
                                                     _thermal_check_done,
                                                     tth, EINA_TRUE);
#endif
}

static void
_thermal_face_shutdown(Instance *inst)
{
#if defined(HAVE_EEZE)
   if (inst->cfg->thermal.poller)
     {
        E_FREE_FUNC(inst->cfg->thermal.poller, ecore_poller_del);
        _thermal_thread_free(inst->cfg->thermal.tth);
     }
#endif
   if (inst->cfg->thermal.sensor_name) eina_stringshare_del(inst->cfg->thermal.sensor_name);
}

static void
_thermal_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->cfg->thermal.o_gadget), 0, 0, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if (inst->cfg->esm == E_SYSINFO_MODULE_THERMAL)
     evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
   else
     evas_object_size_hint_aspect_set(inst->cfg->thermal.o_gadget, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_thermal_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->o_main != event_data) return;

   if (inst->cfg->thermal.popup_pbar)
     E_FREE_FUNC(inst->cfg->thermal.popup_pbar, evas_object_del);
   if (inst->cfg->thermal.popup)
     E_FREE_FUNC(inst->cfg->thermal.popup, evas_object_del);
   if (inst->cfg->thermal.configure)
     E_FREE_FUNC(inst->cfg->thermal.configure, evas_object_del);
   EINA_LIST_FREE(inst->cfg->thermal.handlers, handler)
     ecore_event_handler_del(handler);
   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_thermal_remove, data);
   evas_object_smart_callback_del_full(e_gadget_site_get(inst->o_main), "gadget_removed",
                                       _thermal_removed_cb, inst);
   if (inst->cfg->thermal.th)
     {
        ecore_thread_cancel(inst->cfg->thermal.th);
        inst->cfg->thermal.th = NULL;
     }
   _thermal_face_shutdown(inst);
   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   if (inst->cfg->id >= 0)
     sysinfo_instances = eina_list_remove(sysinfo_instances, inst);
   E_FREE(inst->cfg);
   E_FREE(inst);
}

void
sysinfo_thermal_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->cfg->thermal.popup_pbar)
     E_FREE_FUNC(inst->cfg->thermal.popup_pbar, evas_object_del);
   if (inst->cfg->thermal.popup)
     E_FREE_FUNC(inst->cfg->thermal.popup, evas_object_del);
   if (inst->cfg->thermal.configure)
     E_FREE_FUNC(inst->cfg->thermal.configure, evas_object_del);
   EINA_LIST_FREE(inst->cfg->thermal.handlers, handler)
     ecore_event_handler_del(handler);
   if (inst->cfg->thermal.th)
     {
        ecore_thread_cancel(inst->cfg->thermal.th);
        inst->cfg->thermal.th = NULL;
     }
   _thermal_face_shutdown(inst);
}

static void
_thermal_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   E_Gadget_Site_Orient orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   e_gadget_configure_cb_set(inst->o_main, _thermal_configure_cb);

   inst->cfg->thermal.temp = 900;
   inst->cfg->thermal.percent = 0;
   inst->cfg->thermal.have_temp = EINA_FALSE;

   inst->cfg->thermal.o_gadget = elm_layout_add(inst->o_main);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     e_theme_edje_object_set(inst->cfg->thermal.o_gadget,
                             "base/theme/gadget/thermal",
                             "e/gadget/thermal/main_vert");
   else
     e_theme_edje_object_set(inst->cfg->thermal.o_gadget, "base/theme/gadget/thermal",
                             "e/gadget/thermal/main");
   E_EXPAND(inst->cfg->thermal.o_gadget);
   E_FILL(inst->cfg->thermal.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->thermal.o_gadget);
   evas_object_event_callback_add(inst->cfg->thermal.o_gadget,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _thermal_mouse_down_cb, inst);
   evas_object_event_callback_add(inst->cfg->thermal.o_gadget, EVAS_CALLBACK_RESIZE, _thermal_resize_cb, inst);
   evas_object_show(inst->cfg->thermal.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _thermal_created_cb, data);

   E_LIST_HANDLER_APPEND(inst->cfg->thermal.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->thermal.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);

   _thermal_config_updated(inst);
}

Evas_Object *
sysinfo_thermal_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->thermal.temp = 900;
   inst->cfg->thermal.percent = 0;
   inst->cfg->thermal.have_temp = EINA_FALSE;

   inst->cfg->thermal.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->thermal.o_gadget, "base/theme/gadget/thermal",
                           "e/gadget/thermal/main");
   E_EXPAND(inst->cfg->thermal.o_gadget);
   E_FILL(inst->cfg->thermal.o_gadget);
   evas_object_event_callback_add(inst->cfg->thermal.o_gadget,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _thermal_mouse_down_cb, inst);
   evas_object_event_callback_add(inst->cfg->thermal.o_gadget, EVAS_CALLBACK_RESIZE, _thermal_resize_cb, inst);
   evas_object_show(inst->cfg->thermal.o_gadget);

   E_LIST_HANDLER_APPEND(inst->cfg->thermal.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->thermal.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);

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
     ci->id = eina_list_count(sysinfo_config->items) + 1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_THERMAL;
   ci->thermal.poll_interval = 128;
   ci->thermal.low = 30;
   ci->thermal.high = 80;
   ci->thermal.sensor_type = SENSOR_TYPE_NONE;
   ci->thermal.sensor_name = NULL;
   ci->thermal.units = CELSIUS;
   ci->thermal.configure = NULL;

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
   evas_object_data_set(inst->o_main, "Instance", inst);
   evas_object_smart_callback_add(parent, "gadget_created", _thermal_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _thermal_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_thermal_remove, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;
   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

