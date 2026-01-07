#include "e_mod_main.h"

extern Mod_Inst     *clip_inst; // in e_mod_main.c

struct _E_Config_Dialog_Data
{
  E_Config_Dialog *cfd;
  Evas_Object *obj;

  // store some initial states of clipboard configuration we will need
  unsigned int init_label_length; // initial label length
  // actual options user can change
  int    clip_copy;      // clipboard to use
  int    clip_select;    // clipboard to use
  int    persistence;    // history file persistance
  int    hist_reverse;   // order to display History
  double hist_items;     // number of history items to store
  int    confirm_clear;  // display history confirmation dialog on deletion
  double label_length;   // number of characters of item to display
  int    ignore_ws;      // should we ignore White space in label
  int    ignore_ws_copy; // should we not copy White space only
  int    trim_ws;        // should we trim White space from selection
  int    trim_nl;        // should we trim new lines from selection
};

/////////////////////////////////////////////////////////////////////////////
//
static int           _basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static void         *_create_data(E_Config_Dialog *cfd EINA_UNUSED);
static int           _basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static void          _fill_data(E_Config_Dialog_Data *cfdata);
static void          _free_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static Evas_Object  *_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);

/////////////////////////////////////////////////////////////////////////////

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;

Config *clip_cfg = NULL;

/////////////////////////////////////////////////////////////////////////////

static void *
_create_data(E_Config_Dialog *cfd EINA_UNUSED)
{
  E_Config_Dialog_Data *cfdata = E_NEW(E_Config_Dialog_Data, 1);
  _fill_data(cfdata);
  return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
  EINA_SAFETY_ON_NULL_RETURN(clip_cfg);
  clip_cfg->config_dialog = NULL;
  E_FREE(cfdata);
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
  cfdata->init_label_length = clip_cfg->label_length;

  cfdata->clip_copy       = clip_cfg->clip_copy;
  cfdata->clip_select     = clip_cfg->clip_select;
  cfdata->hist_reverse    = clip_cfg->hist_reverse;
  cfdata->hist_items      = clip_cfg->hist_items;
  cfdata->label_length    = clip_cfg->label_length;
}

static int
_basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
  clip_cfg->clip_copy      = cfdata->clip_copy;
  clip_cfg->clip_select    = cfdata->clip_select;
  clip_cfg->hist_reverse   = cfdata->hist_reverse;
  // truncate hist list if needed
  if (clip_cfg->hist_items != (unsigned int)cfdata->hist_items)
    config_truncate_history(cfdata->hist_items);
  clip_cfg->hist_items     = cfdata->hist_items;
  // has clipboard label name length changed?
  if ((unsigned int)cfdata->label_length != cfdata->init_label_length)
    {
      clip_cfg->label_length_changed = EINA_TRUE;
      cfdata->init_label_length = cfdata->label_length;
    }
  clip_cfg->label_length   = cfdata->label_length;
  // now save configuration
  e_config_save_queue();
  return 1;
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
  Evas_Object *o;
  Evas_Object *ob;
  Evas_Object *of;

  o = e_widget_list_add(evas, 0, 0);
  // clipboard config section
  of = e_widget_framelist_add(evas, _("Clipboards"), 0);
  ob = e_widget_check_add(evas, _(" Use Copy (Ctrl-C)"), &(cfdata->clip_copy));
  e_widget_framelist_object_append(of, ob);

  ob = e_widget_check_add(evas, _(" Use Primary (Selection)"), &(cfdata->clip_select));
  e_widget_framelist_object_append(of, ob);

  e_widget_list_object_append(o, of, 1, 0, 0.5);

  // history config section
  of = e_widget_framelist_add(evas, _("History"), 0);

  ob = e_widget_check_add(evas, _(" Reverse Order"), &(cfdata->hist_reverse));
  e_widget_framelist_object_append(of, ob);

  ob = e_widget_label_add(evas, _(" Items in History"));
  e_widget_framelist_object_append(of, ob);
  ob = e_widget_slider_add(evas, 1, 0, "%2.0f", HIST_MIN, HIST_MAX, 1.0, 0, &(cfdata->hist_items), NULL, 40);
  e_widget_framelist_object_append(of, ob);

  e_widget_list_object_append(o, of, 1, 0, 0.5);

  // label config section
  ob = e_widget_label_add(evas, _(" Label Length"));
  e_widget_framelist_object_append(of, ob);
  ob = e_widget_slider_add(evas, 1, 0, "%2.0f", LABEL_MIN, LABEL_MAX, 1.0, 0, &(cfdata->label_length), NULL, 40);
  e_widget_framelist_object_append(of, ob);

  e_widget_list_object_append(o, of, 1, 0, 0.5);

  e_dialog_resizable_set(cfd->dia, EINA_TRUE);
  return o;
}

E_Config_Dialog *
config_clipboard_module(Evas_Object *parent EINA_UNUSED,
                        const char *params EINA_UNUSED)
{
  E_Config_Dialog *cfd;
  E_Config_Dialog_View *v;

  if (e_config_dialog_find("E", "settings/clipboard")) return NULL;
  v = E_NEW(E_Config_Dialog_View, 1);
  v->create_cfdata = _create_data;
  v->free_cfdata = _free_data;
  v->basic.create_widgets = _basic_create_widgets;
  v->basic.apply_cfdata = _basic_apply_data;
  v->basic.check_changed = _basic_check_changed;

  cfd = e_config_dialog_new(NULL, _("Clipboard Settings"),
                            "E", "preferences/clipboard",
                            "preferences-engine", 0, v, NULL);
  clip_cfg->config_dialog = cfd;
  return cfd;
}

static int
_basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
  if (clip_cfg->clip_copy      != cfdata->clip_copy) return 1;
  if (clip_cfg->clip_select    != cfdata->clip_select) return 1;
  if (clip_cfg->hist_reverse   != cfdata->hist_reverse) return 1;
  if (clip_cfg->hist_items     != (unsigned int)cfdata->hist_items) return 1;
  if (clip_cfg->label_length   != (unsigned int)cfdata->label_length) return 1;
  return 0;
}

/////////////////////////////////////////////////////////////////////////////

Eina_Bool
config_init(void)
{
  conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
  if (!conf_item_edd) return EINA_FALSE;
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
  E_CONFIG_VAL(D, T, str, STRI);
  conf_edd = E_CONFIG_DD_NEW("Config", Config);
  if (!conf_edd) return EINA_FALSE;
#undef T
#undef D
#define T Config
#define D conf_edd
  E_CONFIG_VAL(D, T, version, UINT);
  E_CONFIG_LIST(D, T, items, conf_item_edd);
  E_CONFIG_VAL(D, T, label_length, UINT);
  E_CONFIG_VAL(D, T, hist_items, UINT);
  E_CONFIG_VAL(D, T, clip_copy, UCHAR);
  E_CONFIG_VAL(D, T, clip_select, UCHAR);
  E_CONFIG_VAL(D, T, hist_reverse, UCHAR);

  clip_cfg = e_config_domain_load("module.clipboard", conf_edd);
  if (clip_cfg)
    { // check config version
      if (!e_util_module_config_check("Clipboard", clip_cfg->version,
                                      MOD_CONFIG_FILE_VERSION))
        config_free();
    }
  return EINA_TRUE;
}

void
config_shutdown(void)
{
  config_free();
  E_CONFIG_DD_FREE(conf_edd);
  E_CONFIG_DD_FREE(conf_item_edd);
}

Eina_Bool
conifg_new_limit(void)
{
  if (!clip_cfg)
    {
      clip_cfg = E_NEW(Config, 1);
      if (!clip_cfg) return EINA_FALSE;
      clip_cfg->label_length_changed = EINA_FALSE;
      clip_cfg->clip_copy      = 1;
      clip_cfg->clip_select    = 1;
      clip_cfg->hist_reverse   = 0;
      clip_cfg->hist_items     = 20;
      clip_cfg->label_length   = 50;
    }
  E_CONFIG_LIMIT(clip_cfg->hist_items, HIST_MIN, HIST_MAX);
  E_CONFIG_LIMIT(clip_cfg->label_length, LABEL_MIN, LABEL_MAX);
  E_CONFIG_LIMIT(clip_cfg->clip_copy, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->clip_select, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->hist_reverse, 0, 1);
  clip_cfg->version = MOD_CONFIG_FILE_VERSION;
  return EINA_TRUE;
}

void
config_free(void)
{
  Config_Item *ci;

  if (!clip_cfg) return;
  EINA_LIST_FREE(clip_cfg->items, ci)
    {
      eina_stringshare_del(ci->name);
      eina_stringshare_del(ci->str);
      free(ci);
    }
  clip_cfg->module = NULL;
  E_FREE(clip_cfg);
}

void
config_save(void)
{
  e_config_domain_save("module.clipboard", conf_edd, clip_cfg);
}

void
config_truncate_history(unsigned int max)
{
  EINA_SAFETY_ON_NULL_RETURN(clip_cfg);
  if ((clip_cfg->items) && (eina_list_count(clip_cfg->items) > max))
    {
      Eina_List *discard = NULL;
      Eina_List *last = eina_list_nth_list(clip_cfg->items, max - 1);
      clip_cfg->items = eina_list_split_list(clip_cfg->items, last, &discard);
      if (discard) E_FREE_LIST(discard, config_clip_data_free);
      e_config_save_queue();
    }
}

void
config_clip_data_free(Config_Item *cd)
{
  EINA_SAFETY_ON_NULL_RETURN(cd);
  free(cd->name);
  eina_stringshare_del(cd->str);
  free(cd);
}
