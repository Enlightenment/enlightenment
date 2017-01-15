#include "netstatus.h"

typedef struct _Thread_Config Thread_Config;

struct _Thread_Config
{
   int interval;
   Instance *inst;
   int percent;
   long current;
};

static void
_netstatus_face_update(Instance *inst)
{
   Edje_Message_Int_Set *msg;

   msg = malloc(sizeof(Edje_Message_Int_Set) + 6 * sizeof(int));
   EINA_SAFETY_ON_NULL_RETURN(msg);
   msg->count = 6;
   msg->val[0] = inst->cfg->netstatus.incurrent;
   msg->val[1] = inst->cfg->netstatus.inpercent;
   msg->val[2] = inst->cfg->netstatus.inmax;
   msg->val[3] = inst->cfg->netstatus.outcurrent;
   msg->val[4] = inst->cfg->netstatus.outpercent;
   msg->val[5] = inst->cfg->netstatus.outmax;
   edje_object_message_send(elm_layout_edje_get(inst->cfg->netstatus.o_gadget),
                            EDJE_MESSAGE_INT_SET, 1, msg);
   free(msg);
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
   Evas_Object *label, *popup;
   Evas_Event_Mouse_Down *ev = event_data;
   Instance *inst = data;
   char text[4096];

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 3)
     {
        if (inst->cfg->netstatus.popup)
          {
             elm_ctxpopup_dismiss(inst->cfg->netstatus.popup);
             inst->cfg->netstatus.popup = NULL;
             return;
          }
        popup = elm_ctxpopup_add(e_comp->elm);
        elm_object_style_set(popup, "noblock");
        evas_object_smart_callback_add(popup, "dismissed", _netstatus_popup_dismissed, inst);
        evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _netstatus_popup_deleted, inst);

        snprintf(text, sizeof(text), "%s<br>%s", inst->cfg->netstatus.instring, inst->cfg->netstatus.outstring);
        label = elm_label_add(popup);
        elm_object_style_set(label, "marker");
        elm_object_text_set(label, text);
        elm_object_content_set(popup, label);
        evas_object_show(label);

        e_comp_object_util_autoclose(popup, NULL, NULL, NULL);
        evas_object_show(popup);
        e_gadget_util_ctxpopup_place(inst->o_main, popup, inst->cfg->netstatus.o_gadget);
        inst->cfg->netstatus.popup = popup;
     }
   else
     {
        if (inst->cfg->netstatus.popup)
          {
             elm_ctxpopup_dismiss(inst->cfg->netstatus.popup);
             inst->cfg->netstatus.popup = NULL;
          }
        if (!sysinfo_config) return;
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        if (inst->cfg->esm != E_SYSINFO_MODULE_NETSTATUS)
          netstatus_configure(inst);
        else
          e_gadget_configure(inst->o_main);
     }
}

static void
_netstatus_cb_usage_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;;)
     {
        if (ecore_thread_check(th)) break;
        _netstatus_proc_getrstatus(thc->inst);
        _netstatus_proc_gettstatus(thc->inst);
        ecore_thread_feedback(th, NULL);
        if (ecore_thread_check(th)) break;
        usleep((1000000.0 / 8.0) * (double)thc->interval);
     }
   E_FREE_FUNC(thc, free);
}

static void
_netstatus_cb_usage_check_notify(void *data,
                                   Ecore_Thread *th EINA_UNUSED,
                                   void *msg EINA_UNUSED)
{
   Thread_Config *thc = data;
   Instance *inst = thc->inst;

   if (!inst->cfg) return;
   if (inst->cfg->esm != E_SYSINFO_MODULE_NETSTATUS && inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;

   _netstatus_face_update(inst);
}

void
_netstatus_config_updated(Instance *inst)
{
   Thread_Config *thc;

   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
     }
   thc = E_NEW(Thread_Config, 1);
   if (thc)
     {
        thc->inst = inst;
        thc->interval = inst->cfg->netstatus.poll_interval;
        inst->cfg->netstatus.usage_check_thread =
          ecore_thread_feedback_run(_netstatus_cb_usage_check_main,
                                    _netstatus_cb_usage_check_notify,
                                    NULL, NULL, thc, EINA_TRUE);
     }
   e_config_save_queue();
}

static void
_netstatus_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;

   if (inst->o_main != event_data) return;

   if (inst->cfg->netstatus.popup)
     E_FREE_FUNC(inst->cfg->netstatus.popup, evas_object_del);
   if (inst->cfg->netstatus.configure)
     E_FREE_FUNC(inst->cfg->netstatus.configure, evas_object_del);
   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
     }
   E_FREE_FUNC(inst->cfg->netstatus.instring, eina_stringshare_del);
   E_FREE_FUNC(inst->cfg->netstatus.outstring, eina_stringshare_del);
   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_netstatus_remove, data);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_netstatus_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->cfg->netstatus.popup)
     E_FREE_FUNC(inst->cfg->netstatus.popup, evas_object_del);
   if (inst->cfg->netstatus.configure)
     E_FREE_FUNC(inst->cfg->netstatus.configure, evas_object_del);
   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
     }
   E_FREE_FUNC(inst->cfg->netstatus.instring, eina_stringshare_del);
   E_FREE_FUNC(inst->cfg->netstatus.outstring, eina_stringshare_del);
}

static void
_netstatus_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   e_gadget_configure_cb_set(inst->o_main, _netstatus_configure_cb);

   inst->cfg->netstatus.o_gadget = elm_layout_add(inst->o_main);
   e_theme_edje_object_set(inst->cfg->netstatus.o_gadget, "base/theme/modules/netstatus",
                           "e/modules/netstatus/main");
   E_EXPAND(inst->cfg->netstatus.o_gadget);
   E_FILL(inst->cfg->netstatus.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->netstatus.o_gadget);
   evas_object_event_callback_add(inst->cfg->netstatus.o_gadget, EVAS_CALLBACK_MOUSE_DOWN, _netstatus_mouse_down_cb, inst);
   evas_object_show(inst->cfg->netstatus.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _netstatus_created_cb, data);
   _netstatus_config_updated(inst);
}

Evas_Object *
sysinfo_netstatus_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->netstatus.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->netstatus.o_gadget, "base/theme/modules/netstatus",
                           "e/modules/netstatus/main");
   E_EXPAND(inst->cfg->netstatus.o_gadget);
   E_FILL(inst->cfg->netstatus.o_gadget);
   evas_object_event_callback_add(inst->cfg->netstatus.o_gadget, EVAS_CALLBACK_MOUSE_DOWN, _netstatus_mouse_down_cb, inst);
   evas_object_show(inst->cfg->netstatus.o_gadget);
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
     ci->id = eina_list_count(sysinfo_config->items)+1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_NETSTATUS;
   ci->netstatus.poll_interval = 32;
   ci->netstatus.in = 0;
   ci->netstatus.out = 0;
   ci->netstatus.inmax = 0;
   ci->netstatus.outmax = 0;
   ci->netstatus.incurrent = 0;
   ci->netstatus.outcurrent = 0;
   ci->netstatus.inpercent = 0;
   ci->netstatus.outpercent = 0;
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
   inst->cfg->netstatus.in = 0;
   inst->cfg->netstatus.out = 0;
   inst->cfg->netstatus.inmax = 0;
   inst->cfg->netstatus.outmax = 0;
   inst->cfg->netstatus.incurrent = 0;
   inst->cfg->netstatus.outcurrent = 0;
   inst->cfg->netstatus.inpercent = 0;
   inst->cfg->netstatus.outpercent = 0;
   inst->cfg->netstatus.instring = NULL;
   inst->cfg->netstatus.outstring = NULL;
   inst->cfg->netstatus.popup = NULL;
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_data_set(inst->o_main, "Instance", inst);
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_smart_callback_add(parent, "gadget_created", _netstatus_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _netstatus_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_netstatus_remove, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

