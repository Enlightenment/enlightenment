#include <Elementary.h>
#include "emix.h"

#   ifdef HAVE_GETTEXT
#include <libintl.h>
#define _(str) gettext(str)
#define d_(str, dom) dgettext(PACKAGE dom, str)
#define P_(str, str_p, n) ngettext(str, str_p, n)
#define dP_(str, str_p, n, dom) dngettext(PACKAGE dom, str, str_p, n)
#   else
#define _(str) (str)
#define d_(str, dom) (str)
#define P_(str, str_p, n) (str_p)
#define dP_(str, str_p, n, dom) (str_p)
#   endif

Evas_Object *win;
Evas_Object *source_scroller, *sink_input_scroller, *sink_scroller, *card_scroller;
Evas_Object *source_box, *sink_input_box, *sink_box, *card_box;

Eina_List *source_list = NULL, *sink_input_list = NULL, *sink_list = NULL, *card_list = NULL;

//////////////////////////////////////////////////////////////////////////////

static void _emix_sink_volume_fill(Emix_Sink *sink, Evas_Object *bxv, Evas_Object *bx, Eina_Bool locked);

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

static void
_cb_sink_port_change(void *data,
                     Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Emix_Port *port = data;
   Evas_Object *fr = evas_object_data_get(obj, "parent");
   Emix_Sink *sink = evas_object_data_get(fr, "sink");
   elm_object_text_set(obj, port->description);
   emix_sink_port_set(sink, port);
}

static void
_cb_sink_volume_change(void *data,
                       Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink *sink = evas_object_data_get(fr, "sink");
   double vol = elm_slider_value_get(obj);
   VOLSET(vol, sink->volume, sink, emix_sink_volume_set);
   elm_slider_value_set(obj, vol);
}

static void
_cb_sink_volume_channel_change(void *data,
                       Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink *sink = evas_object_data_get(fr, "sink");
   double vol = elm_slider_value_get(obj);
   sink->volume.volumes[(uintptr_t)evas_object_data_get(obj, "channel")] = vol;
   elm_slider_value_set(obj, vol);
   emix_sink_volume_set(sink, &sink->volume);
}


static void
_cb_sink_mute_change(void *data,
                     Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink *sink = evas_object_data_get(fr, "sink");
   Evas_Object *lock = evas_object_data_get(fr, "lock");
   Eina_Bool mute = elm_check_state_get(obj);
   Eina_List *l;
   Evas_Object *o;

   if (lock && !elm_check_state_get(lock))
     {
        EINA_LIST_FOREACH(evas_object_data_get(fr, "volumes"), l, o)
          {
             elm_object_disabled_set(o, mute);
             o = evas_object_data_get(o, "lb");
             elm_object_disabled_set(o, mute);
          }
     }
   else
     {
        o = evas_object_data_get(fr, "volume");
        elm_object_disabled_set(o, mute);
     }
   emix_sink_mute_set(sink, mute);
}

static void
_cb_sink_lock_change(void *data,
                     Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink *sink = evas_object_data_get(fr, "sink");
   Evas_Object *bx = evas_object_data_get(fr, "volume_bx");
   Eina_Bool lock = elm_check_state_get(obj);
   if (lock)
     VOLSET(sink->volume.volumes[0], sink->volume,
            sink, emix_sink_volume_set);
   _emix_sink_volume_fill(sink, fr, bx, lock);
}

static void
_emix_sink_volume_fill(Emix_Sink *sink, Evas_Object *fr, Evas_Object *bx, Eina_Bool locked)
{
   Evas_Object *bxhv, *sl, *ck, *lb;
   Eina_List *sls = NULL;
   unsigned int i;

   eina_list_free(evas_object_data_get(fr, "volumes"));
   elm_box_clear(bx);

   bxhv = elm_box_add(bx);
   evas_object_size_hint_weight_set(bxhv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bxhv, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx, bxhv);
   evas_object_show(bxhv);

   if (locked)
     {
        sl = elm_slider_add(bx);
        evas_object_data_set(fr, "volume", sl);
        elm_slider_min_max_set(sl, 0.0, emix_max_volume_get());
        elm_slider_span_size_set(sl, emix_max_volume_get() * elm_config_scale_get());
        elm_slider_unit_format_set(sl, "%1.0f");
        elm_slider_indicator_format_set(sl, "%1.0f");
        evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_slider_value_set(sl, sink->volume.volumes[0]);
        elm_box_pack_end(bxhv, sl);
        evas_object_show(sl);
        evas_object_smart_callback_add(sl, "changed", _cb_sink_volume_change, fr);
        elm_object_disabled_set(sl, sink->mute);
     }
   else
     {
        for (i = 0; i < sink->volume.channel_count; ++i)
          {
             lb = elm_label_add(bx);
             if (!sink->volume.channel_names)
               {
                  char buf[1024];
                  snprintf(buf, sizeof(buf), "Channel %d", i);
                  elm_object_text_set(lb, buf);
               }
             else
               elm_object_text_set(lb, sink->volume.channel_names[i]);
             evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.5);
             evas_object_size_hint_align_set(lb, 0.0, 0.5);
             elm_box_pack_end(bxhv, lb);
             elm_object_disabled_set(lb, sink->mute);
             evas_object_show(lb);

             sl = elm_slider_add(bx);
             evas_object_data_set(sl, "lb", lb);
             evas_object_data_set(sl, "channel", (uintptr_t *)(uintptr_t)i);
             elm_slider_min_max_set(sl, 0.0, emix_max_volume_get());
             elm_slider_span_size_set(sl, (emix_max_volume_get()) * elm_config_scale_get());
             elm_slider_unit_format_set(sl, "%1.0f");
             elm_slider_indicator_format_set(sl, "%1.0f");
             evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, EVAS_HINT_FILL);
             elm_slider_value_set(sl, sink->volume.volumes[i]);
             elm_box_pack_end(bxhv, sl);
             evas_object_show(sl);
             evas_object_smart_callback_add(sl, "changed", _cb_sink_volume_channel_change, fr);
             elm_object_disabled_set(sl, sink->mute);
             sls = eina_list_append(sls, sl);
          }
     }
   evas_object_data_set(fr, "volumes", sls);

   bxhv = elm_box_add(bx);
   elm_box_pack_end(bx, bxhv);
   evas_object_show(bxhv);

   ck = elm_check_add(bx);
   evas_object_data_set(fr, "mute", ck);
   elm_object_text_set(ck, "Mute");
   elm_check_state_set(ck, sink->mute);
   elm_box_pack_end(bxhv, ck);
   evas_object_show(ck);
   evas_object_smart_callback_add(ck, "changed", _cb_sink_mute_change, fr);

   if (sink->volume.channel_count > 1)
     {
        ck = elm_check_add(bx);
        elm_object_text_set(ck, "Lock");
        evas_object_data_set(fr, "lock", ck);
        elm_check_state_set(ck, locked);
        elm_box_pack_end(bxhv, ck);
        evas_object_show(ck);
        evas_object_smart_callback_add(ck, "changed", _cb_sink_lock_change, fr);
     }
}



static void
_emix_sink_add(Emix_Sink *sink)
{
   Evas_Object *bxv, *bx, *lb, *hv, *sep, *fr;
   const Eina_List *l;
   Emix_Port *port;
   Eina_Bool locked = EINA_TRUE;
   unsigned int i;

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(fr, EVAS_HINT_FILL, 0.0);
   elm_object_style_set(fr, "pad_medium");
   sink_list = eina_list_append(sink_list, fr);
   evas_object_data_set(fr, "sink", sink);

   bxv = elm_box_add(win);
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
   evas_object_data_set(hv, "parent", fr);
   evas_object_data_set(fr, "port", hv);
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

   /* Compare each volume level and check if they differ. If they differ unlock
   the volume control and let user set each channel volume level */
   for (i = 1; i < sink->volume.channel_count; ++i)
     {
        if (sink->volume.volumes[i - 1] != sink->volume.volumes[i])
          {
             locked = EINA_FALSE;
             break;
          }
     }

   bx = elm_box_add(bxv);
   evas_object_data_set(fr, "volume_bx", bx);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);


   _emix_sink_volume_fill(sink, fr, bx, locked);

   elm_object_content_set(fr, bxv);
   evas_object_show(bxv);

   elm_box_pack_end(sink_box, fr);
   evas_object_show(fr);

   sep = elm_separator_add(win);
   evas_object_data_set(fr, "extra", sep);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(sink_box, sep);
   evas_object_show(sep);
}

static void
_emix_sink_del(Emix_Sink *sink)
{
   Eina_List *l;
   Evas_Object *fr;
   EINA_LIST_FOREACH(sink_list, l, fr)
     {
        if (evas_object_data_get(fr, "sink") == sink)
          {
             sink_list = eina_list_remove_list(sink_list, l);
             evas_object_del(evas_object_data_get(fr, "extra"));
             evas_object_del(fr);
             return;
          }
     }
}

static void
_emix_sink_change(Emix_Sink *sink)
{
   const Eina_List *l;
   Eina_List *ll;
   Evas_Object *fr, *hv, *ck, *sl, *lb;
   Emix_Port *port;

   EINA_LIST_FOREACH(sink_list, l, fr)
     {
        if (evas_object_data_get(fr, "sink") == sink) break;
     }
   if (!l) return;
   hv = evas_object_data_get(fr, "port");
   elm_hoversel_clear(hv);
   EINA_LIST_FOREACH(sink->ports, l, port)
     {
        elm_hoversel_item_add(hv, port->description,
                              NULL, ELM_ICON_NONE,
                              _cb_sink_port_change, port);
        if (port->active) elm_object_text_set(hv, port->description);
     }
   ck = evas_object_data_get(fr, "lock");

   if (ck && !elm_check_state_get(ck))
     {
        ck = evas_object_data_get(fr, "mute");
        elm_check_state_set(ck, sink->mute);
        EINA_LIST_FOREACH(evas_object_data_get(fr, "volumes"), ll, sl)
          {
             elm_slider_value_set(sl,
                                  sink->volume.volumes[(uintptr_t)evas_object_data_get(sl, "channel")]);
             elm_object_disabled_set(sl, sink->mute);
             lb = evas_object_data_get(sl, "lb");
             elm_object_disabled_set(lb, sink->mute);
          }
     }
   else
     {
        ck = evas_object_data_get(fr, "mute");
        elm_check_state_set(ck, sink->mute);

        sl = evas_object_data_get(fr, "volume");
        elm_slider_value_set(sl, sink->volume.volumes[0]);
        elm_object_disabled_set(sl, sink->mute);
     }

}

//////////////////////////////////////////////////////////////////////////////
static void _emix_sink_input_volume_fill(Emix_Sink_Input *input, Evas_Object *fr, Evas_Object *bx, Eina_Bool locked);

static void
_cb_sink_input_port_change(void *data,
                           Evas_Object *obj,
                           void *event_info EINA_UNUSED)
{
   Emix_Sink *sink = data;
   Evas_Object *fr = evas_object_data_get(obj, "parent");
   Emix_Sink_Input *input = evas_object_data_get(fr, "input");
   elm_object_text_set(obj, sink->name);
   emix_sink_input_sink_change(input, sink);
}

static void
_cb_sink_input_volume_change(void *data,
                             Evas_Object *obj,
                             void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink_Input *input = evas_object_data_get(fr, "input");
   double vol = elm_slider_value_get(obj);
   VOLSET(vol, input->volume, input, emix_sink_input_volume_set);
   elm_slider_value_set(obj, vol);
}

static void
_cb_sink_input_volume_channel_change(void *data,
                                     Evas_Object *obj,
                                     void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink_Input *input = evas_object_data_get(fr, "input");
   double vol = elm_slider_value_get(obj);
   input->volume.volumes[(uintptr_t)evas_object_data_get(obj, "channel")] = vol;
   elm_slider_value_set(obj, vol);
   emix_sink_input_volume_set(input, &input->volume);
}

static void
_cb_sink_input_volume_drag_stop(void *data,
                                Evas_Object *obj,
                                void *event EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink_Input *input = evas_object_data_get(fr, "input");
   int vol = input->volume.volumes[0];
   elm_slider_value_set(obj, vol);
}

static void
_cb_sink_input_volume_channel_drag_stop(void *data,
                                        Evas_Object *obj,
                                        void *event EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink_Input *input = evas_object_data_get(fr, "input");
   int vol = input->volume.volumes[(uintptr_t)evas_object_data_get(obj, "channel")];
   elm_slider_value_set(obj, vol);
}

static void
_cb_sink_input_mute_change(void *data,
                           Evas_Object *obj,
                           void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink_Input *input = evas_object_data_get(fr, "input");
   Evas_Object *sl;
   Evas_Object *lock = evas_object_data_get(fr, "lock");
   Eina_Bool mute = elm_check_state_get(obj);
   Evas_Object *lb;
   Eina_List *l;

   if (lock && !elm_check_state_get(lock))
     {
        EINA_LIST_FOREACH(evas_object_data_get(fr, "volumes"), l, sl)
          {
             elm_object_disabled_set(sl, mute);
             lb = evas_object_data_get(sl, "lb");
             elm_object_disabled_set(lb, mute);
          }
     }
   else
     {
        sl = evas_object_data_get(fr, "volume");
        elm_object_disabled_set(sl, mute);
     }
   emix_sink_input_mute_set(input, mute);
}

static void
_cb_sink_input_lock_change(void *data,
                           Evas_Object *obj,
                           void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Sink_Input *input = evas_object_data_get(fr, "input");
   Evas_Object *bx = evas_object_data_get(fr, "volume_bx");
   Eina_Bool lock = elm_check_state_get(obj);
   if (lock)
     VOLSET(input->volume.volumes[0], input->volume,
            input, emix_sink_input_volume_set);
   _emix_sink_input_volume_fill(input, fr, bx, lock);
}

static void
_emix_sink_input_volume_fill(Emix_Sink_Input *input, Evas_Object *fr, Evas_Object *bx, Eina_Bool locked)
{
   Evas_Object *bxhv, *lb, *sl = NULL, *ck;
   unsigned int i;
   Eina_List *sls = NULL, *l;

   eina_list_free(evas_object_data_get(fr, "volumes"));
   elm_box_clear(bx);

   bxhv = elm_box_add(bx);
   evas_object_size_hint_weight_set(bxhv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bxhv, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx, bxhv);
   evas_object_show(bxhv);

   if (locked)
     {
        sl = elm_slider_add(bx);
        evas_object_data_set(fr, "volume", sl);
        elm_slider_min_max_set(sl, 0.0, emix_max_volume_get());
        elm_slider_span_size_set(sl, (emix_max_volume_get()) * elm_config_scale_get());
        elm_slider_unit_format_set(sl, "%1.0f");
        elm_slider_indicator_format_set(sl, "%1.0f");
        evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, 0.5);
        elm_slider_value_set(sl, input->volume.volumes[0]);
        elm_box_pack_end(bxhv, sl);
        evas_object_show(sl);
        evas_object_smart_callback_add(sl, "changed",
                                       _cb_sink_input_volume_change, fr);
        evas_object_smart_callback_add(sl, "slider,drag,stop",
                                       _cb_sink_input_volume_drag_stop, fr);

     }
   else
     {
        for (i = 0; i < input->volume.channel_count; ++i)
          {
             lb = elm_label_add(bx);
             if (!input->volume.channel_names)
               {
                  char buf[1024];
                  snprintf(buf, sizeof(buf), "Channel %d", i);
                  elm_object_text_set(lb, buf);
               }
             else
               elm_object_text_set(lb, input->volume.channel_names[i]);
             evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.5);
             evas_object_size_hint_align_set(lb, 0.0, 0.5);
             elm_box_pack_end(bxhv, lb);
             elm_object_disabled_set(lb, input->mute);
             evas_object_show(lb);

             sl = elm_slider_add(bx);
             evas_object_data_set(sl, "lb", lb);
             evas_object_data_set(sl, "channel", (uintptr_t *)(uintptr_t)i);
             elm_slider_min_max_set(sl, 0.0, emix_max_volume_get());
             elm_slider_span_size_set(sl, (emix_max_volume_get()) * elm_config_scale_get());
             elm_slider_unit_format_set(sl, "%1.0f");
             elm_slider_indicator_format_set(sl, "%1.0f");
             evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, EVAS_HINT_FILL);
             elm_slider_value_set(sl, input->volume.volumes[i]);
             elm_box_pack_end(bxhv, sl);
             evas_object_show(sl);
             evas_object_smart_callback_add(sl, "changed",
                                            _cb_sink_input_volume_channel_change, fr);
             evas_object_smart_callback_add(sl, "slider,drag,stop",
                                            _cb_sink_input_volume_channel_drag_stop, fr);
             elm_object_disabled_set(sl, input->mute);
             sls = eina_list_append(sls, sl);
          }
        sl = NULL;
     }
   evas_object_data_set(fr, "volumes", sls);

   bxhv = elm_box_add(bx);
   elm_box_pack_end(bx, bxhv);
   evas_object_show(bxhv);

   ck = elm_check_add(bx);
   evas_object_data_set(fr, "mute", ck);
   elm_object_text_set(ck, "Mute");
   elm_check_state_set(ck, input->mute);
   if (sl) elm_object_disabled_set(sl, input->mute);
   else if (sls)
     {
        EINA_LIST_FOREACH(sls, l, sl)
          {
             elm_object_disabled_set(sl, input->mute);
          }
     }
   elm_box_pack_end(bxhv, ck);
   evas_object_show(ck);
   evas_object_smart_callback_add(ck, "changed",
                                  _cb_sink_input_mute_change, fr);

   if (input->volume.channel_count > 1)
     {
        ck = elm_check_add(bx);
        evas_object_data_set(fr, "lock", ck);
        elm_object_text_set(ck, "Lock");
        elm_check_state_set(ck, locked);
        elm_box_pack_end(bxhv, ck);
        evas_object_show(ck);
        evas_object_smart_callback_add(ck, "changed",
                                       _cb_sink_input_lock_change, fr);
     }
}


static void
_emix_sink_input_add(Emix_Sink_Input *input)
{
   Evas_Object *bxv, *bx, *lb, *hv, *sep, *ic, *fr;
   const Eina_List *l;
   Emix_Sink *sink;
   Eina_Bool locked = EINA_TRUE;
   unsigned int i;

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(fr, EVAS_HINT_FILL, 0.0);
   elm_object_style_set(fr, "pad_medium");
   sink_input_list = eina_list_append(sink_input_list, fr);
   evas_object_data_set(fr, "input", input);

   bxv = elm_box_add(win);
   evas_object_size_hint_weight_set(bxv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bxv, EVAS_HINT_FILL, 0.0);

   bx = elm_box_add(win);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   ic = elm_icon_add(win);
   elm_icon_standard_set(ic, input->icon);
   evas_object_size_hint_weight_set(ic, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ic, 0.0, EVAS_HINT_FILL);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_box_pack_end(bx, ic);
   evas_object_show(ic);

   lb = elm_label_add(win);
   elm_object_text_set(lb, input->name);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   hv = elm_hoversel_add(win);
   evas_object_data_set(hv, "parent", fr);
   evas_object_data_set(fr, "port", hv);
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
   evas_object_data_set(fr, "volume_bx", bx);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   /* Compare each volume level and check if they differ. If they differ unlock
   the volume control and let user set each channel volume level */
   for (i = 1; i < input->volume.channel_count; ++i)
     {
        if (input->volume.volumes[i - 1] != input->volume.volumes[i])
          {
             locked = EINA_FALSE;
             break;
          }
     }

   _emix_sink_input_volume_fill(input, fr, bx, locked);

   elm_object_content_set(fr, bxv);
   evas_object_show(bxv);

   elm_box_pack_end(sink_input_box, fr);
   evas_object_show(fr);

   sep = elm_separator_add(win);
   evas_object_data_set(fr, "extra", sep);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(sink_input_box, sep);
   evas_object_show(sep);
}

static void
_emix_sink_input_del(Emix_Sink_Input *input)
{
   Eina_List *l;
   Evas_Object *fr;
   EINA_LIST_FOREACH(sink_input_list, l, fr)
     {
        if (evas_object_data_get(fr, "input") == input)
          {
             sink_input_list = eina_list_remove_list(sink_input_list, l);
             evas_object_del(evas_object_data_get(fr, "extra"));
             evas_object_del(fr);
             return;
          }
     }
}

static void
_emix_sink_input_change(Emix_Sink_Input *input)
{
   const Eina_List *l;
   Eina_List *ll;
   Evas_Object *fr, *hv, *ck, *sl, *lb;
   Emix_Sink *sink;

   EINA_LIST_FOREACH(sink_input_list, l, fr)
     {
        if (evas_object_data_get(fr, "input") == input) break;
     }
   if (!l) return;
   hv = evas_object_data_get(fr, "port");
   elm_hoversel_clear(hv);
   EINA_LIST_FOREACH(emix_sinks_get(), l, sink)
     {
        elm_hoversel_item_add(hv, sink->name,
                              NULL, ELM_ICON_NONE,
                              _cb_sink_input_port_change, sink);
        if (input->sink == sink) elm_object_text_set(hv, sink->name);
     }

   ck = evas_object_data_get(fr, "lock");

   if (ck && !elm_check_state_get(ck))
     {
        ck = evas_object_data_get(fr, "mute");
        elm_check_state_set(ck, input->mute);
        EINA_LIST_FOREACH(evas_object_data_get(fr, "volumes"), ll, sl)
          {
             elm_slider_value_set(sl,
                                  input->volume.volumes[(uintptr_t)evas_object_data_get(sl, "channel")]);
             elm_object_disabled_set(sl, input->mute);
             lb = evas_object_data_get(sl, "lb");
             elm_object_disabled_set(lb, input->mute);
          }
     }
   else
     {
        ck = evas_object_data_get(fr, "mute");
        elm_check_state_set(ck, input->mute);

        sl = evas_object_data_get(fr, "volume");
        elm_slider_value_set(sl, input->volume.volumes[0]);
        elm_object_disabled_set(sl, input->mute);
     }
}

//////////////////////////////////////////////////////////////////////////////
static void _emix_source_volume_fill(Emix_Source *source, Evas_Object *fr, Evas_Object *bx, Eina_Bool locked);

static void
_cb_source_volume_change(void *data,
                         Evas_Object *obj,
                         void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Source *source = evas_object_data_get(fr, "source");
   double vol = elm_slider_value_get(obj);
   VOLSET(vol, source->volume, source, emix_source_volume_set);
   elm_slider_value_set(obj, vol);
}

static void
_cb_source_volume_channel_change(void *data,
                         Evas_Object *obj,
                         void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Source *source = evas_object_data_get(fr, "source");
   double vol = elm_slider_value_get(obj);
   source->volume.volumes[(uintptr_t)evas_object_data_get(obj, "channel")] = vol;
   elm_slider_value_set(obj, vol);
   emix_source_volume_set(source, &source->volume);
}


static void
_cb_source_volume_drag_stop(void *data,
                            Evas_Object *obj,
                            void *event EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Source *source = evas_object_data_get(fr, "source");
   int vol = source->volume.volumes[0];
   elm_slider_value_set(obj, vol);
}

static void
_cb_source_volume_channel_drag_stop(void *data,
                            Evas_Object *obj,
                            void *event EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Source *source = evas_object_data_get(fr, "source");
   int vol = source->volume.volumes[(uintptr_t)evas_object_data_get(obj, "channel")];
   elm_slider_value_set(obj, vol);
}

static void
_cb_source_mute_change(void *data,
                       Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Source *source = evas_object_data_get(fr, "source");
   Evas_Object *sl;
   Evas_Object *lock = evas_object_data_get(fr, "lock");
   Eina_Bool mute = elm_check_state_get(obj);
   Evas_Object *lb;
   Eina_List *l;

   if (lock && !elm_check_state_get(lock))
     {
        EINA_LIST_FOREACH(evas_object_data_get(fr, "volumes"), l, sl)
          {
             elm_object_disabled_set(sl, mute);
             lb = evas_object_data_get(sl, "lb");
             elm_object_disabled_set(lb, mute);
          }
     }
   else
     {
        sl = evas_object_data_get(fr, "volume");
        elm_object_disabled_set(sl, mute);
     }
   emix_source_mute_set(source, mute);
}

static void
_cb_source_lock_change(void *data,
                       Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   Evas_Object *fr = data;
   Emix_Source *source = evas_object_data_get(fr, "source");
   Evas_Object *bx = evas_object_data_get(fr, "volume_bx");
   Eina_Bool lock = elm_check_state_get(obj);
   if (lock)
     VOLSET(source->volume.volumes[0], source->volume,
            source, emix_source_volume_set);
   _emix_source_volume_fill(source, fr, bx, lock);
}

static void
_emix_source_volume_fill(Emix_Source *source, Evas_Object *fr, Evas_Object *bx, Eina_Bool locked)
{
   Evas_Object *bxhv, *lb, *sl, *ck;
   Eina_List *sls = NULL;
   unsigned int i;

   eina_list_free(evas_object_data_get(fr, "volumes"));
   elm_box_clear(bx);

   bxhv = elm_box_add(bx);
   evas_object_size_hint_weight_set(bxhv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bxhv, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx, bxhv);
   evas_object_show(bxhv);

   if (locked)
     {
        sl = elm_slider_add(bx);
        evas_object_data_set(fr, "volume", sl);
        elm_slider_min_max_set(sl, 0.0, emix_max_volume_get());
        elm_slider_span_size_set(sl, (emix_max_volume_get()) * elm_config_scale_get());
        elm_slider_unit_format_set(sl, "%1.0f");
        elm_slider_indicator_format_set(sl, "%1.0f");
        evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_slider_value_set(sl, source->volume.volumes[0]);
        elm_box_pack_end(bxhv, sl);
        evas_object_show(sl);
        evas_object_smart_callback_add(sl, "changed",
                                       _cb_source_volume_change, fr);
        evas_object_smart_callback_add(sl, "slider,drag,stop",
                                       _cb_source_volume_drag_stop, fr);
        elm_object_disabled_set(sl, source->mute);
     }
   else
     {
        for (i = 0; i < source->volume.channel_count; ++i)
          {
             lb = elm_label_add(bx);
             if (!source->volume.channel_names)
               {
                  char buf[1024];
                  snprintf(buf, sizeof(buf), "Channel %d", i);
                  elm_object_text_set(lb, buf);
               }
             else
               elm_object_text_set(lb, source->volume.channel_names[i]);
             evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.5);
             evas_object_size_hint_align_set(lb, 0.0, 0.5);
             elm_box_pack_end(bxhv, lb);
             elm_object_disabled_set(lb, source->mute);
             evas_object_show(lb);

             sl = elm_slider_add(bx);
             evas_object_data_set(sl, "lb", lb);
             evas_object_data_set(sl, "channel", (uintptr_t *)(uintptr_t)i);
             elm_slider_min_max_set(sl, 0.0, emix_max_volume_get());
             elm_slider_span_size_set(sl, (emix_max_volume_get()) * elm_config_scale_get());
             elm_slider_unit_format_set(sl, "%1.0f");
             elm_slider_indicator_format_set(sl, "%1.0f");
             evas_object_size_hint_weight_set(sl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(sl, EVAS_HINT_FILL, EVAS_HINT_FILL);
             elm_slider_value_set(sl, source->volume.volumes[i]);
             elm_box_pack_end(bxhv, sl);
             evas_object_show(sl);
             evas_object_smart_callback_add(sl, "changed",
                                            _cb_source_volume_channel_change, fr);
             evas_object_smart_callback_add(sl, "slider,drag,stop",
                                            _cb_source_volume_channel_drag_stop, fr);
             elm_object_disabled_set(sl, source->mute);
             sls = eina_list_append(sls, sl);
          }
     }
   evas_object_data_set(fr, "volumes", sls);

   bxhv = elm_box_add(bx);
   elm_box_pack_end(bx, bxhv);
   evas_object_show(bxhv);

   ck = elm_check_add(bx);
   evas_object_data_set(fr, "mute", ck);
   elm_object_text_set(ck, "Mute");
   elm_check_state_set(ck, source->mute);
   elm_box_pack_end(bxhv, ck);
   evas_object_show(ck);
   evas_object_smart_callback_add(ck, "changed",
                                  _cb_source_mute_change, fr);

   if (source->volume.channel_count > 1)
     {
        ck = elm_check_add(bx);
        evas_object_data_set(fr, "lock", ck);
        elm_object_text_set(ck, "Lock");
        elm_check_state_set(ck, locked);
        elm_box_pack_end(bxhv, ck);
        evas_object_show(ck);
        evas_object_smart_callback_add(ck, "changed",
                                       _cb_source_lock_change, fr);
     }
}


static void
_emix_source_add(Emix_Source *source)
{
   Evas_Object *bxv, *bx, *lb, *sep, *fr;
   unsigned int i;
   Eina_Bool locked = EINA_TRUE;

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(fr, EVAS_HINT_FILL, 0.0);
   elm_object_style_set(fr, "pad_medium");
   source_list = eina_list_append(source_list, fr);
   evas_object_data_set(fr, "source", source);

   bxv = elm_box_add(win);
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
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   bx = elm_box_add(win);
   evas_object_data_set(fr, "volume_bx", bx);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   /* Compare each volume level and check if they differ. If they differ unlock
   the volume control and let user set each channel volume level */
   for (i = 1; i < source->volume.channel_count; ++i)
     {
        if (source->volume.volumes[i - 1] != source->volume.volumes[i])
          {
             locked = EINA_FALSE;
             break;
          }
     }

   _emix_source_volume_fill(source, fr, bx, locked);


   elm_object_content_set(fr, bxv);
   evas_object_show(bxv);

   elm_box_pack_end(source_box, fr);
   evas_object_show(fr);

   sep = elm_separator_add(win);
   evas_object_data_set(fr, "extra", sep);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(source_box, sep);
   evas_object_show(sep);
}

static void
_emix_source_del(Emix_Source *source)
{
   Eina_List *l;
   Evas_Object *fr;
   EINA_LIST_FOREACH(source_list, l, fr)
     {
        if (evas_object_data_get(fr, "source") == source)
          {
             source_list = eina_list_remove_list(source_list, l);
             evas_object_del(evas_object_data_get(fr, "extra"));
             evas_object_del(fr);
             return;
          }
     }
}

static void
_emix_source_change(Emix_Source *source)
{
   const Eina_List *l;
   Eina_List *ll;
   Evas_Object *fr, *ck, *sl, *lb;

   EINA_LIST_FOREACH(source_list, l, fr)
     {
        if (evas_object_data_get(fr, "source") == source) break;
     }
   if (!l) return;

   ck = evas_object_data_get(fr, "lock");

   if (ck && !elm_check_state_get(ck))
     {
        ck = evas_object_data_get(fr, "mute");
        elm_check_state_set(ck, source->mute);
        EINA_LIST_FOREACH(evas_object_data_get(fr, "volumes"), ll, sl)
          {
             elm_slider_value_set(sl,
                                  source->volume.volumes[(uintptr_t)evas_object_data_get(sl, "channel")]);
             elm_object_disabled_set(sl, source->mute);
             lb = evas_object_data_get(sl, "lb");
             elm_object_disabled_set(lb, source->mute);
          }
     }
   else
     {
        ck = evas_object_data_get(fr, "mute");
        elm_check_state_set(ck, source->mute);

        sl = evas_object_data_get(fr, "volume");
        elm_slider_value_set(sl, source->volume.volumes[0]);
        elm_object_disabled_set(sl, source->mute);
     }
}

//////////////////////////////////////////////////////////////////////////////
static void
_cb_card_profile_change(void *data,
                        Evas_Object *obj,
                        void *event_info EINA_UNUSED)
{
   Emix_Profile *profile = data;
   Evas_Object *fr = evas_object_data_get(obj, "parent");
   Emix_Card *card = evas_object_data_get(fr, "card");
   elm_object_text_set(obj, profile->description);
   emix_card_profile_set(card, profile);
}

static void
_emix_card_add(Emix_Card *card)
{
   Evas_Object *bxv, *bx, *lb, *hv, *sep, *fr;
   Eina_List *l;
   Emix_Profile *profile;

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(fr, EVAS_HINT_FILL, 0.0);
   elm_object_style_set(fr, "pad_medium");
   card_list = eina_list_append(card_list, fr);
   evas_object_data_set(fr, "card", card);

   bxv = elm_box_add(win);
   evas_object_size_hint_weight_set(bxv, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bxv, EVAS_HINT_FILL, 0.0);

   bx = elm_box_add(win);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(bxv, bx);
   evas_object_show(bx);

   lb = elm_label_add(win);
   elm_object_text_set(lb, card->name);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   hv = elm_hoversel_add(win);
   evas_object_data_set(hv, "parent", fr);
   evas_object_data_set(fr, "profile", hv);
   elm_hoversel_hover_parent_set(hv, win);
   EINA_LIST_FOREACH(card->profiles, l, profile)
     {
        if (!profile->plugged) continue;
        elm_hoversel_item_add(hv, profile->description,
                              NULL, ELM_ICON_NONE,
                              _cb_card_profile_change, profile);
        if (profile->active) elm_object_text_set(hv, profile->description);
     }
   evas_object_size_hint_weight_set(hv, 0.0, 0.5);
   evas_object_size_hint_align_set(hv, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx, hv);
   evas_object_show(hv);

   elm_object_content_set(fr, bxv);
   evas_object_show(bxv);

   elm_box_pack_end(card_box, fr);
   evas_object_show(fr);

   sep = elm_separator_add(win);
   evas_object_data_set(fr, "extra", sep);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(card_box, sep);
   evas_object_show(sep);
}

static void
_emix_card_change(Emix_Card *card)
{
   const Eina_List *l;
   Evas_Object *fr, *hv;
   Emix_Profile *profile;

   EINA_LIST_FOREACH(card_list, l, fr)
     {
        if (evas_object_data_get(fr, "card") == card) break;
     }
   if (!l) return;
   hv = evas_object_data_get(fr, "profile");
   elm_hoversel_clear(hv);
   EINA_LIST_FOREACH(card->profiles, l, profile)
     {
        if (!profile->plugged) continue;
        elm_hoversel_item_add(hv, profile->description,
                              NULL, ELM_ICON_NONE,
                              _cb_card_profile_change, profile);
        if (profile->active) elm_object_text_set(hv, profile->description);
     }
}


static void
_emix_card_del(Emix_Card *card)
{
   Eina_List *l;
   Evas_Object *fr;
   EINA_LIST_FOREACH(card_list, l, fr)
     {
        if (evas_object_data_get(fr, "card") == card)
          {
             card_list = eina_list_remove_list(card_list, l);
             evas_object_del(evas_object_data_get(fr, "extra"));
             evas_object_del(fr);
             return;
          }
     }
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
      case EMIX_CARD_ADDED_EVENT:
        _emix_card_add(event_info);
        break;
      case EMIX_CARD_REMOVED_EVENT:
        _emix_card_del(event_info);
        break;
      case EMIX_CARD_CHANGED_EVENT:
        _emix_card_change(event_info);
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
   evas_object_hide(card_scroller);
}

static void
_cb_outputs(void *data EINA_UNUSED,
            Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   evas_object_hide(source_scroller);
   evas_object_hide(sink_input_scroller);
   evas_object_show(sink_scroller);
   evas_object_hide(card_scroller);
}

static void
_cb_inputs(void *data EINA_UNUSED,
           Evas_Object *obj EINA_UNUSED,
           void *event_info EINA_UNUSED)
{
   evas_object_show(source_scroller);
   evas_object_hide(sink_input_scroller);
   evas_object_hide(sink_scroller);
   evas_object_hide(card_scroller);
}

static void
_cb_card(void *data EINA_UNUSED,
           Evas_Object *obj EINA_UNUSED,
           void *event_info EINA_UNUSED)
{
   evas_object_hide(source_scroller);
   evas_object_hide(sink_input_scroller);
   evas_object_hide(sink_scroller);
   evas_object_show(card_scroller);
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

static void
_fill_card(void)
{
   const Eina_List *l;
   Emix_Card *card;

   EINA_LIST_FOREACH(emix_cards_get(), l, card)
     {
        _emix_card_add(card);
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

   win = elm_win_util_standard_add("emix", _("Mixer"));
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
   elm_toolbar_icon_size_set(tbar, 16);
   elm_toolbar_select_mode_set(tbar, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_toolbar_homogeneous_set(tbar, EINA_TRUE);
   elm_object_style_set(tbar, "item_horizontal");
   evas_object_size_hint_weight_set(tbar, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tbar, EVAS_HINT_FILL, EVAS_HINT_FILL);

   elm_toolbar_item_append(tbar, "media-playback-start", _("Playback"), _cb_playback, NULL);
   elm_toolbar_item_append(tbar, "audio-volume-medium", _("Outputs"), _cb_outputs, NULL);
   elm_toolbar_item_append(tbar, "audio-input-microphone", _("Inputs"), _cb_inputs, NULL);
   elm_toolbar_item_append(tbar, "audio-card", _("Cards"), _cb_card, NULL);

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

   sc = elm_scroller_add(win);
   card_scroller = sc;
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

   bx = elm_box_add(win);
   card_box = bx;
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(card_scroller, bx);
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
   _fill_card();
   evas_object_show(win);

   elm_run();
done:
   emix_shutdown();
   return 0;
}
ELM_MAIN()
