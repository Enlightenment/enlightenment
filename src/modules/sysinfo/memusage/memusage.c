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

   if (inst->cfg->memusage.popup)
     {
        char text[4096];
        snprintf(text, sizeof(text), "%s: %d%%<br>%s: %d%%", _("Total Memory Usage"),
                 inst->cfg->memusage.real, _("Total Swap Usage"), inst->cfg->memusage.swap);
        elm_object_text_set(inst->cfg->memusage.popup_label, text);
     }
}

static Evas_Object *
_memusage_configure_cb(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "Instance");

   if (!sysinfo_config) return NULL;
   if (inst->cfg->memusage.popup) return NULL;
   return memusage_configure(inst);
}

static void
_memusage_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);
   inst->cfg->memusage.popup = NULL;
}

static void
_memusage_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->cfg->memusage.popup = NULL;
}

static void
_memusage_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Evas_Object *label, *popup;
   Evas_Event_Mouse_Down *ev = event_data;
   Instance *inst = data;
   char text[4096];

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 3)
     {
        if (inst->cfg->memusage.popup)
          {
             elm_ctxpopup_dismiss(inst->cfg->memusage.popup);
             inst->cfg->memusage.popup = NULL;
             return;
          }
        popup = elm_ctxpopup_add(e_comp->elm);
        elm_object_style_set(popup, "noblock");
        evas_object_smart_callback_add(popup, "dismissed", _memusage_popup_dismissed, inst);
        evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _memusage_popup_deleted, inst);

        snprintf(text, sizeof(text), "%s: %d%%<br>%s: %d%%", _("Total Memory Usage"),
                 inst->cfg->memusage.real, _("Total Swap Usage"), inst->cfg->memusage.swap);
        label = elm_label_add(popup);
        elm_object_style_set(label, "marker");
        elm_object_text_set(label, text);
        elm_object_content_set(popup, label);
        evas_object_show(label);
        inst->cfg->memusage.popup_label = label;

        e_comp_object_util_autoclose(popup, NULL, NULL, NULL);
        evas_object_show(popup);
        e_gadget_util_ctxpopup_place(inst->o_main, popup, inst->cfg->memusage.o_gadget);
        inst->cfg->memusage.popup = popup;
     }
   else
     {
        if (inst->cfg->memusage.popup)
          {
             elm_ctxpopup_dismiss(inst->cfg->memusage.popup);
             inst->cfg->memusage.popup = NULL;
          }
        if (!sysinfo_config) return;
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        if (inst->cfg->esm != E_SYSINFO_MODULE_MEMUSAGE)
          memusage_configure(inst);
        else
          e_gadget_configure(inst->o_main);
     }
}

static void
_memusage_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->cfg->memusage.o_gadget), 0, 0, &w, &h);
   if (inst->cfg->esm == E_SYSINFO_MODULE_MEMUSAGE)
     evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
   else
     evas_object_size_hint_aspect_set(inst->cfg->memusage.o_gadget, EVAS_ASPECT_CONTROL_BOTH, w, h);
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

   inst->cfg->memusage.real = thc->memstatus;
   inst->cfg->memusage.swap = thc->swapstatus;
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

   if (inst->cfg->memusage.popup)
     E_FREE_FUNC(inst->cfg->memusage.popup, evas_object_del);
   if (inst->cfg->memusage.configure)
     E_FREE_FUNC(inst->cfg->memusage.configure, evas_object_del);
   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }
   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_memusage_remove, data);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

void
sysinfo_memusage_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->cfg->memusage.popup)
     E_FREE_FUNC(inst->cfg->memusage.popup, evas_object_del);
   if (inst->cfg->memusage.configure)
     E_FREE_FUNC(inst->cfg->memusage.configure, evas_object_del);
   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }
}

static void
_memusage_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   int orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   e_gadget_configure_cb_set(inst->o_main, _memusage_configure_cb);

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
   evas_object_event_callback_add(inst->cfg->memusage.o_gadget, EVAS_CALLBACK_MOUSE_DOWN, _memusage_mouse_down_cb, inst);
   evas_object_event_callback_add(inst->cfg->memusage.o_gadget, EVAS_CALLBACK_RESIZE, _memusage_resize_cb, inst);
   evas_object_show(inst->cfg->memusage.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created", _memusage_created_cb, data);
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
   evas_object_event_callback_add(inst->cfg->memusage.o_gadget, EVAS_CALLBACK_MOUSE_DOWN, _memusage_mouse_down_cb, inst);
   evas_object_event_callback_add(inst->cfg->memusage.o_gadget, EVAS_CALLBACK_RESIZE, _memusage_resize_cb, inst);
   evas_object_show(inst->cfg->memusage.o_gadget);
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
   ci->memusage.real = 0;
   ci->memusage.swap = 0;
   ci->memusage.popup = NULL;
   ci->memusage.configure = NULL;

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
   inst->cfg->memusage.real = 0;
   inst->cfg->memusage.swap = 0;
   inst->cfg->memusage.popup = NULL;
   inst->cfg->memusage.configure = NULL;
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   evas_object_data_set(inst->o_main, "Instance", inst);
   evas_object_smart_callback_add(parent, "gadget_created", _memusage_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _memusage_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, sysinfo_memusage_remove, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

