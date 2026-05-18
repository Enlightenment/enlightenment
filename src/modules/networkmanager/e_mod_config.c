#include "e.h"
#include "e_mod_main.h"

typedef struct _E_Config_Dialog_Data
{
   E_Config_Dialog *cfd;
   double           poll_time;
} E_Config_Dialog_Data;

static void *_create_data(E_Config_Dialog *cfd);
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas,
                                          E_Config_Dialog_Data *cfdata);
static int _basic_apply_data(E_Config_Dialog *cfd,
                             E_Config_Dialog_Data *cfdata);
static int _basic_check_changed(E_Config_Dialog *cfd,
                                E_Config_Dialog_Data *cfdata);

E_Config_Dialog *
e_int_config_networkmanager_module(Evas_Object *parent EINA_UNUSED,
                                   const char *params EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   if (!networkmanager_config) return NULL;
   if (networkmanager_config->config_dialog)
     return networkmanager_config->config_dialog;

   v = E_NEW(E_Config_Dialog_View, 1);
   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply_data;
   v->basic.create_widgets = _basic_create_widgets;
   v->basic.check_changed = _basic_check_changed;

   cfd = e_config_dialog_new(NULL, _("NetworkManager Settings"),
                             "E", "_e_mod_networkmanager_config_dialog",
                             "preferences-network", 0, v, NULL);
   networkmanager_config->config_dialog = cfd;
   return cfd;
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
   if (!networkmanager_config) return;
   cfdata->poll_time = networkmanager_config->poll_time;
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
   if (networkmanager_config) networkmanager_config->config_dialog = NULL;
   E_FREE(cfdata);
}

static void
_cb_interval_slider(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata = data;

   cfdata->poll_time = elm_slider_value_get(obj);
   e_config_dialog_changed_set(cfdata->cfd, EINA_TRUE);
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas EINA_UNUSED,
                      E_Config_Dialog_Data *cfdata)
{
   Evas_Object *win, *table, *obj;

   win = cfd->dia->win;
   table = elm_table_add(e_comp->elm);

   obj = elm_icon_add(win);
   elm_icon_standard_set(obj, "clock");
   evas_object_size_hint_min_set(obj, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   elm_table_pack(table, obj, 0, 0, 1, 1);
   evas_object_show(obj);

   obj = elm_label_add(win);
   elm_object_text_set(obj, _("Update Interval"));
   evas_object_size_hint_align_set(obj, 0.0, 0.5);
   evas_object_size_hint_weight_set(obj, EVAS_HINT_EXPAND, 0.0);
   elm_table_pack(table, obj, 1, 0, 9, 1);
   evas_object_show(obj);

   obj = elm_slider_add(win);
   elm_slider_span_size_set(obj, ELM_SCALE_SIZE(160));
   elm_slider_horizontal_set(obj, EINA_TRUE);
   elm_slider_min_max_set(obj, 0.1, 10.0);
   elm_slider_step_set(obj, 0.1);
   elm_slider_indicator_show_set(obj, EINA_FALSE);
   elm_slider_unit_format_set(obj, "%1.1f sec");
   evas_object_size_hint_align_set(obj, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(obj, EVAS_HINT_EXPAND, 0.0);
   elm_table_pack(table, obj, 0, 1, 10, 1);
   elm_slider_value_set(obj, cfdata->poll_time);
   evas_object_smart_callback_add(obj, "changed", _cb_interval_slider, cfdata);
   evas_object_show(obj);

   cfdata->cfd = cfd;
   return table;
}

static int
_basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED,
                  E_Config_Dialog_Data *cfdata)
{
   if (!networkmanager_config) return 0;

   networkmanager_config->poll_time = cfdata->poll_time;
   enm_config_poll_time_set(networkmanager_config->poll_time);
   e_config_save_queue();
   return 1;
}

static int
_basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED,
                     E_Config_Dialog_Data *cfdata)
{
   if (!networkmanager_config) return 0;
   return (fabs(cfdata->poll_time - networkmanager_config->poll_time) > 0.01);
}
