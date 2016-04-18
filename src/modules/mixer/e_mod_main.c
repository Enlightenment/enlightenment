#include <e.h>
#include <Eina.h>
#include "emix.h"
#include "e_mod_main.h"
#include "e_mod_config.h"

#define VOLUME_STEP 5

int _e_emix_log_domain;
static Eina_Bool init;

/* module requirements */
E_API E_Module_Api e_modapi =
   {
      E_MODULE_API_VERSION,
      "Mixer"
   };

/* necessary forward delcaration */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name,
                                 const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc,
                                   E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class,
                                 Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);

static const E_Gadcon_Client_Class _gadcon_class =
   {
      GADCON_CLIENT_CLASS_VERSION,
      "mixer",
      {
         _gc_init, _gc_shutdown,
         _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
         e_gadcon_site_is_not_toolbar
      },
      E_GADCON_CLIENT_STYLE_PLAIN
   };

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
   } actions;
};

typedef struct _Instance Instance;
struct _Instance
{
   E_Gadcon_Client *gcc;
   E_Gadcon_Orient orient;

   E_Gadcon_Popup *popup;
   Evas *evas;
   Evas_Object *gadget;
   Evas_Object *list;
   Evas_Object *slider;
   Evas_Object *check;

   Eina_Bool mute;
};

static Context *mixer_context = NULL;

static void
_notify_cb(void *data EINA_UNUSED, unsigned int id)
{
   mixer_context->notification_id = id;
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

   n.app_name = _("Emix");
   n.replaces_id = mixer_context->notification_id;
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

static void _popup_del(Instance *inst);

static void
_mixer_gadget_update(void)
{
   Edje_Message_Int_Set *msg;
   Instance *inst;
   Eina_List *l;

   EINA_LIST_FOREACH(mixer_context->instances, l, inst)
     {
        msg = alloca(sizeof(Edje_Message_Int_Set) + (2 * sizeof(int)));
        msg->count = 3;

        if (!mixer_context->sink_default)
          {
             msg->val[0] = EINA_FALSE;
             msg->val[1] = 0;
             msg->val[2] = 0;
             if (inst->popup)
               _popup_del(inst);
          }
        else
          {
             int vol = 0;
             unsigned int i = 0;
             for (i = 0; i <
                  mixer_context->sink_default->volume.channel_count; i++)
               vol += mixer_context->sink_default->volume.volumes[i];
             if (mixer_context->sink_default->volume.channel_count)
               vol /= mixer_context->sink_default->volume.channel_count;
             msg->val[0] = mixer_context->sink_default->mute;
             msg->val[1] = vol;
             msg->val[2] = msg->val[1];
             if (inst->popup)
               _mixer_popup_update(inst, mixer_context->sink_default->mute,
                                    msg->val[1]);
          }
        edje_object_message_send(inst->gadget, EDJE_MESSAGE_INT_SET, 0, msg);
        edje_object_signal_emit(inst->gadget, "e,action,volume,change", "e");
     }
}

static void
_volume_increase_cb(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED)
{
   unsigned int i;
   Emix_Volume volume;

   EINA_SAFETY_ON_NULL_RETURN(mixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)mixer_context->sink_default;
   volume.channel_count = s->volume.channel_count;
   volume.volumes = calloc(s->volume.channel_count, sizeof(int));
   for (i = 0; i < volume.channel_count; i++)
     {
        if (s->volume.volumes[i] < (EMIX_VOLUME_MAX + 50) - VOLUME_STEP)
          volume.volumes[i] = s->volume.volumes[i] + VOLUME_STEP;
        else if (s->volume.volumes[i] < EMIX_VOLUME_MAX + 50)
          volume.volumes[i] = EMIX_VOLUME_MAX + 50;
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

   EINA_SAFETY_ON_NULL_RETURN(mixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)mixer_context->sink_default;
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
   EINA_SAFETY_ON_NULL_RETURN(mixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)mixer_context->sink_default;
   Eina_Bool mute = !s->mute;
   emix_sink_mute_set(s, mute);
   emix_config_save_state_get();
   if (emix_config_save_get()) e_config_save_queue();
}

static void
_actions_register(void)
{
   mixer_context->actions.incr = e_action_add("volume_increase");
   if (mixer_context->actions.incr)
     {
        mixer_context->actions.incr->func.go = _volume_increase_cb;
        e_action_predef_name_set("Mixer", _("Increase Volume"),
                                 "volume_increase", NULL, NULL, 0);
     }

   mixer_context->actions.decr = e_action_add("volume_decrease");
   if (mixer_context->actions.decr)
     {
        mixer_context->actions.decr->func.go = _volume_decrease_cb;
        e_action_predef_name_set("Mixer", _("Decrease Volume"),
                                 "volume_decrease", NULL, NULL, 0);
     }

   mixer_context->actions.mute = e_action_add("volume_mute");
   if (mixer_context->actions.mute)
     {
        mixer_context->actions.mute->func.go = _volume_mute_cb;
        e_action_predef_name_set("Mixer", _("Mute volume"), "volume_mute",
                                 NULL, NULL, 0);
     }

   e_comp_canvas_keys_ungrab();
   e_comp_canvas_keys_grab();
}

static void
_actions_unregister(void)
{
   if (mixer_context->actions.incr)
     {
        e_action_predef_name_del("Mixer", _("Increase Volume"));
        e_action_del("volume_increase");
        mixer_context->actions.incr = NULL;
     }

   if (mixer_context->actions.decr)
     {
        e_action_predef_name_del("Mixer", _("Decrease Volume"));
        e_action_del("volume_decrease");
        mixer_context->actions.decr = NULL;
     }

   if (mixer_context->actions.mute)
     {
        e_action_predef_name_del("Mixer", _("Mute Volume"));
        e_action_del("volume_mute");
        mixer_context->actions.mute = NULL;
     }

   e_comp_canvas_keys_ungrab();
   e_comp_canvas_keys_grab();
}

static void
_popup_del(Instance *inst)
{
   inst->slider = NULL;
   inst->check = NULL;
   E_FREE_FUNC(inst->popup, e_object_del);
}

static void
_popup_del_cb(void *obj)
{
   _popup_del(e_object_data_get(obj));
}

static void
_popup_comp_del_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
   Instance *inst = data;

   E_FREE_FUNC(inst->popup, e_object_del);
}

static Eina_Bool
_emixer_del_cb(void *data EINA_UNUSED, int type EINA_UNUSED,
               void *info EINA_UNUSED)
{
   mixer_context->emixer = NULL;
   if (mixer_context->emix_event_handler)
      ecore_event_handler_del(mixer_context->emix_event_handler);

   return EINA_TRUE;
}

static void
_emixer_exec_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   _popup_del(inst);
   if (mixer_context->emixer) return;

   mixer_context->emixer = ecore_exe_run("emixer", NULL);
   if (mixer_context->emix_event_handler)
      ecore_event_handler_del(mixer_context->emix_event_handler);
   mixer_context->emix_event_handler =
      ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _emixer_del_cb, NULL);
}

static void
_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                  void *event EINA_UNUSED)
{
   EINA_SAFETY_ON_NULL_RETURN(mixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)mixer_context->sink_default;
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
   Emix_Volume v;
   unsigned int i;

   EINA_SAFETY_ON_NULL_RETURN(mixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)mixer_context->sink_default;
   val = (int)elm_slider_value_get(obj);
   v.volumes = calloc(s->volume.channel_count, sizeof(int));
   v.channel_count = s->volume.channel_count;
   for (i = 0; i < s->volume.channel_count; i++) v.volumes[i] = val;
   emix_sink_volume_set(s, v);
   elm_slider_value_set(obj, val);
   emix_config_save_state_get();
   if (emix_config_save_get()) e_config_save_queue();
   free(v.volumes);
}

static void
_slider_drag_stop_cb(void *data EINA_UNUSED, Evas_Object *obj,
                     void *event EINA_UNUSED)
{
   EINA_SAFETY_ON_NULL_RETURN(mixer_context->sink_default);
   Emix_Sink *s = (Emix_Sink *)mixer_context->sink_default;
   int val = s->volume.volumes[0];
   elm_slider_value_set(obj, val);
}

static void
_sink_selected_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Emix_Sink *s = data;

   mixer_context->sink_default = s;
   if (s) emix_config_save_sink_set(s->name);
   _mixer_gadget_update();
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

   EINA_SAFETY_ON_NULL_RETURN(mixer_context->sink_default);
   unsigned int channels = mixer_context->sink_default->volume.channel_count;
   inst->popup = e_gadcon_popup_new(inst->gcc, 0);
   list = elm_box_add(e_comp->elm);

   inst->list = elm_list_add(e_comp->elm);
   elm_list_mode_set(inst->list, ELM_LIST_COMPRESS);
   evas_object_size_hint_align_set(inst->list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(inst->list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(inst->list);

   EINA_LIST_FOREACH((Eina_List *)emix_sinks_get(), l, s)
     {
        Elm_Object_Item *it;

        it = elm_list_item_append(inst->list, s->name, NULL, NULL, _sink_selected_cb, s);
        if (mixer_context->sink_default == s)
          default_it = it;
        num++;
     }
   elm_list_go(inst->list);
   elm_box_pack_end(list, inst->list);

   for (volume = 0, i = 0; i < channels; i++)
     volume += mixer_context->sink_default->volume.volumes[i];
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
   elm_slider_min_max_set(slider, 0.0, EMIX_VOLUME_MAX + 50);
   evas_object_smart_callback_add(slider, "changed", _slider_changed_cb, NULL);
   evas_object_smart_callback_add(slider, "slider,drag,stop", _slider_drag_stop_cb, NULL);
   elm_slider_value_set(slider, volume);
   elm_box_pack_end(bx, slider);
   evas_object_show(slider);

   inst->mute = mixer_context->sink_default->mute;
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


   e_gadcon_popup_content_set(inst->popup, list);
   e_comp_object_util_autoclose(inst->popup->comp_object,
     _popup_comp_del_cb, NULL, inst);
   e_gadcon_popup_show(inst->popup);
   e_object_data_set(E_OBJECT(inst->popup), inst);
   E_OBJECT_DEL_SET(inst->popup, _popup_del_cb);

   if (default_it)
     elm_list_item_selected_set(default_it, EINA_TRUE);
}

static void
_menu_cb(void *data, E_Menu *menu EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   _emixer_exec_cb(data, NULL, NULL);
}

static void
_settings_cb(void *data EINA_UNUSED, E_Menu *menu EINA_UNUSED,
             E_Menu_Item *mi EINA_UNUSED)
{
   emix_config_popup_new(NULL, NULL);
}

static void
_menu_new(Instance *inst, Evas_Event_Mouse_Down *ev)
{
   E_Zone *zone;
   E_Menu *m;
   E_Menu_Item *mi;
   int x, y;

   zone = e_zone_current_get();

   m = e_menu_new();

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Advanced"));
   e_util_menu_item_theme_icon_set(mi, "configure");
   e_menu_item_callback_set(mi, _menu_cb, inst);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Settings"));
   e_util_menu_item_theme_icon_set(mi, "configure");
   e_menu_item_callback_set(mi, _settings_cb, inst);

   m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);

   e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);
   e_menu_activate_mouse(m, zone, x + ev->output.x, y + ev->output.y,
                         1, 1, E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
   evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button,
                            EVAS_BUTTON_NONE, ev->timestamp, NULL);
}

static void
_mouse_down_cb(void *data, Evas *evas EINA_UNUSED,
               Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->button == 1)
     {
        if (!inst->popup)
          _popup_new(inst);
     }
   else if (ev->button == 2)
     {
        _volume_mute_cb(NULL, NULL);
     }
   else if (ev->button == 3)
     {
        _menu_new(inst, ev);
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

/*
 * Gadcon functions
 */
static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   E_Gadcon_Client *gcc;
   Instance *inst;

   inst = E_NEW(Instance, 1);

   inst->gadget = edje_object_add(gc->evas);
   inst->evas = gc->evas;
   e_theme_edje_object_set(inst->gadget,
                           "base/theme/modules/mixer",
                           "e/modules/mixer/main");

   gcc = e_gadcon_client_new(gc, name, id, style, inst->gadget);
   gcc->data = inst;
   inst->gcc = gcc;

   evas_object_event_callback_add(inst->gadget, EVAS_CALLBACK_MOUSE_DOWN,
                                  _mouse_down_cb, inst);
   evas_object_event_callback_add(inst->gadget, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _mouse_wheel_cb, inst);
   mixer_context->instances = eina_list_append(mixer_context->instances, inst);

   if (mixer_context->sink_default)
     _mixer_gadget_update();

   return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;

   inst = gcc->data;
   evas_object_del(inst->gadget);
   mixer_context->instances = eina_list_remove(mixer_context->instances, inst);
   free(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   e_gadcon_client_aspect_set(gcc, 16, 16);
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return "Mixer";
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   char buf[4096] = { 0 };

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-mixer.edj",
            e_module_dir_get(mixer_context->module));
   edje_object_file_set(o, buf, "icon");

   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _gadcon_class.name;
}

static void
_sink_event(int type, void *info)
{
   Emix_Sink *sink = info;
   const Eina_List *l;

   if (type == EMIX_SINK_REMOVED_EVENT)
     {
        if (sink == mixer_context->sink_default)
          {
             l = emix_sinks_get();
             if (l)
               mixer_context->sink_default = l->data;
             else
               mixer_context->sink_default = NULL;
             if (emix_config_save_get()) e_config_save_queue();
             _mixer_gadget_update();
          }
     }
   else if (type == EMIX_SINK_CHANGED_EVENT)
     {
        if (mixer_context->sink_default == sink)
          {
             _mixer_gadget_update();
             _notify(sink->mute ? 0 : sink->volume.volumes[0]);
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
   if (mixer_context) mixer_context->sink_default = NULL;
   _mixer_gadget_update();
}


static void
_ready(void)
{
   init = EINA_TRUE;
   if (emix_sink_default_support())
     mixer_context->sink_default = emix_sink_default_get();
   else
     {
        if (emix_sinks_get())
          mixer_context->sink_default = emix_sinks_get()->data;
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
                       mixer_context->sink_default = s;
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
   if (!mixer_context)
     return ECORE_CALLBACK_PASS_ON;

   if (ev->on)
     {
        if (mixer_context->sink_default)
          {
             _was_mute = mixer_context->sink_default->mute;
             if (!_was_mute)
               emix_sink_mute_set((Emix_Sink *)mixer_context->sink_default, EINA_TRUE);
          }
     }
   else
     {
        if (mixer_context->sink_default)
          {
             if (!_was_mute)
               emix_sink_mute_set((Emix_Sink *)mixer_context->sink_default, EINA_FALSE);
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

E_API void *
e_modapi_init(E_Module *m)
{
   Eina_List *l;
   char buf[4096];
   const char *backend;
   Eina_Bool backend_loaded = EINA_FALSE;

   _e_emix_log_domain = eina_log_domain_register("mixer", EINA_COLOR_RED);

   if (!mixer_context)
     {
        mixer_context = E_NEW(Context, 1);

        mixer_context->desklock_handler =
           ecore_event_handler_add(E_EVENT_DESKLOCK, _desklock_cb, NULL);
        mixer_context->module = m;
        snprintf(buf, sizeof(buf), "%s/mixer.edj",
                 e_module_dir_get(mixer_context->module));
        mixer_context->theme = strdup(buf);
     }


   EINA_SAFETY_ON_FALSE_RETURN_VAL(emix_init(), NULL);
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

   if (!backend_loaded) goto err;

   e_configure_registry_category_add("extensions", 90, _("Extensions"), NULL,
                                     "preferences-extensions");
   e_configure_registry_item_add("extensions/emix", 30, _("Mixer"), NULL,
                                 "preferences-desktop-mixer",
                                 emix_config_popup_new);

   if (emix_sink_default_support())
      mixer_context->sink_default = emix_sink_default_get();

   e_gadcon_provider_register(&_gadcon_class);
   _actions_register();

   return m;

err:
   emix_config_shutdown();
   emix_shutdown();
   return NULL;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   _actions_unregister();
   e_gadcon_provider_unregister((const E_Gadcon_Client_Class *)&_gadcon_class);

   if (mixer_context)
     {
        free(mixer_context->theme);
        E_FREE(mixer_context);
     }

   emix_event_callback_del(_events_cb);
   emix_shutdown();
   emix_config_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   emix_config_save();
   return 1;
}

