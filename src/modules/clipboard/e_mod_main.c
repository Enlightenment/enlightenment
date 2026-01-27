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

Mod mod = { 0 };

static E_Action *act = NULL;
static Ecore_Timer *delay_sel_timer = NULL;
static double _mod_time_start = 0.0;
static Elm_Genlist_Item_Class *list_itc = NULL;

static void       _cb_menu_post_deactivate(void *data, E_Menu *menu EINA_UNUSED);
static void       _cb_context_show(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, Evas_Event_Mouse_Down *event);
static void       _cb_clear_history(void *d1, void *d2 EINA_UNUSED);
static void       _cb_action_switch(E_Object *o EINA_UNUSED, const char *params);

static void       _cb_config_show(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED);
static void       _clipboard_config_show(void *d1, void *d2 EINA_UNUSED);
static void       _clipboard_popup_free(Instance *inst);

static void       _clip_inst_free(Instance *inst);
static void       _clipboard_popup_new(Instance *inst);
static void       _clear_history(void);

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

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
  Instance *inst = NULL;
  Evas_Object *o;
  E_Gadcon_Client *gcc;

  inst = E_NEW(Instance, 1);
  if (!inst) return NULL;

  mod.instances = eina_list_append(mod.instances, inst);

  o = edje_object_add(gc->evas);
  e_theme_edje_object_set(o, "base/theme/modules/clipboard",
                          "e/modules/clipboard/main");

  gcc = e_gadcon_client_new(gc, name, id, style, o);
  gcc->data = inst;

  inst->gcc = gcc;
  inst->o_button = o;
  e_gadcon_client_util_menu_attach(gcc);

  evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                 _clipboard_cb_mouse_down, inst);
  return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
  Instance *inst = gcc->data;
  mod.instances = eina_list_remove(mod.instances, inst);
  _clip_inst_free(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
  e_gadcon_client_aspect_set (gcc, 16, 16);
  e_gadcon_client_min_size_set (gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
  return "Clipboard";
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas * evas)
{
  Evas_Object *o = e_icon_add(evas);
  e_icon_fdo_icon_set(o, "edit-paste");
  return o;
}

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
_cb_del_item(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
  Config_Item *cd = data;
  Eina_List *l;
  Instance *inst;

  if (!cfg) return;
  EINA_LIST_FOREACH(mod.instances, l, inst) _clipboard_popup_free(inst);
  cfg->items = eina_list_remove(cfg->items, cd);
  config_clip_data_free(cd);
  e_config_save_queue();
}

static Evas_Object *
_cb_gl_icon_get(void *data, Evas_Object *obj, const char *part EINA_UNUSED)
{
  Config_Item *cd = data;
  Evas_Object *o, *bx, *bt;
  Eina_Strbuf *buf = eina_strbuf_new();
  char *s;
  int idx = 0, len = 0;
  unsigned int wid = 0, lines = 0;
  Eina_Unicode uc[2] = { 0 };

  if (!buf) return NULL;
  // first cfg->label_length chars per line, first few lines
  for (uc[0] = eina_unicode_utf8_next_get(cd->str, &idx); uc[0];
       uc[0] = eina_unicode_utf8_next_get(cd->str, &idx))
    {
      if (uc[0] == '\n')
        { // add newline to our new str
          eina_strbuf_append(buf, "\n");
          lines++; // count lines - we now hav 1 more
          wid = 0; // back to width being 0
        }
      else if (wid <= cfg->label_length)
        { // if we have less than the max chars per line...
          if (lines > 2)
            { // if max lines and we have more lines after add ...
              eina_strbuf_append(buf, "...");
              break; // no more - we are done with our string
            }
          // get a string from a unicode string of 1 item
          s = eina_unicode_unicode_to_utf8(uc, &len);
          if (s)
            { // append it to our label string
              eina_strbuf_append(buf, s);
              free(s);
              // we hit the max width so add ...
              // we keep skipping on this line until a newline gets wid to 0
              if (wid == cfg->label_length) eina_strbuf_append(buf, "...");
              wid++; // width went up 1
            }
        }
      else wid++;
    }
  bx = o = elm_box_add(obj);
  elm_box_horizontal_set(o, EINA_TRUE);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

  o = elm_label_add(obj);
  elm_object_style_set(o, "default/left");
  evas_object_pass_events_set(o, EINA_TRUE);
  s = elm_entry_utf8_to_markup(eina_strbuf_string_get(buf));
  eina_strbuf_free(buf);
  elm_object_text_set(o, s);
  free(s);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.5);
  elm_box_pack_end(bx, o);
  evas_object_show(o);

  bt = o = elm_button_add(obj);
  evas_object_propagate_events_set(o, EINA_FALSE);
  evas_object_size_hint_weight_set(o, 0.0, EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set(o, 0.0, 0.5);
  evas_object_smart_callback_add(o, "clicked", _cb_del_item, cd);

  o = elm_icon_add(e_comp->elm);
  elm_icon_standard_set(o, "edit-delete");
  elm_object_content_set(bt, o);
  evas_object_show(o);

  elm_box_pack_end(bx, bt);
  evas_object_show(bt);

  return bx;
}

static void
_cb_sel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
  Instance *inst = data;
  int idx = elm_genlist_item_index_get(event_info); // event_info is item
  Config_Item *cd;

  if (idx < 0) return;
  cd = eina_list_nth(cfg->items, idx - 1);
  if (cd) elm_cnp_selection_set(mod.ewin, ELM_SEL_TYPE_CLIPBOARD,
                                ELM_SEL_FORMAT_TEXT, cd->str,
                                strlen(cd->str));
  _clipboard_popup_free(inst);
}

static void
_cb_clear(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
  Eina_List *l;
  Instance *inst;

  EINA_SAFETY_ON_NULL_RETURN(cfg);
  EINA_LIST_FOREACH(mod.instances, l, inst) _clipboard_popup_free(inst);
  _clear_history();
}

static void
_cb_settings(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
  Eina_List *l;
  Instance *inst;

  EINA_SAFETY_ON_NULL_RETURN(cfg);
  EINA_LIST_FOREACH(mod.instances, l, inst) _clipboard_popup_free(inst);
  if (cfg->config_dialog) return;
  config_clipboard_module(NULL, NULL);
}

static void
_clipboard_popup_new(Instance *inst)
{
  Evas_Object *o, *tb, *gl, *bt, *ic;
  Eina_List *l;
  Config_Item *cd;

  if (inst->popup) return;
  inst->popup = e_gadcon_popup_new(inst->gcc, 0);

  if (!list_itc)
    {
      list_itc = elm_genlist_item_class_new();
      if (list_itc)
        {
          list_itc->item_style = "full";
          list_itc->func.text_get = NULL;
          list_itc->func.content_get = _cb_gl_icon_get;
          list_itc->func.state_get = NULL;
        }
    }
  tb = inst->table = o = elm_table_add(e_comp->elm);

  gl = o = elm_genlist_add(e_comp->elm);
  // make scroller expand to min size of items
  elm_genlist_mode_set(o, ELM_LIST_EXPAND);
  EINA_LIST_FOREACH(cfg->items, l, cd)
    {
      elm_genlist_item_append(o, list_itc, cd, NULL, // parent
                              ELM_GENLIST_ITEM_NONE, _cb_sel, inst);
    }
  elm_table_pack(tb, o, 0, 0, 2, 1);

  bt = o = elm_button_add(e_comp->elm);
  evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
  elm_object_text_set(o, _("Clear"));
  evas_object_smart_callback_add(o, "clicked", _cb_clear, inst);
  elm_table_pack(tb, o, 0, 1, 1, 1);
  evas_object_show(o);

  ic = o = elm_icon_add(e_comp->elm);
  elm_icon_standard_set(ic, "edit-delete");
  elm_object_content_set(bt, o);
  evas_object_show(o);

  bt = o = elm_button_add(e_comp->elm);
  evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
  elm_object_text_set(o, _("Settings"));
  evas_object_smart_callback_add(o, "clicked", _cb_settings, inst);
  elm_table_pack(tb, o, 1, 1, 1, 1);
  evas_object_show(o);

  ic = o = elm_icon_add(e_comp->elm);
  elm_icon_standard_set(ic, "preferences-system");
  elm_object_content_set(bt, o);
  evas_object_show(o);

  evas_object_show(gl);

  e_gadcon_popup_content_set(inst->popup, inst->table);
  evas_object_show(inst->table);

  e_gadcon_popup_show(inst->popup);
  e_comp_object_util_autoclose(inst->popup->comp_object,
                               _clipboard_popup_comp_del_cb,
                               NULL, inst);
  e_object_data_set(E_OBJECT(inst->popup), inst);
  E_OBJECT_DEL_SET(inst->popup, _clipboard_popup_del_cb);
}

static void
_clear_history(void)
{
  if (!cfg) return;
  if (cfg->items) E_FREE_LIST(cfg->items, config_clip_data_free);
  elm_object_cnp_selection_clear(e_comp->evas, ELM_SEL_TYPE_CLIPBOARD);
  e_config_save_queue();
}

static void
_cb_clear_history(void *d1, void *d2 EINA_UNUSED)
{
  _clipboard_popup_free(d1);
  _clear_history();
}

static void
_cb_menu_post_deactivate(void *data, E_Menu *menu EINA_UNUSED)
{
  Instance *inst = data;

  edje_object_signal_emit(inst->o_button, "e,state,unfocused", "e");
}

static void
_cb_action_switch(E_Object *o EINA_UNUSED, const char *params)
{
  Instance *inst;

  if (!mod.instances) return;
  inst = mod.instances->data; // just use 1st one...
  if (!strcmp(params, "float")) _clipboard_popup_new(inst);
  else if (!strcmp(params, "settings")) _cb_config_show(inst, NULL, NULL);
  else if ((!strcmp(params, "clear")) && (cfg->items))
    _cb_clear_history(NULL, NULL);
}

static void
_clip_inst_free(Instance *inst)
{
  _clipboard_popup_free(inst);
  inst->gcc = NULL;
  if (inst->o_button) evas_object_del(inst->o_button);
  E_FREE(inst);
}

static Eina_Bool
_cliboard_cb_paste(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                   Elm_Selection_Data *event)
{
  Eina_List *l;
  Instance *inst;
  if (!event) goto done;

  EINA_LIST_FOREACH(mod.instances, l, inst) _clipboard_popup_free(inst);
  config_paste_add(event->data, event->len, event->format);
  e_config_save_queue();
done:
  return ECORE_CALLBACK_DONE;
}

static void
_clipboard_cb_elm_selection_lost(void *data EINA_UNUSED,
                                 Elm_Sel_Type selection)
{
  if (selection != ELM_SEL_TYPE_CLIPBOARD) return;
  elm_cnp_selection_get(mod.ewin, ELM_SEL_TYPE_CLIPBOARD,
                        ELM_SEL_FORMAT_TARGETS, _cliboard_cb_paste,
                        NULL);
}

static Eina_Bool
_cb_sel_change_delay(void *data EINA_UNUSED)
{
  Eina_Bool fetch = EINA_FALSE;

  delay_sel_timer = NULL;
  if ((mod.sel_type == ELM_SEL_TYPE_PRIMARY) && (cfg->clip_select))
    fetch = EINA_TRUE;
  else if (mod.sel_type == ELM_SEL_TYPE_CLIPBOARD)
    fetch = EINA_TRUE;
  if (fetch) elm_cnp_selection_get(e_comp->evas, mod.sel_type,
                                   ELM_SEL_FORMAT_TARGETS,
                                   _cliboard_cb_paste, NULL);
  return EINA_FALSE;
}

static Eina_Bool
_clipboard_cb_event_selection(void *data EINA_UNUSED,
                              Evas_Object *obj EINA_UNUSED,
                              void *event)
{
  Elm_Cnp_Event_Selection_Changed *ev = event;

  // skip sel change event early on
  if ((ecore_time_get() - _mod_time_start) < 1.0) return EINA_TRUE;
  if (delay_sel_timer)
    {
      ecore_timer_del(delay_sel_timer);
      delay_sel_timer = NULL;
    }
  delay_sel_timer = ecore_timer_add(0.25, _cb_sel_change_delay, data);
  mod.sel_type = ev->type;
  return EINA_TRUE;
}

static void
_clipboard_config_show(void *d1, void *d2 EINA_UNUSED)
{
  if (!cfg) return;
  if (cfg->config_dialog) return;
  config_clipboard_module(NULL, NULL);
  _clipboard_popup_free(d1);
}

static void
_cb_config_show(void *data,
                E_Menu *m EINA_UNUSED,
                E_Menu_Item *mi EINA_UNUSED)
{
  _clipboard_config_show(data, NULL);
}

E_API void *
e_modapi_init(E_Module *m)
{
  if (!config_init()) return NULL;
  if (!conifg_new_limit()) return NULL;
  config_hist_limit();

  cfg->module = m;

  act = e_action_add("clipboard");
  if (act)
    {
      act->func.go = _cb_action_switch;
      e_action_predef_name_set(_("Clipboard"), ACT_FLOAT,  "clipboard", "float",    NULL, 0);
      e_action_predef_name_set(_("Clipboard"), ACT_CONFIG, "clipboard", "settings", NULL, 0);
      e_action_predef_name_set(_("Clipboard"), ACT_CLEAR,  "clipboard", "clear",    NULL, 0);
    }
  e_configure_registry_item_add("preferences/clipboard", 10,
                                _("Clipboard Settings"), NULL,
                                "edit-paste", config_clipboard_module);
  memset(&mod, 0, sizeof(mod));
  mod.ewin = elm_win_add(NULL, NULL, ELM_WIN_BASIC);
  elm_cnp_selection_loss_callback_set(e_comp->evas,
                                      ELM_SEL_TYPE_CLIPBOARD,
                                      _clipboard_cb_elm_selection_lost,
                                      NULL);
  E_LIST_HANDLER_APPEND(mod.handles,
                        ECORE_EVENT_MOUSE_BUTTON_UP,
                        _clipboard_cb_event_selection,
                        NULL);
  E_LIST_HANDLER_APPEND(mod.handles,
                        ELM_CNP_EVENT_SELECTION_CHANGED,
                        _clipboard_cb_event_selection,
                        NULL);
  e_gadcon_provider_register(&_gadcon_class);
  _mod_time_start = ecore_time_get(); // rcored start time to skip early events
  return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
  e_gadcon_provider_unregister(&_gadcon_class);
  if (delay_sel_timer) ecore_timer_del(delay_sel_timer);
  delay_sel_timer = NULL;
  E_FREE_LIST(mod.handles, ecore_event_handler_del);
  memset(&mod, 0, sizeof(mod));

  if (!cfg) goto noconfig;

  // kill the config dialog
  while ((cfg->config_dialog =
          e_config_dialog_get("E", "preferences/clipboard")))
    e_object_del(E_OBJECT(cfg->config_dialog));
  if (cfg->config_dialog) e_object_del(E_OBJECT(cfg->config_dialog));
  E_FREE(cfg->config_dialog);
  config_shutdown();

noconfig:
  e_configure_registry_item_del("preferences/clipboard");
  if (act)
    {
      e_action_predef_name_del("Clipboard", ACT_FLOAT);
      e_action_predef_name_del("Clipboard", ACT_CONFIG);
      e_action_predef_name_del("Clipboard", ACT_CLEAR);
      e_action_del("clipboard");
      act = NULL;
    }
  e_gadcon_provider_unregister(&_gadcon_class);
  if (list_itc) elm_gengrid_item_class_free(list_itc);
  list_itc = NULL;
  return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
  config_save();
  return 1;
}
