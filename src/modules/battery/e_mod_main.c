#include "e.h"
#include "e_mod_main.h"

#include <sys/mman.h>
#include <fcntl.h>

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);

Eina_List *device_batteries;
Eina_List *device_ac_adapters;
double init_time;

/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
   GADCON_CLIENT_CLASS_VERSION,
   "battery",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL, NULL
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

#define MAX_UNITS 8

/* actual module specifics */
typedef struct _Instance Instance;

typedef struct __Popup_Widgets
{
   Battery     *bat;

   Evas_Object *pb_usage;
   Evas_Object *state;
   Evas_Object *remaining;
   Evas_Object *health;
   Evas_Object *technology;

   Evas_Object *drain;
   Evas_Object *full;
   Evas_Object *power;
   Evas_Object *charge_lim;

   Evas_Object *gr_bat;
   Evas_Object *gr_pow;
   Evas_Object *gr_crg;
} _Popup_Widgets;

typedef struct __Popup_Data
{
   Instance        *inst;
   unsigned int     n_units;
   _Popup_Widgets   widgets[MAX_UNITS];
} _Popup_Data;

struct _Instance
{
   E_Gadcon_Client *gcc;
   Evas_Object     *o_battery;

   E_Gadcon_Popup  *popup;
   Ecore_Timer     *popup_timer;
   _Popup_Data     *popup_data;

   Evas_Object     *popup_battery;
   E_Gadcon_Popup  *warning;
   unsigned int     notification_id;
};

static void      _battery_update(int full, int time_left, int time_full, Eina_Bool have_battery, Eina_Bool have_power, Eina_Bool charging);
static void      _button_cb_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void      _battery_face_level_set(Evas_Object *battery, double level);
static void      _battery_face_time_set(Evas_Object *battery, int time);
static void      _battery_face_cb_menu_powermanagement(void *data, E_Menu *m, E_Menu_Item *mi);
static void      _battery_face_cb_menu_configure(void *data, E_Menu *m, E_Menu_Item *mi);

static Eina_Bool _battery_cb_warning_popup_timeout(void *data);
static void      _battery_cb_warning_popup_hide(void *data, Evas *e, Evas_Object *obj, void *event);
static void      _battery_warning_popup_destroy(Instance *inst);
static void      _battery_warning_popup(Instance *inst, int time, double percent);

static void      _battery_popup_usage_destroy(Instance *inst);
static void      _battery_popup_usage_new(Instance *inst);

static Eina_Bool _powersave_cb_config_update(void *data, int type, void *event);

static E_Config_DD *conf_battery_edd = NULL;
static E_Config_DD *conf_edd = NULL;
static Ecore_Event_Handler *_handler = NULL;

Config *battery_config = NULL;

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Evas_Object *o;
   E_Gadcon_Client *gcc;
   Instance *inst;

   battery_config->full = -2;
   battery_config->time_left = -2;
   battery_config->time_full = -2;
   battery_config->have_battery = -2;
   battery_config->have_power = -2;

   inst = E_NEW(Instance, 1);

   o = edje_object_add(gc->evas);
   e_theme_edje_object_set(o, "base/theme/modules/battery",
                           "e/modules/battery/main");

   gcc = e_gadcon_client_new(gc, name, id, style, o);
   gcc->data = inst;

   inst->gcc = gcc;
   inst->o_battery = o;
   inst->warning = NULL;
   inst->popup_battery = NULL;

   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _button_cb_mouse_down, inst);
   battery_config->instances =
     eina_list_append(battery_config->instances, inst);
   _battery_config_updated();

   return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;

   inst = gcc->data;
   if (battery_config)
     battery_config->instances =
       eina_list_remove(battery_config->instances, inst);
   evas_object_del(inst->o_battery);
   if (inst->warning)
     {
        e_object_del(E_OBJECT(inst->warning));
        inst->popup_battery = NULL;
     }
   _battery_popup_usage_destroy(inst);
   E_FREE(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   Instance *inst;
   Evas_Coord mw, mh, mxw, mxh;

   inst = gcc->data;
   mw = 0, mh = 0;
   edje_object_size_min_get(inst->o_battery, &mw, &mh);
   edje_object_size_max_get(inst->o_battery, &mxw, &mxh);
   if ((mw < 1) || (mh < 1))
     edje_object_size_min_calc(inst->o_battery, &mw, &mh);
   if (mw < 4) mw = 4;
   if (mh < 4) mh = 4;
   if ((mxw > 0) && (mxh > 0))
     e_gadcon_client_aspect_set(gcc, mxw, mxh);
   e_gadcon_client_min_size_set(gcc, mw, mh);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _("Battery");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   char buf[4096];

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-battery.edj",
            e_module_dir_get(battery_config->module));
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class)
{
   static char buf[4096];

   snprintf(buf, sizeof(buf), "%s.%d", client_class->name,
            eina_list_count(battery_config->instances) + 1);
   return buf;
}

static inline const Battery_Hist_Entry *
_battery_history_get(Battery *bat, int age)
{
// should not have to check
//  if (age >= BATTERY_HIST_MAX) return NULL;
  _battery_history_load(bat);
  if (!bat->hist) return NULL;
  int pos = (bat->hist->history_pos + age) % BATTERY_HIST_MAX;
  return &(bat->hist->history[pos]);
}

static void
_battery_history_store(Battery *bat, unsigned long long tim, int full, int mwatts, Eina_Bool ac, Eina_Bool charging)
{
  // last timepoint
  unsigned long long tprev;
  unsigned short hpos;

  _battery_history_load(bat);
  if (!bat->hist) return;
  hpos = bat->hist->history_pos;
  tprev = bat->hist->history[hpos].timepoint;
  // 10 sec resoolution
  if ((tim - tprev) < 10) return;
  // bump pos with wrap-around
  hpos = (hpos + BATTERY_HIST_MAX - 1) % BATTERY_HIST_MAX;
  bat->hist->history_pos = hpos;
  // store time and values
  bat->hist->history[hpos].timepoint = tim;
  bat->hist->history[hpos].full = full;
  mwatts /= 100000;
  if (mwatts > 65000) mwatts = 65000;
  bat->hist->history[hpos].power_now = mwatts;
  bat->hist->history[hpos].ac = ac;
  bat->hist->history[hpos].charging = charging;
  if (bat->hist->history[hpos].power_now > bat->history_power_now_max)
    bat->history_power_now_max = bat->hist->history[hpos].power_now;
}

static double
_battery_wattage_get(Battery *bat, unsigned long long timeperiod)
{
  int i = 0;
  const Battery_Hist_Entry *h0, *h1;
  int diff;

  h0 = _battery_history_get(bat, i);
  if ((!h0) || (h0->timepoint == 0)) return -1.0;
  for (;;)
    {
      i++;
      h1 = _battery_history_get(bat, i);
      if ((!h1) || (h1->timepoint == 0)) return -1.0;
      if ((h0->timepoint - h1->timepoint) >= timeperiod) break;
    }
  diff = h0->full - h1->full;
  if (diff < 0) diff = -diff;
  return
    (double)(bat->last_full_charge * diff * 3600) /
    (10000.0 * 1000000.0 * (double)timeperiod);
}

static void
_battery_popup_usage_destroy(Instance *inst)
{
   if (inst->popup_timer) ecore_timer_del(inst->popup_timer);
   E_FREE_FUNC(inst->popup, e_object_del);
   if (inst->popup_data) E_FREE(inst->popup_data);
   inst->popup = NULL; inst->popup_timer = NULL;
   inst->popup_data = NULL;
}

static void
_battery_popup_usage_destroy_cb(void *obj)
{
   _battery_popup_usage_destroy(e_object_data_get(obj));
}

static Eina_Bool
_battery_popup_usage_content_update_cb(void *data)
{
   Instance *inst;
   _Popup_Data *pd;
   Eina_List *l;
   Battery *bat;
   int hrs, mins, t = 0;
   unsigned int i = 0;
   char buf[256];

   pd = data;
   inst = pd->inst;

   if (!battery_config->have_battery)
     {
        _battery_popup_usage_destroy(inst);
        return ECORE_CALLBACK_CANCEL;
     }

   if (!inst->popup) return ECORE_CALLBACK_CANCEL;

   EINA_LIST_FOREACH(device_batteries, l, bat)
     {
        _Popup_Widgets *w = &pd->widgets[i++];

        elm_progressbar_value_set(w->pb_usage, (double) bat->percent / 10000.0);

        if ((battery_config->have_power) && (battery_config->full < 10000))
          {
             t = bat->time_full;
             elm_object_text_set(w->state, _("Charging"));
          }
        else if ((!battery_config->have_power) && (battery_config->full < 10000))
          {
             t = bat->time_left;
             elm_object_text_set(w->state, _("Discharging"));
          }
        else
          {
             t = 0;
             elm_object_text_set(w->state, _("Charged"));
          }

        hrs = (t / 3600);
        if (hrs < 0) hrs = 0;
        mins = (t / 60) - (hrs * 60);
        if (mins < 0) mins = 0;
        snprintf(buf, sizeof(buf), "%i:%02i", hrs, mins);
        elm_object_text_set(w->remaining, buf);

        if ((bat->last_full_charge <= 0) || (bat->design_charge <= 0))
          snprintf(buf, sizeof(buf), "???%%");
        else
          {
             double health = 100.0 * (double)bat->last_full_charge / bat->design_charge;
             if (health > 100.0) health = 100.0;
             snprintf(buf, sizeof(buf), "%1.1f%%", health);
          }
        elm_object_text_set(w->health, buf);

        if (bat->technology)
          elm_object_text_set(w->technology, bat->technology);
        else
          elm_object_text_set(w->technology, _("Unknown"));

        if (bat->is_micro_watts)
          {
            // over the past 5 mins
            double watts = _battery_wattage_get(bat, 60);

            if (watts >= 0.0)
              {
                snprintf(buf, sizeof(buf), "%1.2fW", watts);
                elm_object_text_set(w->drain, buf);
              }
            else
              elm_object_text_set(w->drain, _("Unknown"));
          }
        else
          elm_object_text_set(w->drain, _("Unknown"));

        if (bat->is_micro_watts)
          {
            double wh = (double)bat->last_full_charge / 1000000.0;
            snprintf(buf, sizeof(buf), "%1.2fWh", wh);
            elm_object_text_set(w->full, buf);
          }
        else
          elm_object_text_set(w->full, _("Unknown"));

        if (bat->is_micro_watts)
          {
            double watt = (double)bat->power_now / 1000000.0;
            snprintf(buf, sizeof(buf), "%1.2fW", watt);
            elm_object_text_set(w->power, buf);
          }
        else
          elm_object_text_set(w->power, _("Unknown"));

        if ((w->charge_lim) && (bat->charge_lim >= 0))
          elm_slider_value_set(w->charge_lim, bat->charge_lim);

         {
           int *v_full, *v_pow, *v_crg, age;
           unsigned int vals_num;
           const Battery_Hist_Entry *bh;
           unsigned long long time_now = time(NULL), td, tp;
           int full_tot, pow_tot, crg_tot, tot, min_crg, max_crg, full0, full1;

           e_graph_colorspec_set(w->gr_pow, "cc::selected-alt");
           e_graph_colorspec_set(w->gr_bat, "cc::selected");

           e_graph_colorspec_set(w->gr_crg, "cc::selected2");
           e_graph_colorspec_down_set(w->gr_crg, "cc::selected4");

           min_crg = -1;
           max_crg = 1;
           vals_num = 50;
           v_full = calloc(vals_num, sizeof(int));
           v_pow = calloc(vals_num, sizeof(int));
           v_crg = calloc(vals_num, sizeof(int));
           if ((v_full) && (v_pow) && (v_crg))
             {
               age = 0;
               tp = 120;
               for (i = 0; i < vals_num; i++)
                 {
                   full_tot = pow_tot = crg_tot = full0 = full1 = tot = 0;
                   bh = _battery_history_get(bat, age);
                   if (bh) full1 = bh->full;
                   full0 = full1;
                   for (;;)
                     {
                       bh = _battery_history_get(bat, age);
                       if (!bh) break;
                       td = time_now - bh->timepoint;
                       if (td < tp)
                         {
                           full0 = bh->full;
                           full_tot += bh->full;
                           pow_tot += bh->power_now;
                           tot++;
                           age++;
                         }
                       else break;
                     }
                   if (!bh)
                     {
                       bh = _battery_history_get(bat, age - 1);
                       if (bh) full0 = bh->full;
                     }
                   if (tot > 0)
                     {
                       crg_tot = full1 - full0;
                       v_pow[vals_num - i - 1] = pow_tot / tot;
                       v_full[vals_num - i - 1] = full_tot / tot;
                       v_crg[vals_num - i - 1] = crg_tot / tot;
                       if (v_crg[vals_num - i - 1] < min_crg)
                         min_crg = v_crg[vals_num - i - 1];
                       if (v_crg[vals_num - i - 1] > max_crg)
                         max_crg = v_crg[vals_num - i - 1];
                     }
                   tp += 120; // 1 val == 120 sec
                 }
               e_graph_values_set(w->gr_pow, vals_num, v_pow, 0, bat->history_power_now_max);
               e_graph_values_set(w->gr_bat, vals_num, v_full, 0, 10000);
               e_graph_values_set(w->gr_crg, vals_num, v_crg, min_crg, max_crg);
             }
           free(v_full);
           free(v_pow);
           free(v_crg);
        }
        if (i == (pd->n_units - 1)) break;
     }
   return ECORE_CALLBACK_RENEW;
}

static char *
_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   char *buf;
   if (strcmp(part, "elm.text")) return NULL;
   buf = data;
   return strdup(buf);
}

static Evas_Object *
_icon_get(void *data EINA_UNUSED, Evas_Object *obj, const char *part)
{
   Evas_Object *ic;
   if (strcmp(part, "elm.swallow.icon")) return NULL;

   ic = elm_icon_add(obj);
   elm_icon_standard_set(ic, "battery");
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   evas_object_show(ic);
   return ic;
}

static void
_cb_charge_lim_change(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
   _Popup_Widgets *w = data;
   double v = elm_slider_value_get(obj);
   Config_Battery *cb;
   Eina_List *l;

   w->bat->charge_lim = (int)v;
   e_system_send("battery-lim-set", "%s %i\n", w->bat->udi, (int)v);
   EINA_LIST_FOREACH(battery_config->battery_configs, l, cb)
    {
      if ((cb->udi) && (w->bat->udi) && (!strcmp(cb->udi, w->bat->udi))) break;
      cb = NULL;
    }
  if (!cb)
    {
      cb = E_NEW(Config_Battery, 1);
      if (cb)
        {
          battery_config->battery_configs = eina_list_append(battery_config->battery_configs, cb);
          cb->udi = eina_stringshare_add(w->bat->udi);
        }
    }
  if (cb)
    {
      cb->charge_limit = w->bat->charge_lim;
      e_config_save_queue();
    }
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *part)
{
   Evas_Object *tb, *o;
   _Popup_Widgets *w;
   if (strcmp(part, "elm.swallow.content")) return NULL;

   w = data;

   tb = o = elm_table_add(obj);
   elm_table_padding_set(o, ELM_SCALE_SIZE(4), ELM_SCALE_SIZE(4));
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);


   o = evas_object_rectangle_add(evas_object_evas_get(obj));
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(280), ELM_SCALE_SIZE(20));
   elm_table_pack(tb, o, 0, 0, 8, 1);

   w->pb_usage = o = elm_progressbar_add(obj);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_progressbar_span_size_set(o, 1.0);
   elm_table_pack(tb, o, 0, 0, 8, 1);
   evas_object_show(o);


   o = elm_icon_add(obj);
   elm_icon_standard_set(o, "power-plug");
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_table_pack(tb, o, 0, 1, 1, 1);
   evas_object_show(o);

   w->state = o = elm_label_add(obj);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 1, 1, 1, 1);
   evas_object_show(o);

   o = elm_icon_add(obj);
   elm_icon_standard_set(o, "clock");
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_table_pack(tb, o, 2, 1, 1, 1);
   evas_object_show(o);

   w->remaining = o = elm_label_add(obj);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 3, 1, 1, 1);
   evas_object_show(o);

   o = elm_icon_add(obj);
   elm_icon_standard_set(o, "health");
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_table_pack(tb, o, 4, 1, 1, 1);
   evas_object_show(o);

   w->health = o = elm_label_add(obj);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 5, 1, 1, 1);
   evas_object_show(o);

   o = elm_icon_add(obj);
   elm_icon_standard_set(o, "tech");
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_table_pack(tb, o, 6, 1, 1, 1);
   evas_object_show(o);

   w->technology = o = elm_label_add(obj);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 7, 1, 1, 1);
   evas_object_show(o);


   o = elm_icon_add(obj);
   elm_icon_standard_set(o, "battery-good-charging");
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_table_pack(tb, o, 0, 2, 1, 1);
   evas_object_show(o);

   w->drain = o = elm_label_add(obj);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 1, 2, 1, 1);
   evas_object_show(o);

   o = elm_icon_add(obj);
   elm_icon_standard_set(o, "battery-full");
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_table_pack(tb, o, 2, 2, 1, 1);
   evas_object_show(o);

   w->full = o = elm_label_add(obj);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 3, 2, 1, 1);
   evas_object_show(o);

   o = elm_icon_add(obj);
   elm_icon_standard_set(o, "battery-empty-charging");
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_table_pack(tb, o, 4, 2, 1, 1);
   evas_object_show(o);

   w->power = o = elm_label_add(obj);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 5, 2, 1, 1);
   evas_object_show(o);


   w->gr_bat = o = e_graph_add(obj);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, 1.0, 0.0);
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(80), ELM_SCALE_SIZE(40));
   elm_table_pack(tb, o, 0, 3, 8, 1);
   evas_object_show(o);

   o = elm_label_add(obj);
   elm_object_text_set(o, _("Level"));
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 1, 3, 1, 1);
   evas_object_show(o);

   if (w->bat->charge_lim >= 0)
     {
       w->charge_lim = o = elm_slider_add(obj);
       evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
       evas_object_size_hint_weight_set(o, 1.0,  1.0);
       elm_object_text_set(o, _("Charge Limit"));
       elm_slider_unit_format_set(o, "%1.0f%%");
       elm_slider_indicator_format_set(o, "%1.0f%%");
       elm_slider_min_max_set(o, 10, 100);
       elm_slider_step_set(o, 5);
       elm_slider_value_set(o, w->bat->charge_lim);
       evas_object_smart_callback_add(o, "changed", _cb_charge_lim_change, w);
       elm_table_pack(tb, o, 0, 4, 8, 1);
       evas_object_show(o);
     }
   else w->charge_lim = NULL;

   w->gr_pow = o = e_graph_add(obj);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, 1.0,  1.0);
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(80), ELM_SCALE_SIZE(40));
   elm_table_pack(tb, o, 0, 5, 8, 1);
   evas_object_show(o);

   o = elm_label_add(obj);
   elm_object_text_set(o, _("Energy"));
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 1, 6, 1, 1);
   evas_object_show(o);

   w->gr_crg = o = e_graph_add(obj);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, 1.0,  1.0);
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(80), ELM_SCALE_SIZE(40));
   elm_table_pack(tb, o, 0, 7, 8, 1);
   evas_object_show(o);

   o = elm_label_add(obj);
   elm_object_text_set(o, _("(Dis)charge"));
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   evas_object_size_hint_weight_set(o, 1.0, 0);
   elm_table_pack(tb, o, 1, 8, 1, 1);
   evas_object_show(o);

   return tb;
}

static void
_battery_popup_usage_new(Instance *inst)
{
   Evas_Object *base, *tb, *rec, *glist;
   _Popup_Data *pd;
   static char buf[512];
   Eina_List *l;
   Battery *bat;
   int n, i = 0;
   Elm_Genlist_Item_Class *itc, *itc2;

   n = eina_list_count(device_batteries);
   if (!n) return;

   base = e_comp->elm;
   inst->popup = e_gadcon_popup_new(inst->gcc, 0);

   pd = E_NEW(_Popup_Data, 1);
   pd->inst = inst;
   pd->n_units = n;
   inst->popup_data = pd;

   tb = elm_table_add(base);
   E_FILL(tb); E_EXPAND(tb);
   evas_object_show(tb);

   rec = evas_object_rectangle_add(evas_object_evas_get(base));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(400), ELM_SCALE_SIZE(330));
   evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(560), ELM_SCALE_SIZE(480));
   elm_table_pack(tb, rec, 0, 0, 1, 1);

   glist = elm_genlist_add(base);
   E_FILL(glist);
   E_EXPAND(glist);
   elm_genlist_select_mode_set(glist, ELM_OBJECT_SELECT_MODE_NONE);
   elm_table_pack(tb, glist, 0, 0, 1, 1);
   evas_object_show(glist);

   itc = elm_genlist_item_class_new();
   itc->item_style = "full";
   itc->func.text_get = NULL;
   itc->func.content_get = _content_get;
   itc->func.filter_get = NULL;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   itc2 = elm_genlist_item_class_new();
   itc2->item_style = "group_index";
   itc2->func.text_get = _text_get;
   itc2->func.content_get = _icon_get;
   itc2->func.filter_get = NULL;
   itc2->func.state_get = NULL;
   itc2->func.del = NULL;

   EINA_LIST_FOREACH(device_batteries, l, bat)
     {
        _Popup_Widgets *w = &pd->widgets[i++];

        w->bat = bat;
        if ((bat->vendor) && (bat->model))
          snprintf(buf, sizeof(buf), _("Battery: %s (%s)"), bat->vendor, bat->model);
        else if (bat->vendor)
          snprintf(buf, sizeof(buf), _("Battery: %s"), bat->vendor);
        else if (bat->model)
          snprintf(buf, sizeof(buf), _("Battery: %s"), bat->model);
        else
          snprintf(buf, sizeof(buf), _("Battery"));
        elm_genlist_item_append(glist, itc2, buf, NULL, ELM_GENLIST_ITEM_GROUP, NULL, NULL);
        elm_genlist_item_append(glist, itc, w, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }

   if (pd->n_units)
     inst->popup_timer = ecore_timer_add(2.0, _battery_popup_usage_content_update_cb, pd);

   elm_genlist_item_class_free(itc);
   elm_genlist_item_class_free(itc2);

   e_gadcon_popup_content_set(inst->popup, tb);
   e_gadcon_popup_show(inst->popup);
   e_object_data_set(E_OBJECT(inst->popup),inst);
   E_OBJECT_DEL_SET(inst->popup, _battery_popup_usage_destroy_cb);

   _battery_popup_usage_content_update_cb(pd);
}

static void
_button_cb_mouse_down(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Instance *inst;
   Evas_Event_Mouse_Down *ev;

   inst = data;
   ev = event_info;
   if (ev->button == 3)
     {
        E_Menu *m;
        E_Menu_Item *mi;
        int cx, cy;

        m = e_menu_new();
        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _battery_face_cb_menu_configure, NULL);
        if (e_configure_registry_exists("advanced/powermanagement"))
          {
             mi = e_menu_item_new(m);
             e_menu_item_label_set(mi, _("Power Management Timing"));
             e_util_menu_item_theme_icon_set(mi, "preferences-system-power-management");
             e_menu_item_callback_set(mi, _battery_face_cb_menu_powermanagement, NULL);
          }

        m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);

        e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon,
                                          &cx, &cy, NULL, NULL);
        e_menu_activate_mouse(m,
                              e_zone_current_get(),
                              cx + ev->output.x, cy + ev->output.y, 1, 1,
                              E_MENU_POP_DIRECTION_DOWN, ev->timestamp);
        evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button,
                                 EVAS_BUTTON_NONE, ev->timestamp, NULL);
     }
   if (ev->button == 1)
     {
        _battery_cb_warning_popup_hide(data, e, obj, event_info);

        if (!inst->popup)
          _battery_popup_usage_new(inst);
        else
          _battery_popup_usage_destroy(inst);
     }
}

static void
_battery_face_level_set(Evas_Object *battery, double level)
{
   Edje_Message_Float msg;
   char buf[256];

   snprintf(buf, sizeof(buf), "%i", (int)(level * 100.0));
   edje_object_part_text_set(battery, "e.text.reading", buf);

   if (level < 0.0) level = 0.0;
   else if (level > 1.0)
     level = 1.0;
   msg.val = level;
   edje_object_message_send(battery, EDJE_MESSAGE_FLOAT, 1, &msg);
}

static void
_battery_face_time_set(Evas_Object *battery, int t)
{
   char buf[256];
   int hrs, mins;

   if (t < 0) return;

   hrs = (t / 3600);
   mins = ((t) / 60 - (hrs * 60));
   if (hrs < 0) hrs = 0;
   if (mins < 0) mins = 0;
   snprintf(buf, sizeof(buf), "%i:%02i", hrs, mins);
   edje_object_part_text_set(battery, "e.text.time", buf);
}

static void
_battery_face_cb_menu_powermanagement(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   e_configure_registry_call("advanced/powermanagement", NULL, NULL);
}

static void
_battery_face_cb_menu_configure(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   if (!battery_config) return;
   if (battery_config->config_dialog) return;
   e_int_config_battery_module(NULL, NULL);
}

Battery *
_battery_battery_find(const char *udi)
{
   Eina_List *l;
   Battery *bat;
   EINA_LIST_FOREACH(device_batteries, l, bat)
     { /* these are always stringshared */
       if (udi == bat->udi) return bat;
     }

   return NULL;
}

Ac_Adapter *
_battery_ac_adapter_find(const char *udi)
{
   Eina_List *l;
   Ac_Adapter *ac;
   EINA_LIST_FOREACH(device_ac_adapters, l, ac)
     { /* these are always stringshared */
       if (udi == ac->udi) return ac;
     }

   return NULL;
}

void
_battery_device_update(void)
{
   Eina_List *l;
   Battery *bat;
   Ac_Adapter *ac;
   int full = -1;
   int time_left = -1;
   int time_full = -1;
   int have_battery = 0;
   int have_power = 0;
   int charging = 0;
   int batnum = 0;
   int acnum = 0;
   unsigned long long tim = time(NULL);

   EINA_LIST_FOREACH(device_ac_adapters, l, ac)
     {
        if (ac->present)
          {
             acnum++;
             have_power = 1;
          }
     }

   EINA_LIST_FOREACH(device_batteries, l, bat)
     {
        if ((!bat->got_prop)/* || (!bat->technology)*/)
          continue;
        have_battery = 1;
        batnum++;
        if (bat->charging == 1) have_power = 1;
        if (full == -1) full = 0;
        if (bat->percent >= 0)
          full += bat->percent;
        else if (bat->last_full_charge > 0)
          full += (bat->current_charge * 10000) / bat->last_full_charge;
        else if (bat->design_charge > 0)
          full += (bat->current_charge * 10000) / bat->design_charge;
        if (bat->time_left > 0)
          {
             if (time_left < 0) time_left = bat->time_left;
             else time_left += bat->time_left;
          }
        if (bat->time_full > 0)
          {
             if (time_full < 0) time_full = bat->time_full;
             else time_full += bat->time_full;
          }
        charging += bat->charging;
        _battery_history_store(bat, tim,
                               full, bat->power_now,
                               have_power, bat->charging);
     }

   if ((device_batteries) && (batnum == 0))
     return;  /* not ready yet, no properties received for any battery */

   if (batnum > 0) full /= batnum;
   if ((full == 10000) && have_power)
     {
        time_left = -1;
        time_full = -1;
     }
   if (time_left < 1) time_left = -1;
   if (time_full < 1) time_full = -1;

   _battery_update(full, time_left, time_full, have_battery, have_power, charging);
}

void
_battery_config_updated(void)
{
   Eina_List *l;
   Instance *inst;

   if (!battery_config) return;

   if (battery_config->instances)
     {
        EINA_LIST_FOREACH(battery_config->instances, l, inst)
          _battery_warning_popup_destroy(inst);
     }
#ifdef HAVE_EEZE
  if (!eina_list_count(device_batteries)) _battery_udev_start();
#elif defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
  if (!eina_list_count(device_batteries)) _battery_sysctl_start();
#else
  if (!eina_list_count(device_batteries)) _battery_upower_start();
#endif
}

static Eina_Bool
_battery_cb_warning_popup_timeout(void *data)
{
   Instance *inst;

   inst = data;
   e_gadcon_popup_hide(inst->warning);
   battery_config->alert_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_battery_cb_warning_popup_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Instance *inst = NULL;

   inst = (Instance *)data;
   if ((!inst) || (!inst->warning)) return;
   e_gadcon_popup_hide(inst->warning);
}

static void
_battery_warning_popup_destroy(Instance *inst)
{
   if (battery_config->alert_timer)
     {
        ecore_timer_del(battery_config->alert_timer);
        battery_config->alert_timer = NULL;
     }
   if ((!inst) || (!inst->warning)) return;
   E_FREE_FUNC(inst->popup_battery, evas_object_del);
   E_FREE_FUNC(inst->warning, e_object_del);
}

static void
_battery_warning_popup_cb(void *data, unsigned int id)
{
   Instance *inst = data;

   inst->notification_id = id;
}

static void
_battery_warning_popup(Instance *inst, int t, double percent)
{
   Evas *e = NULL;
   Evas_Object *popup_bg = NULL;
   int x, y, w, h;

   if ((!inst) || (inst->warning)) return;

   if (battery_config->desktop_notifications)
     {
        E_Notification_Notify n;
        memset(&n, 0, sizeof(E_Notification_Notify));
        n.app_name = _("Battery");
        n.replaces_id = 0;
        n.icon.icon = "battery-low";
        n.summary = _("Your battery is low!");
        n.body = _("AC power is recommended.");
        n.timeout = battery_config->alert_timeout * 1000;
        e_notification_client_send(&n, _battery_warning_popup_cb, inst);
        return;
     }

   inst->warning = e_gadcon_popup_new(inst->gcc, 0);
   if (!inst->warning) return;

   e = e_comp->evas;

   popup_bg = edje_object_add(e);
   inst->popup_battery = edje_object_add(e);

   if ((!popup_bg) || (!inst->popup_battery))
     {
        e_object_free(E_OBJECT(inst->warning));
        inst->warning = NULL;
        return;
     }

   e_theme_edje_object_set(popup_bg, "base/theme/modules/battery/popup",
                           "e/modules/battery/popup");
   e_theme_edje_object_set(inst->popup_battery, "base/theme/modules/battery",
                           "e/modules/battery/main");
   if (edje_object_part_exists(popup_bg, "e.swallow.battery"))
     edje_object_part_swallow(popup_bg, "e.swallow.battery", inst->popup_battery);
   else
     edje_object_part_swallow(popup_bg, "battery", inst->popup_battery);

   edje_object_part_text_set(popup_bg, "e.text.title",
                             _("Your battery is low!"));
   edje_object_part_text_set(popup_bg, "e.text.label",
                             _("AC power is recommended."));

   e_gadcon_popup_content_set(inst->warning, popup_bg);
   e_gadcon_popup_show(inst->warning);

   evas_object_geometry_get(inst->warning->o_bg, &x, &y, &w, &h);
   evas_object_event_callback_add(inst->warning->comp_object, EVAS_CALLBACK_MOUSE_DOWN,
                               _battery_cb_warning_popup_hide, inst);

   _battery_face_time_set(inst->popup_battery, t);
   _battery_face_level_set(inst->popup_battery, percent);
   edje_object_signal_emit(inst->popup_battery, "e,state,discharging", "e");

   if ((battery_config->alert_timeout > 0) &&
       (!battery_config->alert_timer))
     {
        battery_config->alert_timer =
          ecore_timer_loop_add(battery_config->alert_timeout,
                          _battery_cb_warning_popup_timeout, inst);
     }
}

static Eina_Bool
_powersave_cb_config_update(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   if (!battery_config->have_battery)
     e_powersave_mode_set(E_POWERSAVE_MODE_LOW);
   else
     {
        if (battery_config->have_power)
          e_powersave_mode_set(E_POWERSAVE_MODE_LOW);
        else if (battery_config->full > 95)
          e_powersave_mode_set(E_POWERSAVE_MODE_MEDIUM);
        else if (battery_config->full > 30)
          e_powersave_mode_set(E_POWERSAVE_MODE_HIGH);
        else
          e_powersave_mode_set(E_POWERSAVE_MODE_EXTREME);
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_battery_update(int full, int time_left, int time_full, Eina_Bool have_battery, Eina_Bool have_power, Eina_Bool charging)
{
   Eina_List *l;
   Instance *inst;
   static double debounce_time = 0.0;

   EINA_LIST_FOREACH(battery_config->instances, l, inst)
     {
        if (have_power != battery_config->have_power)
          {
             if (have_power)
               edje_object_signal_emit(inst->o_battery, "e,state,ac,on", "e");
             else
               edje_object_signal_emit(inst->o_battery, "e,state,ac,off", "e");
             if (have_power && (full < 10000))
               {
                  edje_object_signal_emit(inst->o_battery, "e,state,charging", "e");
                  if (inst->popup_battery)
                    edje_object_signal_emit(inst->popup_battery, "e,state,charging", "e");
               }
             else
               {
                  edje_object_signal_emit(inst->o_battery, "e,state,discharging", "e");
                  if (inst->popup_battery)
                    edje_object_signal_emit(inst->popup_battery, "e,state,discharging", "e");
               }
          }
        if (have_battery)
          {
             if (battery_config->full != full)
               {
                  double val;

                  if (full >= 10000) val = 1.0;
                  else val = (double)full / 10000.0;
                  _battery_face_level_set(inst->o_battery, val);
                  if (inst->popup_battery)
                    _battery_face_level_set(inst->popup_battery, val);
               }
          }
        else
          {
             _battery_face_level_set(inst->o_battery, 0.0);
             edje_object_part_text_set(inst->o_battery, "e.text.reading", _("N/A"));
             if (inst->popup_battery)
               {
                  _battery_face_level_set(inst->popup_battery, 0.0);
                  edje_object_part_text_set(inst->popup_battery, "e.text.reading", _("N/A"));
               }
          }

        if ((time_full < 0) && (time_left != battery_config->time_left))
          {
             _battery_face_time_set(inst->o_battery, time_left);
             if (inst->popup_battery)
               _battery_face_time_set(inst->popup_battery,
                                      time_left);
          }
        else if ((time_left < 0) && (time_full != battery_config->time_full))
          {
             _battery_face_time_set(inst->o_battery, time_full);
             if (inst->popup_battery)
               _battery_face_time_set(inst->popup_battery,
                                      time_full);
          }
        if (have_battery &&
            (!have_power) &&
            (full < 10000) &&
            (
              (
                (time_left > 0) &&
                battery_config->alert &&
                ((time_left / 60) <= battery_config->alert)
              ) ||
              (
                battery_config->alert_p &&
                (full <= battery_config->alert_p)
              )
            )
            )
          {
             double t;

             printf("-------------------------------------- bat warn .. why below\n");
             printf("have_battery = %i\n", (int)have_battery);
             printf("have_power = %i\n", (int)have_power);
             printf("full = %i\n", (int)full / 100);
             printf("time_left = %i\n", (int)time_left);
             printf("battery_config->alert = %i\n", (int)battery_config->alert);
             printf("battery_config->alert_p = %i\n", (int)battery_config->alert_p);
             t = ecore_time_get();
             if ((t - debounce_time) > 30.0)
               {
                  printf("t-debounce = %3.3f\n", (t - debounce_time));
                  debounce_time = t;
                  if (((t - init_time) > 5.0) && (full < 1500))
                    _battery_warning_popup(inst, time_left, (double)full / 10000.0);
               }
          }
        else if (have_power || ((time_left / 60) > battery_config->alert))
          _battery_warning_popup_destroy(inst);
        if ((have_battery) && (!have_power) && (full >= 0) &&
            (battery_config->suspend_below > 0) &&
            (full < battery_config->suspend_below / 100))
          {
             printf("battery %i suspend below %i\n", full / 100, battery_config->suspend_below);
             if (battery_config->suspend_method == SUSPEND)
               e_sys_action_do(E_SYS_SUSPEND_MODE, NULL);
             else if (battery_config->suspend_method == HIBERNATE)
               e_sys_action_do(E_SYS_HIBERNATE, NULL);
             else if (battery_config->suspend_method == SHUTDOWN)
               e_sys_action_do(E_SYS_HALT, NULL);
          }
     }
   if (!have_battery)
     e_powersave_mode_set(E_POWERSAVE_MODE_LOW);
   else
     {
        if (have_power)
          e_powersave_mode_set(E_POWERSAVE_MODE_LOW);
        else if (full > 9500)
          e_powersave_mode_set(E_POWERSAVE_MODE_MEDIUM);
        else if (full > 3000)
          e_powersave_mode_set(E_POWERSAVE_MODE_HIGH);
        else
          e_powersave_mode_set(E_POWERSAVE_MODE_EXTREME);
     }
   battery_config->full = full;
   battery_config->time_left = time_left;
   battery_config->have_battery = have_battery;
   battery_config->have_power = have_power;
   battery_config->charging = charging;
}

static void
_battery_history_file_reset(int fd)
{
  int val;

  ftruncate(fd, 0);
  ftruncate(fd, sizeof(Battery_Hist));
  lseek(fd, 0, SEEK_SET);
  val = BATTERY_HISTORY_MAGIC;
  write(fd, &val, sizeof(val));
  val = BATTERY_HISTORY_VERSION;
  write(fd, &val, sizeof(val));
  // rest is zero
}

static void
_battery_history_file_map(Battery *bat, int fd)
{
  bat->hist = mmap(NULL, sizeof(Battery_Hist), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if ((bat->hist == MAP_FAILED) || (bat->hist == NULL))
    { // yes i know - not correct but nothing will sensibly mmap at addr 0/NULL
      if (bat->hist == NULL) munmap(bat->hist, sizeof(Battery_Hist));
      bat->hist = NULL;
    }
}

void
_battery_history_load(Battery *bat)
{
  char buf[PATH_MAX];
  int fd, i;
  off_t off;

  if (bat->hist) return;
  // XXX: some unique host id needed ?
  e_user_dir_concat_static(buf, "battery-history.dat");
  fd = open(buf, O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
  if (fd < 0) fd = open(buf, O_RDWR | O_EXCL | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) return;
  off = lseek(fd, 0, SEEK_END);
  if (off != sizeof(Battery_Hist)) _battery_history_file_reset(fd);
  _battery_history_file_map(bat, fd);
  if (!bat->hist) goto end;
  if ((bat->hist->magic != BATTERY_HISTORY_MAGIC) ||
      (bat->hist->version != BATTERY_HISTORY_VERSION) ||
      (bat->hist->history_pos >= BATTERY_HIST_MAX))
    {
      munmap(bat->hist, sizeof(Battery_Hist));
      _battery_history_file_reset(fd);
      _battery_history_file_map(bat, fd);
      if (!bat->hist) goto end;
    }
  bat->history_power_now_max = 0;
  for (i = 0; i < BATTERY_HIST_MAX; i++)
    {
      if (bat->hist->history[i].power_now > bat->history_power_now_max)
      bat->history_power_now_max = bat->hist->history[i].power_now;
    }
end:
  close(fd); // we can close fd once we have our mapping
}

void
_battery_history_close(Battery *bat)
{
  if (!bat->hist) return;
  munmap(bat->hist, sizeof(Battery_Hist));
  bat->hist = NULL;
}


/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION, "Battery"
};

E_API void *
e_modapi_init(E_Module *m)
{
   conf_battery_edd = E_CONFIG_DD_NEW("Battery_Config_Battery", Config_Battery);
#undef T
#undef D
#define T Config_Battery
#define D conf_battery_edd
   E_CONFIG_VAL(D, T, udi, STR);
   E_CONFIG_VAL(D, T, charge_limit, INT);

   conf_edd = E_CONFIG_DD_NEW("Battery_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_VAL(D, T, alert, INT);
   E_CONFIG_VAL(D, T, alert_p, INT);
   E_CONFIG_VAL(D, T, alert_timeout, INT);
   E_CONFIG_VAL(D, T, suspend_below, INT);
   E_CONFIG_VAL(D, T, suspend_method, INT);
   E_CONFIG_VAL(D, T, desktop_notifications, INT);
   E_CONFIG_LIST(D, T, battery_configs, conf_battery_edd);
#if defined HAVE_EEZE || defined(__OpenBSD__)
   E_CONFIG_VAL(D, T, fuzzy, INT);
#endif

   battery_config = e_config_domain_load("module.battery", conf_edd);
   if (!battery_config)
     {
        battery_config = E_NEW(Config, 1);
        battery_config->alert = 30;
        battery_config->alert_p = 10;
        battery_config->alert_timeout = 0;
        battery_config->suspend_below = 0;
        battery_config->desktop_notifications = 0;
#if defined HAVE_EEZE || defined(__OpenBSD__)
        battery_config->fuzzy = 0;
#endif
     }
   E_CONFIG_LIMIT(battery_config->alert, 0, 60);
   E_CONFIG_LIMIT(battery_config->alert_p, 0, 100);
   E_CONFIG_LIMIT(battery_config->alert_timeout, 0, 300);
   E_CONFIG_LIMIT(battery_config->suspend_below, 0, 50);
   E_CONFIG_LIMIT(battery_config->desktop_notifications, 0, 1);

   battery_config->module = m;
   battery_config->full = -2;
   battery_config->time_left = -2;
   battery_config->time_full = -2;
   battery_config->have_battery = -2;
   battery_config->have_power = -2;

   _handler = ecore_event_handler_add(E_EVENT_POWERSAVE_CONFIG_UPDATE,
                                      _powersave_cb_config_update, NULL);

   e_gadcon_provider_register(&_gadcon_class);

   e_configure_registry_category_add("advanced", 80, _("Advanced"), NULL,
                                     "preferences-advanced");
   e_configure_registry_item_add("advanced/battery", 100, _("Battery Meter"),
                                 NULL, "battery", e_int_config_battery_module);

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   Config_Battery *cb;

   e_configure_registry_item_del("advanced/battery");
   e_configure_registry_category_del("advanced");
   e_gadcon_provider_unregister(&_gadcon_class);

   if (battery_config->alert_timer)
     ecore_timer_del(battery_config->alert_timer);

   if (battery_config->config_dialog)
     e_object_del(E_OBJECT(battery_config->config_dialog));

#ifdef HAVE_EEZE
   _battery_udev_stop();
#elif defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
   _battery_sysctl_stop();
#else
   _battery_upower_stop();
#endif

   EINA_LIST_FREE(battery_config->battery_configs, cb)
     {
        eina_stringshare_del(cb->udi);
        free(cb);
     }

   free(battery_config);
   battery_config = NULL;
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_battery_edd);
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.battery", conf_edd, battery_config);
   return 1;
}

