#include <e.h>
#include <Eina.h>
#include "emix.h"
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "gadget/mixer.h"
#include "gadget/backend.h"

EINTERN int _e_emix_log_domain;

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
   E_Module *module;
   Eina_List *instances;
   E_Menu *menu;
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
};

static Context *mixer_context = NULL;
static Eina_List *_handlers = NULL;

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
   const Eina_List *ll;
   Elm_Object_Item *it;

   EINA_LIST_FOREACH(mixer_context->instances, l, inst)
     {
        msg = alloca(sizeof(Edje_Message_Int_Set) + (2 * sizeof(int)));
        msg->count = 3;

        if (!backend_sink_default_get())
          {
             msg->val[0] = EINA_FALSE;
             msg->val[1] = 0;
             msg->val[2] = 0;
             if (inst->popup)
               _popup_del(inst);
          }
        else
          {
             msg->val[0] = backend_mute_get();
             msg->val[1] = backend_volume_get();
             msg->val[2] = msg->val[1];
             if (inst->popup)
               _mixer_popup_update(inst, msg->val[0], msg->val[1]);
          }
        edje_object_message_send(inst->gadget, EDJE_MESSAGE_INT_SET, 0, msg);
        edje_object_signal_emit(inst->gadget, "e,action,volume,change", "e");

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
_popup_del(Instance *inst)
{
   inst->slider = NULL;
   inst->check = NULL;
   inst->list = NULL;
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

static void
_emixer_exec_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   _popup_del(inst);
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
   backend_sink_default_set(data);
}

static Eina_Bool
_mixer_sinks_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Instance *inst;
   Eina_List *l, *ll;

   EINA_LIST_FOREACH(mixer_context->instances, l, inst)
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
        backend_mute_set(!backend_mute_get());
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
     backend_volume_decrease();
   else if (ev->z < 0)
     backend_volume_increase();
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

E_API void *
e_modapi_init(E_Module *m)
{
   char buf[4096];

   _e_emix_log_domain = eina_log_domain_register("mixer", EINA_COLOR_RED);

   if (!backend_init()) return NULL;
   if (!mixer_context)
     {
        mixer_context = E_NEW(Context, 1);

        mixer_context->module = m;
        snprintf(buf, sizeof(buf), "%s/mixer.edj",
                 e_module_dir_get(mixer_context->module));
        mixer_context->theme = strdup(buf);
     }

   E_LIST_HANDLER_APPEND(_handlers, E_EVENT_MIXER_BACKEND_CHANGED,
                         _mixer_backend_changed, NULL);
   E_LIST_HANDLER_APPEND(_handlers, E_EVENT_MIXER_SINKS_CHANGED,
                         _mixer_sinks_changed, NULL);

   e_modapi_gadget_init(m);

   e_configure_registry_category_add("extensions", 90, _("Extensions"), NULL,
                                     "preferences-extensions");
   e_configure_registry_item_add("extensions/emix", 30, _("Mixer"), NULL,
                                 "preferences-desktop-mixer",
                                 emix_config_popup_new);

   e_gadcon_provider_register(&_gadcon_class);

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m)
{
   e_gadcon_provider_unregister((const E_Gadcon_Client_Class *)&_gadcon_class);

   E_FREE_LIST(_handlers, ecore_event_handler_del);
   if (mixer_context)
     {
        free(mixer_context->theme);
        E_FREE(mixer_context);
     }

   e_modapi_gadget_shutdown(m);

   backend_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m)
{
   e_modapi_gadget_save(m);
   emix_config_save();
   return 1;
}

