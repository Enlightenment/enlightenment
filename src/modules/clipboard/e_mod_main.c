#include "e_mod_main.h"
#include "config_defaults.h"
#include "history.h"

/* Stuff for convenience to compress code */
#define CLIP_TRIM_MODE(x) (x->trim_nl + 2 * (x->trim_ws))
#define MOUSE_BUTTON ECORE_EVENT_MOUSE_BUTTON_DOWN
typedef Evas_Event_Mouse_Down Mouse_Event;

/* gadcon requirements */
static     Evas_Object  *_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas * evas);
static const char       *_gc_id_new(const E_Gadcon_Client_Class *client_class);
static E_Gadcon_Client  *_gc_init(E_Gadcon * gc, const char *name, const char *id, const char *style);
static void              _gc_orient(E_Gadcon_Client * gcc, E_Gadcon_Orient orient EINA_UNUSED);
static const char       *_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED);
static void              _gc_shutdown(E_Gadcon_Client * gcc);

/* Define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class = {
   GADCON_CLIENT_CLASS_VERSION,
   "clipboard",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

/* Set the version and the name IN the code (not just the .desktop file)
 * but more specifically the api version it was compiled for so E can skip
 * modules that are compiled for an incorrect API version safely */
E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Clipboard"};

/* actual module specifics   */
Config *clip_cfg = NULL;
static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;
Mod_Inst *clip_inst = NULL; /* Need by e_mod_config.c */
static E_Action *act = NULL;
static Ecore_Timer *delay_sel_timer = NULL;

/*   First some call backs   */
static void       _clipboard_cb_paste_item(void *d1, void *d2);
static void       _cb_menu_post_deactivate(void *data, E_Menu *menu EINA_UNUSED);
static void       _cb_context_show(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, Mouse_Event *event);
static void       _cb_clear_history(void *d1, void *d2 EINA_UNUSED);
static void       _cb_dialog_delete(void *data EINA_UNUSED);
static void       _cb_dialog_keep(void *data EINA_UNUSED);
static void       _cb_action_switch(E_Object *o EINA_UNUSED, const char *params, Instance *data, Evas *evas, Evas_Object *obj, Mouse_Event *event);

static void       _cb_config_show(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED);
static void       _clipboard_config_show(void *d1, void *d2 EINA_UNUSED);
static void       _clipboard_popup_free(Instance *inst);

/*   And then some auxillary functions */
static void       _clip_config_new(E_Module *m);
static void       _clip_config_free(void);
static void       _clip_inst_free(Instance *inst);
static void       _clip_add_item(Clip_Data *clip_data);
static void       _clipboard_popup_new(Instance *inst);
static void       _clear_history(void);
static Eina_List *_item_in_history(Clip_Data *cd);
static int        _clip_compare(Clip_Data *cd, char *text);

/* new module needs a new config :), or config too old and we need one anyway */
static void
_clip_config_new(E_Module *m)
{
  /* setup defaults */
  if (!clip_cfg)
    {
      clip_cfg = E_NEW(Config, 1);

      clip_cfg->label_length_changed = EINA_FALSE;

      clip_cfg->clip_copy      = CF_DEFAULT_COPY;
      clip_cfg->clip_select    = CF_DEFAULT_SELECT;
      clip_cfg->persistence    = CF_DEFAULT_PERSISTANCE;
      clip_cfg->hist_reverse   = CF_DEFAULT_HIST_REVERSE;
      clip_cfg->hist_items     = CF_DEFAULT_HIST_ITEMS;
      clip_cfg->confirm_clear  = CF_DEFAULT_CONFIRM;
      clip_cfg->label_length   = CF_DEFAULT_LABEL_LENGTH;
      clip_cfg->ignore_ws      = CF_DEFAULT_IGNORE_WS;
      clip_cfg->ignore_ws_copy = CF_DEFAULT_IGNORE_WS_COPY;
      clip_cfg->trim_ws        = CF_DEFAULT_WS;
      clip_cfg->trim_nl        = CF_DEFAULT_NL;
    }
  E_CONFIG_LIMIT(clip_cfg->hist_items, HIST_MIN, HIST_MAX);
  E_CONFIG_LIMIT(clip_cfg->label_length, LABEL_MIN, LABEL_MAX);
  E_CONFIG_LIMIT(clip_cfg->clip_copy, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->clip_select, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->persistence, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->hist_reverse, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->confirm_clear, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->ignore_ws, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->ignore_ws_copy, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->trim_ws, 0, 1);
  E_CONFIG_LIMIT(clip_cfg->trim_nl, 0, 1);


  /* update the version */
  clip_cfg->version = MOD_CONFIG_FILE_VERSION;

  clip_cfg->module = m;
  /* save the config to disk */
  e_config_save_queue();
}

/* This is called when we need to cleanup the actual configuration,
 * for example when our configuration is too old */
static void
_clip_config_free(void)
{
  Config_Item *ci;

  EINA_LIST_FREE(clip_cfg->items, ci)
    {
      eina_stringshare_del(ci->id);
      free(ci);
    }
  clip_cfg->module = NULL;
  E_FREE(clip_cfg);
}

static void
_clipboard_cb_mouse_down(void *data,
                         Evas *evas EINA_UNUSED,
                         Evas_Object *obj EINA_UNUSED,
                         void *event)
{
  Instance *inst = data;
  Evas_Event_Mouse_Down *ev = event;

  if (ev->button == 1)
    {
      if (inst->popup) _clipboard_popup_free(inst);
      else _clipboard_popup_new(inst);
    }
  else if (ev->button == 3) _cb_context_show(data, NULL, NULL, event);
}

/*
 * This function is called when you add the Module to a Shelf or Gadgets,
 *   this is where you want to add functions to do things.
 */
static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
  Instance *inst = NULL;
  Evas_Object *o;
  E_Gadcon_Client *gcc;

  inst = E_NEW(Instance, 1);
  if (!inst) return NULL;

  o = e_icon_add(gc->evas);
  e_icon_fdo_icon_set(o, "edit-paste");
  evas_object_show(o);

  gcc = e_gadcon_client_new(gc, name, id, style, o);
  gcc->data = inst;
  inst->gcc = gcc;
  inst->o_button = o;
  e_gadcon_client_util_menu_attach(gcc);
  evas_object_event_callback_add(inst->o_button,
                                 EVAS_CALLBACK_MOUSE_DOWN,
                                 _clipboard_cb_mouse_down,
                                 inst);
  return gcc;
}

/*
 * This function is called when you remove the Module from a Shelf or Gadgets,
 * what this function really does is clean up, it removes everything the module
 * displays
 */
static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst = gcc->data;
  _clipboard_popup_free(inst);
  _clip_inst_free(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
  e_gadcon_client_aspect_set (gcc, 16, 16);
  e_gadcon_client_min_size_set (gcc, 16, 16);
}

/*
 * This function sets the Gadcon name of the module,
 *  do not confuse this with E_Module_Api
 */
static const char *
_gc_label (const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
  return "Clipboard";
}

/*
 * This functions sets the Gadcon icon, the icon you see when you go to add
 * the module to a Shelf or Gadgets.
 */
static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas * evas)
{
  Evas_Object *o = e_icon_add(evas);
  e_icon_fdo_icon_set(o, "edit-paste");
  return o;
}

/*
 * This function sets the id for the module, so it is unique from other
 * modules
 */
static const char *
_gc_id_new (const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
  return _gadcon_class.name;
}

static void
_cb_context_show(void *data,
                 Evas *evas EINA_UNUSED,
                 Evas_Object *obj EINA_UNUSED,
                 Mouse_Event *event)
{
  Instance *inst = data;
  Evas_Coord x;
  Evas_Coord y;
  E_Menu *m;
  E_Menu_Item *mi;

  EINA_SAFETY_ON_NULL_RETURN(inst);
  EINA_SAFETY_ON_NULL_RETURN(event);
  // ignore all mouse events but left clicks
  IF_TRUE_RETURN(event->button != 3);

  // create popup menu
  m = e_menu_new();
  mi = e_menu_item_new(m);
  e_menu_item_label_set(mi, _("Settings"));
  e_util_menu_item_theme_icon_set(mi, "preferences-system");
  e_menu_item_callback_set(mi, _cb_config_show, inst);

  // each gadget client has a utility menu from the container
  m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);
  e_menu_post_deactivate_callback_set(m, _cb_menu_post_deactivate, inst);

  e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);
  // show the menu relative to gadgets position 
  e_menu_activate_mouse(m, e_zone_current_get(),
                        (x + event->output.x),
                        (y + event->output.y), 1, 1,
                        E_MENU_POP_DIRECTION_AUTO, event->timestamp);
  evas_event_feed_mouse_up(inst->gcc->gadcon->evas, event->button,
                        EVAS_BUTTON_NONE, event->timestamp, NULL);
}

static void
_clipboard_popup_free(Instance *inst)
{
  E_FREE_FUNC(inst->popup, e_object_del);
}

static void
_clipboard_popup_del_cb(void *obj)
{
  _clipboard_popup_free(e_object_data_get(obj));
}

static void
_clipboard_popup_comp_del_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
  _clipboard_popup_free(data);
}

static void
_clipboard_popup_new(Instance *inst)
{
  Evas *evas;
  Evas_Object *o;
  int row = 0;

  if (inst->popup) return;

  inst->popup = e_gadcon_popup_new(inst->gcc, 0);
  evas = e_comp->evas;

  inst->table = e_widget_table_add(e_win_evas_win_get(evas), 0);

  if (clip_inst->items)
    {
      Eina_List *it;
      Clip_Data *clip;

      // Flag to see if Label len changed
      Eina_Bool label_length_changed = clip_cfg->label_length_changed;
      clip_cfg->label_length_changed = EINA_FALSE;

      // revert list if selected
      if (clip_cfg->hist_reverse) clip_inst->items=eina_list_reverse(clip_inst->items);

      // show list in history menu
      EINA_LIST_FOREACH(clip_inst->items, it, clip)
        {
          if (label_length_changed)
            {
              free(clip->name);
              set_clip_name(&clip->name, clip->content,
                            clip_cfg->ignore_ws, clip_cfg->label_length);
            }
          o = e_widget_button_add(evas,
                                  clip->name,
                                  NULL,
                                  _clipboard_cb_paste_item,
                                  clip->content,
                                  inst);
          e_widget_table_object_align_append(inst->table, o, 0, row, 2, 1, 1, 0, 1, 0, 0, 0.5);
          row++;
        }
      // revert list back if selected
      if (clip_cfg->hist_reverse) clip_inst->items=eina_list_reverse(clip_inst->items);
    }
  else
    {
      o = e_widget_label_add(evas, _("Empty"));
      e_widget_table_object_align_append(inst->table, o, 0, row, 2, 1, 1, 0, 1, 0, 0.5, 0.5);
      row++;
    }

  o = e_widget_button_add(evas, _("Clear"), "edit-clear", _cb_clear_history, inst, NULL);
  e_widget_disabled_set(o, !clip_inst->items);
  e_widget_table_object_align_append(inst->table, o, 0, row, 1, 1, 0, 0, 0, 0, 0.5, 0.5);

  o = e_widget_button_add(evas, _("Settings"), "preferences-system", _clipboard_config_show, inst, NULL);
  e_widget_table_object_align_append(inst->table, o, 1, row, 1, 1, 0, 0, 0, 0, 0.5, 0.5);

  e_gadcon_popup_content_set(inst->popup, inst->table);
  e_gadcon_popup_show(inst->popup);
  e_comp_object_util_autoclose(inst->popup->comp_object,
                               _clipboard_popup_comp_del_cb,
                               NULL,
                               inst);
  e_object_data_set(E_OBJECT(inst->popup), inst);
  E_OBJECT_DEL_SET(inst->popup, _clipboard_popup_del_cb);
}

static void
_clip_add_item(Clip_Data *cd)
{
  Eina_List *it;

  EINA_SAFETY_ON_NULL_RETURN(cd);
  if (*cd->content == 0)
    {
      ERR("Warning Clip content is Empty!");
      return;
    }
  if ((it = _item_in_history(cd)))
    { // move to top of list
      clip_inst->items = eina_list_promote_list(clip_inst->items, it);
    }
  else
    { // add item to the list
      if (eina_list_count(clip_inst->items) < clip_cfg->hist_items)
        {
          clip_inst->items = eina_list_prepend(clip_inst->items, cd);
        }
      else
        { // remove last item from the list
          Eina_List *l_last = eina_list_last(clip_inst->items);

          if (l_last)
            {
              free_clip_data(l_last->data);
              clip_inst->items = eina_list_remove_list(clip_inst->items, l_last);
            }
          //  add clipboard data stored in cd to the list as a first item
          clip_inst->items = eina_list_prepend(clip_inst->items, cd);
        }
    }
  // saving list to the file
  clip_save(clip_inst->items);
}

static Eina_List *
_item_in_history(Clip_Data *cd)
{
  EINA_SAFETY_ON_NULL_RETURN_VAL(cd, NULL);
  if (clip_inst->items)
    return eina_list_search_unsorted_list(clip_inst->items, (Eina_Compare_Cb) _clip_compare, cd->content);
  else
    return NULL;
}

static int
_clip_compare(Clip_Data *cd, char *text)
{
  return strcmp(cd->content, text);
}

static void
_clear_history(void)
{
  EINA_SAFETY_ON_NULL_RETURN(clip_inst);
  if (clip_inst->items) E_FREE_LIST(clip_inst->items, free_clip_data);
  elm_object_cnp_selection_clear(e_comp->evas, ELM_SEL_TYPE_CLIPBOARD);
  clip_save(clip_inst->items);
}

Eet_Error
clip_save(Eina_List *items)
{
  if (clip_cfg->persistence) return save_history(items);
  else return EET_ERROR_NONE;
}

static void
_cb_clear_history(void *d1, void *d2 EINA_UNUSED)
{
  EINA_SAFETY_ON_NULL_RETURN(clip_cfg);

  if (clip_cfg->confirm_clear)
    {
      e_confirm_dialog_show(_("Confirm History Deletion"),
                            "application-exit",
                            _("You wish to delete the clipboards history.<br>"
                              "<br>"
                              "Are you sure you want to delete it?"),
                            _("Delete"), _("Keep"),
                            _cb_dialog_delete, NULL, NULL, NULL,
                            _cb_dialog_keep, NULL);
    }
  else _clear_history();
  _clipboard_popup_free((Instance *)d1);
}

static void
_cb_dialog_keep(void *data EINA_UNUSED)
{
  return;
}

static void
_cb_dialog_delete(void *data EINA_UNUSED)
{
  _clear_history();
}

static void
_clipboard_cb_paste_item(void *d1, void *d2)
{
  const char *paste = d1;

  elm_cnp_selection_set(clip_inst->ewin,
                        ELM_SEL_TYPE_CLIPBOARD,
                        ELM_SEL_FORMAT_TEXT,
                        paste,
                        strlen(paste));
  if (d2) _clipboard_popup_free(d2);
}

static void
_cb_menu_post_deactivate(void *data, E_Menu *menu EINA_UNUSED)
{
  Instance *inst = data;

  EINA_SAFETY_ON_NULL_RETURN(inst);
  //e_gadcon_locked_set(inst->gcc->gadcon, 0);
  edje_object_signal_emit(inst->o_button, "e,state,unfocused", "e");
}

static void
_cb_action_switch(E_Object *o EINA_UNUSED,
                  const char *params,
                  Instance *data,
                  Evas *evas EINA_UNUSED,
                  Evas_Object *obj EINA_UNUSED,
                  Mouse_Event *event EINA_UNUSED)
{
  if (!strcmp(params, "float")) _clipboard_popup_new(data);
  else if (!strcmp(params, "settings")) _cb_config_show(data, NULL, NULL);
  // Only call clear dialog if there is something to clear
  else if ((!strcmp(params, "clear")) && (clip_inst->items))
    _cb_clear_history(NULL, NULL);
}

void
free_clip_data(Clip_Data *clip)
{
  EINA_SAFETY_ON_NULL_RETURN(clip);
  free(clip->name);
  free(clip->content);
  free(clip);
}

static void
_clip_inst_free(Instance *inst)
{
  EINA_SAFETY_ON_NULL_RETURN(inst);
  inst->gcc = NULL;
  if (inst->o_button) evas_object_del(inst->o_button);
  E_FREE(inst);
}

static Eina_Bool
_cliboard_cb_paste(void *data,
                   Evas_Object *obj EINA_UNUSED,
                   Elm_Selection_Data *event)
{
  Clip_Data *cd = NULL;
  Instance *instance = data;
  char *paste = NULL;
  char *last = "";

  EINA_SAFETY_ON_NULL_RETURN_VAL(instance, EINA_TRUE);

  if (clip_inst->items)
    last =  ((Clip_Data *)eina_list_data_get(clip_inst->items))->content;
  if (event) paste = event->data;

  if (!paste) return EINA_TRUE;

  // some debug here for now
  printf("CP: [%6.3f] [%s]\n", ecore_time_get(), paste);

  if (!!strcmp(last, paste))
    {
      if (strlen(paste) == 0) return ECORE_CALLBACK_DONE;
      if (clip_cfg->ignore_ws_copy && is_empty(paste)) return ECORE_CALLBACK_DONE;
      cd = E_NEW(Clip_Data, 1);
      if (cd)
        {
          if (!set_clip_content(&cd->content, paste,
                                CLIP_TRIM_MODE(clip_cfg)))
            { // try to continue
              CRI("Something bad happened !!");
              E_FREE(cd);
              goto error;
            }
          if (!set_clip_name(&cd->name, cd->content,
                             clip_cfg->ignore_ws, clip_cfg->label_length))
            { // try to continue
              CRI("Something bad happened !!");
              E_FREE(cd);
              goto error;
            }
          _clip_add_item(cd);
        }
    }
error:
  return EINA_TRUE;
}

static void
_clipboard_cb_elm_selection_lost(void *data, Elm_Sel_Type selection)
{
  Mod_Inst *mod_inst = data;

  if (selection == ELM_SEL_TYPE_CLIPBOARD)
    elm_cnp_selection_get(mod_inst->ewin,
                          ELM_SEL_TYPE_CLIPBOARD,
                          ELM_SEL_FORMAT_TARGETS,
                          _cliboard_cb_paste,
                          mod_inst);
}

static Eina_Bool
_cb_sel_change_delay(void *data)
{
  Mod_Inst *mod_inst = data;
  Eina_Bool fetch = EINA_FALSE;

  delay_sel_timer = NULL;
  if ((mod_inst->sel_type == ELM_SEL_TYPE_PRIMARY) &&
      (clip_cfg->clip_select))
    fetch = EINA_TRUE;
  else if (mod_inst->sel_type == ELM_SEL_TYPE_CLIPBOARD)
    fetch = EINA_TRUE;
  if (fetch)
    elm_cnp_selection_get(e_comp->evas,
                          mod_inst->sel_type,
                          ELM_SEL_FORMAT_TARGETS,
                          _cliboard_cb_paste,
                          mod_inst);
  return EINA_FALSE;
}

static Eina_Bool
_clipboard_cb_event_selection(void *data,
                              Evas_Object *obj EINA_UNUSED,
                              void *event)
{
  Mod_Inst *mod_inst = data;
  Elm_Cnp_Event_Selection_Changed *ev = event;

  if (delay_sel_timer)
    {
      ecore_timer_del(delay_sel_timer);
      delay_sel_timer = NULL;
    }
  delay_sel_timer = ecore_timer_add(0.25, _cb_sel_change_delay, data);
  mod_inst->sel_type = ev->type;
  return EINA_TRUE;
}

/*
 * This is the first function called by e17 when you load the module
 */
E_API void *
e_modapi_init(E_Module *m)
{
  Eet_Error hist_err;

  // display this module's config info in the main config panel
  // under preferences catogory
  e_configure_registry_item_add("preferences/clipboard", 10,
            "Clipboard Settings", NULL,
            "edit-paste", config_clipboard_module);

  conf_item_edd = E_CONFIG_DD_NEW("clip_cfg_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
  E_CONFIG_VAL(D, T, id, STR);
  conf_edd = E_CONFIG_DD_NEW("clip_cfg", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
  E_CONFIG_LIST(D, T, items, conf_item_edd);
  E_CONFIG_VAL(D, T, version, UINT);
  E_CONFIG_VAL(D, T, label_length, UINT);
  E_CONFIG_VAL(D, T, hist_items, INT);
  E_CONFIG_VAL(D, T, clip_copy, INT);
  E_CONFIG_VAL(D, T, clip_select, INT);
  E_CONFIG_VAL(D, T, persistence, INT);
  E_CONFIG_VAL(D, T, hist_reverse, INT);
  E_CONFIG_VAL(D, T, confirm_clear, INT);
  E_CONFIG_VAL(D, T, ignore_ws, INT);
  E_CONFIG_VAL(D, T, ignore_ws_copy, INT);
  E_CONFIG_VAL(D, T, trim_ws, INT);
  E_CONFIG_VAL(D, T, trim_nl, INT);

  // Tell E to find any existing module data. First run ?
  clip_cfg = e_config_domain_load("module.clipboard", conf_edd);

  if (clip_cfg)
    { // Check config version
      if (!e_util_module_config_check("Clipboard", clip_cfg->version, MOD_CONFIG_FILE_VERSION))
        _clip_config_free();
    }

  // If we don't have a config yet, or it got erased above,
  // then create a default one
  if (!clip_cfg) _clip_config_new(m);
  INF("Initialized Clipboard Module");

  // add Module Key Binding actions
  act = e_action_add("clipboard");
  if (act)
    {
      act->func.go = (void *) _cb_action_switch;
      e_action_predef_name_set(_("Clipboard"), ACT_FLOAT,  "clipboard", "float",    NULL, 0);
      e_action_predef_name_set(_("Clipboard"), ACT_CONFIG, "clipboard", "settings", NULL, 0);
      e_action_predef_name_set(_("Clipboard"), ACT_CLEAR,  "clipboard", "clear",    NULL, 0);
    }

  // create a global clip_inst for our module
  // complete with a hidden window for event notification purposes
  clip_inst = E_NEW(Mod_Inst, 1);
  clip_inst->inst = E_NEW(Instance, 1);

  // read History file and set clipboard
  hist_err = read_history(&(clip_inst->items), clip_cfg->ignore_ws, clip_cfg->label_length);

  if ((hist_err == EET_ERROR_NONE) &&
      eina_list_count(clip_inst->items))
    _clipboard_cb_paste_item(eina_list_data_get(clip_inst->items), NULL);
  else
    // something must be wrong with history file
    // so we create a new one
    clip_save(clip_inst->items);
  // make sure the history read has no more items than allowed
  // by clipboard config file. This should never happen without user
  // intervention of some kind.
  if (clip_inst->items)
    {
      if (eina_list_count(clip_inst->items) > clip_cfg->hist_items)
        {
          /* FIXME: Do we need to warn user in case this is backed up data
           *         being restored ? */
          WRN("History File truncation!");
          truncate_history(clip_cfg->hist_items);
        }
    }

  clip_inst->ewin = elm_win_add(NULL, NULL, ELM_WIN_BASIC);

  // now add some callbacks to handle clipboard events
  // re-add to history
  elm_cnp_selection_loss_callback_set(e_comp->evas,
                                      ELM_SEL_TYPE_CLIPBOARD,
                                      _clipboard_cb_elm_selection_lost,
                                      clip_inst);
  E_LIST_HANDLER_APPEND(clip_inst->handle,
                        ECORE_EVENT_MOUSE_BUTTON_UP,
                        _clipboard_cb_event_selection,
                        clip_inst);
  // Does not seem to fire?
  E_LIST_HANDLER_APPEND(clip_inst->handle,
                        ELM_CNP_EVENT_SELECTION_CHANGED,
                        _clipboard_cb_event_selection,
                        clip_inst);

  // tell any gadget containers (shelves, etc) that we provide a module
  e_gadcon_provider_register(&_gadcon_class);
  // give e the module
  return m;
}

static void
_clipboard_config_show(void *d1, void *d2 EINA_UNUSED)
{
  if (!clip_cfg) return;
  if (clip_cfg->config_dialog) return;
  config_clipboard_module(NULL, NULL);
  _clipboard_popup_free((Instance *)d1);
}

static void
_cb_config_show(void *data,
                E_Menu *m EINA_UNUSED,
                E_Menu_Item *mi EINA_UNUSED)
{
  _clipboard_config_show(data, NULL);
}

/*
 * This function is called by e17 when you unload the module,
 * here you should free all resources used while the module was enabled.
 */
E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
  Config_Item *ci;

  if (delay_sel_timer) ecore_timer_del(delay_sel_timer);
  delay_sel_timer = NULL;
  // the 2 following EINA SAFETY checks should never happen
  // and i usually avoid gotos but here I feel their use is harmless */
  EINA_SAFETY_ON_NULL_GOTO(clip_inst, noclip);

  // Kill our clip_inst and cleanup
  E_FREE_LIST(clip_inst->handle, ecore_event_handler_del);
  clip_inst->handle = NULL;
  E_FREE_LIST(clip_inst->items, free_clip_data);
  _clip_inst_free(clip_inst->inst);
  E_FREE(clip_inst);

noclip:
  EINA_SAFETY_ON_NULL_GOTO(clip_cfg, noconfig);

  // kill the config dialog
  while ((clip_cfg->config_dialog = e_config_dialog_get("E", "preferences/clipboard")))
    e_object_del(E_OBJECT(clip_cfg->config_dialog));

  if (clip_cfg->config_dialog) e_object_del(E_OBJECT(clip_cfg->config_dialog));
  E_FREE(clip_cfg->config_dialog);

  // cleanup our item list
  EINA_LIST_FREE(clip_cfg->items, ci)
    {
      eina_stringshare_del(ci->id);
      free(ci);
    }
  clip_cfg->module = NULL;
  // keep the planet green
  E_FREE(clip_cfg);

noconfig:
  // unregister the config dialog from the main panel
  e_configure_registry_item_del("preferences/clipboard");

  // clean up all key binding actions
  if (act)
    {
      e_action_predef_name_del("Clipboard", ACT_FLOAT);
      e_action_predef_name_del("Clipboard", ACT_CONFIG);
      e_action_predef_name_del("Clipboard", ACT_CLEAR);
      e_action_del("clipboard");
      act = NULL;
    }

  // clean EET
  E_CONFIG_DD_FREE(conf_edd);
  E_CONFIG_DD_FREE(conf_item_edd);
  INF("Shutting down Clipboard Module");
  // tell e the module is now unloaded. Gets removed from shelves, etc.
  e_gadcon_provider_unregister(&_gadcon_class);
  return 1;
}

/*
 * This function is used to save and store configuration info on local
 * storage
 */
E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
  e_config_domain_save("module.clipboard", conf_edd, clip_cfg);
  return 1;
}
