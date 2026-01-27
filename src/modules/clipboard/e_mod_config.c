#include "e_mod_main.h"

extern Mod mod;

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
Config *cfg = NULL;
/////////////////////////////////////////////////////////////////////////////
static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;
/////////////////////////////////////////////////////////////////////////////

static Eina_Bool
_empty(const char *s)
{
  // walk to first non-space char
  while ((isspace((unsigned char)*s)) && (*s++));
  // first non-empty char is NOT 0 byte (end of str) thus not empty
  if (s[0]) return EINA_FALSE;
  return EINA_TRUE;
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
  cfdata->init_label_length = cfg->label_length;
  cfdata->clip_copy         = cfg->clip_copy;
  cfdata->clip_select       = cfg->clip_select;
  cfdata->hist_reverse      = cfg->hist_reverse;
  cfdata->hist_items        = cfg->hist_items;
  cfdata->label_length      = cfg->label_length;
}

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
  EINA_SAFETY_ON_NULL_RETURN(cfg);
  cfg->config_dialog = NULL;
  E_FREE(cfdata);
}

static int
_basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
  cfg->clip_copy      = cfdata->clip_copy;
  cfg->clip_select    = cfdata->clip_select;
  cfg->hist_reverse   = cfdata->hist_reverse;
  // truncate hist list if needed
  if (cfg->hist_items != (unsigned int)cfdata->hist_items) config_hist_limit();
  cfg->hist_items     = cfdata->hist_items;
  // has clipboard label name length changed?
  if ((unsigned int)cfdata->label_length != cfdata->init_label_length)
    {
      cfg->label_length_changed = EINA_TRUE;
      cfdata->init_label_length = cfdata->label_length;
    }
  cfg->label_length   = cfdata->label_length;
  // now save configuration
  e_config_save_queue();
  return 1;
}

static int
_basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
  if (cfg->clip_copy    != cfdata->clip_copy) return 1;
  if (cfg->clip_select  != cfdata->clip_select) return 1;
  if (cfg->hist_reverse != cfdata->hist_reverse) return 1;
  if (cfg->hist_items   != (unsigned int)cfdata->hist_items) return 1;
  if (cfg->label_length != (unsigned int)cfdata->label_length) return 1;
  return 0;
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{ // XXX: move to elm?
  Evas_Object *o, *ob, *of;

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
  if (!v) return NULL;
  v->create_cfdata = _create_data;
  v->free_cfdata = _free_data;
  v->basic.create_widgets = _basic_create_widgets;
  v->basic.apply_cfdata = _basic_apply_data;
  v->basic.check_changed = _basic_check_changed;
  cfd = e_config_dialog_new(NULL, _("Clipboard Settings"),
                            "E", "preferences/clipboard",
                            "preferences-engine", 0, v, NULL);
  cfg->config_dialog = cfd;
  return cfd;
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

  cfg = e_config_domain_load("module.clipboard", conf_edd);
  if (cfg)
    { // check config version
      if (!e_util_module_config_check("Clipboard", cfg->version,
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
  if (!cfg)
    {
      cfg = E_NEW(Config, 1);
      if (!cfg) return EINA_FALSE;
      cfg->label_length_changed = EINA_FALSE;
      cfg->clip_copy      = 1;
      cfg->clip_select    = 1;
      cfg->hist_reverse   = 0;
      cfg->hist_items     = 10;
      cfg->label_length   = 50;
    }
  E_CONFIG_LIMIT(cfg->hist_items, HIST_MIN, HIST_MAX);
  E_CONFIG_LIMIT(cfg->label_length, LABEL_MIN, LABEL_MAX);
  E_CONFIG_LIMIT(cfg->clip_copy, 0, 1);
  E_CONFIG_LIMIT(cfg->clip_select, 0, 1);
  E_CONFIG_LIMIT(cfg->hist_reverse, 0, 1);
  cfg->version = MOD_CONFIG_FILE_VERSION;
  return EINA_TRUE;
}

void
config_free(void)
{
  Config_Item *cd;

  if (!cfg) return;
  EINA_LIST_FREE(cfg->items, cd) config_clip_data_free(cd);
  E_FREE(cfg);
}

void
config_save(void)
{
  e_config_domain_save("module.clipboard", conf_edd, cfg);
}

void
config_hist_limit(void)
{
  if (!cfg) return;
  if ((cfg->items) && (eina_list_count(cfg->items) > cfg->hist_items))
    {
      Eina_List *discard = NULL;
      Eina_List *last = eina_list_nth_list(cfg->items, cfg->hist_items - 1);
      cfg->items = eina_list_split_list(cfg->items, last, &discard);
      if (discard) E_FREE_LIST(discard, config_clip_data_free);
      e_config_save_queue();
    }
}

void
config_clip_data_free(Config_Item *cd)
{
  EINA_SAFETY_ON_NULL_RETURN(cd);
  eina_stringshare_del(cd->str);
  free(cd);
}

void
config_paste_add(const char *data, size_t size, int type)
{
  Config_Item *cd = NULL;
  const char *last = "";
  Eina_List *l;

  if (!data) return;
  if (cfg->items) last = ((Config_Item *)eina_list_data_get(cfg->items))->str;
  if (!strcmp(last, data)) return; // same as last one we added
  if (_empty(data)) return; // empty - we don't want it
  EINA_LIST_FOREACH(cfg->items, l, cd)
    {
      if (!strcmp(data, cd->str))
        {// promote to last - already there
          cfg->items = eina_list_promote_list(cfg->items, l);
          return;
        }
    }
  // not there already
  cd = E_NEW(Config_Item, 1);
  if (!cd) return;
  // XXX: if we select huge amounts of text this could use a lot of ram
  cd->str = eina_stringshare_add(data);
  cfg->items = eina_list_prepend(cfg->items, cd);
  config_hist_limit();
}
