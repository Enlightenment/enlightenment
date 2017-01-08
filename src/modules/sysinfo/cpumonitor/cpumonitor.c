#include "cpumonitor.h"

typedef struct _Thread_Config Thread_Config;

struct _Thread_Config
{
   int interval;
   int cores;
   Instance *inst;
};

static void
_cpumonitor_face_update(Instance *inst)
{
   Edje_Message_Int_Set *usage_msg;
   Eina_List *l;
   CPU_Core *core;

   usage_msg = malloc(sizeof(Edje_Message_Int_Set) + 1 * sizeof(int));
   EINA_SAFETY_ON_NULL_RETURN(usage_msg);
   EINA_LIST_FOREACH(inst->cfg->cpumonitor.cores, l, core)
     {
        usage_msg->count = 1;
        usage_msg->val[0] = core->percent;
        edje_object_message_send(elm_layout_edje_get(core->layout), EDJE_MESSAGE_INT_SET, 1,
                            usage_msg);
     }
}

static void
_cpumonitor_cb_usage_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;;)
     {
        if (ecore_thread_check(th)) break;
        _cpumonitor_proc_getusage(thc->inst);
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
   _cpumonitor_face_update(inst);
}

Evas_Object *
_cpumonitor_add_layout(Instance *inst)
{
   Evas_Object *layout;

   layout = elm_layout_add(inst->cfg->cpumonitor.o_gadget);
   e_theme_edje_object_set(layout, "base/theme/modules/cpumonitor",
                           "e/modules/cpumonitor/main");
   E_EXPAND(layout);
   E_FILL(layout);
   elm_box_pack_end(inst->cfg->cpumonitor.o_gadget, layout);
   evas_object_show(layout);

   return layout;
}

void
_cpumonitor_config_updated(Instance *inst)
{
   Thread_Config *thc;
   CPU_Core *core;
   int i = 0;
   
   if (inst->cfg->cpumonitor.usage_check_thread)
     {
        EINA_LIST_FREE(inst->cfg->cpumonitor.cores, core)
          {
             evas_object_del(core->layout);
             E_FREE_FUNC(core, free);
          }
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
   thc = E_NEW(Thread_Config, 1);
   if (thc)
     {
        thc->inst = inst;
        thc->interval = inst->cfg->cpumonitor.poll_interval;
        thc->cores = _cpumonitor_proc_getcores();
        for (i = 0; i < thc->cores; i++)
          {
             core = E_NEW(CPU_Core, 1);
             core->layout = _cpumonitor_add_layout(inst);
             core->percent = 0;
             core->total = 0;
             core->idle = 0;
             inst->cfg->cpumonitor.cores = eina_list_append(inst->cfg->cpumonitor.cores, core);
          }
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
   CPU_Core *core;

   if (inst->o_main != event_data) return;

   if (inst->cfg->cpumonitor.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
   EINA_LIST_FREE(inst->cfg->cpumonitor.cores, core)
     {
        evas_object_del(core->layout);
        E_FREE_FUNC(core, free);
     }
   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_cpumonitor_remove(Instance *inst)
{
   CPU_Core *core;

   if (inst->cfg->cpumonitor.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
   EINA_LIST_FREE(inst->cfg->cpumonitor.cores, core)
     {
        evas_object_del(core->layout);
        E_FREE_FUNC(core, free);
     }
}

static void
_cpumonitor_eval_instance_aspect(Instance *inst)
{
   Evas_Coord w, h;
   Evas_Coord sw = 1, sh = 1;
   Evas_Object *owner, *ed;
   CPU_Core *first_core;
   int num_cores = eina_list_count(inst->cfg->cpumonitor.cores);

   if (num_cores < 1)
     return;

   owner = e_gadget_site_get(inst->o_main);
   switch (e_gadget_site_orient_get(owner))
     {
        case E_GADGET_SITE_ORIENT_HORIZONTAL:
           evas_object_geometry_get(owner, NULL, NULL, NULL, &sh);
           break;

        case E_GADGET_SITE_ORIENT_VERTICAL:
           evas_object_geometry_get(owner, NULL, NULL, &sw, NULL);
           break;

        default:
           sw = sh = 48;
           break;
     }

   first_core = eina_list_nth(inst->cfg->cpumonitor.cores, 0);
   evas_object_resize(first_core->layout, sw, sh);
   ed = elm_layout_edje_get(first_core->layout);
   edje_object_parts_extends_calc(ed, NULL, NULL, &w, &h);
   if (e_gadget_site_orient_get(owner) == E_GADGET_SITE_ORIENT_VERTICAL)
     h *= num_cores;
   else
     w *= num_cores;
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_cpumonitor_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->cfg->cpumonitor.o_gadget = elm_box_add(inst->o_main);
   elm_box_horizontal_set(inst->cfg->cpumonitor.o_gadget, EINA_TRUE);
   E_EXPAND(inst->cfg->cpumonitor.o_gadget);
   E_FILL(inst->cfg->cpumonitor.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->cpumonitor.o_gadget);
   evas_object_show(inst->cfg->cpumonitor.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _cpumonitor_created_cb, data);
   _cpumonitor_config_updated(inst);
   _cpumonitor_eval_instance_aspect(inst);
}

Evas_Object *
sysinfo_cpumonitor_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->cpumonitor.o_gadget = elm_box_add(parent);
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
   evas_object_smart_callback_add(parent, "gadget_created", _cpumonitor_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _cpumonitor_removed_cb, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

