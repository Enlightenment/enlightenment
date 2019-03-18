#include "mixer.h"
#include "backend.h"

typedef struct _Context Context;
struct _Context
{
   char                *theme;
   E_Module            *module;
   Eina_List           *instances;
   E_Menu              *menu;
   unsigned int         notification_id;
};

typedef struct _Instance Instance;
struct _Instance
{
   int                  id;
   Evas_Object         *o_main;
   Evas_Object         *o_mixer;
   Evas_Object         *popup;
   Evas_Object         *list;
   Evas_Object         *slider;
   Evas_Object         *check;
   E_Gadget_Site_Orient orient;
};

static Context *gmixer_context = NULL;
static Eina_List *_handlers = NULL;

static void
_mixer_popup_update(Instance *inst, int mute, int vol)
{
   elm_check_state_set(inst->check, !!mute);
   elm_slider_value_set(inst->slider, vol);
}

static void
_mixer_gadget_update(void)
{
   Edje_Message_Int_Set *msg;
   Instance *inst;
   Eina_List *l;
   const Eina_List *ll;
   Elm_Object_Item *it;

   EINA_LIST_FOREACH(gmixer_context->instances, l, inst)
     {
        msg = alloca(sizeof(Edje_Message_Int_Set) + (2 * sizeof(int)));
        msg->count = 3;

        if (!backend_sink_default_get())
          {
             msg->val[0] = EINA_FALSE;
             msg->val[1] = 0;
             msg->val[2] = 0;
             if (inst->popup)
               elm_ctxpopup_dismiss(inst->popup);
          }
        else
          {
             msg->val[0] = backend_mute_get();
             msg->val[1] = backend_volume_get();
             msg->val[2] = msg->val[1];
             if (inst->popup)
               _mixer_popup_update(inst, msg->val[0], msg->val[1]);
          }
        edje_object_message_send(elm_layout_edje_get(inst->o_mixer), EDJE_MESSAGE_INT_SET, 0, msg);
        elm_layout_signal_emit(inst->o_mixer, "e,action,volume,change", "e");

        if (inst->list)
          {
             EINA_LIST_FOREACH(elm_list_items_get(inst->list), ll, it)
               {
                  if (backend_sink_default_get() == elm_object_item_data_get(it))
                    elm_list_item_selected_set(it, EINA_TRUE);
               }
          }
     }
}

static Eina_Bool
_mixer_backend_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   _mixer_gadget_update();
   return ECORE_CALLBACK_PASS_ON;
}

static void
_emixer_exec_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   elm_ctxpopup_dismiss(inst->popup);

   backend_emixer_exec();
}

static void
_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                  void *event EINA_UNUSED)
{
   backend_mute_set(elm_check_state_get(obj));
}

static void
_slider_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                   void *event EINA_UNUSED)
{
   int val;


   val = (int)elm_slider_value_get(obj);
   backend_volume_set(val);
}

static void
_sink_selected_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Emix_Sink *s = data;

   backend_sink_default_set(s);
}

static void
_mixer_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);
   inst->popup = NULL;
}

static void
_mixer_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->popup = NULL;
   inst->list = NULL;
   inst->slider = NULL;
   inst->check = NULL;
}

static Eina_Bool
_mixer_sinks_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l, *ll;
   Instance *inst;

   EINA_LIST_FOREACH(gmixer_context->instances, l, inst)
     {
        if (inst->list)
          {
             Elm_Object_Item *default_it = NULL;
             Emix_Sink *s;

             elm_list_clear(inst->list);
             EINA_LIST_FOREACH((Eina_List *)emix_sinks_get(), ll, s)
               {
                  Elm_Object_Item *it;

                  it = elm_list_item_append(inst->list, s->name, NULL, NULL,
                                            _sink_selected_cb, s);
                  if (backend_sink_default_get() == s)
                    default_it = it;
               }
             elm_list_go(inst->list);
             if (default_it)
               elm_list_item_selected_set(default_it, EINA_TRUE);
          }
     }


   return ECORE_CALLBACK_PASS_ON;
}

static void
_popup_new(Instance *inst)
{
   Evas_Object *button, *list, *slider, *bx;
   Emix_Sink *s;
   Eina_List *l;
   Elm_Object_Item *default_it = NULL;

   inst->popup = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(inst->popup, "noblock");
   evas_object_smart_callback_add(inst->popup, "dismissed", _mixer_popup_dismissed, inst);
   evas_object_event_callback_add(inst->popup, EVAS_CALLBACK_DEL, _mixer_popup_deleted, inst);

   list = elm_box_add(e_comp->elm);
   elm_object_content_set(inst->popup, list);

   inst->list = elm_list_add(e_comp->elm);
   elm_list_mode_set(inst->list, ELM_LIST_COMPRESS);
   evas_object_size_hint_align_set(inst->list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(inst->list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(inst->list);

   EINA_LIST_FOREACH((Eina_List *)emix_sinks_get(), l, s)
     {
        Elm_Object_Item *it;

        it = elm_list_item_append(inst->list, s->name, NULL, NULL, _sink_selected_cb, s);
        if (backend_sink_default_get() == s)
          default_it = it;
     }
   elm_list_go(inst->list);
   elm_box_pack_end(list, inst->list);

   bx = elm_box_add(e_comp->elm);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(list, bx);
   evas_object_show(bx);

   slider = elm_slider_add(e_comp->elm);
   inst->slider = slider;
   elm_slider_span_size_set(slider, 128 * elm_config_scale_get());
   elm_slider_unit_format_set(slider, "%1.0f");
   elm_slider_indicator_format_set(slider, "%1.0f");
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
   evas_object_show(slider);
   elm_slider_min_max_set(slider, 0.0, emix_max_volume_get());
   evas_object_smart_callback_add(slider, "changed", _slider_changed_cb, NULL);
   elm_slider_value_set(slider, backend_volume_get());
   elm_box_pack_end(bx, slider);
   evas_object_show(slider);

   inst->check = elm_check_add(e_comp->elm);
   evas_object_size_hint_align_set(inst->check, 0.5, EVAS_HINT_FILL);
   elm_object_text_set(inst->check, _("Mute"));
   elm_check_state_set(inst->check, backend_mute_get());
   evas_object_smart_callback_add(inst->check, "changed", _check_changed_cb,
                                  NULL);
   elm_box_pack_end(bx, inst->check);
   evas_object_show(inst->check);

   button = elm_button_add(e_comp->elm);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0.0);
   elm_object_text_set(button, _("Mixer"));
   evas_object_smart_callback_add(button, "clicked", _emixer_exec_cb, inst);
   elm_box_pack_end(list, button);
   evas_object_show(button);

   evas_object_size_hint_min_set(list, 208, 208);

   e_gadget_util_ctxpopup_place(inst->o_main, inst->popup, inst->o_mixer);
   evas_object_show(inst->popup);

   if (default_it)
     elm_list_item_selected_set(default_it, EINA_TRUE);
}

static void
_mouse_up_cb(void *data, Evas *evas EINA_UNUSED,
             Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Up *ev = event;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;

   if (ev->button == 1)
     {
        if (inst->popup)
          {
             elm_ctxpopup_dismiss(inst->popup);
             return;
          }
        _popup_new(inst);
     }
   else if (ev->button == 2)
     {
        backend_mute_set(!backend_mute_get());
     }
}

static void
_mouse_wheel_cb(void *data EINA_UNUSED, Evas *evas EINA_UNUSED,
                Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Wheel *ev = event;

   if (ev->z > 0)
     backend_volume_decrease();
   else if (ev->z < 0)
     backend_volume_increase();
}

static Evas_Object *
_mixer_gadget_configure(Evas_Object *g EINA_UNUSED)
{
   if (e_configure_registry_exists("extensions/emix"))
     {
        e_configure_registry_call("extensions/emix", NULL, NULL);
     }
   return NULL;
}

static void
_mixer_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->o_mixer), 0, 0, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_mixer_gadget_created_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->o_main)
     {
        e_gadget_configure_cb_set(inst->o_main, _mixer_gadget_configure);

        inst->o_mixer = elm_layout_add(inst->o_main);
        E_EXPAND(inst->o_mixer);
        E_FILL(inst->o_mixer);
        if (inst->orient == E_GADGET_SITE_ORIENT_VERTICAL)
          e_theme_edje_object_set(inst->o_mixer,
                                  "base/theme/gadget/mixer",
                                  "e/gadget/mixer/main_vert");
        else
          e_theme_edje_object_set(inst->o_mixer,
                                  "base/theme/gadget/mixer",
                                  "e/gadget/mixer/main");
        evas_object_event_callback_add(inst->o_mixer, EVAS_CALLBACK_MOUSE_UP,
                                       _mouse_up_cb, inst);
        evas_object_event_callback_add(inst->o_mixer, EVAS_CALLBACK_MOUSE_WHEEL,
                                       _mouse_wheel_cb, inst);
        evas_object_event_callback_add(inst->o_mixer, EVAS_CALLBACK_RESIZE,
                                       _mixer_resize_cb, inst);
        elm_box_pack_end(inst->o_main, inst->o_mixer);
        evas_object_show(inst->o_mixer);
        if (inst->id != -1)
          gmixer_context->instances = eina_list_append(gmixer_context->instances, inst);
        if (inst->id == -1)
          {
             Edje_Message_Int_Set *msg;

             msg = alloca(sizeof(Edje_Message_Int_Set) + (2 * sizeof(int)));
             msg->count = 3;
             msg->val[0] = EINA_FALSE;
             msg->val[1] = 60;
             msg->val[2] = 60;
             edje_object_message_send(elm_layout_edje_get(inst->o_mixer), EDJE_MESSAGE_INT_SET, 0, msg);
             elm_layout_signal_emit(inst->o_mixer, "e,action,volume,change", "e");
          }
        else
          _mixer_gadget_update();
     }
   evas_object_smart_callback_del_full(obj, "gadget_created", _mixer_gadget_created_cb, data);
}

static void
mixer_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   gmixer_context->instances = eina_list_remove(gmixer_context->instances, inst);
   free(inst);
}

EINTERN Evas_Object *
mixer_gadget_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Instance *inst;

   if (*id != -1)
     {
        if (!mixer_init())
          return NULL;
     }
   inst = E_NEW(Instance, 1);
   inst->o_main = elm_box_add(parent);
   inst->orient = orient;
   inst->id = *id;
   evas_object_show(inst->o_main);

   evas_object_smart_callback_add(parent, "gadget_created", _mixer_gadget_created_cb, inst);
   if (*id != -1)
     evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, mixer_del, inst);
   return inst->o_main;
}

EINTERN Eina_Bool
mixer_init(void)
{
   char buf[4096];

   if (!gmixer_context)
     {
        gmixer_context = E_NEW(Context, 1);

        snprintf(buf, sizeof(buf), "%s/mixer.edj",
                 e_module_dir_get(gmixer_context->module));
        gmixer_context->theme = strdup(buf);
        E_LIST_HANDLER_APPEND(_handlers, E_EVENT_MIXER_BACKEND_CHANGED,
                              _mixer_backend_changed, NULL);
        E_LIST_HANDLER_APPEND(_handlers, E_EVENT_MIXER_SINKS_CHANGED,
                              _mixer_sinks_changed, NULL);
     }

   return EINA_TRUE;
}

EINTERN void
mixer_shutdown(void)
{
   E_FREE_LIST(_handlers, ecore_event_handler_del);
   if (gmixer_context)
     {
        free(gmixer_context->theme);
        E_FREE(gmixer_context);
     }
}

