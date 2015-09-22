#include "e.h"
#include "e_mod_main.h"
#include "e_mod_parse.h"

struct _E_Config_Dialog_Data
{
   Evas        *evas, *dlg_evas;
   Evas_Object *layout_list, *used_list;
   Evas_Object *dmodel_list, *model_list, *variant_list;
   Evas_Object *btn_add, *btn_del, *btn_up, *btn_down;
   Ecore_Timer *fill_delay;
   Ecore_Timer *dlg_fill_delay;

   Eina_List   *cfg_layouts;
   Eina_List   *cfg_options;
   const char  *default_model;

   int          only_label;
   int          dont_touch_my_damn_keyboard;

   E_Dialog    *dlg_add_new;
   E_Config_Dialog *cfd;
};

typedef struct _E_XKB_Dialog_Option
{
   int         enabled;
   const char *name;
} E_XKB_Dialog_Option;

/* Local prototypes */

static void        *_create_data(E_Config_Dialog *cfd);
static void         _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int          _basic_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int          _basic_check_changed(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);

static void         _cb_add(void *data, void *data2 EINA_UNUSED);
static void         _cb_del(void *data, void *data2 EINA_UNUSED);

static void         _cb_up(void *data, void *data2 EINA_UNUSED);
static void         _cb_dn(void *data, void *data2 EINA_UNUSED);

static void         _dlg_add_cb_ok(void *data, E_Dialog *dlg);
static void         _dlg_add_cb_cancel(void *data, E_Dialog *dlg);

static E_Dialog    *_dlg_add_new(E_Config_Dialog_Data *cfdata);

static void         _dlg_add_cb_del(void *obj);

static Eina_Bool    _cb_dlg_fill_delay(void *data);

static void         _cb_layout_select(void *data);
static void         _cb_used_select(void *data);

static Eina_Bool    _cb_fill_delay(void *data);

/* Externals */

E_Config_Dialog *
_xkb_cfg_dialog(Evas_Object *parent EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   if (e_config_dialog_find("E", "keyboard_and_mouse/xkbswitch"))
     return NULL;
   if (!(v = E_NEW(E_Config_Dialog_View, 1))) return NULL;

   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.create_widgets = _basic_create;
   v->basic.apply_cfdata = _basic_apply;
   v->basic.check_changed = _basic_check_changed;

   cfd = e_config_dialog_new(NULL, _("Keyboard Settings"), "E",
                             "keyboard_and_mouse/xkbswitch",
                             "preferences-desktop-keyboard",
                             0, v, NULL);
   _xkb.cfd = cfd;
   return cfd;
}

/* Locals */

static void *
_create_data(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata;
   Eina_List *l, *ll, *lll;
   E_Config_XKB_Layout *cl, *nl;
   E_XKB_Dialog_Option *od;
   E_XKB_Option *op;
   E_XKB_Option_Group *gr;

   find_rules();
   parse_rules(); /* XXX: handle in case nothing was found? */

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   cfdata->cfd = cfd;

   cfdata->cfg_layouts = NULL;
   EINA_LIST_FOREACH(e_config->xkb.used_layouts, l, cl)
     {
        nl = E_NEW(E_Config_XKB_Layout, 1);
        nl->name = eina_stringshare_add(cl->name);
        nl->model = eina_stringshare_add(cl->model);
        nl->variant = eina_stringshare_add(cl->variant);

        cfdata->cfg_layouts = eina_list_append(cfdata->cfg_layouts, nl);
     }

   /* Initialize options */

   cfdata->only_label = e_config->xkb.only_label;
   cfdata->dont_touch_my_damn_keyboard = e_config->xkb.dont_touch_my_damn_keyboard;
   cfdata->cfg_options = NULL;

   lll = e_config->xkb.used_options;
   EINA_LIST_FOREACH(optgroups, l, gr)
     {
        EINA_LIST_FOREACH(gr->options, ll, op)
          {
             od = E_NEW(E_XKB_Dialog_Option, 1);
             od->name = eina_stringshare_add(op->name);
             if (lll &&
                 (od->name == ((E_Config_XKB_Option *)
                               eina_list_data_get(lll))->name))
               {
                  od->enabled = 1;
                  lll = eina_list_next(lll);
               }
             else od->enabled = 0;
             cfdata->cfg_options = eina_list_append(cfdata->cfg_options, od);
          }
     }

   return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   E_Config_XKB_Layout *cl;
   E_XKB_Dialog_Option *od;

   _xkb.cfd = NULL;

   EINA_LIST_FREE(cfdata->cfg_layouts, cl)
     {
        eina_stringshare_del(cl->name);
        eina_stringshare_del(cl->model);
        eina_stringshare_del(cl->variant);
        E_FREE(cl);
     }

   EINA_LIST_FREE(cfdata->cfg_options, od)
     {
        eina_stringshare_del(od->name);
        E_FREE(od);
     }

   eina_stringshare_del(cfdata->default_model);
   E_FREE(cfdata);
   clear_rules();
}

static int
_basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   Eina_List *l, *l2, *l3;
   E_Config_XKB_Layout *cl, *nl;
   E_Config_XKB_Option *co;
   E_XKB_Dialog_Option *od;
   Eina_Bool found;

   if ((eina_list_count(e_config->xkb.used_layouts) !=
         eina_list_count(cfdata->cfg_layouts)) ||
       (e_config->xkb.default_model != cfdata->default_model) ||
       (e_config->xkb.only_label != cfdata->only_label) ||
       (e_config->xkb.dont_touch_my_damn_keyboard != cfdata->dont_touch_my_damn_keyboard))
     return 1;

   l2 = cfdata->cfg_layouts;
   EINA_LIST_FOREACH(e_config->xkb.used_layouts, l, cl)
     {
        nl = eina_list_data_get(l2);
        if ((cl->name != nl->name) ||
            (cl->model != nl->model) ||
            (cl->variant != nl->variant))
          return 1;
        l2 = eina_list_next(l2);
     }

   l2 = e_config->xkb.used_options;
   EINA_LIST_FOREACH(cfdata->cfg_options, l, od)
     {
        found = EINA_FALSE;
        EINA_LIST_FOREACH(l2, l3, co)
          {
             if (od->name == co->name)
               {
                  found = EINA_TRUE;
                  break;
               }
          }
        if ((!found) && (!od->enabled))
          continue;
        if (found && od->enabled)
          {
             l2 = eina_list_next(l3);
             continue;
          }
        return 1;
     }

   return 0;
}

static int
_basic_apply(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   Eina_List *l;
   E_Config_XKB_Layout *cl, *nl;
   E_Config_XKB_Option *oc;
   E_XKB_Dialog_Option *od;
   Eina_Bool cur_ok = EINA_FALSE, sel_ok = EINA_FALSE;

   EINA_LIST_FREE(e_config->xkb.used_layouts, cl)
     {
        eina_stringshare_del(cl->name);
        eina_stringshare_del(cl->model);
        eina_stringshare_del(cl->variant);
        E_FREE(cl);
     }

   EINA_LIST_FOREACH(cfdata->cfg_layouts, l, cl)
     {
        nl = E_NEW(E_Config_XKB_Layout, 1);
        nl->name = eina_stringshare_ref(cl->name);
        nl->model = eina_stringshare_ref(cl->model);
        nl->variant = eina_stringshare_ref(cl->variant);

        e_config->xkb.used_layouts =
          eina_list_append(e_config->xkb.used_layouts, nl);
        if (e_config_xkb_layout_eq(e_config->xkb.current_layout, nl))
          cur_ok = EINA_TRUE;
        if (e_config_xkb_layout_eq(e_config->xkb.sel_layout, nl))
          sel_ok = EINA_TRUE;
     }
   if (!cur_ok)
     {
        E_FREE_FUNC(e_config->xkb.current_layout, e_config_xkb_layout_free);
        EINA_LIST_FOREACH(e_config->xkb.used_layouts, l, cl)
          if (e_config->xkb.cur_layout == cl->name)
            {
               e_config->xkb.current_layout = e_config_xkb_layout_dup(cl);
               break;
            }
        if (!e_config->xkb.current_layout)
          eina_stringshare_replace(&e_config->xkb.cur_layout, NULL);
     }
   if (!sel_ok)
     {
        E_FREE_FUNC(e_config->xkb.sel_layout, e_config_xkb_layout_free);
        EINA_LIST_FOREACH(e_config->xkb.used_layouts, l, cl)
          if (e_config->xkb.selected_layout == cl->name)
            {
               e_config->xkb.sel_layout = e_config_xkb_layout_dup(cl);
               break;
            }
        if (!e_config->xkb.sel_layout)
          eina_stringshare_replace(&e_config->xkb.selected_layout, NULL);
     }

   eina_stringshare_replace(&e_config->xkb.default_model, cfdata->default_model);

   /* Save options */
   e_config->xkb.only_label = cfdata->only_label;
   e_config->xkb.dont_touch_my_damn_keyboard = cfdata->dont_touch_my_damn_keyboard;

   EINA_LIST_FREE(e_config->xkb.used_options, oc)
     {
        eina_stringshare_del(oc->name);
        E_FREE(oc);
     }

   EINA_LIST_FOREACH(cfdata->cfg_options, l, od)
     {
        if (!od->enabled) continue;

        oc = E_NEW(E_Config_XKB_Option, 1);
        oc->name = eina_stringshare_ref(od->name);
        e_config->xkb.used_options = eina_list_append(e_config->xkb.used_options, oc);
     }

   e_xkb_init();

   e_config_save_queue();
   return 1;
}

static Evas_Object *
_basic_create(E_Config_Dialog *cfd EINA_UNUSED, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *mainn, *layoutss, *modelss, *options, *configs, *buttons,
     *general, *scroller, *only_label, *dont_touch_my_damn_keyboard;
   E_XKB_Option *option;
   E_XKB_Option_Group *group;
   Eina_List *l, *ll, *lll;
   Evas_Coord mw, mh;
   /* Holds the dialog contents, displays a toolbar on the top */
   e_dialog_resizable_set(cfd->dia, 1);
   mainn = e_widget_toolbook_add(evas, 24, 24);

   /* Holds the used layouts ilist and the button table */
   layoutss = e_widget_list_add(evas, 0, 0);

   /* Holds the used layouts */
   configs = e_widget_ilist_add(evas, 32, 32, NULL);

   e_widget_size_min_set(configs, 220, 160);
   e_widget_ilist_go(configs);

   e_widget_list_object_append(layoutss, configs, 1, 1, 0.5);
   cfdata->used_list = configs;

   /* Holds the buttons */
   buttons = e_widget_table_add(e_win_evas_win_get(evas), 1);
   cfdata->btn_up = e_widget_button_add(evas, _("Up"), "go-up", _cb_up, cfdata, NULL);
   e_widget_disabled_set(cfdata->btn_up, EINA_TRUE);
   e_widget_table_object_append(buttons, cfdata->btn_up, 0, 0, 1, 1, 1, 1, 1, 0);

   cfdata->btn_down = e_widget_button_add(evas, _("Down"), "go-down", _cb_dn, cfdata, NULL);
   e_widget_disabled_set(cfdata->btn_down, EINA_TRUE);
   e_widget_table_object_append(buttons, cfdata->btn_down, 1, 0, 1, 1, 1, 1, 1, 0);

   cfdata->btn_add = e_widget_button_add(evas, _("Add"), "list-add", _cb_add, cfdata, NULL);
   e_widget_table_object_append(buttons, cfdata->btn_add, 0, 1, 1, 1, 1, 1, 1, 0);

   cfdata->btn_del = e_widget_button_add(evas, _("Remove"), "list-remove", _cb_del, cfdata, NULL);
   e_widget_disabled_set(cfdata->btn_del, EINA_TRUE);
   e_widget_table_object_append(buttons, cfdata->btn_del, 1, 1, 1, 1, 1, 1, 1, 0);

   e_widget_list_object_append(layoutss, buttons, 1, 0, 1);

   e_widget_toolbook_page_append(mainn, NULL, _("Configurations"), layoutss, 1, 1, 1, 1, 0.5, 0.0);

   /* Holds the default models */
   modelss = e_widget_ilist_add(evas, 32, 32, &cfdata->default_model);
   e_widget_size_min_set(modelss, 220, 160);
   cfdata->dmodel_list = modelss;

   e_widget_toolbook_page_append(mainn, NULL, _("Models"), modelss, 1, 1, 1, 1, 0.5, 0.0);

   /* Holds the options */
   options = e_widget_list_add(evas, 0, 0);

   general = e_widget_framelist_add(evas, _("General"), 0);
   dont_touch_my_damn_keyboard = e_widget_check_add(evas, _("Do not apply any keyboard settings ever"), &(cfdata->dont_touch_my_damn_keyboard));
   e_widget_framelist_object_append(general, dont_touch_my_damn_keyboard);
   only_label = e_widget_check_add(evas, _("Label only in gadgets"), &(cfdata->only_label));
   e_widget_check_widget_disable_on_checked_add(dont_touch_my_damn_keyboard, only_label);
   e_widget_framelist_object_append(general, only_label);
   e_widget_list_object_append(options, general, 1, 1, 0.0);

   lll = cfdata->cfg_options;

   EINA_LIST_FOREACH(optgroups, l, group)
     {
        Evas_Object *grp;

        grp = e_widget_framelist_add(evas, group->description, 0);

        EINA_LIST_FOREACH(group->options, ll, option)
          {
             Evas_Object *chk;

             chk = e_widget_check_add(evas, option->description,
                                     &(((E_XKB_Dialog_Option *)
                                        eina_list_data_get(lll))->enabled));
             e_widget_check_widget_disable_on_checked_add(dont_touch_my_damn_keyboard, chk);
             e_widget_framelist_object_append(grp, chk);
             lll = eina_list_next(lll);
          }
        e_widget_list_object_append(options, grp, 1, 1, 0.0);
     }

   e_widget_size_min_get(options, &mw, &mh);

   if (mw < 220) mw = 220;
   if (mh < 160) mh = 160;

   evas_object_size_hint_min_set(options, mw, mh);
   E_EXPAND(options);
   E_FILL(options);

   scroller = elm_scroller_add(e_win_evas_win_get(evas));
   E_EXPAND(scroller);
   E_FILL(scroller);
   elm_scroller_bounce_set(scroller, 0, 0);
   elm_object_content_set(scroller, options);
   e_widget_sub_object_add(mainn, options);

   e_widget_toolbook_page_append(mainn, NULL, _("Options"), scroller, 1, 1, 1, 1, 0.5, 0.0);

   /* Display the first page by default */
   e_widget_toolbook_page_show(mainn, 0);

   /* The main evas */
   cfdata->evas = evas;

   /* Clear up any previous timer */
   if (cfdata->fill_delay)
     ecore_timer_del(cfdata->fill_delay);

   /* Trigger the fill */
   cfdata->fill_delay = ecore_timer_add(0.2, _cb_fill_delay, cfdata);

   return mainn;
}

static void
_cb_add(void *data, void *data2 EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata;
   if (!(cfdata = data)) return;

   if (cfdata->dlg_add_new) elm_win_raise(cfdata->dlg_add_new->win);
   else cfdata->dlg_add_new = _dlg_add_new(cfdata);
}

static void
_cb_del(void *data, void *data2 EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata;
   int n = 0;

   if (!(cfdata = data)) return;
   if ((n = e_widget_ilist_selected_get(cfdata->used_list)) < 0) return;

   cfdata->cfg_layouts = eina_list_remove_list(cfdata->cfg_layouts, eina_list_nth_list(cfdata->cfg_layouts, n));

   /* Update the list */
   evas_event_freeze(cfdata->evas);
   edje_freeze();
   e_widget_ilist_freeze(cfdata->used_list);
   e_widget_ilist_remove_num(cfdata->used_list, n);
   e_widget_ilist_go(cfdata->used_list);
   e_widget_ilist_thaw(cfdata->used_list);
   edje_thaw();
   evas_event_thaw(cfdata->evas);
}

static void
_cb_up(void *data, void *data2 EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata;
   void *nddata;
   Evas_Object *ic;
   Eina_List *l;
   const char *lbl, *file;
   int n;

   if (!(cfdata = data)) return;
   if ((n = e_widget_ilist_selected_get(cfdata->used_list)) < 0) return;

   l = eina_list_nth_list(cfdata->cfg_layouts, n);

   nddata = eina_list_data_get(eina_list_prev(l));
   eina_list_data_set(eina_list_prev(l), eina_list_data_get(l));
   eina_list_data_set(l, nddata);

   /* Update the list */

   evas_event_freeze(cfdata->evas);
   edje_freeze();
   e_widget_ilist_freeze(cfdata->used_list);

   ic = e_icon_add(cfdata->evas);
   e_icon_file_get(e_widget_ilist_nth_icon_get(cfdata->used_list, n), &file, NULL);
   e_icon_file_set(ic, file);
   lbl = e_widget_ilist_nth_label_get(cfdata->used_list, n);
   e_widget_ilist_prepend_relative_full(cfdata->used_list, ic, NULL, lbl, _cb_used_select, cfdata, NULL, (n - 1));
   e_widget_ilist_remove_num(cfdata->used_list, n);

   e_widget_ilist_go(cfdata->used_list);
   e_widget_ilist_thaw(cfdata->used_list);
   edje_thaw();
   evas_event_thaw(cfdata->evas);

   e_widget_ilist_selected_set(cfdata->used_list, (n - 1));
}

static void
_cb_dn(void *data, void *data2 EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata;
   void *nddata;
   Evas_Object *ic;
   Eina_List *l;
   const char *lbl, *file;
   int n;

   if (!(cfdata = data)) return;
   if ((n = e_widget_ilist_selected_get(cfdata->used_list)) < 0) return;

   l = eina_list_nth_list(cfdata->cfg_layouts, n);

   nddata = eina_list_data_get(eina_list_next(l));
   eina_list_data_set(eina_list_next(l), eina_list_data_get(l));
   eina_list_data_set(l, nddata);

   /* Update the list */

   evas_event_freeze(cfdata->evas);
   edje_freeze();
   e_widget_ilist_freeze(cfdata->used_list);

   ic = e_icon_add(cfdata->evas);
   e_icon_file_get(e_widget_ilist_nth_icon_get(cfdata->used_list, n), &file, NULL);
   e_icon_file_set(ic, file);
   lbl = e_widget_ilist_nth_label_get(cfdata->used_list, n);
   e_widget_ilist_append_relative_full(cfdata->used_list, ic, NULL, lbl, _cb_used_select, cfdata, NULL, n);
   e_widget_ilist_remove_num(cfdata->used_list, n);

   e_widget_ilist_go(cfdata->used_list);
   e_widget_ilist_thaw(cfdata->used_list);
   edje_thaw();
   evas_event_thaw(cfdata->evas);

   e_widget_ilist_selected_set(cfdata->used_list, (n + 1));
}

static E_Dialog *
_dlg_add_new(E_Config_Dialog_Data *cfdata)
{
   E_Dialog *dlg;
   Evas *evas;
   Evas_Coord mw, mh;
   Evas_Object *mainn, *available, *modelss, *variants;

   if (!(dlg = e_dialog_new(NULL, "E", "xkbswitch_config_add_dialog"))) return NULL;

   e_dialog_resizable_set(dlg, 1);
   dlg->data = cfdata;
   
   e_object_del_attach_func_set(E_OBJECT(dlg), _dlg_add_cb_del);
   elm_win_center(dlg->win, 1, 1);

   evas = evas_object_evas_get(dlg->win);
   e_dialog_title_set(dlg, _("Add New Configuration"));

   /* The main toolbook, holds the lists and tabs */
   mainn = e_widget_toolbook_add(evas, 24, 24);
   /* Holds the available layouts */
   available = e_widget_ilist_add(evas, 32, 32, NULL);

   e_widget_size_min_set(available, 220, 160);
   e_widget_ilist_go(available);
   e_widget_toolbook_page_append(mainn, NULL, _("Available"), available, 1, 1, 1, 1, 0.5, 0.0);
   cfdata->layout_list = available;

   /* Holds the available models */
   modelss = e_widget_ilist_add(evas, 32, 32, NULL);
   e_widget_toolbook_page_append(mainn, NULL, _("Model"), modelss, 1, 1, 1, 1, 0.5, 0.0);
   cfdata->model_list = modelss;

   /* Holds the available variants */
   variants = e_widget_ilist_add(evas, 32, 32, NULL);
   e_widget_toolbook_page_append(mainn, NULL, _("Variant"), variants, 1, 1, 1, 1, 0.5, 0.0);
   cfdata->variant_list = variants;
   e_widget_toolbook_page_show(mainn, 0);

   e_widget_size_min_get(mainn, &mw, &mh);
   e_dialog_content_set(dlg, mainn, mw, mh);

   cfdata->dlg_evas = evas;

   /* Clear up any previous timer */
   if (cfdata->dlg_fill_delay) ecore_timer_del(cfdata->dlg_fill_delay);

   /* Trigger the fill */
   cfdata->dlg_fill_delay = ecore_timer_add(0.2, _cb_dlg_fill_delay, cfdata);

   /* Some buttons */
   e_dialog_button_add(dlg, _("OK"), NULL, _dlg_add_cb_ok, cfdata);
   e_dialog_button_add(dlg, _("Cancel"), NULL, _dlg_add_cb_cancel, cfdata);

   e_dialog_button_disable_num_set(dlg, 0, 1);
   e_dialog_button_disable_num_set(dlg, 1, 0);

   e_dialog_show(dlg);

   return dlg;
}

static void
_dlg_add_cb_ok(void *data EINA_UNUSED, E_Dialog *dlg)
{
   E_Config_Dialog_Data *cfdata = dlg->data;
   E_Config_XKB_Layout *cl;
   char buf[4096];
   Evas_Object *ic;
   /* Configuration information */
   Eina_Stringshare *layout, *model, *variant;

   layout = e_widget_ilist_selected_value_get(cfdata->layout_list);
   model = e_widget_ilist_selected_value_get(cfdata->model_list);
   variant = e_widget_ilist_selected_value_get(cfdata->variant_list);

   /* The new configuration */
   cl = E_NEW(E_Config_XKB_Layout, 1);
   cl->name = eina_stringshare_ref(layout);
   cl->model = eina_stringshare_ref(model);
   cl->variant = eina_stringshare_ref(variant);

   cfdata->cfg_layouts = eina_list_append(cfdata->cfg_layouts, cl);

   /* Update the main list */
   evas_event_freeze(cfdata->evas);
   edje_freeze();
   e_widget_ilist_freeze(cfdata->used_list);

   ic = e_icon_add(cfdata->evas);

   e_xkb_e_icon_flag_setup(ic, cl->name);
   snprintf(buf, sizeof(buf), "%s (%s, %s)",
            cl->name, cl->model, cl->variant);
   e_widget_ilist_append_full(cfdata->used_list, ic, NULL, buf,
                              _cb_used_select, cfdata, NULL);

   e_widget_ilist_go(cfdata->used_list);
   e_widget_ilist_thaw(cfdata->used_list);
   edje_thaw();
   evas_event_thaw(cfdata->evas);

   cfdata->dlg_add_new = NULL;
   e_object_unref(E_OBJECT(dlg));
   e_config_dialog_changed_set(cfdata->cfd, 1);
}

static void
_dlg_add_cb_cancel(void *data EINA_UNUSED, E_Dialog *dlg)
{
   E_Config_Dialog_Data *cfdata = dlg->data;
   cfdata->dlg_add_new = NULL;
   e_object_unref(E_OBJECT(dlg));
}

static void
_dlg_add_cb_del(void *obj)
{
   E_Dialog *dlg = obj;
   E_Config_Dialog_Data *cfdata = dlg->data;
   cfdata->dlg_add_new = NULL;
   e_object_unref(E_OBJECT(dlg));
}

static Eina_Bool
_cb_dlg_fill_delay(void *data)
{
   E_Config_Dialog_Data *cfdata;
   Eina_List *l;
   E_XKB_Layout *layout;
   char buf[4096];

   if (!(cfdata = data)) return ECORE_CALLBACK_RENEW;

   /* Update the list of available layouts */
   evas_event_freeze(cfdata->dlg_evas);
   edje_freeze();

   e_widget_ilist_freeze(cfdata->layout_list);
   e_widget_ilist_clear(cfdata->layout_list);

   EINA_LIST_FOREACH(layouts, l, layout)
     {
        Evas_Object *ic;

        ic = e_icon_add(cfdata->dlg_evas);
        e_xkb_e_icon_flag_setup(ic, layout->name);
        snprintf(buf, sizeof(buf), "%s (%s)",
                 layout->description, layout->name);
        e_widget_ilist_append_full(cfdata->layout_list, ic, NULL, buf,
                                   _cb_layout_select, cfdata, layout->name);
     }

   e_widget_ilist_go(cfdata->layout_list);
   e_widget_ilist_thaw(cfdata->layout_list);

   edje_thaw();
   evas_event_thaw(cfdata->dlg_evas);

   cfdata->dlg_fill_delay = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_cb_layout_select(void *data)
{
   E_Config_Dialog_Data *cfdata;
   E_XKB_Variant *variant;
   E_XKB_Layout *layout;
   E_XKB_Model *model;
   Eina_List *l;
   const char *label;
   int n;
   char buf[4096];

   if (!(cfdata = data)) return;

   /* Find the right layout */

   if ((n = e_widget_ilist_selected_get(cfdata->layout_list)) < 0)
     return;
   if (!(label = e_widget_ilist_nth_label_get(cfdata->layout_list, n)))
     return;

   if (!(layout = eina_list_search_unsorted
             (layouts, layout_sort_by_name_cb,
             e_widget_ilist_nth_value_get(cfdata->layout_list, n)
             ))) return;

   /* Update the lists */
   evas_event_freeze(cfdata->dlg_evas);
   edje_freeze();

   /* Models */
   e_widget_ilist_freeze(cfdata->model_list);
   e_widget_ilist_clear(cfdata->model_list);

   EINA_LIST_FOREACH(models, l, model)
     {
        snprintf(buf, sizeof(buf), "%s (%s)", model->description, model->name);
        e_widget_ilist_append(cfdata->model_list, NULL, buf, NULL, cfdata, model->name);
     }

   e_widget_ilist_go(cfdata->model_list);
   e_widget_ilist_thaw(cfdata->model_list);

   /* Variants */
   e_widget_ilist_freeze(cfdata->variant_list);
   e_widget_ilist_clear(cfdata->variant_list);

   EINA_LIST_FOREACH(layout->variants, l, variant)
     {
        snprintf(buf, sizeof(buf), "%s (%s)", variant->name, variant->description);
        e_widget_ilist_append(cfdata->variant_list, NULL, buf, NULL, cfdata, variant->name);
     }

   e_widget_ilist_go(cfdata->variant_list);
   e_widget_ilist_thaw(cfdata->variant_list);

   edje_thaw();
   evas_event_thaw(cfdata->dlg_evas);

   e_widget_ilist_selected_set(cfdata->model_list, 0);
   e_widget_ilist_selected_set(cfdata->variant_list, 0);

   e_dialog_button_disable_num_set(cfdata->dlg_add_new, 0, 0);
}

static Eina_Bool
_cb_fill_delay(void *data)
{
   E_Config_Dialog_Data *cfdata;
   Eina_List *l;
   E_Config_XKB_Layout *cl;
   E_XKB_Model *model;
   int n = 0;
   char buf[4096];

   if (!(cfdata = data)) return ECORE_CALLBACK_RENEW;

   /* Update the list of used layouts */
   evas_event_freeze(cfdata->evas);
   edje_freeze();

   e_widget_ilist_freeze(cfdata->used_list);
   e_widget_ilist_clear(cfdata->used_list);

   EINA_LIST_FOREACH(cfdata->cfg_layouts, l, cl)
     {
        Evas_Object *ic = e_icon_add(cfdata->evas);
        const char *name = cl->name;

        e_xkb_e_icon_flag_setup(ic, name);
        snprintf(buf, sizeof(buf), "%s (%s, %s)",
                 cl->name, cl->model, cl->variant);
        e_widget_ilist_append_full(cfdata->used_list, ic, NULL, buf,
                                   _cb_used_select, cfdata, NULL);
     }

   e_widget_ilist_go(cfdata->used_list);
   e_widget_ilist_thaw(cfdata->used_list);

   e_widget_ilist_freeze(cfdata->dmodel_list);
   e_widget_ilist_clear(cfdata->dmodel_list);

   /* Update the global model list */
   EINA_LIST_FOREACH(models, l, model)
     {
        snprintf(buf, sizeof(buf), "%s (%s)", model->description, model->name);
        e_widget_ilist_append(cfdata->dmodel_list, NULL, buf, NULL,
                              cfdata, model->name);
        if (model->name == e_config->xkb.default_model)
          e_widget_ilist_selected_set(cfdata->dmodel_list, n);
        n++;
     }

   e_widget_ilist_go(cfdata->dmodel_list);
   e_widget_ilist_thaw(cfdata->dmodel_list);
   edje_thaw();
   evas_event_thaw(cfdata->evas);

   cfdata->fill_delay = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_cb_used_select(void *data)
{
   E_Config_Dialog_Data *cfdata;
   int n, c;

   if (!(cfdata = data)) return;
   if ((n = e_widget_ilist_selected_get(cfdata->used_list)) < 0) return;

   c = e_widget_ilist_count(cfdata->used_list);
   e_widget_disabled_set(cfdata->btn_del, EINA_FALSE);

   if (c == 1)
     {
        e_widget_disabled_set(cfdata->btn_up, EINA_TRUE);
        e_widget_disabled_set(cfdata->btn_down, EINA_TRUE);
     }
   else if (n == (c - 1))
     {
        e_widget_disabled_set(cfdata->btn_up, EINA_FALSE);
        e_widget_disabled_set(cfdata->btn_down, EINA_TRUE);
     }
   else if (n == 0)
     {
        e_widget_disabled_set(cfdata->btn_up, EINA_TRUE);
        e_widget_disabled_set(cfdata->btn_down, EINA_FALSE);
     }
   else
     {
        e_widget_disabled_set(cfdata->btn_up, EINA_FALSE);
        e_widget_disabled_set(cfdata->btn_down, EINA_FALSE);
     }
}

