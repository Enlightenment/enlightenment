#include "netstatus.h"

typedef struct _Thread_Config Thread_Config;

struct _Thread_Config
{
   int                  interval;
   Instance            *inst;
   Eina_Bool            automax;
   time_t               checktime;
   int                  inpercent;
   unsigned long        in;
   unsigned long        incurrent;
   unsigned long        inmax;
   Eina_Stringshare    *instring;
   int                  outpercent;
   unsigned long        out;
   unsigned long        outcurrent;
   unsigned long        outmax;
   Eina_Stringshare    *outstring;
   E_Powersave_Sleeper *sleeper;
};

static void
_netstatus_face_update(Thread_Config *thc)
{
   Edje_Message_Int_Set *msg;

   msg = malloc(sizeof(Edje_Message_Int_Set) + 6 * sizeof(int));
   EINA_SAFETY_ON_NULL_RETURN(msg);
   msg->count = 6;
   msg->val[0] = thc->incurrent;
   msg->val[1] = thc->inpercent;
   msg->val[2] = thc->inmax;
   msg->val[3] = thc->outcurrent;
   msg->val[4] = thc->outpercent;
   msg->val[5] = thc->outmax;
   edje_object_message_send(elm_layout_edje_get(thc->inst->cfg->netstatus.o_gadget),
                            EDJE_MESSAGE_INT_SET, 1, msg);
   E_FREE(msg);

   if (thc->inst->cfg->netstatus.popup)
     {
        char buf[4096];
        snprintf(buf, sizeof(buf), "%s (%d %%)",
                thc->inst->cfg->netstatus.instring,
                thc->inst->cfg->netstatus.inpercent);
        elm_progressbar_value_set(thc->inst->cfg->netstatus.popup_inpbar,
                                  (float)thc->inst->cfg->netstatus.inpercent / 100);
        elm_progressbar_unit_format_set(thc->inst->cfg->netstatus.popup_inpbar, buf);
        memset(buf, 0x00, sizeof(buf));
        snprintf(buf, sizeof(buf), "%s (%d %%)",
                thc->inst->cfg->netstatus.outstring,
                thc->inst->cfg->netstatus.outpercent);
        elm_progressbar_value_set(thc->inst->cfg->netstatus.popup_outpbar,
                                  (float)thc->inst->cfg->netstatus.outpercent / 100);
        elm_progressbar_unit_format_set(thc->inst->cfg->netstatus.popup_outpbar, buf);
     }
}

static Evas_Object *
_netstatus_configure_cb(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "Instance");

   if (!sysinfo_config) return NULL;
   if (inst->cfg->netstatus.popup) return NULL;
   return netstatus_configure(inst);
}

static void
_netstatus_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);
   inst->cfg->netstatus.popup = NULL;
}

static void
_netstatus_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->cfg->netstatus.popup = NULL;
}

static void
_netstatus_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Evas_Object *label, *popup, *table, *pbar;
   Evas_Event_Mouse_Down *ev = event_data;
   Instance *inst = data;
   char text[4096], buf[4096];

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 3)
     {
        if (inst->cfg->netstatus.popup)
          {
             elm_ctxpopup_dismiss(inst->cfg->netstatus.popup);
             return;
          }
        popup = elm_ctxpopup_add(e_comp->elm);
        elm_object_style_set(popup, "noblock");
        evas_object_smart_callback_add(popup, "dismissed", _netstatus_popup_dismissed, inst);
        evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _netstatus_popup_deleted, inst);

        table = elm_table_add(popup);
        E_EXPAND(table);
        E_FILL(table);
        elm_object_content_set(popup, table);
        evas_object_show(table);

        snprintf(text, sizeof(text), "<big><b>%s</b></big>", _("Network Throughput"));

        label = elm_label_add(table);
        E_EXPAND(label); E_ALIGN(label, 0.5, 0.5);
        elm_object_text_set(label, text);
        elm_table_pack(table, label, 0, 0, 2, 1);
        evas_object_show(label);

        label = elm_label_add(table);
        E_ALIGN(label, 0.0, 0.5);
        elm_object_text_set(label, _("Receiving"));
        elm_table_pack(table, label, 0, 1, 1, 1);
        evas_object_show(label);

        snprintf(buf, sizeof(buf), "%s (%d %%)",
                inst->cfg->netstatus.instring,
                inst->cfg->netstatus.inpercent);

        pbar = elm_progressbar_add(table);
        E_EXPAND(pbar);
        E_FILL(pbar);
        elm_progressbar_span_size_set(pbar, 200 * e_scale);
        elm_progressbar_value_set(pbar, (float)inst->cfg->netstatus.inpercent / 100);
        elm_table_pack(table, pbar, 1, 1, 1, 1);
        evas_object_show(pbar);
        inst->cfg->netstatus.popup_inpbar = pbar;

        label = elm_label_add(table);
        E_ALIGN(label, 0.0, 0.5);
        elm_object_text_set(label, _("Sending"));
        elm_table_pack(table, label, 0, 2, 1, 1);
        evas_object_show(label);

        memset(buf, 0x00, sizeof(buf));
        snprintf(buf, sizeof(buf), "%s (%d %%)",
                inst->cfg->netstatus.outstring,
                inst->cfg->netstatus.outpercent);

        pbar = elm_progressbar_add(table);
        E_EXPAND(pbar);
        E_FILL(pbar);
        elm_progressbar_span_size_set(pbar, 200 * e_scale);
        elm_progressbar_value_set(pbar, (float)inst->cfg->netstatus.outpercent / 100);
        elm_progressbar_unit_format_set(pbar, buf);
        elm_table_pack(table, pbar, 1, 2, 1, 1);
        evas_object_show(pbar);
        inst->cfg->netstatus.popup_outpbar = pbar;

        e_gadget_util_ctxpopup_place(inst->o_main, popup,
                                     inst->cfg->netstatus.o_gadget);
        evas_object_show(popup);
        inst->cfg->netstatus.popup = popup;
     }
   else
     {
        if (inst->cfg->netstatus.popup)
          elm_ctxpopup_dismiss(inst->cfg->netstatus.popup);
        if (!sysinfo_config) return;
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        if (inst->cfg->esm != E_SYSINFO_MODULE_NETSTATUS)
          netstatus_configure(inst);
        else
          e_gadget_configure(inst->o_main);
     }
}

static void
_netstatus_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->cfg->netstatus.o_gadget), 0, 0, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if (inst->cfg->esm == E_SYSINFO_MODULE_NETSTATUS)
     evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
   else
     evas_object_size_hint_aspect_set(inst->cfg->netstatus.o_gadget, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_netstatus_cb_usage_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;; )
     {
        char rin[4096], rout[4096];

        if (ecore_thread_check(th)) break;
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
        _netstatus_sysctl_getstatus(thc->automax, &thc->checktime, &thc->in, &thc->incurrent,
            &thc->inmax, &thc->inpercent, &thc->out, &thc->outcurrent, &thc->outmax,
            &thc->outpercent);
#else
        _netstatus_proc_getstatus(thc->automax, &thc->checktime, &thc->in, &thc->incurrent,
            &thc->inmax, &thc->inpercent, &thc->out, &thc->outcurrent, &thc->outmax,
            &thc->outpercent);
#endif
        if (!thc->incurrent)
          {
             snprintf(rin, sizeof(rin), "0 B/s");
          }
        else
          {
             if (thc->incurrent > 1048576)
               snprintf(rin, sizeof(rin), "%.2f MB/s", ((float)thc->incurrent / 1048576));
             else if ((thc->incurrent > 1024) && (thc->incurrent < 1048576))
               snprintf(rin, sizeof(rin), "%lu KB/s", (thc->incurrent / 1024));
             else
               snprintf(rin, sizeof(rin), "%lu B/s", thc->incurrent);
          }
        eina_stringshare_replace(&thc->instring, rin);
        if (!thc->outcurrent)
          {
             snprintf(rout, sizeof(rout), "0 B/s");
          }
        else
          {
             if (thc->outcurrent > 1048576)
               snprintf(rout, sizeof(rout), "%.2f MB/s", ((float)thc->outcurrent / 1048576));
             else if ((thc->outcurrent > 1024) && (thc->outcurrent < 1048576))
               snprintf(rout, sizeof(rout), "%lu KB/s", (thc->outcurrent / 1024));
             else
               snprintf(rout, sizeof(rout), "%lu B/s", thc->outcurrent);
          }
        eina_stringshare_replace(&thc->outstring, rout);
        ecore_thread_feedback(th, NULL);
        if (ecore_thread_check(th)) break;
        e_powersave_sleeper_sleep(thc->sleeper, thc->interval);
        if (ecore_thread_check(th)) break;
     }
}

static void
_netstatus_cb_usage_check_notify(void *data,
                                 Ecore_Thread *th EINA_UNUSED,
                                 void *msg EINA_UNUSED)
{
   Thread_Config *thc = data;

   if (!thc->inst->cfg) return;
   if (thc->inst->cfg->esm != E_SYSINFO_MODULE_NETSTATUS && thc->inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;

   eina_stringshare_replace(&thc->inst->cfg->netstatus.instring, thc->instring);
   eina_stringshare_replace(&thc->inst->cfg->netstatus.outstring, thc->outstring);
   thc->inst->cfg->netstatus.inpercent = thc->inpercent;
   thc->inst->cfg->netstatus.outpercent = thc->outpercent;
   _netstatus_face_update(thc);
}

static void
_netstatus_cb_usage_check_end(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Thread_Config *thc = data;
   e_powersave_sleeper_free(thc->sleeper);
   E_FREE_FUNC(thc->instring, eina_stringshare_del);
   E_FREE_FUNC(thc->outstring, eina_stringshare_del);
   E_FREE(thc);
}

static Eina_Bool
_screensaver_on(void *data)
{
   Instance *inst = data;

   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_screensaver_off(void *data)
{
   Instance *inst = data;

   _netstatus_config_updated(inst);

   return ECORE_CALLBACK_RENEW;
}

void
_netstatus_config_updated(Instance *inst)
{
   Thread_Config *thc;

   if (inst->cfg->id == -1)
     {
        thc = E_NEW(Thread_Config, 1);
        if (thc)
          {
             thc->inst = inst;
             thc->inpercent = 75;
             thc->outpercent = 30;
             _netstatus_face_update(thc);
             E_FREE(thc);
          }
        return;
     }
   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
     }
   thc = E_NEW(Thread_Config, 1);
   if (thc)
     {
        thc->inst = inst;
        thc->sleeper = e_powersave_sleeper_new();
        thc->interval = inst->cfg->netstatus.poll_interval;
        thc->in = 0;
        thc->inmax = inst->cfg->netstatus.inmax;
        thc->incurrent = 0;
        thc->inpercent = 0;
        thc->instring = NULL;
        thc->out = 0;
        thc->outmax = inst->cfg->netstatus.outmax;
        thc->outcurrent = 0;
        thc->outpercent = 0;
        thc->outstring = NULL;
        thc->automax = inst->cfg->netstatus.automax;
        inst->cfg->netstatus.usage_check_thread =
          ecore_thread_feedback_run(_netstatus_cb_usage_check_main,
                                    _netstatus_cb_usage_check_notify,
                                    _netstatus_cb_usage_check_end,
                                    _netstatus_cb_usage_check_end, thc, EINA_TRUE);
     }
   e_config_save_queue();
}

static void
_netstatus_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->o_main != event_data) return;

   if (inst->cfg->netstatus.popup)
     E_FREE_FUNC(inst->cfg->netstatus.popup, evas_object_del);
   if (inst->cfg->netstatus.configure)
     E_FREE_FUNC(inst->cfg->netstatus.configure, evas_object_del);
   evas_object_smart_callback_del_full(e_gadget_site_get(inst->o_main), "gadget_removed",
                                       _netstatus_removed_cb, inst);
   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_netstatus_remove, data);
   EINA_LIST_FREE(inst->cfg->netstatus.handlers, handler)
     ecore_event_handler_del(handler);
   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
     }
   E_FREE_FUNC(inst->cfg->netstatus.instring, eina_stringshare_del);
   E_FREE_FUNC(inst->cfg->netstatus.outstring, eina_stringshare_del);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   if (inst->cfg->id >= 0)
     sysinfo_instances = eina_list_remove(sysinfo_instances, inst);
   E_FREE(inst->cfg);
   E_FREE(inst);
}

void
sysinfo_netstatus_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->cfg->netstatus.popup)
     E_FREE_FUNC(inst->cfg->netstatus.popup, evas_object_del);
   if (inst->cfg->netstatus.configure)
     E_FREE_FUNC(inst->cfg->netstatus.configure, evas_object_del);
   EINA_LIST_FREE(inst->cfg->netstatus.handlers, handler)
     ecore_event_handler_del(handler);
   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
        return;
     }
   E_FREE_FUNC(inst->cfg->netstatus.instring, eina_stringshare_del);
   E_FREE_FUNC(inst->cfg->netstatus.outstring, eina_stringshare_del);
}

static void
_netstatus_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   E_Gadget_Site_Orient orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   e_gadget_configure_cb_set(inst->o_main, _netstatus_configure_cb);

   inst->cfg->netstatus.popup = NULL;
   inst->cfg->netstatus.instring = NULL;
   inst->cfg->netstatus.outstring = NULL;
   inst->cfg->netstatus.inpercent = 0;
   inst->cfg->netstatus.outpercent = 0;

   inst->cfg->netstatus.o_gadget = elm_layout_add(inst->o_main);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     e_theme_edje_object_set(inst->cfg->netstatus.o_gadget,
                             "base/theme/gadget/netstatus",
                             "e/gadget/netstatus/main_vert");
   else
     e_theme_edje_object_set(inst->cfg->netstatus.o_gadget, "base/theme/gadget/netstatus",
                             "e/gadget/netstatus/main");
   E_EXPAND(inst->cfg->netstatus.o_gadget);
   E_FILL(inst->cfg->netstatus.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->netstatus.o_gadget);
   evas_object_event_callback_add(inst->cfg->netstatus.o_gadget, EVAS_CALLBACK_MOUSE_DOWN, _netstatus_mouse_down_cb, inst);
   evas_object_event_callback_add(inst->cfg->netstatus.o_gadget, EVAS_CALLBACK_RESIZE, _netstatus_resize_cb, inst);
   evas_object_show(inst->cfg->netstatus.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _netstatus_created_cb, data);

   E_LIST_HANDLER_APPEND(inst->cfg->netstatus.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->netstatus.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);

   _netstatus_config_updated(inst);
}

Evas_Object *
sysinfo_netstatus_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->netstatus.popup = NULL;
   inst->cfg->netstatus.instring = NULL;
   inst->cfg->netstatus.outstring = NULL;
   inst->cfg->netstatus.inpercent = 0;
   inst->cfg->netstatus.outpercent = 0;

   inst->cfg->netstatus.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->netstatus.o_gadget, "base/theme/gadget/netstatus",
                           "e/gadget/netstatus/main");
   E_EXPAND(inst->cfg->netstatus.o_gadget);
   E_FILL(inst->cfg->netstatus.o_gadget);
   evas_object_event_callback_add(inst->cfg->netstatus.o_gadget, EVAS_CALLBACK_MOUSE_DOWN, _netstatus_mouse_down_cb, inst);
   evas_object_event_callback_add(inst->cfg->netstatus.o_gadget, EVAS_CALLBACK_RESIZE, _netstatus_resize_cb, inst);
   evas_object_show(inst->cfg->netstatus.o_gadget);

   E_LIST_HANDLER_APPEND(inst->cfg->netstatus.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->netstatus.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);

   _netstatus_config_updated(inst);

   return inst->cfg->netstatus.o_gadget;
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(sysinfo_config->items, l, ci)
          if (*id == ci->id && ci->esm == E_SYSINFO_MODULE_NETSTATUS) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(sysinfo_config->items) + 1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_NETSTATUS;
   ci->netstatus.poll_interval = 32;
   ci->netstatus.automax = EINA_TRUE;
   ci->netstatus.receive_units = NETSTATUS_UNIT_BYTES;
   ci->netstatus.send_units = NETSTATUS_UNIT_BYTES;
   ci->netstatus.instring = NULL;
   ci->netstatus.outstring = NULL;
   ci->netstatus.popup = NULL;
   ci->netstatus.configure = NULL;
   sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);

   return ci;
}

Evas_Object *
netstatus_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->cfg->netstatus.instring = NULL;
   inst->cfg->netstatus.outstring = NULL;
   inst->cfg->netstatus.popup = NULL;
   inst->o_main = elm_box_add(parent);
   evas_object_data_set(inst->o_main, "Instance", inst);
   evas_object_smart_callback_add(parent, "gadget_created", _netstatus_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _netstatus_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_netstatus_remove, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

