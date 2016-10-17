#include "clock.h"

EINTERN Config *time_config = NULL;
EINTERN Eina_List *clock_instances = NULL;
static Ecore_Timer *clock_timer;

static void
_clock_calendar_month_update(Instance *inst)
{
   Evas_Object *od, *oi;
   int x, y;

   oi = elm_layout_edje_get(inst->o_cal);
   edje_object_part_text_set(oi, "e.text.month", inst->month);
   edje_object_part_text_set(oi, "e.text.year", inst->year);
   for (x = 0; x < 7; x++)
     {
        od = edje_object_part_table_child_get(oi, "e.table.daynames", x, 0);
        edje_object_part_text_set(od, "e.text.label", inst->daynames[x]);
        edje_object_message_signal_process(od);
        if (inst->dayweekends[x][0])
          edje_object_signal_emit(od, "e,state,weekend", "e");
        else
          edje_object_signal_emit(od, "e,state,weekday", "e");
     }

   for (y = 0; y < 6; y++)
     {
        for (x = 0; x < 7; x++)
          {
             char buf[32];

             od = edje_object_part_table_child_get(oi, "e.table.days", x, y);
             snprintf(buf, sizeof(buf), "%i", (int)inst->daynums[x][y]);
             edje_object_part_text_set(od, "e.text.label", buf);
             if (inst->dayweekends[x][y])
               edje_object_signal_emit(od, "e,state,weekend", "e");
             else
               edje_object_signal_emit(od, "e,state,weekday", "e");
             if (inst->dayvalids[x][y])
               edje_object_signal_emit(od, "e,state,visible", "e");
             else
               edje_object_signal_emit(od, "e,state,hidden", "e");
             if (inst->daytoday[x][y])
               edje_object_signal_emit(od, "e,state,today", "e");
             else
               edje_object_signal_emit(od, "e,state,someday", "e");
             edje_object_message_signal_process(od);
          }
     }
   edje_object_message_signal_process(oi);
}

static void
_clock_month_prev_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Instance *inst = data;
   inst->madj--;
   time_instance_update(inst);
   _clock_calendar_month_update(inst);
}

static void
_clock_month_next_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Instance *inst = data;
   inst->madj++;
   time_instance_update(inst);
   _clock_calendar_month_update(inst);
}

static void
_clock_popup_dismissed(void *data EINA_UNUSED, Evas_Object *obj, void *info EINA_UNUSED)
{
   evas_object_del(obj);
}

static void
_eval_instance_size(Instance *inst)
{
   Evas_Coord mw, mh;
   int sw = 0, sh = 0;
   Evas_Object *ed = elm_layout_edje_get(inst->o_clock);

   edje_object_size_min_get(ed, &mw, &mh);

   if ((mw < 1) || (mh < 1))
     {
        if (edje_object_part_exists(ed, "e.sizer"))
          {
             edje_object_part_geometry_get(ed, "e.sizer", NULL, NULL, &mw, &mh);
          }
        else
          {
             Evas_Object *owner;

             owner = e_gadget_site_get(inst->o_clock);
             switch (e_gadget_site_orient_get(owner))
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  evas_object_geometry_get(owner, NULL, NULL, NULL, &sh);
                  break;

                case E_GADGET_SITE_ORIENT_VERTICAL:
                  evas_object_geometry_get(owner, NULL, NULL, &sw, NULL);
                  break;

                default: break;
               }

             evas_object_resize(inst->o_clock, sw, sh);
             edje_object_message_signal_process(ed);

             edje_object_parts_extends_calc(ed, NULL, NULL, &mw, &mh);
          }
     }

   if (mw < 4) mw = 4;
   if (mh < 4) mh = 4;

   if (mw < sw) mw = sw;
   if (mh < sh) mh = sh;

   evas_object_size_hint_aspect_set(inst->o_clock, EVAS_ASPECT_CONTROL_BOTH, mw, mh);
}

static void
_clock_edje_init(Instance *inst, Evas_Object *o)
{
   char datestr[128];
   const char *digital[] =
   {
      "e/gadget/clock/digital",
      "e/gadget/clock/digital/advanced",
   };

   time_datestring_format(inst, datestr, sizeof(datestr) - 1);
   if (inst->cfg->digital_clock)
     e_theme_edje_object_set(o, NULL, digital[inst->cfg->advanced]);
   else
     e_theme_edje_object_set(o, NULL, "e/gadget/clock/analog");
   if (inst->cfg->show_date)
     elm_layout_signal_emit(o, "e,state,date,on", "e");
   else
     elm_layout_signal_emit(o, "e,state,date,off", "e");
   if (inst->cfg->digital_24h)
     elm_layout_signal_emit(o, "e,state,24h,on", "e");
   else
     elm_layout_signal_emit(o, "e,state,24h,off", "e");
   if (inst->cfg->show_seconds)
     elm_layout_signal_emit(o, "e,state,seconds,on", "e");
   else
     elm_layout_signal_emit(o, "e,state,seconds,off", "e");

   elm_object_part_text_set(o, "e.text.sub", datestr);
   if (inst->cfg->timezone)
     {
        Edje_Message_String msg;

        msg.str = (char*)inst->cfg->timezone;
        edje_object_message_send(elm_layout_edje_get(o), EDJE_MESSAGE_STRING, 1, &msg);
     }
   {
      Edje_Message_String_Int msg;
      msg.str = (char*)inst->cfg->colorclass[0] ?: "";
      msg.val = !!inst->cfg->colorclass[0];
      edje_object_message_send(elm_layout_edje_get(o), EDJE_MESSAGE_STRING_INT, 2, &msg);
      msg.str = (char*)inst->cfg->colorclass[1] ?: "";
      msg.val = !!inst->cfg->colorclass[1];
      edje_object_message_send(elm_layout_edje_get(o), EDJE_MESSAGE_STRING_INT, 3, &msg);
   }
   edje_object_message_signal_process(elm_layout_edje_get(o));
}

static Eina_Bool
_clock_timer(void *d EINA_UNUSED)
{
   Eina_List *l;
   Instance *inst;
   Eina_Bool seconds = EINA_FALSE;
   int sec = 0;
   char buf[128];

   EINA_LIST_FOREACH(clock_instances, l, inst)
     {
        if (!inst->cfg->advanced) continue;
        seconds |= inst->cfg->show_seconds;
        sec = time_string_format(inst, buf, sizeof(buf));
        elm_object_part_text_set(inst->o_clock, "e.text", buf);
        _eval_instance_size(inst);
     }
   sec = seconds ? 1 : (61 - sec);
   if (clock_timer)
     ecore_timer_interval_set(clock_timer, sec);
   else
     clock_timer = ecore_timer_add(sec, _clock_timer, NULL);
   return EINA_TRUE;
}

static void
_clock_popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
  Instance *inst = data;

  if (obj != inst->popup) return;
  inst->popup = inst->o_table = inst->o_cal = NULL;
}

EINTERN void
clock_popup_new(Instance *inst)
{
   Evas_Object *oi;

   if (inst->popup) return;

   inst->madj = 0;

   time_instance_update(inst);

   inst->popup = elm_ctxpopup_add(inst->o_clock);
   elm_object_style_set(inst->popup, "noblock");
   evas_object_smart_callback_add(inst->popup, "dismissed", _clock_popup_dismissed, inst);
   evas_object_event_callback_add(inst->popup, EVAS_CALLBACK_DEL, _clock_popup_del, inst);

   inst->o_table = elm_table_add(inst->popup);

   oi = elm_layout_add(inst->o_table);
   inst->o_cal = oi;
   e_theme_edje_object_set(oi, "base/theme/gadget/clock",
                           "e/gadget/clock/calendar");
   _clock_calendar_month_update(inst);

   elm_object_signal_callback_add(oi, "e,action,prev", "*",
                                   _clock_month_prev_cb, inst);
   elm_object_signal_callback_add(oi, "e,action,next", "*",
                                   _clock_month_next_cb, inst);
   edje_object_message_signal_process(elm_layout_edje_get(oi));
   elm_layout_sizing_eval(oi);
   elm_table_pack(inst->o_table, oi, 0, 1, 1, 1);
   evas_object_show(oi);

   elm_object_content_set(inst->popup, inst->o_table);
   e_gadget_util_ctxpopup_place(inst->o_clock, inst->popup, NULL);
   evas_object_show(inst->popup);
}

void
clock_instances_redo(void)
{
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(clock_instances, l, inst)
     {
        _clock_edje_init(inst, inst->o_clock);
        _eval_instance_size(inst);
     }
}

static void
_clock_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   if (ev->button == 1)
     {
        if (inst->popup) elm_ctxpopup_dismiss(inst->popup);
        else clock_popup_new(inst);
     }
   else if (ev->button == 3)
     e_gadget_configure(inst->o_clock);
}

static void
_clock_sizing_changed_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   _eval_instance_size(data);
}

static void
clock_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Eina_List *l;
   Eina_Bool advanced = EINA_FALSE, seconds = EINA_FALSE;

   clock_instances = eina_list_remove(clock_instances, inst);
   evas_object_del(inst->popup);
   time_daynames_clear(inst);
   free(inst);
   EINA_LIST_FOREACH(clock_instances, l, inst)
     {
        advanced |= !!inst->cfg->advanced;
        seconds |= !!inst->cfg->show_seconds;
        if (seconds) break;
     }
   if (seconds) return; //no change possible
   E_FREE_FUNC(clock_timer, ecore_timer_del);
   if (advanced)
     _clock_timer(NULL);
}

static Config_Item *
_conf_item_get(int *id, Eina_Bool digital)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(time_config->items, l, ci)
          if (*id == ci->id) return ci;
     }

   ci = E_NEW(Config_Item, 1);
   if (!*id)
     ci->id = time_config->items ? eina_list_count(time_config->items) + 1 : 1;
   else
     ci->id = -1;

   ci->weekend.start = 6;
   ci->weekend.len = 2;
   ci->week.start = 1;
   ci->digital_clock = digital;
   ci->digital_24h = 0;
   ci->show_seconds = 0;
   ci->show_date = 0;
   ci->time_str[0] = eina_stringshare_add("%I:%M");
   ci->time_str[1] = eina_stringshare_add("%F");

   if (ci->id < 1) return ci;
   time_config->items = eina_list_append(time_config->items, ci);
   e_config_save_queue();

   return ci;
}

static Evas_Object *
_clock_gadget_configure(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "clock");
   return config_clock(inst->cfg, e_comp_object_util_zone_get(g));
}

static void
_clock_gadget_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst = data;

   if (inst->o_clock != event_info) return;
   time_config->items = eina_list_remove(time_config->items, inst->cfg);
   eina_stringshare_del(inst->cfg->timezone);
   eina_stringshare_del(inst->cfg->time_str[0]);
   eina_stringshare_del(inst->cfg->time_str[1]);
   E_FREE(inst->cfg);
}

static void
_clock_gadget_created_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   e_gadget_configure_cb_set(inst->o_clock, _clock_gadget_configure);
   evas_object_smart_callback_del_full(obj, "gadget_created", _clock_gadget_created_cb, data);
   if (inst->cfg->advanced)
     {
        _clock_timer(NULL);
        ecore_timer_reset(clock_timer);
     }
   _eval_instance_size(inst);
}

static Evas_Object *
clock_create(Evas_Object *parent, Instance *inst, E_Gadget_Site_Orient orient)
{
   Evas_Object *o;
   const char *sig = NULL;

   inst->o_clock = o = elm_layout_add(parent);
   elm_layout_signal_callback_add(o, "e,state,sizing,changed", "*",
                                   _clock_sizing_changed_cb, inst);

   _clock_edje_init(inst, o);

   switch (orient)
     {
      case E_GADGET_SITE_ORIENT_HORIZONTAL:
        sig = "e,state,horizontal";
        break;

      case E_GADGET_SITE_ORIENT_VERTICAL:
        sig = "e,state,vertical";
        break;

      default:
        sig = "e,state,float";
     }

   elm_layout_signal_emit(inst->o_clock, sig, "e");

   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, clock_del, inst);
   evas_object_smart_callback_add(parent, "gadget_created", _clock_gadget_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _clock_gadget_removed_cb, inst);
   evas_object_data_set(o, "clock", inst);

   evas_object_event_callback_add(inst->o_clock,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _clock_cb_mouse_down,
                                  inst);

   if (inst->cfg->id < 0) return o;
   clock_instances = eina_list_append(clock_instances, inst);

   return o;
}

EINTERN Evas_Object *
digital_clock_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id, 1);
   return clock_create(parent, inst, orient);
}

EINTERN Evas_Object *
analog_clock_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id, 0);
   return clock_create(parent, inst, orient);
}

typedef struct Wizard_Item
{
   E_Gadget_Wizard_End_Cb cb;
   void *data;
   int id;
} Wizard_Item;

static void
_wizard_end(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Wizard_Item *wi = data;

   wi->cb(wi->data, wi->id);
   free(wi);
}

static void
clock_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Eina_Bool digital)
{
   int id = 0;
   Config_Item *ci;
   Wizard_Item *wi;

   wi = E_NEW(Wizard_Item, 1);
   wi->cb = cb;
   wi->data = data;

   ci = _conf_item_get(&id, digital);
   wi->id = ci->id;
   evas_object_event_callback_add(config_clock(ci, NULL), EVAS_CALLBACK_DEL, _wizard_end, wi); 
}

EINTERN void
digital_clock_wizard(E_Gadget_Wizard_End_Cb cb, void *data)
{
   clock_wizard(cb, data, 1);
}

EINTERN void
analog_clock_wizard(E_Gadget_Wizard_End_Cb cb, void *data)
{
   clock_wizard(cb, data, 0);
}

EINTERN void
time_config_update(Config_Item *ci)
{
   Eina_List *l;
   Instance *inst;
   Eina_Bool advanced = EINA_FALSE;

   ci->week.start = (ci->weekend.start + ci->weekend.len) % 7;
   EINA_LIST_FOREACH(clock_instances, l, inst)
     {
         if (inst->cfg != ci) continue;
         _clock_edje_init(inst, inst->o_clock);
         if (!advanced)
           {
              advanced |= inst->cfg->advanced;
              if (!inst->cfg->advanced) continue;
              _clock_timer(NULL);
              ecore_timer_reset(clock_timer);
           }
        _eval_instance_size(inst);
     }
   if (!advanced)
     E_FREE_FUNC(clock_timer, ecore_timer_del);
   e_config_save_queue();
}
