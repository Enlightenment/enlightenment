#include "e.h"
#include "e_mod_main.h"

struct _E_Config_Dialog_Data
{
  E_Config_Dialog *cfd;
  double check_interval;
  int    power_lo;
  int    power_hi;
};

/* Protos */
static void        *_create_data(E_Config_Dialog *cfd);
static void         _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int          _basic_apply_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int          _basic_check_changed(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);

E_Config_Dialog *
e_int_config_cpufreq_module(Evas_Object *parent EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   v = E_NEW(E_Config_Dialog_View, 1);
   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply_data;
   v->basic.create_widgets = _basic_create_widgets;
   v->basic.check_changed = _basic_check_changed;

   cfd = e_config_dialog_new(NULL, _("Cpu Frequency Control Settings"),
                             "E", "_e_mod_cpufreq_config_dialog",
                             "preferences-cpu-speed", 0, v, NULL);
   cpufreq_config->config_dialog = cfd;
   return cfd;
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
   if (!cpufreq_config) return;
   cfdata->check_interval   = cpufreq_config->check_interval;
   cfdata->power_hi         = cpufreq_config->power_hi;
   cfdata->power_lo         = cpufreq_config->power_lo;
}

static void *
_create_data(E_Config_Dialog *cfd EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   _fill_data(cfdata);
   return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   if (!cpufreq_config) return;
   cpufreq_config->config_dialog = NULL;
   E_FREE(cfdata);
}

static char *
_cb_unit_func(double v)
{
  if (v < (1.0 / 4.0)) return strdup(_("Powersave"));
  else if (v < (2.0 / 4.0)) return strdup(_("Balanced Low"));
  else if (v < (3.0 / 4.0)) return strdup(_("Balanced Hi"));
  else return strdup(_("Performance"));
}

static void
_handle_slider_step(Evas_Object *o, int *val)
{
  double v = elm_slider_value_get(o);

  if (v < (1.0 / 4.0))
    {
      elm_slider_value_set(o, (0.0 / 3.0));
      *val = 0;
    }
  else if (v < (2.0 / 4.0))
    {
      elm_slider_value_set(o, (1.0 / 3.0));
      *val = 33;
    }
  else if (v < (3.0 / 4.0))
    {
      elm_slider_value_set(o, (2.0 / 3.0));
      *val = 67;
    }
  else
    {
      elm_slider_value_set(o, (3.0 / 3.0));
      *val = 100;
    }
}

static void
_cb_power_lo_slider(void *data, Evas_Object *o, void *info EINA_UNUSED)
{
  E_Config_Dialog_Data *cfdata = data;
  _handle_slider_step(o, &cfdata->power_lo);
  e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_power_hi_slider(void *data, Evas_Object *o, void *info EINA_UNUSED)
{
  E_Config_Dialog_Data *cfdata = data;
  _handle_slider_step(o, &cfdata->power_hi);
  e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static void
_cb_interval_slider(void *data, Evas_Object *o, void *info EINA_UNUSED)
{
  E_Config_Dialog_Data *cfdata = data;
  cfdata->check_interval       = elm_slider_value_get(o);
  e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas EINA_UNUSED,
                      E_Config_Dialog_Data *cfdata)
{
  Evas_Object *win = cfd->dia->win;
  Evas_Object *o, *o_table;
  int          lv;

  o_table = o = elm_table_add(e_comp->elm);

  o = elm_icon_add(win);
  elm_icon_standard_set(o, "power-plug");
  evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
  elm_table_pack(o_table, o, 0, 0, 1, 1);
  evas_object_show(o);

  o = elm_label_add(win);
  elm_object_text_set(o, _("High Power Level"));
  evas_object_size_hint_align_set(o, 0.0, 0.5);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
  elm_table_pack(o_table, o, 1, 0, 9, 1);
  evas_object_show(o);

  o = elm_slider_add(win);
  elm_slider_span_size_set(o, ELM_SCALE_SIZE(160));
  elm_slider_horizontal_set(o, EINA_TRUE);
  elm_slider_min_max_set(o, 0, 1);
  elm_slider_step_set(o, (1.0 / 4.0));
  elm_slider_indicator_show_set(o, EINA_FALSE);
  elm_slider_units_format_function_set(o, _cb_unit_func, NULL);
  evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
  elm_table_pack(o_table, o, 0, 1, 10, 1);
  evas_object_show(o);

  lv = cfdata->power_lo;
  if (lv < 33) elm_slider_value_set(o, (0.0 / 3.0));
  else if (lv < 67) elm_slider_value_set(o, (1.0 / 3.0));
  else if (lv < 100) elm_slider_value_set(o, (2.0 / 3.0));
  else elm_slider_value_set(o, (3.0 / 3.0));

  evas_object_smart_callback_add(o, "changed", _cb_power_lo_slider, cfdata);

  o = elm_icon_add(win);
  elm_icon_standard_set(o, "battery");
  evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
  elm_table_pack(o_table, o, 0, 2, 1, 1);
  evas_object_show(o);

  o = elm_label_add(win);
  elm_object_text_set(o, _("Low Power Level"));
  evas_object_size_hint_align_set(o, 0.0, 0.5);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
  elm_table_pack(o_table, o, 1, 2, 9, 1);
  evas_object_show(o);

  o = elm_slider_add(win);
  elm_slider_span_size_set(o, ELM_SCALE_SIZE(160));
  elm_slider_horizontal_set(o, EINA_TRUE);
  elm_slider_min_max_set(o, 0, 1);
  elm_slider_step_set(o, (1.0 / 4.0));
  elm_slider_indicator_show_set(o, EINA_FALSE);
  elm_slider_units_format_function_set(o, _cb_unit_func, NULL);
  evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
  elm_table_pack(o_table, o, 0, 3, 10, 1);
  evas_object_show(o);

  lv = cfdata->power_hi;
  if (lv < 33) elm_slider_value_set(o, (0.0 / 3.0));
  else if (lv < 67) elm_slider_value_set(o, (1.0 / 3.0));
  else if (lv < 100) elm_slider_value_set(o, (2.0 / 3.0));
  else elm_slider_value_set(o, (3.0 / 3.0));

  evas_object_smart_callback_add(o, "changed", _cb_power_hi_slider, cfdata);

  o = elm_icon_add(win);
  elm_icon_standard_set(o, "clock");
  evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
  elm_table_pack(o_table, o, 0, 4, 1, 1);
  evas_object_show(o);

  o = elm_label_add(win);
  elm_object_text_set(o, _("Update Interval"));
  evas_object_size_hint_align_set(o, 0.0, 0.5);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
  elm_table_pack(o_table, o, 1, 4, 9, 1);
  evas_object_show(o);

  o = elm_slider_add(win);
  elm_slider_span_size_set(o, ELM_SCALE_SIZE(160));
  elm_slider_horizontal_set(o, EINA_TRUE);
  elm_slider_min_max_set(o, 0.1, 1.0);
  elm_slider_step_set(o, 0.1);
  elm_slider_indicator_show_set(o, EINA_FALSE);
  elm_slider_unit_format_set(o, "%1.1f sec");
  evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
  elm_table_pack(o_table, o, 0, 5, 10, 1);
  evas_object_show(o);

  elm_slider_value_set(o, cfdata->check_interval);

  evas_object_smart_callback_add(o, "changed", _cb_interval_slider, cfdata);
  cfdata->cfd = cfd;

  return o_table;
}

static int
_basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
  if (!cpufreq_config) return 0;
  cpufreq_config->check_interval = cfdata->check_interval;
  cpufreq_config->power_hi       = cfdata->power_hi;
  cpufreq_config->power_lo       = cfdata->power_lo;
  cpf_poll_time_set(cpufreq_config->check_interval);
  e_config_save_queue();
  return 1;
}

static int
_basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata EINA_UNUSED)
{
  if ((fabs(cfdata->check_interval - cpufreq_config->check_interval) > 0.01)
      || (cfdata->power_lo != cpufreq_config->power_lo)
      || (cfdata->power_hi != cpufreq_config->power_hi))
    return 1;
  return 0;
}
