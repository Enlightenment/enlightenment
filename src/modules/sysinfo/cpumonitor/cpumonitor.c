#include "cpumonitor.h"

typedef struct _Thread_Config Thread_Config;

struct _Thread_Config
{
   int                  interval;
   int                  num_cores;
   int                  percent;
   unsigned long        total;
   unsigned long        idle;
   Instance            *inst;
   E_Powersave_Sleeper *sleeper;
   Eina_List           *cores;
};

static void
_cpumonitor_face_update(Thread_Config *thc)
{
   Eina_List *l;
   CPU_Core *core;

   EINA_LIST_FOREACH(thc->cores, l, core)
     {
        Edje_Message_Int_Set *usage_msg;
        usage_msg = malloc(sizeof(Edje_Message_Int_Set) + 1 * sizeof(int));
        EINA_SAFETY_ON_NULL_RETURN(usage_msg);
        usage_msg->count = 1;
        usage_msg->val[0] = core->percent;
        edje_object_message_send(elm_layout_edje_get(core->layout), EDJE_MESSAGE_INT_SET, 1,
                                 usage_msg);
        E_FREE(usage_msg);
     }
   if (thc->inst->cfg->cpumonitor.popup)
     {
        elm_progressbar_value_set(thc->inst->cfg->cpumonitor.popup_pbar,
                                  (float)thc->percent / 100);
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
   Evas_Object *popup, *box, *pbar, *label;
   Evas_Event_Mouse_Down *ev = event_data;
   Instance *inst = data;
   char text[256];

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 3)
     {
        if (inst->cfg->cpumonitor.popup)
          {
             elm_ctxpopup_dismiss(inst->cfg->cpumonitor.popup);
             return;
          }
        popup = elm_ctxpopup_add(e_comp->elm);
        elm_object_style_set(popup, "noblock");
        evas_object_smart_callback_add(popup, "dismissed", _cpumonitor_popup_dismissed, inst);
        evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _cpumonitor_popup_deleted, inst);

        box = elm_box_add(popup);
        E_EXPAND(box); E_FILL(box);
        elm_object_content_set(popup, box);
        evas_object_show(box);

        snprintf(text, sizeof(text), "<big><b>%s</b></big>", _("Total CPU Usage"));
        label = elm_label_add(box);
        E_EXPAND(label); E_ALIGN(label, 0.5, 0.5);
        elm_object_text_set(label, text);
        elm_box_pack_end(box, label);
        evas_object_show(label);

        pbar = elm_progressbar_add(box);
        E_EXPAND(pbar); E_FILL(pbar);
        elm_progressbar_span_size_set(pbar, 200 * e_scale);
        elm_progressbar_value_set(pbar, (float)inst->cfg->cpumonitor.percent / 100);
        elm_box_pack_end(box, pbar);
        evas_object_show(pbar);
        inst->cfg->cpumonitor.popup_pbar = pbar;

        e_gadget_util_ctxpopup_place(inst->o_main, popup,
                                     inst->cfg->cpumonitor.o_gadget);
        evas_object_show(popup);
        inst->cfg->cpumonitor.popup = popup;
     }
   else
     {
        if (inst->cfg->cpumonitor.popup)
          elm_ctxpopup_dismiss(inst->cfg->cpumonitor.popup);
        if (!sysinfo_config) return;
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        if (inst->cfg->esm != E_SYSINFO_MODULE_CPUMONITOR)
          cpumonitor_configure(inst);
        else
          e_gadget_configure(inst->o_main);
     }
}

static void
_cpumonitor_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Evas_Coord w = 1, h = 1, sw, sh;
   Instance *inst = data;
   int num_cores = inst->cfg->cpumonitor.cores;

   if (!num_cores || !inst->o_main) return;

   edje_object_parts_extends_calc(elm_layout_edje_get(obj), 0, 0, &w, &h);
   evas_object_event_callback_del_full(obj, EVAS_CALLBACK_RESIZE, _cpumonitor_resize_cb, inst);
   if (inst->cfg->esm == E_SYSINFO_MODULE_CPUMONITOR)
     {
        evas_object_geometry_get(inst->o_main, 0, 0, &sw, &sh);
     }
   else
     {
        sw = w;
        sh = h;
     }
   if (e_gadget_site_orient_get(e_gadget_site_get(inst->o_main)) == E_GADGET_SITE_ORIENT_VERTICAL)
     {
        w = sw;
        h *= num_cores;
     }
   else
     {
        w *= num_cores;
        h = sh;
     }
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   evas_object_size_hint_aspect_set(inst->cfg->cpumonitor.o_gadget_box, EVAS_ASPECT_CONTROL_BOTH, w, h);
   if (inst->cfg->esm == E_SYSINFO_MODULE_CPUMONITOR)
     evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_cpumonitor_cb_usage_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;; )
     {
        if (ecore_thread_check(th)) break;
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
        _cpumonitor_sysctl_getusage(&thc->total, &thc->idle, &thc->percent, thc->cores);
#else
        _cpumonitor_proc_getusage(&thc->total, &thc->idle, &thc->percent, thc->cores);
#endif
        ecore_thread_feedback(th, NULL);
        if (ecore_thread_check(th)) break;
        e_powersave_sleeper_sleep(thc->sleeper, thc->interval);
        if (ecore_thread_check(th)) break;
     }
}

static void
_cpumonitor_cb_usage_check_notify(void *data,
                                  Ecore_Thread *th EINA_UNUSED,
                                  void *msg EINA_UNUSED)
{
   Thread_Config *thc = data;

   if (!thc->inst || !thc->inst->cfg) return;
   if (thc->inst->cfg->esm != E_SYSINFO_MODULE_CPUMONITOR && thc->inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;
   thc->inst->cfg->cpumonitor.percent = thc->percent;
   _cpumonitor_face_update(thc);
}

static void
_cpumonitor_cb_usage_check_end(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Thread_Config *thc = data;
   CPU_Core *core;

   e_powersave_sleeper_free(thc->sleeper);
   EINA_LIST_FREE(thc->cores, core)
     E_FREE(core);
   E_FREE(thc);
}

Evas_Object *
_cpumonitor_add_layout(Instance *inst)
{
   Evas_Object *layout;
   int orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   layout = elm_layout_add(inst->cfg->cpumonitor.o_gadget_box);
   edje_object_update_hints_set(elm_layout_edje_get(layout), EINA_TRUE);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     e_theme_edje_object_set(layout, "base/theme/gadget/cpumonitor",
                             "e/gadget/cpumonitor/core/main_vert");
   else
     e_theme_edje_object_set(layout, "base/theme/gadget/cpumonitor",
                             "e/gadget/cpumonitor/core/main");
   E_EXPAND(layout);
   E_FILL(layout);
   elm_box_pack_end(inst->cfg->cpumonitor.o_gadget_box, layout);
   evas_object_show(layout);

   return layout;
}

static void
_cpumonitor_del_layouts(Instance *inst)
{
   elm_box_clear(inst->cfg->cpumonitor.o_gadget_box);
}

static Eina_Bool
_screensaver_on(void *data)
{
   Instance *inst = data;

   if (!ecore_thread_check(inst->cfg->cpumonitor.usage_check_thread))
     {
        _cpumonitor_del_layouts(inst);
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_screensaver_off(void *data)
{
   Instance *inst = data;

   _cpumonitor_config_updated(inst);

   return ECORE_CALLBACK_RENEW;
}

void
_cpumonitor_config_updated(Instance *inst)
{
   Thread_Config *thc;
   CPU_Core *core;
   int i = 0;

   if (inst->cfg->id == -1)
     {
        int percent = 15;
        thc = E_NEW(Thread_Config, 1);
        if (thc)
          {
             thc->inst = inst;
             thc->total = 0;
             thc->idle = 0;
             thc->percent = 60;
             thc->num_cores = 4;
             inst->cfg->cpumonitor.cores = thc->num_cores;
             for (i = 0; i < 4; i++)
               {
                  core = E_NEW(CPU_Core, 1);
                  core->layout = _cpumonitor_add_layout(inst);
                  if (i == 0)
                    evas_object_event_callback_add(core->layout, EVAS_CALLBACK_RESIZE, _cpumonitor_resize_cb, inst);
                  core->percent = percent;
                  core->total = 0;
                  core->idle = 0;
                  thc->cores = eina_list_append(thc->cores, core);
                  percent += 15;
               }
             _cpumonitor_face_update(thc);
             EINA_LIST_FREE(thc->cores, core)
               E_FREE(core);
             E_FREE(thc);
          }
        return;
     }
   if (!ecore_thread_check(inst->cfg->cpumonitor.usage_check_thread))
     {
        _cpumonitor_del_layouts(inst);
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
   thc = E_NEW(Thread_Config, 1);
   if (thc)
     {
        thc->inst = inst;
        thc->total = 0;
        thc->idle = 0;
        thc->percent = 0;
        thc->interval = inst->cfg->cpumonitor.poll_interval;
        thc->sleeper = e_powersave_sleeper_new();
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
        thc->num_cores = _cpumonitor_sysctl_getcores();
#else
        thc->num_cores = _cpumonitor_proc_getcores();
#endif
        inst->cfg->cpumonitor.cores = thc->num_cores;
        for (i = 0; i < thc->num_cores; i++)
          {
             core = E_NEW(CPU_Core, 1);
             core->layout = _cpumonitor_add_layout(inst);
             if (i == 0)
               evas_object_event_callback_add(core->layout, EVAS_CALLBACK_RESIZE, _cpumonitor_resize_cb, inst);
             core->percent = 0;
             core->total = 0;
             core->idle = 0;
             thc->cores = eina_list_append(thc->cores, core);
          }
        inst->cfg->cpumonitor.usage_check_thread =
          ecore_thread_feedback_run(_cpumonitor_cb_usage_check_main,
                                    _cpumonitor_cb_usage_check_notify,
                                    _cpumonitor_cb_usage_check_end,
                                    _cpumonitor_cb_usage_check_end, thc, EINA_TRUE);
     }
   e_config_save_queue();
}

static void
_cpumonitor_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->o_main != event_data) return;

   if (inst->cfg->cpumonitor.popup)
     E_FREE_FUNC(inst->cfg->cpumonitor.popup, evas_object_del);
   if (inst->cfg->cpumonitor.configure)
     E_FREE_FUNC(inst->cfg->cpumonitor.configure, evas_object_del);
   EINA_LIST_FREE(inst->cfg->cpumonitor.handlers, handler)
     ecore_event_handler_del(handler);
   evas_object_smart_callback_del_full(e_gadget_site_get(inst->o_main), "gadget_removed",
                                       _cpumonitor_removed_cb, inst);
   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_cpumonitor_remove, data);
   if (!ecore_thread_check(inst->cfg->cpumonitor.usage_check_thread))
     {
        _cpumonitor_del_layouts(inst);
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   if (inst->cfg->id >= 0)
     sysinfo_instances = eina_list_remove(sysinfo_instances, inst);
   E_FREE(inst->cfg);
   E_FREE(inst);
}

void
sysinfo_cpumonitor_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->cfg->cpumonitor.popup)
     E_FREE_FUNC(inst->cfg->cpumonitor.popup, evas_object_del);
   if (inst->cfg->cpumonitor.configure)
     E_FREE_FUNC(inst->cfg->cpumonitor.configure, evas_object_del);
   EINA_LIST_FREE(inst->cfg->cpumonitor.handlers, handler)
     ecore_event_handler_del(handler);
   if (!ecore_thread_check(inst->cfg->cpumonitor.usage_check_thread))
     {
        _cpumonitor_del_layouts(inst);
        ecore_thread_cancel(inst->cfg->cpumonitor.usage_check_thread);
        inst->cfg->cpumonitor.usage_check_thread = NULL;
     }
}

static void
_cpumonitor_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   int orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   e_gadget_configure_cb_set(inst->o_main, _cpumonitor_configure_cb);

   inst->cfg->cpumonitor.o_gadget = elm_layout_add(inst->o_main);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     e_theme_edje_object_set(inst->cfg->cpumonitor.o_gadget,
                             "base/theme/gadget/cpumonitor",
                             "e/gadget/cpumonitor/main_vert");
   else
     e_theme_edje_object_set(inst->cfg->cpumonitor.o_gadget,
                             "base/theme/gadget/cpumonitor",
                             "e/gadget/cpumonitor/main");
   E_EXPAND(inst->cfg->cpumonitor.o_gadget);
   E_FILL(inst->cfg->cpumonitor.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->cpumonitor.o_gadget);
   evas_object_event_callback_add(inst->cfg->cpumonitor.o_gadget, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cpumonitor_mouse_down_cb, inst);
   evas_object_show(inst->cfg->cpumonitor.o_gadget);

   inst->cfg->cpumonitor.o_gadget_box = elm_box_add(inst->cfg->cpumonitor.o_gadget);
   elm_box_homogeneous_set(inst->cfg->cpumonitor.o_gadget_box, EINA_TRUE);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     elm_box_horizontal_set(inst->cfg->cpumonitor.o_gadget_box, EINA_FALSE);
   else
     elm_box_horizontal_set(inst->cfg->cpumonitor.o_gadget_box, EINA_TRUE);
   E_EXPAND(inst->cfg->cpumonitor.o_gadget_box);
   E_FILL(inst->cfg->cpumonitor.o_gadget_box);
   elm_layout_content_set(inst->cfg->cpumonitor.o_gadget, "e.swallow.content", inst->cfg->cpumonitor.o_gadget_box);
   evas_object_show(inst->cfg->cpumonitor.o_gadget_box);

   evas_object_smart_callback_del_full(obj, "gadget_created", _cpumonitor_created_cb, data);

   E_LIST_HANDLER_APPEND(inst->cfg->cpumonitor.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->cpumonitor.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);

   _cpumonitor_config_updated(inst);
}

Evas_Object *
sysinfo_cpumonitor_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->cpumonitor.percent = 0;
   inst->cfg->cpumonitor.popup = NULL;
   inst->cfg->cpumonitor.configure = NULL;
   inst->cfg->cpumonitor.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->cpumonitor.o_gadget,
                           "base/theme/gadget/cpumonitor",
                           "e/gadget/cpumonitor/main");
   E_EXPAND(inst->cfg->cpumonitor.o_gadget);
   E_FILL(inst->cfg->cpumonitor.o_gadget);
   evas_object_event_callback_add(inst->cfg->cpumonitor.o_gadget, EVAS_CALLBACK_MOUSE_DOWN,
                                  _cpumonitor_mouse_down_cb, inst);
   evas_object_show(inst->cfg->cpumonitor.o_gadget);

   inst->cfg->cpumonitor.o_gadget_box = elm_box_add(inst->cfg->cpumonitor.o_gadget);
   elm_box_homogeneous_set(inst->cfg->cpumonitor.o_gadget_box, EINA_TRUE);
   elm_box_horizontal_set(inst->cfg->cpumonitor.o_gadget_box, EINA_TRUE);
   E_EXPAND(inst->cfg->cpumonitor.o_gadget_box);
   E_FILL(inst->cfg->cpumonitor.o_gadget_box);
   elm_layout_content_set(inst->cfg->cpumonitor.o_gadget, "e.swallow.content", inst->cfg->cpumonitor.o_gadget_box);
   evas_object_show(inst->cfg->cpumonitor.o_gadget_box);

   E_LIST_HANDLER_APPEND(inst->cfg->cpumonitor.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->cpumonitor.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);

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
     ci->id = eina_list_count(sysinfo_config->items) + 1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_CPUMONITOR;
   ci->cpumonitor.poll_interval = 32;
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
   inst->cfg->cpumonitor.popup = NULL;
   inst->cfg->cpumonitor.configure = NULL;
   inst->o_main = elm_box_add(parent);
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

