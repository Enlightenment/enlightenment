#include "netstatus.h"

typedef struct _Thread_Config Thread_Config;

struct _Thread_Config
{
   int interval;
   Instance *inst;
   const char *rstatus;
   const char *tstatus;
};

static void
_netstatus_face_update(Instance *inst, Eina_Bool trans, const char *status)
{
   if (!trans && status)
     {
        elm_layout_signal_emit(inst->cfg->netstatus.o_gadget, "e,state,received,active", "e");
        elm_layout_text_set(inst->cfg->netstatus.o_gadget, "e.text.received", status);
     }
   else if (trans && status)
     {
        elm_layout_signal_emit(inst->cfg->netstatus.o_gadget, "e,state,transmitted,active", "e");
        elm_layout_text_set(inst->cfg->netstatus.o_gadget, "e.text.transmitted", status);
     }
   else if (!trans && !status)
     {
        elm_layout_signal_emit(inst->cfg->netstatus.o_gadget, "e,state,received,idle", "e");
        elm_layout_text_set(inst->cfg->netstatus.o_gadget, "e.text.received", "Rx: 0");
     }
   else if (trans && !status)
     {
        elm_layout_signal_emit(inst->cfg->netstatus.o_gadget, "e,state,transmitted,idle", "e");
       elm_layout_text_set(inst->cfg->netstatus.o_gadget, "e.text.transmitted", "Tx: 0");
     }
}

static void
_netstatus_cb_usage_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;;)
     {
        if (ecore_thread_check(th)) break;
        E_FREE_FUNC(thc->rstatus, eina_stringshare_del);
        E_FREE_FUNC(thc->tstatus, eina_stringshare_del);
        thc->rstatus = _netstatus_proc_getrstatus(thc->inst);
        thc->tstatus = _netstatus_proc_gettstatus(thc->inst);
        ecore_thread_feedback(th, NULL);
        if (ecore_thread_check(th)) break;
        usleep((1000000.0 / 8.0) * (double)thc->interval);
     }
   E_FREE_FUNC(thc->rstatus, eina_stringshare_del);
   E_FREE_FUNC(thc->tstatus, eina_stringshare_del);
   E_FREE_FUNC(thc, free);
}

static void
_netstatus_cb_usage_check_notify(void *data,
                                   Ecore_Thread *th EINA_UNUSED,
                                   void *msg EINA_UNUSED)
{
   Thread_Config *thc = data;
   Instance *inst = thc->inst;

   if (inst->cfg->esm != E_SYSINFO_MODULE_NETSTATUS && inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;
   if (!inst->cfg) return;
   _netstatus_face_update(inst, EINA_FALSE, thc->rstatus);
   _netstatus_face_update(inst, EINA_TRUE, thc->tstatus);
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

   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
     }

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_netstatus_remove(Instance *inst)
{
   if (inst->cfg->netstatus.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->netstatus.usage_check_thread);
        inst->cfg->netstatus.usage_check_thread = NULL;
     }
}

static void
_netstatus_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->cfg->netstatus.o_gadget = elm_layout_add(inst->o_main);
   e_theme_edje_object_set(inst->cfg->netstatus.o_gadget, "base/theme/modules/netstatus",
                           "e/modules/netstatus/main");
   E_EXPAND(inst->cfg->netstatus.o_gadget);
   E_FILL(inst->cfg->netstatus.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->netstatus.o_gadget);
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
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_smart_callback_add(parent, "gadget_created", _netstatus_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _netstatus_removed_cb, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

