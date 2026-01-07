#include "e_mod_main.h"

// Stuff for convenience to compress code
#define CLIP_TRIM_MODE(x) (x->trim_nl + 2 * (x->trim_ws))

// gadcon requirements
static     Evas_Object  *_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas * evas);
static const char       *_gc_id_new(const E_Gadcon_Client_Class *client_class);
static E_Gadcon_Client  *_gc_init(E_Gadcon * gc, const char *name, const char *id, const char *style);
static void              _gc_orient(E_Gadcon_Client * gcc, E_Gadcon_Orient orient EINA_UNUSED);
static const char       *_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED);
static void              _gc_shutdown(E_Gadcon_Client * gcc);

/* Define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
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

// actual module specifics
Mod_Inst *clip_inst = NULL; // Need by e_mod_config.c

static E_Action *act = NULL;
static Ecore_Timer *delay_sel_timer = NULL;
static double _mod_time_start = 0.0;

// first some call backs
static void       _clipboard_cb_paste_item(void *d1, void *d2);
static void       _cb_menu_post_deactivate(void *data, E_Menu *menu EINA_UNUSED);
static void       _cb_context_show(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, Evas_Event_Mouse_Down *event);
static void       _cb_clear_history(void *d1, void *d2 EINA_UNUSED);
static void       _cb_action_switch(E_Object *o EINA_UNUSED, const char *params);

static void       _cb_config_show(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED);
static void       _clipboard_config_show(void *d1, void *d2 EINA_UNUSED);
static void       _clipboard_popup_free(Instance *inst);

// and then some auxillary functions
static void       _clip_inst_free(Instance *inst);
static void       _clip_add_item(Config_Item *cd);
static void       _clipboard_popup_new(Instance *inst);
static void       _clear_history(void);
static Eina_List *_item_in_history(Config_Item *cd);
static int        _clip_compare(Config_Item *cd, char *text);

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

  clip_inst->instances = eina_list_append(clip_inst->instances, inst);

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
  clip_inst->instances = eina_list_remove(clip_inst->instances, inst);
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
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
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
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
  return _gadcon_class.name;
}

static void
_cb_context_show(void *data,
                 Evas *evas EINA_UNUSED,
                 Evas_Object *obj EINA_UNUSED,
                 Evas_Event_Mouse_Down *event)
{
  Instance *inst = data;
  Evas_Coord x;
  Evas_Coord y;
  E_Menu *m;
  E_Menu_Item *mi;

  EINA_SAFETY_ON_NULL_RETURN(inst);
  EINA_SAFETY_ON_NULL_RETURN(event);
  // ignore all mouse events but left clicks
  if (event->button != 3) return;

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

  // make this a scrollable genlist...
  if (clip_cfg->items)
    {
      Eina_List *it;
      Config_Item *cd;

      // flag to see if label len changed
      Eina_Bool label_length_changed = clip_cfg->label_length_changed;
      clip_cfg->label_length_changed = EINA_FALSE;

      // revert list if selected
      if (clip_cfg->hist_reverse) clip_cfg->items=eina_list_reverse(clip_cfg->items);

      // show list in history menu
      EINA_LIST_FOREACH(clip_cfg->items, it, cd)
        {
          if (label_length_changed)
            {
              free(cd->name);
              cd->name = NULL;
            }
          if (!cd->name)
            {
              eina_stringshare_del(cd->name);
              set_clip_name(&cd->name, cd->str, clip_cfg->label_length);
            }
          o = e_widget_button_add(evas,
                                  cd->name,
                                  NULL,
                                  _clipboard_cb_paste_item,
                                  (void *)cd->str, // XXX: what if items list changes while buttons up?
                                  inst);
          e_widget_table_object_align_append(inst->table, o, 0, row, 2, 1, 1, 0, 1, 0, 0, 0.5);
          row++;
        }
      // revert list back if selected
      if (clip_cfg->hist_reverse) clip_cfg->items=eina_list_reverse(clip_cfg->items);
    }
  else
    {
      o = e_widget_label_add(evas, _("Empty"));
      e_widget_table_object_align_append(inst->table, o, 0, row, 2, 1, 1, 0, 1, 0, 0.5, 0.5);
      row++;
    }

  // XXX: make this a box - homogeneous below lsit above
  o = e_widget_button_add(evas, _("Clear"), "edit-clear", _cb_clear_history, inst, NULL);
  e_widget_disabled_set(o, !clip_cfg->items);
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
_clip_add_item(Config_Item *cd)
{
  Eina_List *it;

  EINA_SAFETY_ON_NULL_RETURN(cd);
  if (cd->str[0] == 0) return;
  if ((it = _item_in_history(cd)))
    { // move to top of list
      clip_cfg->items = eina_list_promote_list(clip_cfg->items, it);
    }
  else
    { // add item to the list
      if (eina_list_count(clip_cfg->items) < clip_cfg->hist_items)
        { // add to start of list
          clip_cfg->items = eina_list_prepend(clip_cfg->items, cd);
        }
      else
        { // remove last item from the list
          Eina_List *l_last = eina_list_last(clip_cfg->items);

          if (l_last)
            {
              config_clip_data_free(l_last->data);
              clip_cfg->items = eina_list_remove_list(clip_cfg->items, l_last);
            }
          //  add clipboard data stored in cd to the list as a first item
          clip_cfg->items = eina_list_prepend(clip_cfg->items, cd);
        }
    }
  // saving list to the file
  e_config_save_queue();
}

static Eina_List *
_item_in_history(Config_Item *cd)
{
  EINA_SAFETY_ON_NULL_RETURN_VAL(cd, NULL);
  if (clip_cfg->items)
    return eina_list_search_unsorted_list(clip_cfg->items, (Eina_Compare_Cb) _clip_compare, cd->str);
  else
    return NULL;
}

static int
_clip_compare(Config_Item *cd, char *text)
{
  return strcmp(cd->str, text);
}

static void
_clear_history(void)
{
  EINA_SAFETY_ON_NULL_RETURN(clip_cfg);
  if (clip_cfg->items) E_FREE_LIST(clip_cfg->items, config_clip_data_free);
  elm_object_cnp_selection_clear(e_comp->evas, ELM_SEL_TYPE_CLIPBOARD);
  e_config_save_queue();
}

static void
_cb_clear_history(void *d1, void *d2 EINA_UNUSED)
{
  EINA_SAFETY_ON_NULL_RETURN(clip_cfg);
  _clipboard_popup_free((Instance *)d1);
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
_cb_action_switch(E_Object *o EINA_UNUSED, const char *params)
{
  Instance *inst;

  if ((!clip_inst) || (!clip_inst->instances)) return;
  inst = clip_inst->instances->data; // just use 1st one...
  if (!strcmp(params, "float")) _clipboard_popup_new(inst);
  else if (!strcmp(params, "settings")) _cb_config_show(inst, NULL, NULL);
  // Only call clear dialog if there is something to clear
  else if ((!strcmp(params, "clear")) && (clip_cfg->items))
    _cb_clear_history(NULL, NULL);
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
  Config_Item *cd = NULL;
  Instance *instance = data;
  char *paste = NULL;
  const char *last = "";

  EINA_SAFETY_ON_NULL_RETURN_VAL(instance, EINA_TRUE);

  if (clip_cfg->items)
    last = ((Config_Item *)eina_list_data_get(clip_cfg->items))->str;
  if (event) paste = event->data;

  if (!paste) return EINA_TRUE;

  if (!!strcmp(last, paste))
    { // if new past differs to most recent stored...
      if (strlen(paste) == 0) return ECORE_CALLBACK_DONE;
      if (is_empty(paste)) return ECORE_CALLBACK_DONE;
      cd = E_NEW(Config_Item, 1);
      if (cd)
        {
          // XXX: if we select huge amounts of text this could use a lot of ram
          cd->str = eina_stringshare_add(paste);
          if (!set_clip_name(&cd->name, cd->str, clip_cfg->label_length))
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

  printf("CP: delyay from sel change... get sel!\n");
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

  // skip sel change event early on
  if ((ecore_time_get() - _mod_time_start) < 1.0) return EINA_TRUE;
  if (delay_sel_timer)
    {
      ecore_timer_del(delay_sel_timer);
      delay_sel_timer = NULL;
    }
  delay_sel_timer = ecore_timer_add(0.25, _cb_sel_change_delay, data);
  mod_inst->sel_type = ev->type;
  return EINA_TRUE;
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
 * This is the first function called by e17 when you load the module
 */
E_API void *
e_modapi_init(E_Module *m)
{
  if (!config_init()) return NULL;
  if (!conifg_new_limit()) return NULL;
  config_truncate_history(clip_cfg->hist_items);

  clip_cfg->module = m;

  act = e_action_add("clipboard"); // module key binding actions
  if (act)
    {
      act->func.go = _cb_action_switch;
      e_action_predef_name_set(_("Clipboard"), ACT_FLOAT,  "clipboard", "float",    NULL, 0);
      e_action_predef_name_set(_("Clipboard"), ACT_CONFIG, "clipboard", "settings", NULL, 0);
      e_action_predef_name_set(_("Clipboard"), ACT_CLEAR,  "clipboard", "clear",    NULL, 0);
    }
  // display this module's config info in the main config panel
  // under preferences catogory
  e_configure_registry_item_add("preferences/clipboard", 10,
                                "Clipboard Settings", NULL,
                                "edit-paste", config_clipboard_module);
  // create a global clip_inst for our module
  // complete with a hidden window for event notification purposes
  clip_inst = E_NEW(Mod_Inst, 1);
  if (!clip_inst) return NULL;
  clip_inst->ewin = elm_win_add(NULL, NULL, ELM_WIN_BASIC);
  // now add some callbacks to handle clipboard events
  // re-add to history
  elm_cnp_selection_loss_callback_set(e_comp->evas,
                                      ELM_SEL_TYPE_CLIPBOARD,
                                      _clipboard_cb_elm_selection_lost,
                                      clip_inst);
  E_LIST_HANDLER_APPEND(clip_inst->handles,
                        ECORE_EVENT_MOUSE_BUTTON_UP,
                        _clipboard_cb_event_selection,
                        clip_inst);
  // Does not seem to fire?
  E_LIST_HANDLER_APPEND(clip_inst->handles,
                        ELM_CNP_EVENT_SELECTION_CHANGED,
                        _clipboard_cb_event_selection,
                        clip_inst);
  // tell any gadget containers (shelves, etc) that we provide a module
  e_gadcon_provider_register(&_gadcon_class);
  // give e the module
  _mod_time_start = ecore_time_get(); // rcored start time to skip early events
  return m;
}

/*
 * This function is called by e17 when you unload the module,
 * here you should free all resources used while the module was enabled.
 */
E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
  e_gadcon_provider_unregister(&_gadcon_class);
  if (delay_sel_timer) ecore_timer_del(delay_sel_timer);
  delay_sel_timer = NULL;
  // the 2 following EINA SAFETY checks should never happen
  // and i usually avoid gotos but here I feel their use is harmless */
  EINA_SAFETY_ON_NULL_GOTO(clip_inst, noclip);

  // kill our clip_inst and cleanup
  E_FREE_LIST(clip_inst->handles, ecore_event_handler_del);
  clip_inst->handles = NULL;
  E_FREE(clip_inst);

noclip:
  EINA_SAFETY_ON_NULL_GOTO(clip_cfg, noconfig);

  // kill the config dialog
  while ((clip_cfg->config_dialog = e_config_dialog_get("E", "preferences/clipboard")))
    e_object_del(E_OBJECT(clip_cfg->config_dialog));

  if (clip_cfg->config_dialog) e_object_del(E_OBJECT(clip_cfg->config_dialog));
  E_FREE(clip_cfg->config_dialog);

  config_shutdown();

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
  config_save();
  return 1;
}
