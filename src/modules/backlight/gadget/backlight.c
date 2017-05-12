#include "backlight.h"

typedef struct _Instance Instance;

struct _Instance
{
   Evas_Object     *o_main;
   Evas_Object     *o_backlight, *o_table, *o_slider;
   Evas_Object     *popup, *box;
   E_Gadget_Site_Orient orient;
   double           val;
};

static Eina_List *ginstances = NULL;
static E_Action *act = NULL;
static Eina_List *handlers;

static void
_backlight_gadget_update(Instance *inst)
{
   Edje_Message_Float msg;

   msg.val = inst->val;
   if (msg.val < 0.0) msg.val = 0.0;
   else if (msg.val > 1.0) msg.val = 1.0;
   edje_object_message_send(elm_layout_edje_get(inst->o_backlight), EDJE_MESSAGE_FLOAT, 0, &msg);
}

static void
_backlight_level_set(Instance *inst, double val, Eina_Bool set_slider)
{
   if (val > 1.0) val = 1.0;
   if (val < 0.0) val = 0.0;
   if (set_slider)
     e_widget_slider_value_double_set(inst->o_slider, val);
   inst->val = val;
   e_backlight_mode_set(e_zone_current_get(), E_BACKLIGHT_MODE_NORMAL);
   e_backlight_level_set(e_zone_current_get(), val, 0.0);
   e_config->backlight.normal = val;
   e_config_save_queue();
}

static void
_backlight_settings_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   e_gadget_configure(inst->o_main);
   elm_ctxpopup_dismiss(inst->popup);
}

static void
_slider_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->val = elm_slider_value_get(inst->o_slider);
   _backlight_level_set(inst, inst->val, EINA_FALSE);
}

static void
_backlight_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);
   inst->popup = NULL;
}

static void
_backlight_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->popup = NULL;
}

static void
_backlight_popup_new(Instance *inst)
{
   Evas_Object *ic, *o;

   if (inst->popup) return;

   e_backlight_mode_set(e_zone_current_get(), E_BACKLIGHT_MODE_NORMAL);
   inst->val = e_backlight_level_get(e_zone_current_get());
   _backlight_gadget_update(inst);

   inst->popup = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(inst->popup, "noblock");
   evas_object_smart_callback_add(inst->popup, "dismissed", _backlight_popup_dismissed, inst);
   evas_object_event_callback_add(inst->popup, EVAS_CALLBACK_DEL, _backlight_popup_deleted, inst);

   inst->box = elm_box_add(e_comp->elm);
   elm_box_horizontal_set(inst->box, EINA_FALSE);
   evas_object_size_hint_weight_set(inst->box, 0.0, 1.0);
   evas_object_size_hint_align_set(inst->box, 0.0, 0.0);
   elm_object_content_set(inst->popup, inst->box);
   evas_object_show(inst->box);

   o = elm_slider_add(inst->box);
   elm_slider_horizontal_set(o, EINA_FALSE);
   elm_slider_inverted_set(o, EINA_TRUE);
   elm_slider_unit_format_set(o, NULL);
   elm_slider_indicator_show_set(o, EINA_FALSE);
   elm_slider_min_max_set(o, 0.05, 1.0);
   elm_slider_step_set(o, 0.05);
   elm_slider_span_size_set(o, 100);
   elm_slider_value_set(o, inst->val);
   evas_object_smart_callback_add(o, "changed", _slider_cb, inst);
   elm_box_pack_end(inst->box, o);
   evas_object_show(o);
   inst->o_slider = o;

   ic = elm_icon_add(inst->box);
   elm_icon_standard_set(ic, "preferences-system");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   evas_object_show(ic);

   o = elm_button_add(inst->box);
   elm_object_part_content_set(o, "icon", ic);
   evas_object_smart_callback_add(o, "clicked", _backlight_settings_cb, inst);
   elm_box_pack_end(inst->box, o);
   evas_object_show(o);

   e_gadget_util_ctxpopup_place(inst->o_main, inst->popup, inst->o_backlight);
   evas_object_show(inst->popup);
}

static void
_backlight_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;

   if (ev->button == 1)
     {
        if (inst->popup) elm_ctxpopup_dismiss(inst->popup);
        else _backlight_popup_new(inst);
     }
   else if (ev->button == 3)
     {
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        e_gadget_configure(inst->o_main);
     }
}

static void
_backlight_cb_mouse_wheel(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Wheel *ev = event;
   Instance *inst = data;

   inst->val = e_backlight_level_get(e_zone_current_get());
   if (ev->z > 0)
     _backlight_level_set(inst, inst->val - 0.1, EINA_FALSE);
   else if (ev->z < 0)
     _backlight_level_set(inst, inst->val + 0.1, EINA_FALSE);
}

static void
_e_mod_action_cb(E_Object *obj EINA_UNUSED,
                 const char *params)
{
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(ginstances, l, inst)
     {
        if (!params)
          {
             if (inst->popup) elm_ctxpopup_dismiss(inst->popup);
             else _backlight_popup_new(inst);
          }
        else
          {
             _backlight_level_set(inst, inst->val + atof(params), EINA_FALSE);
             if (inst->popup) elm_ctxpopup_dismiss(inst->popup);
             _backlight_popup_new(inst);
          }
     }
}

static Eina_Bool
_backlight_cb_mod_init_end(void *d EINA_UNUSED, int type EINA_UNUSED, void *ev EINA_UNUSED)
{
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(ginstances, l, inst)
     {
        inst->val = e_backlight_level_get(e_zone_current_get());
        _backlight_gadget_update(inst);
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_backlight_cb_changed(void *d EINA_UNUSED, int type EINA_UNUSED, void *ev EINA_UNUSED)
{
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(ginstances, l, inst)
     {
        inst->val = e_backlight_level_get(e_zone_current_get());
        _backlight_gadget_update(inst);
     }
   return ECORE_CALLBACK_RENEW;
}

static Evas_Object *
_backlight_gadget_configure(Evas_Object *g EINA_UNUSED)
{
   if (e_configure_registry_exists("screen/power_management"))
     {
        e_configure_registry_call("screen/power_management", NULL, NULL);
     }
   return NULL;
}

static void
_backlight_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->o_backlight), 0, 0, &w, &h);
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_backlight_gadget_created_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->o_main)
     {
        e_gadget_configure_cb_set(inst->o_main, _backlight_gadget_configure);
         
        inst->o_backlight = elm_layout_add(inst->o_main);
        E_EXPAND(inst->o_backlight);
        E_FILL(inst->o_backlight);
        if (inst->orient == E_GADGET_SITE_ORIENT_VERTICAL)
          e_theme_edje_object_set(inst->o_backlight,
                             "base/theme/modules/backlight",
                             "e/modules/backlight/main_vert");
        else
          e_theme_edje_object_set(inst->o_backlight,
                             "base/theme/modules/backlight",
                             "e/modules/backlight/main");
        evas_object_event_callback_add(inst->o_backlight,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _backlight_cb_mouse_down,
                                  inst);
        evas_object_event_callback_add(inst->o_backlight,
                                  EVAS_CALLBACK_MOUSE_WHEEL,
                                  _backlight_cb_mouse_wheel,
                                  inst);
        evas_object_event_callback_add(inst->o_backlight,
                                  EVAS_CALLBACK_RESIZE,
                                  _backlight_resize_cb,
                                  inst);
        elm_box_pack_end(inst->o_main, inst->o_backlight);
        evas_object_show(inst->o_backlight);
     }
   evas_object_smart_callback_del_full(obj, "gadget_created", _backlight_gadget_created_cb, data);
}

static void
backlight_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->popup)
     elm_ctxpopup_dismiss(inst->popup);
   ginstances = eina_list_remove(ginstances, inst);
   free(inst);
}

EINTERN Evas_Object *
backlight_gadget_create(Evas_Object *parent, int *id EINA_UNUSED, E_Gadget_Site_Orient orient)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->o_main = elm_box_add(parent);
   inst->orient = orient;
   E_EXPAND(inst->o_main);
   E_FILL(inst->o_main);
   evas_object_smart_callback_add(parent, "gadget_created", _backlight_gadget_created_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, backlight_del, inst);

   ginstances = eina_list_append(ginstances, inst);
   return inst->o_main;
}

EINTERN void
backlight_init(void)
{
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_BACKLIGHT_CHANGE, _backlight_cb_changed, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_MODULE_INIT_END, _backlight_cb_mod_init_end, NULL);
   act = e_action_add("backlight");
   if (act)
     {
        act->func.go = _e_mod_action_cb;
        e_action_predef_name_set(N_("Screen"), N_("Backlight Controls"), "backlight",
                                 NULL, "syntax: brightness change(-1.0 - 1.0), example: -0.1", 1);
     }
}

EINTERN void
backlight_shutdown(void)
{
   if (act)
     {
        e_action_predef_name_del("Screen", "Backlight Controls");
        e_action_del("backlight");
        act = NULL;
     }
   E_FREE_LIST(handlers, ecore_event_handler_del);
}
