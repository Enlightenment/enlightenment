#include "e.h"
#include "e_mod_main.h"

#include <sys/time.h>
#include <time.h>

/* actual module specifics */
typedef struct _Instance Instance;

struct _Instance
{
   E_Gadcon_Client *gcc;
   Evas_Object     *o_clock, *o_table, *o_popclock, *o_cal;
   E_Gadcon_Popup  *popup;

   int              madj;

   char             year[8];
   char             month[64];
   const char      *daynames[7];
   unsigned char    daynums[7][6];
   Eina_Bool        dayweekends[7][6];
   Eina_Bool        dayvalids[7][6];
   Eina_Bool        daytoday[7][6];
   Config_Item     *cfg;
};

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);
static Config_Item     *_conf_item_get(const char *id);
static void             _clock_popup_free(Instance *inst);

static Eio_Monitor *clock_tz_monitor = NULL;
static Eio_Monitor *clock_tz2_monitor = NULL;
static Eio_Monitor *clock_tzetc_monitor = NULL;
static Eina_List *handlers = NULL;
Config *clock_config = NULL;

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;
static Eina_List *clock_instances = NULL;
static E_Action *act = NULL;
static Ecore_Timer *update_today = NULL;

/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
   GADCON_CLIENT_CLASS_VERSION,
   "clock",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL, NULL
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

static void
_clear_timestrs(Instance *inst)
{
   int x;

   for (x = 0; x < 7; x++)
     {
        if (inst->daynames[x])
          {
             eina_stringshare_del(inst->daynames[x]);
             inst->daynames[x] = NULL;
          }
     }
}

static void
_todaystr_eval(Instance *inst, char *buf, int bufsz)
{
   if (!inst->cfg->show_date)
     {
        buf[0] = 0;
     }
   else
     {
        struct timeval timev;
        struct tm *tm;
        time_t tt;

        tzset();
        gettimeofday(&timev, NULL);
        tt = (time_t)(timev.tv_sec);
        tm = localtime(&tt);
        if (tm)
          {
             if (inst->cfg->show_date == 1)
               strftime(buf, bufsz, _("%a, %e %b, %Y"), (const struct tm *)tm);
             else if (inst->cfg->show_date == 2)
               strftime(buf, bufsz, _("%a, %x"), (const struct tm *)tm);
             else if (inst->cfg->show_date == 3)
               strftime(buf, bufsz, "%x", (const struct tm *)tm);
             else if (inst->cfg->show_date == 4)
               strftime(buf, bufsz, "%F", (const struct tm *)tm);
          }
        else
          buf[0] = 0;
     }
}

static void
_time_eval(Instance *inst)
{
   struct timeval timev;
   struct tm *tm, tms, tmm, tm2;
   time_t tt;
   int started = 0, num, i;

   tzset();
   gettimeofday(&timev, NULL);
   tt = (time_t)(timev.tv_sec);
   tm = localtime(&tt);

   _clear_timestrs(inst);
   if (tm)
     {
        int day;

        // tms == current date time "saved"
        // tm2 == date to look at adjusting for madj
        // tm2 == month baseline @ 1st
        memcpy(&tms, tm, sizeof(struct tm));
        num = 0;
        for (day = (0 - 6); day < (31 + 16); day++)
          {
             memcpy(&tmm, &tms, sizeof(struct tm));
             tmm.tm_sec = 0;
             tmm.tm_min = 0;
             tmm.tm_hour = 10;
             tmm.tm_mon += inst->madj;
             tmm.tm_mday = 1; // start at the 1st of the month
             tmm.tm_wday = 0; // ignored by mktime
             tmm.tm_yday = 0; // ignored by mktime
             tmm.tm_isdst = 0; // ignored by mktime
             tt = mktime(&tmm);
             tm = localtime(&tt);
             memcpy(&tm2, tm, sizeof(struct tm));

             tt = mktime(&tmm);
             tt += (day * 60 * 60 * 24);
             tm = localtime(&tt);
             memcpy(&tmm, tm, sizeof(struct tm));
             if (!started)
               {
                  if (tm->tm_wday == inst->cfg->week.start)
                    {
                       char buf[32];

                       for (i = 0; i < 7; i++, tm->tm_wday = (tm->tm_wday + 1) % 7)
                         {
                            strftime(buf, sizeof(buf), "%a", tm);
                            inst->daynames[i] = eina_stringshare_add(buf);
                         }
                       started = 1;
                    }
               }
             if (started)
               {
                  int y = num / 7;
                  int x = num % 7;

                  if (y < 6)
                    {
                       inst->daynums[x][y] = tmm.tm_mday;

                       inst->dayvalids[x][y] = 0;
                       if (tmm.tm_mon == tm2.tm_mon) inst->dayvalids[x][y] = 1;

                       inst->daytoday[x][y] = 0;
                       if ((tmm.tm_mon == tms.tm_mon) &&
                           (tmm.tm_year == tms.tm_year) &&
                           (tmm.tm_mday == tms.tm_mday))
                         inst->daytoday[x][y] = 1;

                       inst->dayweekends[x][y] = 0;
                       for (i = inst->cfg->weekend.start;
                            i < (inst->cfg->weekend.start + inst->cfg->weekend.len);
                            i++)
                         {
                            if (tmm.tm_wday == (i % 7))
                              {
                                 inst->dayweekends[x][y] = 1;
                                 break;
                              }
                         }
                    }
                  num++;
               }
          }

        memcpy(&tmm, &tms, sizeof(struct tm));
        tmm.tm_sec = 0;
        tmm.tm_min = 0;
        tmm.tm_hour = 10;
        tmm.tm_mon += inst->madj;
        tmm.tm_mday = 1; // start at the 1st of the month
        tmm.tm_wday = 0; // ignored by mktime
        tmm.tm_yday = 0; // ignored by mktime
        tmm.tm_isdst = 0; // ignored by mktime
        tt = mktime(&tmm);
        tm = localtime(&tt);
        memcpy(&tm2, tm, sizeof(struct tm));
        inst->year[sizeof(inst->year) - 1] = 0;
        strftime(inst->year, sizeof(inst->year) - 1, "%Y", (const struct tm *)&tm2);
        inst->month[sizeof(inst->month) - 1] = 0;
        strftime(inst->month, sizeof(inst->month) - 1, "%B", (const struct tm *)&tm2); // %b for short month
     }
}

static void
_clock_month_update(Instance *inst)
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
   _time_eval(inst);
   _clock_month_update(inst);
}

static void
_clock_month_next_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Instance *inst = data;
   inst->madj++;
   _time_eval(inst);
   _clock_month_update(inst);
}

static void
_clock_settings_cb(void *d1, Evas_Object *obj EINA_UNUSED, void *info EINA_UNUSED)
{
   Instance *inst = d1;
   e_int_config_clock_module(NULL, inst->cfg);
   e_object_del(E_OBJECT(inst->popup));
   inst->popup = NULL;
   inst->o_popclock = NULL;
}

static void
_popclock_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *info EINA_UNUSED)
{
   Instance *inst = data;
   if (inst->o_popclock == obj)
     {
        inst->o_popclock = NULL;
     }
}

static void
_clock_popup_new(Instance *inst)
{
   Evas *evas;
   Evas_Object *o, *oi, *o_button;
   char todaystr[128];

   if (inst->popup) return;

   evas = e_comp->evas;
   evas_event_freeze(evas);
   _todaystr_eval(inst, todaystr, sizeof(todaystr) - 1);

   inst->madj = 0;

   _time_eval(inst);

   inst->popup = e_gadcon_popup_new(inst->gcc, 0);

   inst->o_table = elm_table_add(e_comp->elm);

   oi = elm_layout_add(inst->o_table);
   inst->o_popclock = oi;
   evas_object_size_hint_weight_set(oi, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(oi, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(oi, EVAS_CALLBACK_DEL, _popclock_del_cb, inst);

   if (inst->cfg->digital_clock)
     e_theme_edje_object_set(oi, "base/theme/modules/clock",
                             "e/modules/clock/digital");
   else
     e_theme_edje_object_set(oi, "base/theme/modules/clock",
                             "e/modules/clock/main");
   if (inst->cfg->show_date)
     elm_object_signal_emit(oi, "e,state,date,on", "e");
   else
     elm_object_signal_emit(oi, "e,state,date,off", "e");
   if (inst->cfg->digital_24h)
     elm_object_signal_emit(oi, "e,state,24h,on", "e");
   else
     elm_object_signal_emit(oi, "e,state,24h,off", "e");
   if (inst->cfg->show_seconds)
     elm_object_signal_emit(oi, "e,state,seconds,on", "e");
   else
     elm_object_signal_emit(oi, "e,state,seconds,off", "e");

   elm_object_part_text_set(oi, "e.text.today", todaystr);

   elm_layout_sizing_eval(oi);
   elm_table_pack(inst->o_table, oi, 0, 0, 1, 1);
   evas_object_show(oi);

   o = evas_object_rectangle_add(evas);
   evas_object_size_hint_min_set(o, 80 * e_scale, 80 * e_scale);
   elm_table_pack(inst->o_table, o, 0, 0, 1, 1);

   o_button = o = elm_button_add(e_comp->elm);
   elm_object_text_set(o, _("Settings"));
   evas_object_size_hint_align_set(o, 0.5, 0.5);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   elm_table_pack(inst->o_table, o, 0, 2, 1, 1);
   evas_object_show(o);

   evas_object_smart_callback_add(o, "clicked", _clock_settings_cb, inst);

   o = elm_icon_add(e_comp->elm);
   elm_icon_standard_set(o, "preferences-system");
   elm_object_content_set(o_button, o);
   evas_object_show(o);

   oi = elm_layout_add(inst->o_table);
   inst->o_cal = oi;
   e_theme_edje_object_set(oi, "base/theme/modules/clock",
                           "e/modules/clock/calendar");
   _clock_month_update(inst);

   elm_object_signal_callback_add(oi, "e,action,prev", "*",
                                   _clock_month_prev_cb, inst);
   elm_object_signal_callback_add(oi, "e,action,next", "*",
                                   _clock_month_next_cb, inst);
   edje_object_message_signal_process(elm_layout_edje_get(oi));
   elm_layout_sizing_eval(oi);
   elm_table_pack(inst->o_table, oi, 0, 1, 1, 1);
   evas_object_show(oi);

   evas_smart_objects_calculate(evas);
   e_gadcon_popup_content_set(inst->popup, inst->o_table);
   evas_event_thaw(evas);
   evas_event_thaw_eval(evas);
   e_gadcon_popup_show(inst->popup);
}

static void
_eval_instance_size(Instance *inst)
{
   Evas_Coord mw, mh, omw, omh;

   edje_object_size_min_get(inst->o_clock, &mw, &mh);
   omw = mw;
   omh = mh;

   if ((mw < 1) || (mh < 1))
     {
        Evas_Coord x, y, sw = 0, sh = 0, ow, oh;
        Eina_Bool horiz;
        const char *orient;

        switch (inst->gcc->gadcon->orient)
          {
           case E_GADCON_ORIENT_TOP:
           case E_GADCON_ORIENT_CORNER_TL:
           case E_GADCON_ORIENT_CORNER_TR:
           case E_GADCON_ORIENT_BOTTOM:
           case E_GADCON_ORIENT_CORNER_BL:
           case E_GADCON_ORIENT_CORNER_BR:
           case E_GADCON_ORIENT_HORIZ:
             horiz = EINA_TRUE;
             orient = "e,state,horizontal";
             break;

           case E_GADCON_ORIENT_LEFT:
           case E_GADCON_ORIENT_CORNER_LB:
           case E_GADCON_ORIENT_CORNER_LT:
           case E_GADCON_ORIENT_RIGHT:
           case E_GADCON_ORIENT_CORNER_RB:
           case E_GADCON_ORIENT_CORNER_RT:
           case E_GADCON_ORIENT_VERT:
             horiz = EINA_FALSE;
             orient = "e,state,vertical";
             break;

           default:
             horiz = EINA_TRUE;
             orient = "e,state,float";
          }

        if (inst->gcc->gadcon->shelf)
          {
             if (horiz)
               sh = inst->gcc->gadcon->shelf->h;
             else
               sw = inst->gcc->gadcon->shelf->w;
          }

        evas_object_geometry_get(inst->o_clock, NULL, NULL, &ow, &oh);
        if (orient)
          edje_object_signal_emit(inst->o_clock, orient, "e");
        evas_object_resize(inst->o_clock, sw, sh);
        edje_object_message_signal_process(inst->o_clock);

        edje_object_parts_extends_calc(inst->o_clock, &x, &y, &mw, &mh);
        evas_object_resize(inst->o_clock, ow, oh);
     }

   if (mw < 4) mw = 4;
   if (mh < 4) mh = 4;

   if (mw < omw) mw = omw;
   if (mh < omh) mh = omh;

   e_gadcon_client_aspect_set(inst->gcc, mw, mh);
   e_gadcon_client_min_size_set(inst->gcc, mw, mh);
}

void
e_int_clock_instances_redo(Eina_Bool all)
{
   Eina_List *l;
   Instance *inst;
   char todaystr[128];

   EINA_LIST_FOREACH(clock_instances, l, inst)
     {
        Evas_Object *o = inst->o_clock;

         if ((!all) && (!inst->cfg->changed)) continue;
        _todaystr_eval(inst, todaystr, sizeof(todaystr) - 1);
        if (inst->cfg->digital_clock)
          e_theme_edje_object_set(o, "base/theme/modules/clock",
                                  "e/modules/clock/digital");
        else
          e_theme_edje_object_set(o, "base/theme/modules/clock",
                                  "e/modules/clock/main");
        if (inst->cfg->show_date)
          edje_object_signal_emit(o, "e,state,date,on", "e");
        else
          edje_object_signal_emit(o, "e,state,date,off", "e");
        if (inst->cfg->digital_24h)
          edje_object_signal_emit(o, "e,state,24h,on", "e");
        else
          edje_object_signal_emit(o, "e,state,24h,off", "e");
        if (inst->cfg->show_seconds)
          edje_object_signal_emit(o, "e,state,seconds,on", "e");
        else
          edje_object_signal_emit(o, "e,state,seconds,off", "e");

        edje_object_part_text_set(o, "e.text.today", todaystr);
        edje_object_message_signal_process(o);
        _eval_instance_size(inst);

        if (inst->o_popclock)
          {
             o = inst->o_popclock;

             if (inst->cfg->digital_clock)
               e_theme_edje_object_set(o, "base/theme/modules/clock",
                                       "e/modules/clock/digital");
             else
               e_theme_edje_object_set(o, "base/theme/modules/clock",
                                       "e/modules/clock/main");
             if (inst->cfg->show_date)
               edje_object_signal_emit(o, "e,state,date,on", "e");
             else
               edje_object_signal_emit(o, "e,state,date,off", "e");
             if (inst->cfg->digital_24h)
               edje_object_signal_emit(o, "e,state,24h,on", "e");
             else
               edje_object_signal_emit(o, "e,state,24h,off", "e");
             if (inst->cfg->show_seconds)
               edje_object_signal_emit(o, "e,state,seconds,on", "e");
             else
               edje_object_signal_emit(o, "e,state,seconds,off", "e");

             edje_object_part_text_set(o, "e.text.today", todaystr);
             edje_object_message_signal_process(o);
          }
     }
}

static Eina_Bool
_update_today_timer(void *data EINA_UNUSED)
{
   time_t t, t_tomorrow;
   const struct tm *now;
   struct tm today;

   e_int_clock_instances_redo(EINA_TRUE);
   if (!clock_instances)
     {
        update_today = NULL;
        return EINA_FALSE;
     }

   t = time(NULL);
   now = localtime(&t);
   memcpy(&today, now, sizeof(today));
   today.tm_sec = 1;
   today.tm_min = 0;
   today.tm_hour = 0;

   t_tomorrow = mktime(&today) + 24 * 60 * 60;
   if (update_today) ecore_timer_interval_set(update_today, t_tomorrow - t);
   else update_today = ecore_timer_loop_add(t_tomorrow - t, _update_today_timer, NULL);
   return EINA_TRUE;
}

static void
_clock_popup_free(Instance *inst)
{
   if (!inst->popup) return;
   E_FREE_FUNC(inst->popup, e_object_del);
   inst->o_popclock = NULL;
}

static void
_clock_menu_cb_cfg(void *data, E_Menu *menu EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Instance *inst = data;

   E_FREE_FUNC(inst->popup, e_object_del);
   e_int_config_clock_module(NULL, inst->cfg);
}

static void
_clock_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->button == 1)
     {
        if (inst->popup) _clock_popup_free(inst);
        else _clock_popup_new(inst);
     }
   else if (ev->button == 3)
     {
        E_Zone *zone;
        E_Menu *m;
        E_Menu_Item *mi;
        int x, y;

        zone = e_zone_current_get();

        m = e_menu_new();

        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _clock_menu_cb_cfg, inst);

        m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);

        e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);
        e_menu_activate_mouse(m, zone, x + ev->output.x, y + ev->output.y,
                              1, 1, E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
        evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button,
                                 EVAS_BUTTON_NONE, ev->timestamp, NULL);
     }
}

static void
_clock_sizing_changed_cb(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   _eval_instance_size(data);
}

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Evas_Object *o;
   E_Gadcon_Client *gcc;
   Instance *inst;
   char todaystr[128];

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);

   _todaystr_eval(inst, todaystr, sizeof(todaystr) - 1);

   o = edje_object_add(gc->evas);
   edje_object_signal_callback_add(o, "e,state,sizing,changed", "*",
                                   _clock_sizing_changed_cb, inst);
   if (inst->cfg->digital_clock)
     e_theme_edje_object_set(o, "base/theme/modules/clock",
                             "e/modules/clock/digital");
   else
     e_theme_edje_object_set(o, "base/theme/modules/clock",
                             "e/modules/clock/main");
   if (inst->cfg->show_date)
     edje_object_signal_emit(o, "e,state,date,on", "e");
   else
     edje_object_signal_emit(o, "e,state,date,off", "e");
   if (inst->cfg->digital_24h)
     edje_object_signal_emit(o, "e,state,24h,on", "e");
   else
     edje_object_signal_emit(o, "e,state,24h,off", "e");
   if (inst->cfg->show_seconds)
     edje_object_signal_emit(o, "e,state,seconds,on", "e");
   else
     edje_object_signal_emit(o, "e,state,seconds,off", "e");

   edje_object_part_text_set(o, "e.text.today", todaystr);

   gcc = e_gadcon_client_new(gc, name, id, style, o);
   gcc->data = inst;

   inst->gcc = gcc;
   inst->o_clock = o;

   evas_object_event_callback_add(inst->o_clock,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _clock_cb_mouse_down,
                                  inst);
   edje_object_message_signal_process(o);
   evas_object_show(o);

   clock_instances = eina_list_append(clock_instances, inst);

   if (!update_today) _update_today_timer(NULL);

   return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;

   inst = gcc->data;
   clock_instances = eina_list_remove(clock_instances, inst);
   evas_object_del(inst->o_clock);
   _clock_popup_free(inst);
   _clear_timestrs(inst);
   free(inst);

   if ((!clock_instances) && (update_today))
     {
        ecore_timer_del(update_today);
        update_today = NULL;
     }
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   _eval_instance_size(gcc->data);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _("Clock");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   char buf[4096];

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-clock.edj",
            e_module_dir_get(clock_config->module));
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   Config_Item *ci = NULL;

   ci = _conf_item_get(NULL);
   return ci->id;
}

static Config_Item *
_conf_item_get(const char *id)
{
   Config_Item *ci;

   GADCON_CLIENT_CONFIG_GET(Config_Item, clock_config->items, _gadcon_class, id);

   ci = E_NEW(Config_Item, 1);
   ci->id = eina_stringshare_add(id);
   ci->weekend.start = 6;
   ci->weekend.len = 2;
   ci->week.start = 1;
   ci->digital_clock = 1;
   ci->digital_24h = 0;
   ci->show_seconds = 0;
   ci->show_date = 0;

   clock_config->items = eina_list_append(clock_config->items, ci);
   e_config_save_queue();

   return ci;
}

static void
_e_mod_action(const char *params)
{
   Eina_List *l;
   Instance *inst;

   if (!params) return;
   if (strcmp(params, "show_calendar")) return;

   EINA_LIST_FOREACH(clock_instances, l, inst)
     if (inst->popup)
       _clock_popup_free(inst);
     else
       _clock_popup_new(inst);
}

static void
_e_mod_action_cb_edge(E_Object *obj EINA_UNUSED, const char *params, E_Event_Zone_Edge *ev EINA_UNUSED)
{
   _e_mod_action(params);
}

static void
_e_mod_action_cb(E_Object *obj EINA_UNUSED, const char *params)
{
   _e_mod_action(params);
}

static void
_e_mod_action_cb_key(E_Object *obj EINA_UNUSED, const char *params, Ecore_Event_Key *ev EINA_UNUSED)
{
   _e_mod_action(params);
}

static Eina_Bool
_e_mod_action_cb_mouse(E_Object *obj EINA_UNUSED, const char *params, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   _e_mod_action(params);
   return EINA_TRUE;
}

static Eina_Bool
_clock_eio_update(void *d EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Eio_Monitor_Event *ev = event;

   if ((ev->monitor == clock_tz_monitor) ||
       (ev->monitor == clock_tz2_monitor) ||
       (ev->monitor == clock_tzetc_monitor))
     {
        if ((ev->filename) &&
            ((!strcmp(ev->filename, "/etc/localtime")) ||
             (!strcmp(ev->filename, "/etc/timezone"))))
          {
             e_int_clock_instances_redo(EINA_TRUE);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_clock_eio_error(void *d EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Eio_Monitor_Event *ev = event;

   if ((ev->monitor == clock_tz_monitor) ||
       (ev->monitor == clock_tz2_monitor) ||
       (ev->monitor == clock_tzetc_monitor))
     {
        if (clock_tz_monitor)
          {
             eio_monitor_del(clock_tz_monitor);
             clock_tz_monitor = NULL;
          }
        if (ecore_file_exists("/etc/localtime"))
          clock_tz_monitor = eio_monitor_add("/etc/localtime");

        if (clock_tz2_monitor)
          {
             eio_monitor_del(clock_tz2_monitor);
             clock_tz2_monitor = NULL;
          }
        if (ecore_file_exists("/etc/timezone"))
          clock_tz2_monitor = eio_monitor_add("/etc/timezone");
        if (clock_tzetc_monitor)
          {
             eio_monitor_del(clock_tzetc_monitor);
             clock_tzetc_monitor = NULL;
          }
        if (ecore_file_is_dir("/etc"))
          clock_tzetc_monitor = eio_monitor_add("/etc");
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_clock_time_update(void *d EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   e_int_clock_instances_redo(EINA_TRUE);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_clock_screensaver_on()
{
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(clock_instances, l, inst)
     {
        edje_object_signal_emit(inst->o_clock, "e,state,freeze", "e");
        if (inst->o_popclock)
          edje_object_signal_emit(inst->o_popclock, "e,state,freeze", "e");
     }
   E_FREE_FUNC(update_today, ecore_timer_del);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_clock_screensaver_off()
{
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(clock_instances, l, inst)
     {
        edje_object_signal_emit(inst->o_clock, "e,state,thaw", "e");
        if (inst->o_popclock)
          edje_object_signal_emit(inst->o_popclock, "e,state,thaw", "e");
     }
   if (clock_instances) _update_today_timer(NULL);
   return ECORE_CALLBACK_RENEW;
}

/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Clock"
};

E_API void *
e_modapi_init(E_Module *m)
{
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, weekend.start, INT);
   E_CONFIG_VAL(D, T, weekend.len, INT);
   E_CONFIG_VAL(D, T, week.start, INT);
   E_CONFIG_VAL(D, T, digital_clock, INT);
   E_CONFIG_VAL(D, T, digital_24h, INT);
   E_CONFIG_VAL(D, T, show_seconds, INT);
   E_CONFIG_VAL(D, T, show_date, INT);

   conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   clock_config = e_config_domain_load("module.clock", conf_edd);

   if (!clock_config)
     clock_config = E_NEW(Config, 1);

   act = e_action_add("clock");
   if (act)
     {
        act->func.go = _e_mod_action_cb;
        act->func.go_key = _e_mod_action_cb_key;
        act->func.go_mouse = _e_mod_action_cb_mouse;
        act->func.go_edge = _e_mod_action_cb_edge;

        e_action_predef_name_set(N_("Clock"), N_("Toggle calendar"), "clock", "show_calendar", NULL, 0);
     }

   clock_config->module = m;
   if (ecore_file_exists("/etc/localtime"))
     clock_tz_monitor = eio_monitor_add("/etc/localtime");
   if (ecore_file_exists("/etc/timezone"))
     clock_tz2_monitor = eio_monitor_add("/etc/timezone");
   if (ecore_file_is_dir("/etc"))
     clock_tzetc_monitor = eio_monitor_add("/etc");
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_ERROR, _clock_eio_error, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_FILE_CREATED, _clock_eio_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_FILE_MODIFIED, _clock_eio_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_FILE_DELETED, _clock_eio_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_SELF_DELETED, _clock_eio_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_SELF_RENAME, _clock_eio_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_SYS_RESUME, _clock_time_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_SYSTEM_TIMEDATE_CHANGED, _clock_time_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_SCREENSAVER_ON, _clock_screensaver_on, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_SCREENSAVER_OFF, _clock_screensaver_off, NULL);

   e_gadcon_provider_register(&_gadcon_class);

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
   if (act)
     {
        e_action_predef_name_del("Clock", "Toggle calendar");
        e_action_del("clock");
        act = NULL;
     }
   if (clock_config)
     {
        Config_Item *ci;

        if (clock_config->config_dialog)
          e_object_del(E_OBJECT(clock_config->config_dialog));

        EINA_LIST_FREE(clock_config->items, ci)
          {
             eina_stringshare_del(ci->id);
             free(ci);
          }

        free(clock_config);
        clock_config = NULL;
     }
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);
   conf_item_edd = NULL;
   conf_edd = NULL;

   e_gadcon_provider_unregister(&_gadcon_class);

   if (update_today)
     {
        ecore_timer_del(update_today);
        update_today = NULL;
     }
   if (clock_tz_monitor) eio_monitor_del(clock_tz_monitor);
   if (clock_tz2_monitor) eio_monitor_del(clock_tz2_monitor);
   if (clock_tzetc_monitor) eio_monitor_del(clock_tzetc_monitor);
   clock_tz_monitor = NULL;
   clock_tz2_monitor = NULL;
   clock_tzetc_monitor = NULL;

   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.clock", conf_edd, clock_config);
   return 1;
}

