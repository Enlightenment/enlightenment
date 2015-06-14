#include <Elementary.h>
#include "emix.h"

Evas_Object *win;
Evas_Object *source_scroller, *sink_input_scroller, *sink_scroller;
Evas_Object *source_box, *sink_input_box, *sink_box;

Eina_List *source_list = NULL, *sink_input_list = NULL, *sink_list = NULL;

//////////////////////////////////////////////////////////////////////////////

static Eina_Bool
_backend_init(const char *back)
{
   const Eina_List *l;
   const char *name;

   if (!back) back = "PULSEAUDIO";
   if (emix_backend_set(back)) return EINA_TRUE;
   EINA_LIST_FOREACH(emix_backends_available(), l, name)
     {
        if (emix_backend_set(name)) return EINA_TRUE;
     }
   return EINA_FALSE;
}

//////////////////////////////////////////////////////////////////////////////

#define VOLSET(vol, srcvol, target, func) \
   do { \
      Emix_Volume _v; \
      int _pvol = srcvol.volumes[0]; \
      if ((_pvol > 80) && (_pvol <= 100) && \
          (vol > 100) && (vol < 120)) vol = 100; \
      _v.channel_count = srcvol.channel_count; \
      _v.volumes = calloc(srcvol.channel_count, sizeof(int)); \
      if (_v.volumes) { \
         unsigned int _i; \
         for (_i = 0; _i < _v.channel_count; _i++) _v.volumes[_i] = vol; \
         func(target, _v); \
         free(_v.volumes); \
      } \
   } while (0)


//////////////////////////////////////////////////////////////////////////////
static void
_cb_sink_port_change(void *data,
                     Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Emix_Port *port = data;
   Evas_Object *bxv = evas_object_data_get(obj, "parent");
   Emix_Sink *sink = evas_object_data_get(bxv, "sink");
   elm_object_text_set(obj, port->description);
   emix_sink_port_set(sink, port);
}

static void
_cb_sink_volume_change(void *data,
                       Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Sink *sink = evas_object_data_get(bxv, "sink");
   double vol = elm_slider_value_get(obj);
   VOLSET(vol, sink->volume, sink, emix_sink_volume_set);
   elm_slider_value_set(obj, vol);
}

static void
_cb_sink_volume_drag_stop(void *data,
                          Evas_Object *obj,
                          void *event EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Sink *sink = evas_object_data_get(bxv, "sink");
   int vol = sink->volume.volumes[0];
   elm_slider_value_set(obj, vol);
}

static void
_cb_sink_mute_change(void *data,
                     Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Sink *sink = evas_object_data_get(bxv, "sink");
   Evas_Object *sl = evas_object_data_get(bxv, "volume");
   Eina_Bool mute = elm_check_state_get(obj);
   elm_object_disabled_set(sl, mute);
   emix_sink_mute_set(sink, mute);
}

static void
_emix_sink_add(Emix_Sink *sink)
{
   Evas_Object *bxv, *bx, *lb, *ck, *sl, *hv, *sep;
   const Eina_List *l;
   Emix_Port *port;

   bxv = elm_box_add(win);
   sink_list = eina_list_append(sink_list, bxv);
   evas_object_data_set(bxv, "sink", sink);
   evas_object_size_hint_weight_set(bxv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bxv, EVAS_HINT_FILL, 0.0);

   bx = elm_box_add(win);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   lb = elm_label_add(win);
   elm_object_text_set(lb, sink->name);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   hv = elm_hoversel_add(win);
   evas_object_data_set(hv, "parent", bxv);
   evas_object_data_set(bxv, "port", hv);
   elm_hoversel_hover_parent_set(hv, win);
   EINA_LIST_FOREACH(sink->ports, l, port)
     {
        elm_hoversel_item_add(hv, port->description,
                              NULL, ELM_ICON_NONE,
                              _cb_sink_port_change, port);
        if (port->active) elm_object_text_set(hv, port->description);
     }
   evas_object_size_hint_weight_set(hv, 0.0, 0.5);
   evas_object_size_hint_align_set(hv, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx, hv);
   evas_object_show(hv);

   bx = elm_box_add(win);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   sl = elm_slider_add(win);
   evas_object_data_set(bxv, "volume", sl);
   elm_slider_min_max_set(sl, 0.0, EMIX_VOLUME_MAX + 50);
   elm_slider_span_size_set(sl, (EMIX_VOLUME_MAX + 50) * elm_config_scale_get());
   elm_slider_unit_format_set(sl, "%1.0f");
   elm_slider_indicator_format_set(sl, "%1.0f");
   evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, 0.5);
   elm_slider_value_set(sl, sink->volume.volumes[0]);
   elm_box_pack_end(bx, sl);
   evas_object_show(sl);
   evas_object_smart_callback_add(sl, "changed", _cb_sink_volume_change, bxv);
   evas_object_smart_callback_add(sl, "slider,drag,stop",
                                  _cb_sink_volume_drag_stop, bxv);

   ck = elm_check_add(win);
   evas_object_data_set(bxv, "mute", ck);
   elm_object_text_set(ck, "Mute");
   elm_check_state_set(ck, sink->mute);
   elm_object_disabled_set(sl, sink->mute);
   elm_box_pack_end(bx, ck);
   evas_object_show(ck);
   evas_object_smart_callback_add(ck, "changed", _cb_sink_mute_change, bxv);

   sep = elm_separator_add(win);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, sep);
   evas_object_show(sep);

   elm_box_pack_end(sink_box, bxv);
   evas_object_show(bxv);
}

static void
_emix_sink_del(Emix_Sink *sink)
{
   Eina_List *l;
   Evas_Object *bxv;
   EINA_LIST_FOREACH(sink_list, l, bxv)
     {
        if (evas_object_data_get(bxv, "sink") == sink)
          {
             sink_list = eina_list_remove_list(sink_list, l);
             evas_object_del(bxv);
             return;
          }
     }
}

static void
_emix_sink_change(Emix_Sink *sink)
{
   const Eina_List *l;
   Evas_Object *bxv, *hv, *ck, *sl;
   Emix_Port *port;

   EINA_LIST_FOREACH(sink_list, l, bxv)
     {
        if (evas_object_data_get(bxv, "sink") == sink) break;
     }
   if (!l) return;
   hv = evas_object_data_get(bxv, "port");
   elm_hoversel_clear(hv);
   EINA_LIST_FOREACH(sink->ports, l, port)
     {
        elm_hoversel_item_add(hv, port->description,
                              NULL, ELM_ICON_NONE,
                              _cb_sink_port_change, port);
        if (port->active) elm_object_text_set(hv, port->description);
     }
   sl = evas_object_data_get(bxv, "volume");
   elm_slider_value_set(sl, sink->volume.volumes[0]);

   ck = evas_object_data_get(bxv, "mute");
   elm_check_state_set(ck, sink->mute);
   elm_object_disabled_set(sl, sink->mute);
}

//////////////////////////////////////////////////////////////////////////////

static void
_cb_sink_input_port_change(void *data,
                           Evas_Object *obj,
                           void *event_info EINA_UNUSED)
{
   Emix_Sink *sink = data;
   Evas_Object *bxv = evas_object_data_get(obj, "parent");
   Emix_Sink_Input *input = evas_object_data_get(bxv, "input");
   elm_object_text_set(obj, sink->name);
   emix_sink_input_sink_change(input, sink);
}

static void
_cb_sink_input_volume_change(void *data,
                             Evas_Object *obj,
                             void *event_info EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Sink_Input *input = evas_object_data_get(bxv, "input");
   double vol = elm_slider_value_get(obj);
   VOLSET(vol, input->volume, input, emix_sink_input_volume_set);
   elm_slider_value_set(obj, vol);
}

static void
_cb_sink_input_volume_drag_stop(void *data,
                                Evas_Object *obj,
                                void *event EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Sink_Input *input = evas_object_data_get(bxv, "input");
   int vol = input->volume.volumes[0];
   elm_slider_value_set(obj, vol);
}

static void
_cb_sink_input_mute_change(void *data,
                           Evas_Object *obj,
                           void *event_info EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Sink_Input *input = evas_object_data_get(bxv, "input");
   Evas_Object *sl = evas_object_data_get(bxv, "volume");
   Eina_Bool mute = elm_check_state_get(obj);
   elm_object_disabled_set(sl, mute);
   emix_sink_input_mute_set(input, mute);
}

static void
_emix_sink_input_add(Emix_Sink_Input *input)
{
   Evas_Object *bxv, *bx, *lb, *ck, *sl, *hv, *sep;
   const Eina_List *l;
   Emix_Sink *sink;

   bxv = elm_box_add(win);
   sink_input_list = eina_list_append(sink_input_list, bxv);
   evas_object_data_set(bxv, "input", input);
   evas_object_size_hint_weight_set(bxv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bxv, EVAS_HINT_FILL, 0.0);

   bx = elm_box_add(win);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   lb = elm_label_add(win);
   elm_object_text_set(lb, input->name);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   hv = elm_hoversel_add(win);
   evas_object_data_set(hv, "parent", bxv);
   evas_object_data_set(bxv, "port", hv);
   elm_hoversel_hover_parent_set(hv, win);
   EINA_LIST_FOREACH(emix_sinks_get(), l, sink)
     {
        elm_hoversel_item_add(hv, sink->name,
                              NULL, ELM_ICON_NONE,
                              _cb_sink_input_port_change, sink);
        if (input->sink == sink) elm_object_text_set(hv, sink->name);
     }
   evas_object_size_hint_weight_set(hv, 0.0, 0.5);
   evas_object_size_hint_align_set(hv, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx, hv);
   evas_object_show(hv);

   bx = elm_box_add(win);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   sl = elm_slider_add(win);
   evas_object_data_set(bxv, "volume", sl);
   elm_slider_min_max_set(sl, 0.0, EMIX_VOLUME_MAX + 50);
   elm_slider_span_size_set(sl, (EMIX_VOLUME_MAX + 50) * elm_config_scale_get());
   elm_slider_unit_format_set(sl, "%1.0f");
   elm_slider_indicator_format_set(sl, "%1.0f");
   evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, 0.5);
   elm_slider_value_set(sl, input->volume.volumes[0]);
   elm_box_pack_end(bx, sl);
   evas_object_show(sl);
   evas_object_smart_callback_add(sl, "changed",
                                  _cb_sink_input_volume_change, bxv);
   evas_object_smart_callback_add(sl, "slider,drag,stop",
                                  _cb_sink_input_volume_drag_stop, bxv);

   ck = elm_check_add(win);
   evas_object_data_set(bxv, "mute", ck);
   elm_object_text_set(ck, "Mute");
   elm_check_state_set(ck, input->mute);
   elm_object_disabled_set(sl, input->mute);
   elm_box_pack_end(bx, ck);
   evas_object_show(ck);
   evas_object_smart_callback_add(ck, "changed",
                                  _cb_sink_input_mute_change, bxv);

   sep = elm_separator_add(win);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, sep);
   evas_object_show(sep);

   elm_box_pack_end(sink_input_box, bxv);
   evas_object_show(bxv);
}

static void
_emix_sink_input_del(Emix_Sink_Input *input)
{
   Eina_List *l;
   Evas_Object *bxv;
   EINA_LIST_FOREACH(sink_input_list, l, bxv)
     {
        if (evas_object_data_get(bxv, "input") == input)
          {
             sink_input_list = eina_list_remove_list(sink_input_list, l);
             evas_object_del(bxv);
             return;
          }
     }
}

static void
_emix_sink_input_change(Emix_Sink_Input *input)
{
   const Eina_List *l;
   Evas_Object *bxv, *hv, *ck, *sl;
   Emix_Sink *sink;

   EINA_LIST_FOREACH(sink_input_list, l, bxv)
     {
        if (evas_object_data_get(bxv, "input") == input) break;
     }
   if (!l) return;
   hv = evas_object_data_get(bxv, "port");
   elm_hoversel_clear(hv);
   EINA_LIST_FOREACH(emix_sinks_get(), l, sink)
     {
        elm_hoversel_item_add(hv, sink->name,
                              NULL, ELM_ICON_NONE,
                              _cb_sink_input_port_change, sink);
        if (input->sink == sink) elm_object_text_set(hv, sink->name);
     }
   sl = evas_object_data_get(bxv, "volume");
   elm_slider_value_set(sl, input->volume.volumes[0]);

   ck = evas_object_data_get(bxv, "mute");
   elm_check_state_set(ck, input->mute);
   elm_object_disabled_set(sl, input->mute);
}

//////////////////////////////////////////////////////////////////////////////

static void
_cb_source_volume_change(void *data,
                         Evas_Object *obj,
                         void *event_info EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Source *source = evas_object_data_get(bxv, "source");
   double vol = elm_slider_value_get(obj);
   VOLSET(vol, source->volume, source, emix_source_volume_set);
   elm_slider_value_set(obj, vol);
}

static void
_cb_source_volume_drag_stop(void *data,
                            Evas_Object *obj,
                            void *event EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Source *source = evas_object_data_get(bxv, "source");
   int vol = source->volume.volumes[0];
   elm_slider_value_set(obj, vol);
}

static void
_cb_source_mute_change(void *data,
                       Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   Evas_Object *bxv = data;
   Emix_Source *source = evas_object_data_get(bxv, "source");
   Evas_Object *sl = evas_object_data_get(bxv, "volume");
   Eina_Bool mute = elm_check_state_get(obj);
   elm_object_disabled_set(sl, mute);
   emix_source_mute_set(source, mute);
}

static void
_emix_source_add(Emix_Source *source)
{
   Evas_Object *bxv, *bx, *lb, *ck, *sl, *sep;

   bxv = elm_box_add(win);
   source_list = eina_list_append(source_list, bxv);
   evas_object_data_set(bxv, "source", source);
   evas_object_size_hint_weight_set(bxv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bxv, EVAS_HINT_FILL, 0.0);

   bx = elm_box_add(win);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   lb = elm_label_add(win);
   elm_object_text_set(lb, source->name);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   bx = elm_box_add(win);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   sl = elm_slider_add(win);
   evas_object_data_set(bxv, "volume", sl);
   elm_slider_min_max_set(sl, 0.0, EMIX_VOLUME_MAX + 50);
   elm_slider_span_size_set(sl, (EMIX_VOLUME_MAX + 50) * elm_config_scale_get());
   elm_slider_unit_format_set(sl, "%1.0f");
   elm_slider_indicator_format_set(sl, "%1.0f");
   evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, 0.5);
   elm_slider_value_set(sl, source->volume.volumes[0]);
   elm_box_pack_end(bx, sl);
   evas_object_show(sl);
   evas_object_smart_callback_add(sl, "changed",
                                  _cb_source_volume_change, bxv);
   evas_object_smart_callback_add(sl, "slider,drag,stop",
                                  _cb_source_volume_drag_stop, bxv);

   ck = elm_check_add(win);
   evas_object_data_set(bxv, "mute", ck);
   elm_object_text_set(ck, "Mute");
   elm_check_state_set(ck, source->mute);
   elm_object_disabled_set(sl, source->mute);
   elm_box_pack_end(bx, ck);
   evas_object_show(ck);
   evas_object_smart_callback_add(ck, "changed",
                                  _cb_source_mute_change, bxv);

   sep = elm_separator_add(win);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, sep);
   evas_object_show(sep);

   elm_box_pack_end(source_box, bxv);
   evas_object_show(bxv);
}

static void
_emix_source_del(Emix_Source *source)
{
   Eina_List *l;
   Evas_Object *bxv;
   EINA_LIST_FOREACH(source_list, l, bxv)
     {
        if (evas_object_data_get(bxv, "source") == source)
          {
             source_list = eina_list_remove_list(source_list, l);
             evas_object_del(bxv);
             return;
          }
     }
}

static void
_emix_source_change(Emix_Source *source)
{
   const Eina_List *l;
   Evas_Object *bxv, *ck, *sl;

   EINA_LIST_FOREACH(source_list, l, bxv)
     {
        if (evas_object_data_get(bxv, "source") == source) break;
     }
   if (!l) return;
   sl = evas_object_data_get(bxv, "volume");
   elm_slider_value_set(sl, source->volume.volumes[0]);

   ck = evas_object_data_get(bxv, "mute");
   elm_check_state_set(ck, source->mute);
   elm_object_disabled_set(sl, source->mute);
}

//////////////////////////////////////////////////////////////////////////////

static void
_cb_emix_event(void *data EINA_UNUSED, enum Emix_Event event, void *event_info)
{
   switch (event)
     {
      case EMIX_READY_EVENT:
        break;
      case EMIX_DISCONNECTED_EVENT:
        elm_exit();
        break;
      case EMIX_SINK_ADDED_EVENT:
        _emix_sink_add(event_info);
        break;
      case EMIX_SINK_REMOVED_EVENT:
        _emix_sink_del(event_info);
        break;
      case EMIX_SINK_CHANGED_EVENT:
        _emix_sink_change(event_info);
        break;
      case EMIX_SINK_INPUT_ADDED_EVENT:
        _emix_sink_input_add(event_info);
        break;
      case EMIX_SINK_INPUT_REMOVED_EVENT:
        _emix_sink_input_del(event_info);
        break;
      case EMIX_SINK_INPUT_CHANGED_EVENT:
        _emix_sink_input_change(event_info);
        break;
      case EMIX_SOURCE_ADDED_EVENT:
        _emix_source_add(event_info);
        break;
      case EMIX_SOURCE_REMOVED_EVENT:
        _emix_source_del(event_info);
        break;
      case EMIX_SOURCE_CHANGED_EVENT:
        _emix_source_change(event_info);
        break;
      default:
        break;
     }
}

//////////////////////////////////////////////////////////////////////////////

static void
_cb_playback(void *data EINA_UNUSED,
             Evas_Object *obj EINA_UNUSED,
             void *event_info EINA_UNUSED)
{
   evas_object_hide(source_scroller);
   evas_object_show(sink_input_scroller);
   evas_object_hide(sink_scroller);
}

static void
_cb_outputs(void *data EINA_UNUSED,
            Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   evas_object_hide(source_scroller);
   evas_object_hide(sink_input_scroller);
   evas_object_show(sink_scroller);
}

static void
_cb_inputs(void *data EINA_UNUSED,
           Evas_Object *obj EINA_UNUSED,
           void *event_info EINA_UNUSED)
{
   evas_object_show(source_scroller);
   evas_object_hide(sink_input_scroller);
   evas_object_hide(sink_scroller);
}

//////////////////////////////////////////////////////////////////////////////

static void
_event_init(void)
{
   emix_event_callback_add(_cb_emix_event, NULL);
}

static void
_fill_source(void)
{
   const Eina_List *l;
   Emix_Source *source;

   EINA_LIST_FOREACH(emix_sources_get(), l, source)
     {
        _emix_source_add(source);
     }
}

static void
_fill_sink_input(void)
{
   const Eina_List *l;
   Emix_Sink_Input *input;

   EINA_LIST_FOREACH(emix_sink_inputs_get(), l, input)
     {
        _emix_sink_input_add(input);
     }
}

static void
_fill_sink(void)
{
   const Eina_List *l;
   Emix_Sink *sink;

   EINA_LIST_FOREACH(emix_sinks_get(), l, sink)
     {
        _emix_sink_add(sink);
     }
}

//////////////////////////////////////////////////////////////////////////////

EAPI_MAIN int
elm_main(int argc, char **argv)
{
   Evas_Object *tb, *tbar, *sc, *rect, *bx;
   const char *back = NULL;

   emix_init();
   if (argc > 1) back = argv[1];
   if (!_backend_init(back)) goto done;
   _event_init();

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   win = elm_win_util_standard_add("emix", "Mixer");
   elm_win_autodel_set(win, EINA_TRUE);

/*
   icon = evas_object_image_add(evas_object_evas_get(mw->win));
   snprintf(buf, sizeof(buf), "%s/icons/emixer.png",
            elm_app_data_dir_get());
   evas_object_image_file_set(icon, buf, NULL);
   elm_win_icon_object_set(mw->win, icon);
   elm_win_icon_name_set(mw->win, "emixer");
 */

   tb = elm_table_add(win);
   evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, tb);
   evas_object_show(tb);

   tbar = elm_toolbar_add(win);
   elm_toolbar_select_mode_set(tbar, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_toolbar_homogeneous_set(tbar, EINA_TRUE);
   evas_object_size_hint_weight_set(tbar, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tbar, EVAS_HINT_FILL, EVAS_HINT_FILL);

   elm_toolbar_item_append(tbar, NULL, "Playback", _cb_playback, NULL);
   elm_toolbar_item_append(tbar, NULL, "Outputs", _cb_outputs, NULL);
   elm_toolbar_item_append(tbar, NULL, "Inputs", _cb_inputs, NULL);

   elm_table_pack(tb, tbar, 0, 0, 1, 1);
   evas_object_show(tbar);

   sc = elm_scroller_add(win);
   source_scroller = sc;
   evas_object_size_hint_weight_set(sc, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sc, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, sc, 0, 1, 1, 1);

   sc = elm_scroller_add(win);
   sink_input_scroller = sc;
   evas_object_size_hint_weight_set(sc, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sc, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, sc, 0, 1, 1, 1);
   evas_object_show(sc);

   sc = elm_scroller_add(win);
   sink_scroller = sc;
   evas_object_size_hint_weight_set(sc, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sc, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, sc, 0, 1, 1, 1);

   bx = elm_box_add(win);
   source_box = bx;
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(source_scroller, bx);
   evas_object_show(bx);

   bx = elm_box_add(win);
   sink_input_box = bx;
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(sink_input_scroller, bx);
   evas_object_show(bx);

   bx = elm_box_add(win);
   sink_box = bx;
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(sink_scroller, bx);
   evas_object_show(bx);

   rect = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_weight_set(rect, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(rect, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(rect,
                                 440 * elm_config_scale_get(),
                                 220 * elm_config_scale_get());
   elm_table_pack(tb, rect, 0, 1, 1, 1);

   _fill_source();
   _fill_sink_input();
   _fill_sink();
   evas_object_show(win);

   elm_run();
done:
   emix_shutdown();
   return 0;
}
ELM_MAIN()
