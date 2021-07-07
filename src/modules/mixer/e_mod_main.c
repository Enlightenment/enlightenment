#include <e.h>
#include <Eina.h>
#include "emix.h"
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "mixer.h"
#include "backend.h"

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

typedef struct _Mon_Data Mon_Data;
struct _Mon_Data
{
   Emix_Sink *sink;
   Emix_Source *source;
   Evas_Object *vu;
   Ecore_Animator *animator;
   double last_time;
   float samp_max;
   float samp_max2;
   int mon_skips;
   int mon_update;
   int mon_samps;
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
   Evas_Object *slider_ic;
   Evas_Object *check;
   Evas_Object *vu;
   Evas_Object *playback_box;
   Evas_Object *recbox;
   Evas_Object *recslider;
   Evas_Object *reccheck;
   Evas_Object *recbx;
   Evas_Object *recic;
   Evas_Object *recvu;
   Evas_Object *recording_box;
   Mon_Data mon_data;
   Mon_Data recmon_data;
};

static Context *mixer_context = NULL;
static Eina_List *_handlers = NULL;

static char *
_sink_icon_find(const char *name)
{
   const char *dir;
   char buf[PATH_MAX], *res = NULL, **strs, *glob, *icon;
   FILE *f;
   size_t len;

   dir = e_module_dir_get(mixer_context->module);
   if (!dir) return NULL;
   snprintf(buf, sizeof(buf), "%s/sink-icons.txt", dir);
   f = fopen(buf, "r");
   if (!f) return NULL;
   while (fgets(buf, sizeof(buf), f))
     {
        buf[sizeof(buf) - 1] = 0;
        len = strlen(buf);
        if (len > 0)
          {
             buf[len - 1] = 0;
             strs = eina_str_split(buf, "|", 0);
             if (strs)
               {
                  glob = strs[0];
                  icon = strs[1];
                  if (icon)
                    {
                       if (e_util_glob_case_match(name, glob))
                         res = strdup(icon);
                    }
                  free(strs[0]);
                  free(strs);
               }
             if (res) break;
          }
        else break;
     }
   fclose(f);
   return res;
}

static void _sink_unmonitor(Instance *inst, Emix_Sink *s);
static void _sink_monitor(Instance *inst, Emix_Sink *s);

static void _source_unmonitor(Instance *inst, Emix_Source *s);
static void _source_monitor(Instance *inst, Emix_Source *s);

static void _popup_playback_box_refill(Instance *inst);
static void _popup_recording_fill(Instance *inst);

static void
_cb_emix_event(void *data, enum Emix_Event event, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if ((event == EMIX_SINK_INPUT_ADDED_EVENT) ||
       (event == EMIX_SINK_INPUT_REMOVED_EVENT))
     {
        _popup_playback_box_refill(inst);
     }
   else if ((event == EMIX_SOURCE_OUTPUT_ADDED_EVENT) ||
            (event == EMIX_SOURCE_OUTPUT_REMOVED_EVENT))
     {
        _popup_recording_fill(inst);
     }
}

static Eina_Bool
_cb_emix_monitor_update(void *data)
{
   Mon_Data *md = data;
   double t = ecore_loop_time_get();
   double td = t - md->last_time;

   if (md->samp_max2 < md->samp_max) md->samp_max2 = md->samp_max;
   else
     {
        md->samp_max2 = md->samp_max2 * (1.0 - (0.5 * td));
        if (md->samp_max2 < 0.001) md->samp_max2 = 0.0;
     }

   if (md->mon_update == 0)
     {
        md->mon_skips++;
        if (md->mon_skips > 5)
          {
             elm_progressbar_part_value_set(md->vu, "elm.cur.progressbar", 0.0);
             elm_progressbar_part_value_set(md->vu, "elm.cur.progressbar1", 0.0);
             md->animator = NULL;
             return EINA_FALSE;
          }
        return EINA_TRUE;
     }
   elm_progressbar_part_value_set(md->vu, "elm.cur.progressbar", md->samp_max);
   elm_progressbar_part_value_set(md->vu, "elm.cur.progressbar1", md->samp_max2);
   md->mon_update = 0;
   md->samp_max = 0;
   md->mon_skips = 0;
   md->mon_samps = 0;
   md->last_time = t;
   return EINA_TRUE;
}

static void
_cb_emix_sink_monitor_event(void *data, enum Emix_Event event, void *event_info)
{
   Mon_Data *md = data;
   Emix_Sink *sink = event_info;

   if (sink != md->sink) return;
   if (event == EMIX_SINK_MONITOR_EVENT)
     {
        unsigned int i, num = sink->mon_num * 2;
        float samp, max = 0.0;

        for (i = 0; i < num; i++)
          {
             samp = fabs(sink->mon_buf[i]);
             if (samp > max) max = samp;
          }
        md->mon_samps += num;
        if (md->samp_max < max) md->samp_max = max;
        md->mon_update++;
        if (!md->animator)
          md->animator = ecore_animator_add(_cb_emix_monitor_update, md);
     }
}

static Eina_Bool
_cb_emix_source_monitor_update(void *data)
{
   Mon_Data *md = data;
   double t = ecore_loop_time_get();
   double td = t - md->last_time;

   if (md->samp_max2 < md->samp_max) md->samp_max2 = md->samp_max;
   else
     {
        md->samp_max2 = md->samp_max2 * (1.0 - (0.5 * td));
        if (md->samp_max2 < 0.001) md->samp_max2 = 0.0;
     }

   if (md->mon_update == 0)
     {
        md->mon_skips++;
        if (md->mon_skips > 5)
          {
             elm_progressbar_part_value_set(md->vu, "elm.cur.progressbar", 0.0);
             elm_progressbar_part_value_set(md->vu, "elm.cur.progressbar1", 0.0);
             md->animator = NULL;
             return EINA_FALSE;
          }
        return EINA_TRUE;
     }
   elm_progressbar_part_value_set(md->vu, "elm.cur.progressbar", md->samp_max);
   elm_progressbar_part_value_set(md->vu, "elm.cur.progressbar1", md->samp_max2);
   md->mon_update = 0;
   md->samp_max = 0;
   md->mon_skips = 0;
   md->mon_samps = 0;
   md->last_time = t;
   return EINA_TRUE;
}

static void
_cb_emix_source_monitor_event(void *data, enum Emix_Event event, void *event_info)
{
   Mon_Data *md = data;
   Emix_Source *source = event_info;

   if (source != md->source) return;
   if (event == EMIX_SOURCE_MONITOR_EVENT)
     {
        unsigned int i, num = source->mon_num * 2;
        float samp, max = 0.0;

        for (i = 0; i < num; i++)
          {
             samp = fabs(source->mon_buf[i]);
             if (samp > max) max = samp;
          }
        md->mon_samps += num;
        if (md->samp_max < max) md->samp_max = max;
        md->mon_update++;
        if (!md->animator)
          md->animator = ecore_animator_add(_cb_emix_source_monitor_update, md);
     }
}

static void
_mixer_popup_update(Instance *inst, int mute, int vol)
{
   Emix_Sink *s;
   char *icname = NULL;

   s = (Emix_Sink *)backend_sink_default_get();
   if (s)
     {
        if (s->name) icname = _sink_icon_find(s->name);
        if (!icname) icname = strdup("audio-volume");

        elm_icon_standard_set(inst->slider_ic, icname);
        free(icname);
     }
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
   inst->slider_ic = NULL;
   inst->check = NULL;
   inst->list = NULL;
   inst->vu = NULL;
   inst->playback_box = NULL;
   inst->recbox = NULL;
   inst->recslider = NULL;
   inst->reccheck = NULL;
   inst->recbx = NULL;
   inst->recic = NULL;
   inst->recvu = NULL;
   inst->recording_box = NULL;
   emix_event_callback_del(_cb_emix_event, inst);
   if (inst->mon_data.sink) _sink_unmonitor(inst, inst->mon_data.sink);
   if (inst->recmon_data.source) _source_unmonitor(inst, inst->recmon_data.source);
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

static void
_sink_unmonitor(Instance *inst, Emix_Sink *s)
{
   Mon_Data *md = &(inst->mon_data);
   if (md->sink != s) return;
   emix_event_callback_del(_cb_emix_sink_monitor_event, md);
   if (md->animator)
     {
        ecore_animator_del(md->animator);
        md->animator = NULL;
     }
   emix_sink_monitor(md->sink, EINA_FALSE);
   md->sink = NULL;
   md->vu = NULL;
   md->mon_update = 0;
   md->samp_max = 0;
   md->mon_skips = 0;
   md->mon_samps = 0;
}

static void
_sink_monitor(Instance *inst, Emix_Sink *s)
{
   Mon_Data *md = &(inst->mon_data);
   if (md->sink == s) return;

   if (md->sink) _sink_unmonitor(inst, md->sink);
   md->sink = s;
   md->vu = inst->vu;
   emix_event_callback_add(_cb_emix_sink_monitor_event, md);
   emix_sink_monitor(md->sink, EINA_TRUE);
}

static void
_source_unmonitor(Instance *inst, Emix_Source *s)
{
   Mon_Data *md = &(inst->recmon_data);
   if (md->source != s) return;
   emix_event_callback_del(_cb_emix_source_monitor_event, md);
   if (md->animator)
     {
        ecore_animator_del(md->animator);
        md->animator = NULL;
     }
   emix_source_monitor(md->source, EINA_FALSE);
   md->source = NULL;
   md->vu = NULL;
   md->mon_update = 0;
   md->samp_max = 0;
   md->mon_skips = 0;
   md->mon_samps = 0;
}

static void
_source_monitor(Instance *inst, Emix_Source *s)
{
   Mon_Data *md = &(inst->recmon_data);
   if (md->source == s) return;

   if (md->source) _source_unmonitor(inst, md->source);
   md->source = s;
   md->vu = inst->recvu;
   emix_event_callback_add(_cb_emix_source_monitor_event, md);
   emix_source_monitor(md->source, EINA_TRUE);
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

             if (inst->mon_data.sink)
               _sink_unmonitor(inst, inst->mon_data.sink);
             elm_list_clear(inst->list);
             EINA_LIST_FOREACH((Eina_List *)emix_sinks_get(), ll, s)
               {
                  Elm_Object_Item *it;
                  Evas_Object *ic;
                  char *icname = NULL;

                  if (s->name) icname = _sink_icon_find(s->name);
                  if (!icname) icname = strdup("audio-volume");
                  ic = elm_icon_add(e_comp->elm);
                  evas_object_size_hint_min_set(ic, 20 * e_scale, 20 * e_scale);
                  elm_icon_standard_set(ic, icname);
                  free(icname);
                  it = elm_list_item_append(inst->list, s->name, ic, NULL,
                                            _sink_selected_cb, s);
                  if (backend_sink_default_get() == s)
                    {
                       default_it = it;
                       _sink_monitor(inst, s);
                    }
               }
             elm_list_go(inst->list);
             if (default_it)
               elm_list_item_selected_set(default_it, EINA_TRUE);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static char *
_cb_vu_format_cb(double v EINA_UNUSED)
{
   return "";
}

static void
_popup_playback_box_refill(Instance *inst)
{
   Evas_Object *ic;
   Eina_List *children, *l, *del_list = NULL;
   Eina_List *playbacks;
   Evas_Object *o;
   Emix_Sink_Input *s;
   int num = 0;

   children = elm_box_children_get(inst->playback_box);
   EINA_LIST_FOREACH(children, l, o)
     {
        // skip first item - it's the rect spacer
        if (l->prev) del_list = eina_list_append(del_list, o);
     }
   EINA_LIST_FREE(del_list, o) evas_object_del(o);
   playbacks = (Eina_List *)emix_sink_inputs_get();
   EINA_LIST_FOREACH(playbacks, l, s)
     {
        E_Client *ec = NULL;
        Eina_List *clients, *ll;
        pid_t pid;

        ic = NULL;
        pid = s->pid;
        for (;;)
          {
             if ((pid <= 1) || (pid == getpid())) return;

             clients = e_client_focus_stack_get();
             EINA_LIST_FOREACH(clients, ll, ec)
               {
                  if ((ec->netwm.pid == pid) && (!ec->parent))
                    {
                       ic = e_client_icon_add(ec, e_comp->evas);
                       break;
                    }
               }
             pid = backend_util_get_ppid(pid);
             if (ic) break;
          }
        if (!ic)
          {
             if (s->icon)
               {
                  ic = elm_icon_add(e_comp->elm);
                  elm_icon_standard_set(ic, s->icon);
               }
          }
        if (ic)
          {
             evas_object_size_hint_min_set(ic, 20 * e_scale, 20 * e_scale);
             elm_box_pack_end(inst->playback_box, ic);
             evas_object_show(ic);
          }

        // max 8 app icons
        num++;
        if (num > 8) break;
     }
}

static void
_reccheck_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                     void *event EINA_UNUSED)
{
   backend_source_mute_set(elm_check_state_get(obj));
}

static void
_recslider_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                      void *event EINA_UNUSED)
{
   int val;

   val = (int)elm_slider_value_get(obj);
   backend_source_volume_set(val);
}

static void
_popup_recording_box_refill(Instance *inst)
{
   Evas_Object *ic;
   Eina_List *children, *l, *del_list = NULL;
   Eina_List *recordings;
   Evas_Object *o;
   Emix_Source_Output *s;
   int num = 0;

   children = elm_box_children_get(inst->recording_box);
   EINA_LIST_FOREACH(children, l, o)
     {
        // skip first item - it's the rect spacer
        if (l->prev) del_list = eina_list_append(del_list, o);
     }
   EINA_LIST_FREE(del_list, o) evas_object_del(o);
   recordings = (Eina_List *)emix_source_outputs_get();
   EINA_LIST_FOREACH(recordings, l, s)
     {
        E_Client *ec = NULL;
        Eina_List *clients, *ll;
        pid_t pid;

        ic = NULL;
        pid = s->pid;
        for (;;)
          {
             if ((pid <= 1) || (pid == getpid())) return;

             clients = e_client_focus_stack_get();
             EINA_LIST_FOREACH(clients, ll, ec)
               {
                  if ((ec->netwm.pid == pid) && (!ec->parent))
                    {
                       ic = e_client_icon_add(ec, e_comp->evas);
                       break;
                    }
               }
             pid = backend_util_get_ppid(pid);
             if (ic) break;
          }
        if (!ic)
          {
             if (s->icon)
               {
                  ic = elm_icon_add(e_comp->elm);
                  elm_icon_standard_set(ic, s->icon);
               }
          }
        if (ic)
          {
             evas_object_size_hint_min_set(ic, 20 * e_scale, 20 * e_scale);
             elm_box_pack_end(inst->recording_box, ic);
             evas_object_show(ic);
          }

        // max 8 app icons
        num++;
        if (num > 8) break;
     }
}

static void
_popup_recording_fill(Instance *inst)
{
   Emix_Source *ss;

   if (inst->recording_box) evas_object_del(inst->recording_box);
   if (inst->recvu) evas_object_del(inst->recvu);
   if (inst->recic) evas_object_del(inst->recic);
   if (inst->recbx) evas_object_del(inst->recbx);
   if (inst->reccheck) evas_object_del(inst->reccheck);
   if (inst->recslider) evas_object_del(inst->recslider);

   inst->recslider = NULL;
   inst->reccheck = NULL;
   inst->recbx = NULL;
   inst->recic = NULL;
   inst->recvu = NULL;
   inst->recording_box = NULL;

   ss = (Emix_Source *)backend_source_default_get();
   if (ss) _source_unmonitor(inst, ss);

   if (backend_source_active_get())
     {
        Evas_Object *bx, *r, *slider, *ic;

        bx = elm_box_add(e_comp->elm);
        elm_box_horizontal_set(bx, EINA_TRUE);
        evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.5);
        elm_box_pack_end(inst->recbox, bx);
        evas_object_show(bx);

        inst->recvu = elm_progressbar_add(e_comp->elm);
        elm_object_style_set(inst->recvu, "double");
        elm_progressbar_unit_format_function_set(inst->recvu, _cb_vu_format_cb, NULL);
        evas_object_size_hint_weight_set(inst->recvu, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(inst->recvu, EVAS_HINT_FILL, 0.5);
        elm_box_pack_end(bx, inst->recvu);
        evas_object_show(inst->recvu);

        inst->recording_box = elm_box_add(e_comp->elm);
        elm_box_horizontal_set(inst->recording_box, EINA_TRUE);
        evas_object_size_hint_weight_set(inst->recording_box, 0.0, 0.0);
        evas_object_size_hint_align_set(inst->recording_box, 1.0, 0.5);
        elm_box_pack_end(bx, inst->recording_box);
        evas_object_show(inst->recording_box);

        r = evas_object_rectangle_add(evas_object_evas_get(e_comp->elm));
        evas_object_size_hint_min_set(r, 0, 20 * e_scale);
        evas_object_color_set(r, 0, 0, 0, 0);
        elm_box_pack_end(inst->recording_box, r);

        _popup_recording_box_refill(inst);

        bx = elm_box_add(e_comp->elm);
        inst->recbx = bx;
        elm_box_horizontal_set(bx, EINA_TRUE);
        evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
        elm_box_pack_end(inst->recbox, bx);
        evas_object_show(bx);

        ss = (Emix_Source *)backend_source_default_get();
        if (ss)
          {
             ic = elm_icon_add(e_comp->elm);
             inst->recic = ic;
             evas_object_size_hint_min_set(ic, 20 * e_scale, 20 * e_scale);
             elm_icon_standard_set(ic, "audio-input-microphone");
             elm_box_pack_end(bx, ic);
             evas_object_show(ic);
          }

        slider = elm_slider_add(e_comp->elm);
        inst->recslider = slider;
        elm_slider_span_size_set(slider, 128 * elm_config_scale_get());
        elm_slider_unit_format_set(slider, "%1.0f");
        elm_slider_indicator_format_set(slider, "%1.0f");
        evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, 0.0);
        evas_object_show(slider);
        elm_slider_min_max_set(slider, 0.0, emix_max_volume_get());
        evas_object_smart_callback_add(slider, "changed", _recslider_changed_cb, NULL);
        elm_slider_value_set(slider, backend_source_volume_get());
        elm_box_pack_end(bx, slider);
        evas_object_show(slider);

        inst->reccheck = elm_check_add(e_comp->elm);
        evas_object_size_hint_align_set(inst->reccheck, 0.5, EVAS_HINT_FILL);
        elm_object_text_set(inst->reccheck, _("Mute"));
        elm_check_state_set(inst->reccheck, backend_source_mute_get());
        evas_object_smart_callback_add(inst->reccheck, "changed", _reccheck_changed_cb, NULL);
        elm_box_pack_end(bx, inst->reccheck);
        evas_object_show(inst->reccheck);

        if (ss) _source_monitor(inst, ss);
     }
}

static void
_popup_new(Instance *inst)
{
   Evas_Object *button, *list, *slider, *bx, *ic, *r;
   Emix_Sink *s;
   Eina_List *l;
   Elm_Object_Item *default_it = NULL;
   char *icname = NULL;

   emix_event_callback_add(_cb_emix_event, inst);

   inst->popup = e_gadcon_popup_new(inst->gcc, 0);
   list = elm_box_add(e_comp->elm);

   inst->list = elm_list_add(e_comp->elm);
   elm_list_mode_set(inst->list, ELM_LIST_COMPRESS);
   evas_object_size_hint_align_set(inst->list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(inst->list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(inst->list);
   elm_box_pack_end(list, inst->list);

   bx = elm_box_add(e_comp->elm);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(list, bx);
   evas_object_show(bx);

   inst->vu = elm_progressbar_add(e_comp->elm);
   elm_object_style_set(inst->vu, "double");
   elm_progressbar_unit_format_function_set(inst->vu, _cb_vu_format_cb, NULL);
   evas_object_size_hint_weight_set(inst->vu, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(inst->vu, EVAS_HINT_FILL, 0.5);
   elm_box_pack_end(bx, inst->vu);
   evas_object_show(inst->vu);

   inst->playback_box = elm_box_add(e_comp->elm);
   elm_box_horizontal_set(inst->playback_box, EINA_TRUE);
   evas_object_size_hint_weight_set(inst->playback_box, 0.0, 0.0);
   evas_object_size_hint_align_set(inst->playback_box, 1.0, 0.5);
   elm_box_pack_end(bx, inst->playback_box);
   evas_object_show(inst->playback_box);

   r = evas_object_rectangle_add(evas_object_evas_get(e_comp->elm));
   evas_object_size_hint_min_set(r, 0, 20 * e_scale);
   evas_object_color_set(r, 0, 0, 0, 0);
   elm_box_pack_end(inst->playback_box, r);

   _popup_playback_box_refill(inst);

   bx = elm_box_add(e_comp->elm);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   elm_box_pack_end(list, bx);
   evas_object_show(bx);

   s = (Emix_Sink *)backend_sink_default_get();
   if (s)
     {
        if (s->name) icname = _sink_icon_find(s->name);
        if (!icname) icname = strdup("audio-volume");

        ic = elm_icon_add(e_comp->elm);
        evas_object_size_hint_min_set(ic, 20 * e_scale, 20 * e_scale);
        elm_icon_standard_set(ic, icname);
        free(icname);
        elm_box_pack_end(bx, ic);
        evas_object_show(ic);
        inst->slider_ic = ic;
     }

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

   inst->recbox = elm_box_add(e_comp->elm);
   evas_object_size_hint_align_set(inst->recbox, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(inst->recbox, EVAS_HINT_EXPAND, 0.0);
   elm_box_pack_end(list, inst->recbox);
   evas_object_show(inst->recbox);

   _popup_recording_fill(inst);

   button = elm_button_add(e_comp->elm);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0.0);
   elm_object_text_set(button, _("Mixer"));
   evas_object_smart_callback_add(button, "clicked", _emixer_exec_cb, inst);
   elm_box_pack_end(list, button);
   evas_object_show(button);

   EINA_LIST_FOREACH((Eina_List *)emix_sinks_get(), l, s)
     {
        Elm_Object_Item *it;

        if (s->name) icname = _sink_icon_find(s->name);
        if (!icname) icname = strdup("audio-volume");
        ic = elm_icon_add(e_comp->elm);
        evas_object_size_hint_min_set(ic, 20 * e_scale, 20 * e_scale);
        elm_icon_standard_set(ic, icname);
        free(icname);

        it = elm_list_item_append(inst->list, s->name, ic, NULL, _sink_selected_cb, s);
        if (backend_sink_default_get() == s)
          {
             default_it = it;
             _sink_monitor(inst, s);
          }
     }
   elm_list_go(inst->list);

   evas_object_size_hint_min_set(list, 240 * e_scale, 280 * e_scale);

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

   if (backend_mute_get())
     backend_mute_set(EINA_FALSE);

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
   if (inst->popup) _popup_del(inst);
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
e_modapi_init(E_Module *m EINA_UNUSED)
{
   char buf[PATH_MAX];
   const char *dir;

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

   e_configure_registry_category_add("extensions", 90, _("Extensions"), NULL,
                                     "preferences-extensions");
   e_configure_registry_item_add("extensions/emix", 30, _("Mixer"), NULL,
                                 "preferences-desktop-mixer",
                                 emix_config_popup_new);

   e_gadcon_provider_register(&_gadcon_class);

   dir = e_module_dir_get(mixer_context->module);
   if (!dir) return NULL;
   snprintf(buf, sizeof(buf), "%s/sink-icons.txt", dir);
   e_util_env_set("EMIX_SINK_ICONS", buf);
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   e_gadcon_provider_unregister((const E_Gadcon_Client_Class *)&_gadcon_class);

   E_FREE_LIST(_handlers, ecore_event_handler_del);
   if (mixer_context)
     {
        free(mixer_context->theme);
        E_FREE(mixer_context);
     }

   backend_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   emix_config_save();
   return 1;
}

