#include "cpumonitor.h"

typedef struct _Thread_Config Thread_Config;

struct _Thread_Config
{
   int interval;
   Instance *inst;
   int status;
};

static void
_cpumonitor_face_update(Instance *inst, int status)
{
   Edje_Message_Int_Set *usage_msg;

   usage_msg = malloc(sizeof(Edje_Message_Int_Set) + 1 * sizeof(int));
   EINA_SAFETY_ON_NULL_RETURN(usage_msg);
   usage_msg->count = 1;
   usage_msg->val[0] = status;
   edje_object_message_send(elm_layout_edje_get(inst->cfg->cpumonitor.o_gadget), EDJE_MESSAGE_INT_SET, 1,
                            usage_msg);
}

static void
_cpumonitor_cb_usage_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;;)
     {
        if (ecore_thread_check(th)) break;
        thc->status = _cpumonitor_proc_getusage(thc->inst);
        ecore_thread_feedback(th, NULL);
        if (ecore_thread_check(th)) break;
        usleep((1000000.0 / 8.0) * (double)thc->interval);
     }
   E_FREE_FUNC(thc, free);
}

static void
_cpumonitor_cb_usage_check_notify(void *data,
                                   Ecore_Thread *th EINA_UNUSED,
                                   void *msg EINA_UNUSED)
{
   Thread_Config *thc = data;
   Instance *inst = thc->inst;

   if (inst->cfg->esm != E_SYSINFO_MODULE_CPUMONITOR && inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;
   if (!inst->cfg) return;
   _cpumonitor_face_update(inst, thc->status);
}

void
_cpumonitor_config_updated(Instance *inst)
{
   Thread_Config *thc;

   if (inst->cfg->cpumonitor.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
   thc = E_NEW(Thread_Config, 1);
   if (thc)
     {
        thc->inst = inst;
        thc->interval = inst->cfg->cpumonitor.poll_interval;
        thc->status = 0;
        inst->cfg->cpumonitor.usage_check_thread =
          ecore_thread_feedback_run(_cpumonitor_cb_usage_check_main,
                                    _cpumonitor_cb_usage_check_notify,
                                    NULL, NULL, thc, EINA_TRUE);
     }
   e_config_save_queue();
}

static void
_cpumonitor_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;

   if (inst->o_main != event_data) return;

   if (inst->cfg->cpumonitor.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_cpumonitor_remove(Instance *inst)
{
   if (inst->cfg->cpumonitor.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
}

static void
_cpumonitor_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->cfg->cpumonitor.o_gadget = elm_layout_add(inst->o_main);
   e_theme_edje_object_set(inst->cfg->cpumonitor.o_gadget, "base/theme/modules/cpumonitor",
                           "e/modules/cpumonitor/main");
   E_EXPAND(inst->cfg->cpumonitor.o_gadget);
   E_FILL(inst->cfg->cpumonitor.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->cpumonitor.o_gadget);
   evas_object_show(inst->cfg->cpumonitor.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _cpumonitor_created_cb, data);
   _cpumonitor_config_updated(inst);
}

Evas_Object *
sysinfo_cpumonitor_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->cpumonitor.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->cpumonitor.o_gadget, "base/theme/modules/cpumonitor",
                           "e/modules/cpumonitor/main");
   E_EXPAND(inst->cfg->cpumonitor.o_gadget);
   E_FILL(inst->cfg->cpumonitor.o_gadget);
   evas_object_show(inst->cfg->cpumonitor.o_gadget);
   _cpumonitor_config_updated(inst);

   return inst->cfg->cpumonitor.o_gadget;
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(sysinfo_config->items, l, ci)
          if (*id == ci->id && ci->esm == E_SYSINFO_MODULE_CPUMONITOR) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(sysinfo_config->items)+1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_CPUMONITOR;
   ci->cpumonitor.poll_interval = 32;
   ci->cpumonitor.total = 0;
   ci->cpumonitor.idle = 0;

   sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);

   return ci;
}

Evas_Object *
cpumonitor_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->cfg->cpumonitor.total = 0;
   inst->cfg->cpumonitor.idle = 0;
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_smart_callback_add(parent, "gadget_created", _cpumonitor_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _cpumonitor_removed_cb, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

