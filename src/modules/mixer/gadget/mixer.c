#include "mixer.h"

#define VOLUME_STEP 5

#define BARRIER_CHECK(old_val, new_val) \
   (old_val > EMIX_VOLUME_BARRIER - 20) && \
   (old_val <= EMIX_VOLUME_BARRIER) && \
   (new_val > EMIX_VOLUME_BARRIER) && \
   (new_val < EMIX_VOLUME_BARRIER + 20)

static Eina_Bool init;
static Eina_List *_client_sinks = NULL;
static Eina_List *_client_mixers = NULL;
static Eina_List *_client_handlers = NULL;

typedef struct _Context Context;
struct _Context
{
   char *theme;
   Ecore_Exe *emixer;
   Ecore_Event_Handler *desklock_handler;
   Ecore_Event_Handler *emix_event_handler;
   const Emix_Sink *sink_default;
   E_Module *module;
   Eina_List *instances;
   E_Menu *menu;
   unsigned int notification_id;

   struct {
      E_Action *incr;
      E_Action *decr;
      E_Action *mute;
      E_Action *incr_app;
      E_Action *decr_app;
      E_Action *mute_app;
   } actions;
};

typedef struct _Instance Instance;
struct _Instance
{
   Evas_Object *o_main;
   Evas_Object *o_mixer;
   Evas_Object *popup;
   Evas_Object *list;
   Evas_Object *slider;
   Evas_Object *check;
   Eina_Bool mute;
};

typedef struct _Client_Mixer Client_Mixer;
struct _Client_Mixer
{
   Evas_Object *win;
   Evas_Object *volume;
   Evas_Object *mute;
   E_Client *ec;
   Evas_Object *bx;
   Eina_List *sinks;
};

static Context *gmixer_context = NULL;

static void
_notify_cb(void *data EINA_UNUSED, unsigned int id)
{
   gmixer_context->notification_id = id;
}

static void
_notify(const int val)
{
   E_Notification_Notify n;
   char *icon, buf[56];
   int ret;

   if (!emix_config_notify_get()) return;

   memset(&n, 0, sizeof(E_Notification_Notify));
   if (val < 0) return;

   ret = snprintf(buf, (sizeof(buf) - 1), "%s: %d%%", _("New volume"), val);
   if ((ret < 0) || ((unsigned int)ret > sizeof(buf)))
     return;
   //Names are taken from FDO icon naming scheme
   if (val == 0)
     icon = "audio-volume-muted";
   else if ((val > 33) && (val < 66))
     icon = "audio-volume-medium";
   else if (val <= 33)
     icon = "audio-volume-low";
   else
     icon = "audio-volume-high";

   n.app_name = _("Mixer");
   n.replaces_id = gmixer_context->notification_id;
   n.icon.icon = icon;
   n.summary = _("Volume changed");
   n.body = buf;
   n.timeout = 2000;
   e_notification_client_send(&n, _notify_cb, NULL);
}

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

   EINA_LIST_FOREACH(gmixer_context->instances, l, inst)
     {
        msg = alloca(sizeof(Edje_Message_Int_Set) + (2 * sizeof(int)));
        msg->count = 3;

        if (!gmixer_context->sink_default)
          {
             msg->val[0] = EINA_FALSE;
             msg->val[1] = 0;
             msg->val[2] = 0;
             if (inst->popup)
               elm_ctxpopup_dismiss(inst->popup);
          }
        else
          {
             int vol = 0;
             unsigned int i = 0;
             for (i = 0; i <
                  gmixer_context->sink_default->volume.channel_count; i++)
               vol += gmixer_context->sink_default->volume.volumes[i];
             if (gmixer_context->sink_default->volume.channel_count)
               vol /= gmixer_context->sink_default->volume.channel_count;
             msg->val[0] = gmixer_context->sink_default->mute;
             msg->val[1] = vol;
             msg->val[2] = msg->val[1];
             if (inst->popup)
               _mixer_popup_update(inst, gmixer_context->sink_default->mute,
                                    msg->val[1]);
          }
        edje_object_message_send(elm_layout_edje_get(inst->o_mixer), EDJE_MESSAGE_INT_SET, 0, msg);
        elm_layout_signal_emit(inst->o_mixer, "e,action,volume,change", "e");
     }
}

static void
_volume_increase_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   unsigned int i;
   Emix_Volume volume;

   EINA_SAFETY_ON_NULL_RETURN(gmixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)gmixer_context->sink_default;

   if (!s->volume.channel_count) return;

   if (BARRIER_CHECK(s->volume.volumes[0], s->volume.volumes[0] + VOLUME_STEP))
     return;

   volume.channel_count = s->volume.channel_count;
   volume.volumes = calloc(s->volume.channel_count, sizeof(int));
   for (i = 0; i < volume.channel_count; i++)
     {
        if (s->volume.volumes[i] < (emix_max_volume_get()) - VOLUME_STEP)
          volume.volumes[i] = s->volume.volumes[i] + VOLUME_STEP;
        else if (s->volume.volumes[i] < emix_max_volume_get())
          volume.volumes[i] = emix_max_volume_get();
        else
          volume.volumes[i] = s->volume.volumes[i];
     }

   emix_sink_volume_set(s, volume);
   emix_config_save_state_get();
   if (emix_config_save_get()) e_config_save_queue();
   free(volume.volumes);
}

static void
_volume_decrease_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   unsigned int i;
   Emix_Volume volume;

   EINA_SAFETY_ON_NULL_RETURN(gmixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)gmixer_context->sink_default;
   volume.channel_count = s->volume.channel_count;
   volume.volumes = calloc(s->volume.channel_count, sizeof(int));
   for (i = 0; i < volume.channel_count; i++)
     {
        if (s->volume.volumes[i] > VOLUME_STEP)
          volume.volumes[i] = s->volume.volumes[i] - VOLUME_STEP;
        else if (s->volume.volumes[i] < VOLUME_STEP)
          volume.volumes[i] = 0;
        else
          volume.volumes[i] = s->volume.volumes[i];
     }

   emix_sink_volume_set(s, volume);
   emix_config_save_state_get();
   if (emix_config_save_get()) e_config_save_queue();
   free(volume.volumes);
}

static void
_volume_mute_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   EINA_SAFETY_ON_NULL_RETURN(gmixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)gmixer_context->sink_default;
   Eina_Bool mute = !s->mute;
   emix_sink_mute_set(s, mute);
   emix_config_save_state_get();
   if (emix_config_save_get()) e_config_save_queue();
}

static void
_volume_increase_app_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Client *ec;

   ec = e_client_focused_get();
   if (ec && ec->volume_control_enabled)
     {
        e_client_volume_set(ec, ec->volume + VOLUME_STEP);
     }
}

static void
_volume_decrease_app_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Client *ec;

   ec = e_client_focused_get();
   if (ec && ec->volume_control_enabled)
     {
        e_client_volume_set(ec, ec->volume - VOLUME_STEP);
     }
}

static void
_volume_mute_app_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Client *ec;

   ec = e_client_focused_get();
   if (ec && ec->volume_control_enabled)
     {
        e_client_volume_mute_set(ec, !ec->mute);
     }
}

static void
_actions_register(void)
{
   gmixer_context->actions.incr = e_action_add("volume_increase");
   if (gmixer_context->actions.incr)
     {
        gmixer_context->actions.incr->func.go = _volume_increase_cb;
        e_action_predef_name_set("Mixer", _("Increase Volume"),
                                 "volume_increase", NULL, NULL, 0);
     }

   gmixer_context->actions.decr = e_action_add("volume_decrease");
   if (gmixer_context->actions.decr)
     {
        gmixer_context->actions.decr->func.go = _volume_decrease_cb;
        e_action_predef_name_set("Mixer", _("Decrease Volume"),
                                 "volume_decrease", NULL, NULL, 0);
     }

   gmixer_context->actions.mute = e_action_add("volume_mute");
   if (gmixer_context->actions.mute)
     {
        gmixer_context->actions.mute->func.go = _volume_mute_cb;
        e_action_predef_name_set("Mixer", _("Mute volume"), "volume_mute",
                                 NULL, NULL, 0);
     }
   gmixer_context->actions.incr_app = e_action_add("volume_increase_app");
   if (gmixer_context->actions.incr_app)
     {
        gmixer_context->actions.incr_app->func.go = _volume_increase_app_cb;
        e_action_predef_name_set("Mixer",
                                 _("Increase Volume of Focused Application"),
                                 "volume_increase_app", NULL, NULL, 0);
     }
   gmixer_context->actions.decr_app = e_action_add("volume_decrease_app");
   if (gmixer_context->actions.decr_app)
     {
        gmixer_context->actions.decr_app->func.go = _volume_decrease_app_cb;
        e_action_predef_name_set("Mixer",
                                 _("Decrease Volume of Focused Application"),
                                 "volume_decrease_app", NULL, NULL, 0);
     }
   gmixer_context->actions.mute_app = e_action_add("volume_mute_app");
   if (gmixer_context->actions.mute_app)
     {
        gmixer_context->actions.mute_app->func.go = _volume_mute_app_cb;
        e_action_predef_name_set("Mixer",
                                 _("Mute Volume of Focused Application"),
                                 "volume_mute_app", NULL, NULL, 0);
     }

   e_comp_canvas_keys_ungrab();
   e_comp_canvas_keys_grab();
}

static void
_actions_unregister(void)
{
   if (gmixer_context->actions.incr)
     {
        e_action_predef_name_del("Mixer", _("Increase Volume"));
        e_action_del("volume_increase");
        gmixer_context->actions.incr = NULL;
     }

   if (gmixer_context->actions.decr)
     {
        e_action_predef_name_del("Mixer", _("Decrease Volume"));
        e_action_del("volume_decrease");
        gmixer_context->actions.decr = NULL;
     }

   if (gmixer_context->actions.mute)
     {
        e_action_predef_name_del("Mixer", _("Mute Volume"));
        e_action_del("volume_mute");
        gmixer_context->actions.mute = NULL;
     }

   if (gmixer_context->actions.incr_app)
     {
        e_action_predef_name_del("Mixer",
                                 _("Increase Volume of Focuse Application"));
        e_action_del("volume_increase_app");
        gmixer_context->actions.incr_app = NULL;
     }

   if (gmixer_context->actions.decr_app)
     {
        e_action_predef_name_del("Mixer",
                                 _("Decrease Volume of Focuse Application"));
        e_action_del("volume_decrease_app");
        gmixer_context->actions.decr_app = NULL;
     }

   if (gmixer_context->actions.mute_app)
     {
        e_action_predef_name_del("Mixer",
                                 _("Mute Volume of Focuse Application"));
        e_action_del("volume_mute_app");
        gmixer_context->actions.mute_app = NULL;
     }

   e_comp_canvas_keys_ungrab();
   e_comp_canvas_keys_grab();
}

static Eina_Bool
_emixer_del_cb(void *data EINA_UNUSED, int type EINA_UNUSED,
               void *info EINA_UNUSED)
{
   gmixer_context->emixer = NULL;
   if (gmixer_context->emix_event_handler)
      ecore_event_handler_del(gmixer_context->emix_event_handler);

   return EINA_TRUE;
}

static void
_emixer_exec_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   elm_ctxpopup_dismiss(inst->popup);
   if (gmixer_context->emixer) return;

   gmixer_context->emixer = ecore_exe_run("emixer", NULL);
   if (gmixer_context->emix_event_handler)
      ecore_event_handler_del(gmixer_context->emix_event_handler);
   gmixer_context->emix_event_handler =
      ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _emixer_del_cb, NULL);
}

static void
_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                  void *event EINA_UNUSED)
{
   EINA_SAFETY_ON_NULL_RETURN(gmixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)gmixer_context->sink_default;
   emix_sink_mute_set(s, !s->mute);
   emix_config_save_state_get();
   if (emix_config_save_get()) e_config_save_queue();
   /*
    *TODO: is it really necessary ? or it will be update
    *      with the sink changed hanlder
    */
   _mixer_gadget_update();
}

static void
_slider_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                   void *event EINA_UNUSED)
{
   int val;

   EINA_SAFETY_ON_NULL_RETURN(gmixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)gmixer_context->sink_default;

   val = (int)elm_slider_value_get(obj);
   VOLSET(val, s->volume, s, emix_sink_volume_set);
   emix_config_save_state_get();
   if (emix_config_save_get()) e_config_save_queue();
}

static void
_sink_selected_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Emix_Sink *s = data;

   gmixer_context->sink_default = s;
   if (s) emix_config_save_sink_set(s->name);
   _mixer_gadget_update();
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
}

static void
_popup_new(Instance *inst)
{
   Evas_Object *button, *list, *slider, *bx;
   Emix_Sink *s;
   Eina_List *l;
   int num = 0;
   Elm_Object_Item *default_it = NULL;
   unsigned int volume = 0, i;

   EINA_SAFETY_ON_NULL_RETURN(gmixer_context->sink_default);
   unsigned int channels = gmixer_context->sink_default->volume.channel_count;

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
        if (gmixer_context->sink_default == s)
          default_it = it;
        num++;
     }
   elm_list_go(inst->list);
   elm_box_pack_end(list, inst->list);

   for (volume = 0, i = 0; i < channels; i++)
     volume += gmixer_context->sink_default->volume.volumes[i];
   if (channels) volume = volume / channels;

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
   elm_slider_value_set(slider, volume);
   elm_box_pack_end(bx, slider);
   evas_object_show(slider);

   inst->mute = gmixer_context->sink_default->mute;
   inst->check = elm_check_add(e_comp->elm);
   evas_object_size_hint_align_set(inst->check, 0.5, EVAS_HINT_FILL);
   elm_object_text_set(inst->check, _("Mute"));
   elm_check_state_pointer_set(inst->check, &(inst->mute));
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
_mouse_down_cb(void *data, Evas *evas EINA_UNUSED,
               Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

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
        _volume_mute_cb(NULL, NULL);
     }
   else if (ev->button == 3)
     {
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        e_gadget_configure(inst->o_main);
     }
}

static void
_mouse_wheel_cb(void *data EINA_UNUSED, Evas *evas EINA_UNUSED,
                Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Wheel *ev = event;

   if (ev->z > 0)
     _volume_decrease_cb(NULL, NULL);
   else if (ev->z < 0)
     _volume_increase_cb(NULL, NULL);
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
_mixer_gadget_created_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->o_main)
     {
        e_gadget_configure_cb_set(inst->o_main, _mixer_gadget_configure);

        inst->o_mixer = elm_layout_add(inst->o_main);
        E_EXPAND(inst->o_mixer);
        E_FILL(inst->o_mixer);
        e_theme_edje_object_set(inst->o_mixer, "base/theme/modules/mixer", "e/modules/mixer/main");
        evas_object_event_callback_add(inst->o_mixer, EVAS_CALLBACK_MOUSE_DOWN,
                                  _mouse_down_cb, inst);
        evas_object_event_callback_add(inst->o_mixer, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _mouse_wheel_cb, inst);
        elm_box_pack_end(inst->o_main, inst->o_mixer);
        evas_object_show(inst->o_mixer);
        gmixer_context->instances = eina_list_append(gmixer_context->instances, inst);
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
mixer_gadget_create(Evas_Object *parent, int *id EINA_UNUSED, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->o_main = elm_box_add(parent);
   E_EXPAND(inst->o_main);
   E_FILL(inst->o_main);
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_show(inst->o_main);   

   evas_object_smart_callback_add(parent, "gadget_created", _mixer_gadget_created_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, mixer_del, inst);
   return inst->o_main;
}

static void
_sink_event(int type, void *info)
{
   Emix_Sink *sink = info;
   const Eina_List *l;

   if (type == EMIX_SINK_REMOVED_EVENT)
     {
        if (sink == gmixer_context->sink_default)
          {
             l = emix_sinks_get();
             if (l)
               gmixer_context->sink_default = l->data;
             else
               gmixer_context->sink_default = NULL;
             if (emix_config_save_get()) e_config_save_queue();
             _mixer_gadget_update();
          }
     }
   else if (type == EMIX_SINK_CHANGED_EVENT)
     {
        if (gmixer_context->sink_default == sink)
          {
             int vol;

             _mixer_gadget_update();
             if (sink->mute || !sink->volume.channel_count)
               vol = 0;
             else
               vol = sink->volume.volumes[0];

             _notify(vol);
          }
     }
   else
     {
        DBG("Sink added");
     }
   /*
     Only safe the state if we are not in init mode,
     If we are in init mode, this is a result of the restore call.
     Restore iterates over a list of sinks which would get deleted in the
     save_state_get call.
    */
   if (!init)
     {
        emix_config_save_state_get();
        if (emix_config_save_get()) e_config_save_queue();
     }
}

static void
_disconnected(void)
{
   if (gmixer_context) gmixer_context->sink_default = NULL;
   _mixer_gadget_update();
}

static void
_ready(void)
{
   init = EINA_TRUE;
   if (emix_sink_default_support())
     gmixer_context->sink_default = emix_sink_default_get();
   else
     {
        if (emix_sinks_get())
          gmixer_context->sink_default = emix_sinks_get()->data;
     }

   if (emix_config_save_get())
     {
        Emix_Sink *s;
        const char *sinkname;

        sinkname = emix_config_save_sink_get();
        if (sinkname)
          {
             Eina_List *sinks = (Eina_List *)emix_sinks_get();
             Eina_List *l;

             EINA_LIST_FOREACH(sinks, l, s)
               {
                  if ((s->name) && (!strcmp(s->name, sinkname)))
                    {
                       gmixer_context->sink_default = s;
                       break;
                    }
               }
          }
        emix_config_save_state_restore();
     }

   _mixer_gadget_update();
   init = EINA_FALSE;
}

static void
_sink_input_get(int *volume, Eina_Bool *muted, void *data)
{
   Emix_Sink_Input *input;

   input = data;

   if (input->volume.channel_count > 0)
     {
        if (volume) *volume = input->volume.volumes[0];
     }
   if (muted) *muted = input->mute;
}

static void
_sink_input_set(int volume, Eina_Bool muted, void *data)
{
   Emix_Sink_Input *input;

   input = data;

   VOLSET(volume, input->volume, input, emix_sink_input_volume_set);
   emix_sink_input_mute_set(input, muted);
}

static int
_sink_input_min_get(void *data EINA_UNUSED)
{
   return 0;
}

static int
_sink_input_max_get(void *data EINA_UNUSED)
{
   return emix_max_volume_get();
}

static const char *
_sink_input_name_get(void *data)
{
   Emix_Sink_Input *input;

   input = data;
   return input->name;
}

static pid_t
_get_ppid(pid_t pid)
{
   int fd;
   char buf[128];
   char *s;
   pid_t ppid;

   /* Open the status info process file provided by kernel to get the parent
    * process id. 'man 5 proc' and go to /proc/[pid]/stat to get information
    * about the content of this file.
    */
   snprintf(buf, sizeof(buf), "/proc/%d/stat", pid);
   fd = open(buf, O_RDONLY);
   if (fd == -1)
     {
        ERR("Can't open %s, maybee the process exited.", buf);
        return -1;
     }
   if ((read(fd, buf, sizeof(buf))) < 4)
     {
        close(fd);
        return -1;
     }
   buf[sizeof(buf) - 1] = 0;
   s = strrchr(buf, ')');
   s += 3;
   ppid = atoi(s);
   close(fd);
   return ppid;
}

static void
_sink_input_event(int type, Emix_Sink_Input *input)
{
   Eina_List *clients, *l;
   E_Client *ec;
   E_Client_Volume_Sink *sink;
   pid_t pid;

   switch (type)
     {
      case EMIX_SINK_INPUT_ADDED_EVENT:
         pid = input->pid;
         while (42)
           {
              if (pid <= 1 || pid == getpid()) return;
              clients = e_client_focus_stack_get();
              EINA_LIST_FOREACH(clients, l, ec)
                {
                   if ((ec->netwm.pid == pid) && (!ec->parent))
                     {
                        DBG("Sink found the client %s",
                            e_client_util_name_get(ec));
                        sink = e_client_volume_sink_new(_sink_input_get,
                                                        _sink_input_set,
                                                        _sink_input_min_get,
                                                        _sink_input_max_get,
                                                        _sink_input_name_get,
                                                        input);
                        e_client_volume_sink_append(ec, sink);
                        _client_sinks = eina_list_append(_client_sinks, sink);
                        return;
                     }
                }
              pid = _get_ppid(pid);
           }
         break;
      case EMIX_SINK_INPUT_REMOVED_EVENT:
         EINA_LIST_FOREACH(_client_sinks, l, sink)
           {
              if (sink->data == input)
                {
                   e_client_volume_sink_del(sink);
                   _client_sinks = eina_list_remove_list(_client_sinks, l);
                   break;
                }
           }
         break;
      case EMIX_SINK_INPUT_CHANGED_EVENT:
         EINA_LIST_FOREACH(_client_sinks, l, sink)
           {
              if (sink->data == input)
                {
                   e_client_volume_sink_update(sink);
                }
           }
         break;
     }
}

static void
_events_cb(void *data EINA_UNUSED, enum Emix_Event type, void *event_info)
{
   switch (type)
     {
      case EMIX_SINK_ADDED_EVENT:
      case EMIX_SINK_CHANGED_EVENT:
      case EMIX_SINK_REMOVED_EVENT:
         _sink_event(type, event_info);
         break;
      case EMIX_DISCONNECTED_EVENT:
         _disconnected();
         break;
      case EMIX_READY_EVENT:
         _ready();
         break;
      case EMIX_SINK_INPUT_ADDED_EVENT:
      case EMIX_SINK_INPUT_REMOVED_EVENT:
      case EMIX_SINK_INPUT_CHANGED_EVENT:
         _sink_input_event(type, event_info);
         break;

      default:
         break;
     }
}

static Eina_Bool
_desklock_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *info)
{
   E_Event_Desklock *ev = info;
   static Eina_Bool _was_mute = EINA_FALSE;

   if (emix_config_desklock_mute_get() == EINA_FALSE)
     return ECORE_CALLBACK_PASS_ON;
   if (!gmixer_context)
     return ECORE_CALLBACK_PASS_ON;

   if (ev->on)
     {
        if (gmixer_context->sink_default)
          {
             _was_mute = gmixer_context->sink_default->mute;
             if (!_was_mute)
               emix_sink_mute_set((Emix_Sink *)gmixer_context->sink_default, EINA_TRUE);
          }
     }
   else
     {
        if (gmixer_context->sink_default)
          {
             if (!_was_mute)
               emix_sink_mute_set((Emix_Sink *)gmixer_context->sink_default, EINA_FALSE);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_backend_changed(const char *backend, void *data EINA_UNUSED)
{
   _disconnected();

   if (emix_backend_set(backend) == EINA_FALSE)
     ERR("Could not load backend: %s", backend);
}

static Eina_Bool
_e_client_volume_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   Client_Mixer *cm;
   E_Client_Volume_Sink *sink;
   Evas_Object *o;
   Eina_List *l;

   ev = event;

   EINA_LIST_FOREACH(_client_mixers, l, cm)
     {
        if (cm->ec == ev->ec)
          {
             elm_slider_value_set(cm->volume, cm->ec->volume);
             EINA_LIST_FOREACH(cm->sinks, l, o)
               {
                  int volume;
                  sink = evas_object_data_get(o, "e_sink");
                  e_client_volume_sink_get(sink, &volume, NULL);
                  elm_slider_value_set(o, volume);
               }
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_client_mute_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   Client_Mixer *cm;
   E_Client_Volume_Sink *sink;
   Evas_Object *o, *check;
   Eina_List *l;
   Eina_Bool mute;

   ev = event;

   EINA_LIST_FOREACH(_client_mixers, l, cm)
     {
        if (cm->ec == ev->ec)
          {
             elm_check_state_set(cm->mute, !!cm->ec->mute);
             elm_object_disabled_set(cm->volume, !!cm->ec->mute);
             EINA_LIST_FOREACH(cm->sinks, l, o)
               {
                  sink = evas_object_data_get(o, "e_sink");
                  check = evas_object_data_get(o, "e_sink_check");
                  e_client_volume_sink_get(sink, NULL, &mute);
                  elm_check_state_set(check, mute);
                  elm_object_disabled_set(o, mute);
               }
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_bd_hook_sink_volume_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Client_Volume_Sink *sink;
   Evas_Object *check;

   sink = data;

   check = evas_object_data_get(obj, "e_sink_check");

   e_client_volume_sink_set(sink,
                            elm_slider_value_get(obj),
                            elm_check_state_get(check));
}

static void
_bd_hook_sink_volume_drag_stop(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Client_Volume_Sink *sink;
   Evas_Object *check;
   Eina_Bool mute;
   int vol;

   sink = data;

   check = evas_object_data_get(obj, "e_sink_check");

   e_client_volume_sink_get(sink, &vol, &mute);
   elm_slider_value_set(obj, vol);
   elm_check_state_set(check, mute);
}


static void
_bd_hook_sink_mute_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Client_Volume_Sink *sink;
   Evas_Object *slider;

   sink = data;
   slider = evas_object_data_get(obj, "e_sink_volume");

   e_client_volume_sink_set(sink,
                            elm_slider_value_get(slider),
                            elm_check_state_get(obj));
}

static void
_e_client_mixer_sink_append(E_Client_Volume_Sink *sink, Client_Mixer *cm)
{
   Evas_Object *lbl, *slider, *check, *sep;
   int volume;
   int min, max;
   Eina_Bool mute;

   min = e_client_volume_sink_min_get(sink);
   max = e_client_volume_sink_max_get(sink);
   e_client_volume_sink_get(sink, &volume, &mute);

   sep = elm_separator_add(cm->bx);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(cm->bx, sep);
   evas_object_show(sep);

   lbl = elm_label_add(cm->bx);
   elm_object_text_set(lbl, e_client_volume_sink_name_get(sink));
   evas_object_size_hint_align_set(lbl, 0.0, EVAS_HINT_FILL);
   elm_box_pack_end(cm->bx, lbl);
   evas_object_show(lbl);

   slider = elm_slider_add(cm->bx);
   elm_slider_horizontal_set(slider, EINA_TRUE);
   elm_slider_min_max_set(slider, min, max);
   elm_slider_span_size_set(slider, max * elm_config_scale_get());
   elm_slider_unit_format_set(slider, "%.0f");
   elm_slider_indicator_format_set(slider, "%.0f");
   evas_object_size_hint_weight_set(slider, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_slider_value_set(slider, volume);
   evas_object_smart_callback_add(slider, "changed",
                                  _bd_hook_sink_volume_changed, sink);
   evas_object_smart_callback_add(slider, "slider,drag,stop",
                                  _bd_hook_sink_volume_drag_stop, sink);
   elm_box_pack_end(cm->bx, slider);
   evas_object_show(slider);

   check = elm_check_add(cm->bx);
   elm_object_text_set(check, _("Mute"));
   evas_object_size_hint_align_set(check, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(check, !!mute);
   elm_object_disabled_set(slider, !!mute);
   evas_object_smart_callback_add(check, "changed",
                                  _bd_hook_sink_mute_changed, sink);

   elm_box_pack_end(cm->bx, check);
   evas_object_show(check);

   evas_object_data_set(slider, "e_sink", sink);
   evas_object_data_set(slider, "e_sink_check", check);
   evas_object_data_set(slider, "e_sink_label", lbl);
   evas_object_data_set(slider, "e_sink_separator", sep);
   evas_object_data_set(check, "e_sink_volume", slider);
   cm->sinks = eina_list_append(cm->sinks, slider);
}

static Eina_Bool
_e_client_volume_sink_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Volume_Sink *ev;
   Client_Mixer *cm;
   Eina_List *l;

   ev = event;

   EINA_LIST_FOREACH(_client_mixers, l, cm)
     {
        if (cm->ec == ev->ec)
          {
             _e_client_mixer_sink_append(ev->sink, cm);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_client_volume_sink_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Volume_Sink *ev;
   E_Client_Volume_Sink *sink;
   Client_Mixer *cm;
   Evas_Object *o, *lbl, *check, *sep;
   Eina_List *l;

   ev = event;

   EINA_LIST_FOREACH(_client_mixers, l, cm)
     {
        if (cm->ec == ev->ec)
          {
             EINA_LIST_FOREACH(cm->sinks, l, o)
               {
                  sink = evas_object_data_get(o, "e_sink");
                  if (sink == ev->sink)
                    {
                       lbl = evas_object_data_get(o, "e_sink_label");
                       check = evas_object_data_get(o, "e_sink_check");
                       sep = evas_object_data_get(o, "e_sink_separator");
                       evas_object_del(sep);
                       evas_object_del(lbl);
                       evas_object_del(o);
                       evas_object_del(check);
                       cm->sinks = eina_list_remove_list(cm->sinks, l);
                    }
               }
             break;
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_client_volume_sink_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Volume_Sink *ev;
   Client_Mixer *cm;
   E_Client_Volume_Sink *sink;
   Evas_Object *o, *check;
   Eina_List *l;
   int volume;
   Eina_Bool mute;

   ev = event;

   EINA_LIST_FOREACH(_client_mixers, l, cm)
     {
        if (cm->ec == ev->ec)
          {
             EINA_LIST_FOREACH(cm->sinks, l, o)
               {
                  sink = evas_object_data_get(o, "e_sink");
                  if (sink != ev->sink) continue;
                  check = evas_object_data_get(o, "e_sink_check");
                  e_client_volume_sink_get(sink, &volume, &mute);
                  elm_slider_value_set(o, volume);
                  elm_object_disabled_set(o, mute);
                  elm_check_state_set(check, mute);
               }
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_client_mixer_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Client_Mixer *cm;

   cm = data;

   _client_mixers = eina_list_remove(_client_mixers, cm);
   free(cm);
}

static Eina_Bool
_e_client_remove(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   Client_Mixer *cm;
   Eina_List *l;

   ev = event;

   EINA_LIST_FOREACH(_client_mixers, l, cm)
     {
        if (cm->ec == ev->ec)
          {
             evas_object_event_callback_del_full(cm->win, EVAS_CALLBACK_DEL,
                                                 _client_mixer_del, cm);
             evas_object_del(cm->win);
             _client_mixers = eina_list_remove_list(_client_mixers, l);
             free(cm);
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

EINTERN Eina_Bool
mixer_init(void)
{
   Eina_List *l;
   char buf[4096];
   const char *backend;
   Eina_Bool backend_loaded = EINA_FALSE;

   if (!gmixer_context)
     {
        gmixer_context = E_NEW(Context, 1);

        gmixer_context->desklock_handler =
           ecore_event_handler_add(E_EVENT_DESKLOCK, _desklock_cb, NULL);
        gmixer_context->module = gm;
        snprintf(buf, sizeof(buf), "%s/mixer.edj",
                 e_module_dir_get(gmixer_context->module));
        gmixer_context->theme = strdup(buf);
     }

   emix_config_init(_backend_changed, NULL);
   emix_event_callback_add(_events_cb, NULL);

   backend = emix_config_backend_get();
   if (backend && emix_backend_set(backend))
      backend_loaded = EINA_TRUE;
   else
     {
        if (backend)
           WRN("Could not load %s, trying another one ...", backend);
        EINA_LIST_FOREACH((Eina_List *)emix_backends_available(), l,
                          backend)
          {
             if (emix_backend_set(backend) == EINA_TRUE)
               {
                  DBG("Loaded backend: %s!", backend);
                  backend_loaded = EINA_TRUE;
                  emix_config_backend_set(backend);
                  break;
               }
          }
     }

   if (!backend_loaded) return EINA_FALSE;

   if (emix_sink_default_support())
     gmixer_context->sink_default = emix_sink_default_get();

   _actions_register();

   E_LIST_HANDLER_APPEND(_client_handlers, E_EVENT_CLIENT_VOLUME,
                         _e_client_volume_changed, NULL);
   E_LIST_HANDLER_APPEND(_client_handlers, E_EVENT_CLIENT_MUTE,
                         _e_client_mute_changed, NULL);
   E_LIST_HANDLER_APPEND(_client_handlers, E_EVENT_CLIENT_UNMUTE,
                         _e_client_mute_changed, NULL);
   E_LIST_HANDLER_APPEND(_client_handlers, E_EVENT_CLIENT_REMOVE,
                         _e_client_remove, NULL);
   E_LIST_HANDLER_APPEND(_client_handlers, E_EVENT_CLIENT_VOLUME_SINK_ADD,
                         _e_client_volume_sink_add, NULL);
   E_LIST_HANDLER_APPEND(_client_handlers, E_EVENT_CLIENT_VOLUME_SINK_DEL,
                         _e_client_volume_sink_del, NULL);
   E_LIST_HANDLER_APPEND(_client_handlers, E_EVENT_CLIENT_VOLUME_SINK_CHANGED,
                         _e_client_volume_sink_changed, NULL);

   return EINA_TRUE;
}

EINTERN void
mixer_shutdown(void)
{
   E_Client_Volume_Sink *sink;
   Client_Mixer *cm;

   E_FREE_LIST(_client_handlers, ecore_event_handler_del);
   EINA_LIST_FREE(_client_mixers, cm)
     {
        evas_object_event_callback_del_full(cm->win, EVAS_CALLBACK_DEL,
                                            _client_mixer_del, cm);
        evas_object_del(cm->win);
        free(cm);
     }

   _actions_unregister();

   if (gmixer_context)
     {
        free(gmixer_context->theme);
        E_FREE(gmixer_context);
     }
   EINA_LIST_FREE(_client_sinks, sink)
      e_client_volume_sink_del(sink);
   emix_event_callback_del(_events_cb);
}

