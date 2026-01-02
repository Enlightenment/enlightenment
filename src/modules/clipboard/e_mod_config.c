#include "e_mod_main.h"

struct _E_Config_Dialog_Data
{
  E_Config_Dialog *cfd;
  Evas_Object *obj;

  /* Store some initial states of clipboard configuration we will need    */

  double init_label_length;  /* Initial label length.                     */
  /* Actual options user can change */
  int   clip_copy;      /* Clipboard to use                                */
  int   clip_select;    /* Clipboard to use                                */
  int   persistence;    /* History file persistance                        */
  int   hist_reverse;   /* Order to display History                        */
  double hist_items;    /* Number of history items to store                */
  int   confirm_clear;  /* Display history confirmation dialog on deletion */
  double label_length;  /* Number of characters of item to display         */
  int   ignore_ws;      /* Should we ignore White space in label           */
  int   ignore_ws_copy; /* Should we not copy White space only             */
  int   trim_ws;        /* Should we trim White space from selection       */
  int   trim_nl;        /* Should we trim new lines from selection         */
};

static int           _basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static void         *_create_data(E_Config_Dialog *cfd EINA_UNUSED);
static int           _basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static void          _fill_data(E_Config_Dialog_Data *cfdata);
void                 _free_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata);
static Evas_Object  *_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
extern Mod_Inst     *clip_inst; /* Found in e_mod_main.c */

static void *
_create_data(E_Config_Dialog *cfd EINA_UNUSED)
{
  E_Config_Dialog_Data *cfdata = E_NEW(E_Config_Dialog_Data, 1);
  _fill_data(cfdata);
  return cfdata;
}

void
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
  cfdata->persistence     = clip_cfg->persistence;
  cfdata->hist_reverse    = clip_cfg->hist_reverse;
  cfdata->hist_items      = clip_cfg->hist_items;
  cfdata->confirm_clear   = clip_cfg->confirm_clear;
  cfdata->label_length    = clip_cfg->label_length;
  cfdata->ignore_ws       = clip_cfg->ignore_ws;
  cfdata->ignore_ws_copy  = clip_cfg->ignore_ws_copy;
  cfdata->trim_ws         = clip_cfg->trim_ws;
  cfdata->trim_nl         = clip_cfg->trim_nl;
}

static int
_basic_apply_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
  clip_cfg->clip_copy      = cfdata->clip_copy;
  clip_cfg->clip_select    = cfdata->clip_select;
  clip_cfg->persistence    = cfdata->persistence;
  clip_cfg->hist_reverse   = cfdata->hist_reverse;
  
  /* Do we need to Truncate our history list? */
  if (clip_cfg->hist_items != cfdata->hist_items)
    truncate_history(cfdata-> hist_items);
    
  clip_cfg->hist_items     = cfdata->hist_items;
  clip_cfg->confirm_clear  = cfdata->confirm_clear;
  
  /* Has clipboard label name length changed ? */
  if (cfdata->label_length != cfdata->init_label_length) {
    clip_cfg->label_length_changed = EINA_TRUE;
    cfdata->init_label_length = cfdata->label_length;
  }
  clip_cfg->label_length   = cfdata->label_length;

  clip_cfg->ignore_ws      = cfdata->ignore_ws;
  clip_cfg->ignore_ws_copy = cfdata->ignore_ws_copy;
  clip_cfg->trim_ws        = cfdata->trim_ws;
  clip_cfg->trim_nl        = cfdata->trim_nl;

  /* Now save configuration */
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
  /* Clipboard Config Section     */
  of = e_widget_framelist_add(evas, _("Clipboards"), 0);
  ob = e_widget_check_add(evas, _(" Use Copy (Ctrl-C)"), &(cfdata->clip_copy));
  e_widget_framelist_object_append(of, ob);

  ob = e_widget_check_add(evas, _(" Use Primary (Selection)"), &(cfdata->clip_select));
  e_widget_framelist_object_append(of, ob);

  e_widget_list_object_append(o, of, 1, 0, 0.5);
  
  /* History Config Section       */
  of = e_widget_framelist_add(evas, _("History"), 0);
  ob = e_widget_check_add(evas, _(" Save History"), &(cfdata->persistence));
  e_widget_framelist_object_append(of, ob);

  ob = e_widget_check_add(evas, _(" Reverse Order"), &(cfdata->hist_reverse));
  e_widget_framelist_object_append(of, ob);
  
  ob = e_widget_check_add(evas, _(" Confirm before Clearing"), &(cfdata->confirm_clear));
  e_widget_framelist_object_append(of, ob);

  ob = e_widget_label_add(evas, _(" Items in History"));
  e_widget_framelist_object_append(of, ob);
  ob = e_widget_slider_add(evas, 1, 0, "%2.0f", HIST_MIN, HIST_MAX, 1.0, 0, &(cfdata->hist_items), NULL, 40);
  e_widget_framelist_object_append(of, ob);

  e_widget_list_object_append(o, of, 1, 0, 0.5);
  
  /* Label Config Section       */
  of = e_widget_framelist_add(evas, _("Labels"), 0);
  ob = e_widget_check_add(evas, _(" Ignore Whitespace"), &(cfdata->ignore_ws));
  e_widget_framelist_object_append(of, ob);
  
  ob = e_widget_label_add(evas, _(" Label Length"));
  e_widget_framelist_object_append(of, ob);
  ob = e_widget_slider_add(evas, 1, 0, "%2.0f", LABEL_MIN, LABEL_MAX, 1.0, 0, &(cfdata->label_length), NULL, 40);
  e_widget_framelist_object_append(of, ob);
  
  e_widget_list_object_append(o, of, 1, 0, 0.5);
  
  /* Content Config Section */
  of = e_widget_framelist_add(evas, _("Content"), 0);
  ob = e_widget_check_add(evas, _(" Ignore Whitespace"), &(cfdata->ignore_ws_copy));
  e_widget_framelist_object_append(of, ob);
  
  ob = e_widget_check_add(evas, _(" Trim Whitespace"), &(cfdata->trim_ws));
  e_widget_framelist_object_append(of, ob);

  ob = e_widget_check_add(evas, _(" Trim Newlines"), &(cfdata->trim_nl));
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

  if(e_config_dialog_find("E", "settings/clipboard")) return NULL;
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
  if (clip_cfg->persistence    != cfdata->persistence) return 1;
  if (clip_cfg->hist_reverse   != cfdata->hist_reverse) return 1;
  if (clip_cfg->hist_items     != cfdata->hist_items) return 1;
  if (clip_cfg->confirm_clear  != cfdata->confirm_clear) return 1;  
  if (clip_cfg->label_length   != cfdata->label_length) return 1;
  if (clip_cfg->ignore_ws      != cfdata->ignore_ws) return 1;
  if (clip_cfg->ignore_ws_copy != cfdata->ignore_ws_copy) return 1;
  if (clip_cfg->trim_ws        != cfdata->trim_ws) return 1;
  if (clip_cfg->trim_nl        != cfdata->trim_nl) return 1;

  return 0;
}

Eet_Error
truncate_history(const unsigned int n)
{
  Eet_Error err = EET_ERROR_NONE;

  EINA_SAFETY_ON_NULL_RETURN_VAL(clip_inst, EET_ERROR_BAD_OBJECT);
  if (clip_inst->items) {
    if (eina_list_count(clip_inst->items) > n) {
      Eina_List *last;
      Eina_List *discard;
      last = eina_list_nth_list(clip_inst->items, n-1);
      clip_inst->items = eina_list_split_list(clip_inst->items, last, &discard);
      if (discard)
        E_FREE_LIST(discard, free_clip_data);
      err = clip_save(clip_inst->items);
    }
  }
  else
    err = EET_ERROR_EMPTY;
  return err;
}
