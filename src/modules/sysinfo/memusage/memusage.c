#include "memusage.h"

typedef struct _Thread_Config Thread_Config;

struct _Thread_Config
{
   int interval;
   Instance *inst;
   int memstatus;
   int swapstatus;
};

static void
_memusage_face_update(Instance *inst, int mem, int swap)
{
   Edje_Message_Int_Set *msg;

   msg = malloc(sizeof(Edje_Message_Int_Set) + 2 * sizeof(int));
   EINA_SAFETY_ON_NULL_RETURN(msg);
   msg->count = 2;
   msg->val[0] = mem;
   msg->val[1] = swap;
   edje_object_message_send(elm_layout_edje_get(inst->cfg->memusage.o_gadget),
                            EDJE_MESSAGE_INT_SET, 1, msg);
   free(msg);
}

static void
_memusage_cb_usage_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;;)
     {
        if (ecore_thread_check(th)) break;
        thc->memstatus = _memusage_proc_getmemusage();
        thc->swapstatus = _memusage_proc_getswapusage();
        ecore_thread_feedback(th, NULL);
        if (ecore_thread_check(th)) break;
        usleep((1000000.0 / 8.0) * (double)thc->interval);
     }
   E_FREE_FUNC(thc, free);
}

static void
_memusage_cb_usage_check_notify(void *data,
                                   Ecore_Thread *th EINA_UNUSED,
                                   void *msg EINA_UNUSED)
{
   Thread_Config *thc = data;
   Instance *inst = thc->inst;

   if (!inst->cfg) return;
   if (inst->cfg->esm != E_SYSINFO_MODULE_MEMUSAGE && inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;

   _memusage_face_update(inst, thc->memstatus, thc->swapstatus);
}

void
_memusage_config_updated(Instance *inst)
{
   Thread_Config *thc;

   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }
   thc = E_NEW(Thread_Config, 1);
   if (thc)
     {
        thc->inst = inst;
        thc->interval = inst->cfg->memusage.poll_interval;
        thc->memstatus = 0;
        thc->swapstatus = 0;
        inst->cfg->memusage.usage_check_thread =
          ecore_thread_feedback_run(_memusage_cb_usage_check_main,
                                    _memusage_cb_usage_check_notify,
                                    NULL, NULL, thc, EINA_TRUE);
     }
   e_config_save_queue();
}

static void
_memusage_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;

   if (inst->o_main != event_data) return;

   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_memusage_remove(Instance *inst)
{
   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }
}

static void
_memusage_eval_instance_aspect(Instance *inst)
{
   Evas_Coord w, h;
   Evas_Coord sw = 0, sh = 0;
   Evas_Object *owner, *ed;

   if (!inst->o_main)
     return;

   owner = e_gadget_site_get(inst->o_main);
   if (!owner)
     return;

   switch (e_gadget_site_orient_get(owner))
     {
        case E_GADGET_SITE_ORIENT_HORIZONTAL:
        case E_GADGET_SITE_ORIENT_NONE:
           evas_object_geometry_get(owner, NULL, NULL, NULL, &sh);
           sw = sh;
           break;

        case E_GADGET_SITE_ORIENT_VERTICAL:
           evas_object_geometry_get(owner, NULL, NULL, &sw, NULL);
           sh = sw;
           break;

        default:
           sw = sh = 48;
           break;
     }

   evas_object_resize(inst->cfg->memusage.o_gadget, sw, sh);
   ed = elm_layout_edje_get(inst->cfg->memusage.o_gadget);
   edje_object_parts_extends_calc(ed, NULL, NULL, &w, &h);
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_memusage_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   int orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   inst->cfg->memusage.o_gadget = elm_layout_add(inst->o_main);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     e_theme_edje_object_set(inst->cfg->memusage.o_gadget,
                             "base/theme/modules/memusage",
                             "e/modules/memusage/main_vert");
   else
     e_theme_edje_object_set(inst->cfg->memusage.o_gadget,
                             "base/theme/modules/memusage",
                             "e/modules/memusage/main");

   E_EXPAND(inst->cfg->memusage.o_gadget);
   E_FILL(inst->cfg->memusage.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->memusage.o_gadget);
   evas_object_show(inst->cfg->memusage.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _memusage_created_cb, data);
   _memusage_eval_instance_aspect(inst);
   _memusage_config_updated(inst);
}

Evas_Object *
sysinfo_memusage_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->memusage.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->memusage.o_gadget, "base/theme/modules/memusage",
                           "e/modules/memusage/main");
   E_EXPAND(inst->cfg->memusage.o_gadget);
   E_FILL(inst->cfg->memusage.o_gadget);
   evas_object_show(inst->cfg->memusage.o_gadget);
   _memusage_eval_instance_aspect(inst);
   _memusage_config_updated(inst);

   return inst->cfg->memusage.o_gadget;
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(sysinfo_config->items, l, ci)
          if (*id == ci->id && ci->esm == E_SYSINFO_MODULE_MEMUSAGE) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(sysinfo_config->items)+1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_MEMUSAGE;
   ci->memusage.poll_interval = 32;

   sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);

   return ci;
}

Evas_Object *
memusage_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_smart_callback_add(parent, "gadget_created", _memusage_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _memusage_removed_cb, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

