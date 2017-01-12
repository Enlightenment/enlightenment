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
   Eina_List *l;
   CPU_Core *core;

   EINA_LIST_FOREACH(inst->cfg->cpumonitor.cores, l, core)
     {
        Edje_Message_Int_Set *usage_msg;
        usage_msg = malloc(sizeof(Edje_Message_Int_Set) + 1 * sizeof(int));
        EINA_SAFETY_ON_NULL_RETURN(usage_msg);
        usage_msg->count = 1;
        usage_msg->val[0] = core->percent;
        edje_object_message_send(elm_layout_edje_get(core->layout), EDJE_MESSAGE_INT_SET, 1,
                            usage_msg);
        free(usage_msg);
     }
}

static Evas_Object *
_cpumonitor_configure_cb(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "Instance");

   if (!sysinfo_config) return NULL;
   if (inst->cfg->cpumonitor.popup) return NULL;
   return cpumonitor_configure(inst);
}

static void
_cpumonitor_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);
   inst->cfg->cpumonitor.popup = NULL;
}

static void
_cpumonitor_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->cfg->cpumonitor.popup = NULL;
}

static void
_cpumonitor_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Evas_Object *label, *popup;
   Evas_Event_Mouse_Down *ev = event_data;
   Instance *inst = data;
   char text[4096];

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 3)
     {
        if (inst->cfg->cpumonitor.popup)
          {
             elm_ctxpopup_dismiss(inst->cfg->cpumonitor.popup);
             inst->cfg->cpumonitor.popup = NULL;
             return;
          }
        popup = elm_ctxpopup_add(e_comp->elm);
        elm_object_style_set(popup, "noblock");
        evas_object_smart_callback_add(popup, "dismissed", _cpumonitor_popup_dismissed, inst);   
        evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _cpumonitor_popup_deleted, inst);

        snprintf(text, sizeof(text), "%s: %d%%", _("Total CPU Usage"), inst->cfg->cpumonitor.percent);
        label = elm_label_add(popup);
        elm_object_style_set(label, "marker");
        elm_object_text_set(label, text);
        elm_object_content_set(popup, label);
        evas_object_show(label);
   
        e_comp_object_util_autoclose(popup, NULL, NULL, NULL);
        evas_object_show(popup);
        e_gadget_util_ctxpopup_place(inst->o_main, popup, inst->cfg->cpumonitor.o_gadget);
        inst->cfg->cpumonitor.popup = popup;
     }
   else
     {
        if (inst->cfg->cpumonitor.popup)
          {
             elm_ctxpopup_dismiss(inst->cfg->cpumonitor.popup);
             inst->cfg->cpumonitor.popup = NULL;
          }
        if (!sysinfo_config) return;
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        if (inst->cfg->esm != E_SYSINFO_MODULE_CPUMONITOR)
          cpumonitor_configure(inst);
        else
          e_gadget_configure(inst->o_main);
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

   if (!inst->cfg) return;
   if (inst->cfg->esm != E_SYSINFO_MODULE_CPUMONITOR && inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;

   _cpumonitor_face_update(inst);
}

Evas_Object *
_cpumonitor_add_layout(Instance *inst)
{
   Evas_Object *layout;
   int orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   layout = elm_layout_add(inst->cfg->cpumonitor.o_gadget);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     e_theme_edje_object_set(layout, "base/theme/modules/cpumonitor",
                             "e/modules/cpumonitor/main_vert");
   else
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

   if (inst->cfg->cpumonitor.popup)
     E_FREE_FUNC(inst->cfg->cpumonitor.popup, evas_object_del);
   if (inst->cfg->cpumonitor.configure)
     E_FREE_FUNC(inst->cfg->cpumonitor.configure, evas_object_del);
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
   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_cpumonitor_remove, data);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_cpumonitor_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   CPU_Core *core;

   if (inst->cfg->cpumonitor.popup)
     E_FREE_FUNC(inst->cfg->cpumonitor.popup, evas_object_del);
   if (inst->cfg->cpumonitor.configure)
     E_FREE_FUNC(inst->cfg->cpumonitor.configure, evas_object_del);
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
   int orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   e_gadget_configure_cb_set(inst->o_main, _cpumonitor_configure_cb);

   inst->cfg->cpumonitor.o_gadget = elm_box_add(inst->o_main);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     elm_box_horizontal_set(inst->cfg->cpumonitor.o_gadget, EINA_FALSE);
   else
     elm_box_horizontal_set(inst->cfg->cpumonitor.o_gadget, EINA_TRUE);
   E_EXPAND(inst->cfg->cpumonitor.o_gadget);
   E_FILL(inst->cfg->cpumonitor.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->cpumonitor.o_gadget);
   evas_object_event_callback_add(inst->cfg->cpumonitor.o_gadget, EVAS_CALLBACK_MOUSE_DOWN, _cpumonitor_mouse_down_cb, inst);
   evas_object_show(inst->cfg->cpumonitor.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _cpumonitor_created_cb, data);
   _cpumonitor_config_updated(inst);
   _cpumonitor_eval_instance_aspect(inst);
}

Evas_Object *
sysinfo_cpumonitor_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->cpumonitor.o_gadget = elm_box_add(parent);
   elm_box_horizontal_set(inst->cfg->cpumonitor.o_gadget, EINA_TRUE);
   E_EXPAND(inst->cfg->cpumonitor.o_gadget);
   E_FILL(inst->cfg->cpumonitor.o_gadget);
   evas_object_event_callback_add(inst->cfg->cpumonitor.o_gadget, EVAS_CALLBACK_MOUSE_DOWN, _cpumonitor_mouse_down_cb, inst);
   evas_object_show(inst->cfg->cpumonitor.o_gadget);
   _cpumonitor_config_updated(inst);
   _cpumonitor_eval_instance_aspect(inst);

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
   ci->cpumonitor.percent = 0;
   ci->cpumonitor.idle = 0;
   ci->cpumonitor.popup = NULL;
   ci->cpumonitor.configure = NULL;
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
   inst->cfg->cpumonitor.percent = 0;
   inst->cfg->cpumonitor.popup = NULL;
   inst->cfg->cpumonitor.configure = NULL;
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_data_set(inst->o_main, "Instance", inst);
   evas_object_smart_callback_add(parent, "gadget_created", _cpumonitor_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _cpumonitor_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_cpumonitor_remove, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

