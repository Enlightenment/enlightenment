#include "e.h"

static void *_create_data(E_Config_Dialog *cfd);
static void _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static int _basic_apply_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create_widgets(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static int _basic_check_changed(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);

struct _E_Config_Dialog_Data
{
   E_Config_Dialog *cfd;

   int        show_cursor;
   int        idle_cursor;
   int        use_e_cursor;
   int        cursor_size;

   struct
   {
      Evas_Object *idle_cursor;
   } gui;

   int mouse_hand;
   double numerator;
   double denominator;
   double threshold;
};

E_Config_Dialog *
e_int_config_mouse(Evas_Object *parent EINA_UNUSED, const char *params __UNUSED__)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;

   if (e_config_dialog_find("E", "keyboard_and_mouse/mouse_settings"))
     return NULL;

   v = E_NEW(E_Config_Dialog_View, 1);
   if (!v) return NULL;
   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply_data;
   v->basic.create_widgets = _basic_create_widgets;
   v->basic.check_changed = _basic_check_changed;

   cfd = e_config_dialog_new(NULL, _("Mouse Settings"), "E",
			     "keyboard_and_mouse/mouse_settings",
			     "preferences-desktop-mouse", 0, v, NULL);
   return cfd;
}

static void
_fill_data(E_Config_Dialog_Data *cfdata)
{
   cfdata->show_cursor = e_config->show_cursor;
   cfdata->idle_cursor = e_config->idle_cursor;
   cfdata->use_e_cursor = e_config->use_e_cursor;
   cfdata->cursor_size = e_config->cursor_size;

   cfdata->mouse_hand = e_config->mouse_hand;
   cfdata->numerator = e_config->mouse_accel_numerator;
   cfdata->denominator = e_config->mouse_accel_denominator;
   cfdata->threshold = e_config->mouse_accel_threshold;
}

static void *
_create_data(E_Config_Dialog *cfd)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
   cfdata->cfd = cfd;

   _fill_data(cfdata);
   return cfdata;
}

static int
_basic_check_changed(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   return !((cfdata->show_cursor == e_config->show_cursor) &&
	    (cfdata->idle_cursor == e_config->idle_cursor) &&
	    (cfdata->use_e_cursor == e_config->use_e_cursor) &&
	    (cfdata->cursor_size == e_config->cursor_size) &&
	    (cfdata->mouse_hand == e_config->mouse_hand) &&
	    (cfdata->numerator == e_config->mouse_accel_numerator) &&
	    (cfdata->denominator == e_config->mouse_accel_denominator) &&
	    (cfdata->threshold == e_config->mouse_accel_threshold));
}

static void
_free_data(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   E_FREE(cfdata);
}

/* advanced window */
static int
_basic_apply_data(E_Config_Dialog *cfd __UNUSED__, E_Config_Dialog_Data *cfdata)
{
   const Eina_List *l;
   E_Comp *comp;

   e_config->use_e_cursor = cfdata->use_e_cursor;
   e_config->show_cursor = cfdata->show_cursor;
   e_config->idle_cursor = cfdata->idle_cursor;
   e_config->cursor_size = cfdata->cursor_size;

   e_config->mouse_hand = cfdata->mouse_hand;
   e_config->mouse_accel_numerator = cfdata->numerator;
   e_config->mouse_accel_denominator = cfdata->denominator;
   e_config->mouse_accel_threshold = cfdata->threshold;
   e_config_save_queue();

   /* Apply the above settings */
   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     {
        if ((comp->comp_type == E_PIXMAP_TYPE_X) && (!e_config->show_cursor))
          e_pointer_hide(comp->pointer);
        else
          e_pointers_size_set(e_config->cursor_size);
     }

   e_mouse_update();

   return 1;
}


static void
_use_e_cursor_cb_change(void *data, Evas_Object *obj __UNUSED__)
{
   E_Config_Dialog_Data *cfdata = data;
   Eina_Bool disabled = ((!cfdata->use_e_cursor) || (!cfdata->show_cursor));
   e_widget_disabled_set(cfdata->gui.idle_cursor, disabled);
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd __UNUSED__, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *otb, *ol, *of, *ob, *oc;
   E_Radio_Group *rg;

   otb = e_widget_toolbook_add(evas, (24 * e_scale), (24 * e_scale));

   /* Cursor */
   ol = e_widget_list_add(evas, 0, 0);

   oc = e_widget_check_add(evas, _("Show Cursor"), &(cfdata->show_cursor));
   e_widget_list_object_append(ol, oc, 1, 0, 0.5);

   of = e_widget_framelist_add(evas, _("Settings"), 0);
   rg = e_widget_radio_group_new(&cfdata->use_e_cursor);

   ob = e_widget_label_add(evas, _("Size"));
   e_widget_framelist_object_append(of, ob);
   e_widget_check_widget_disable_on_unchecked_add(oc, ob);

   ob = e_widget_slider_add(evas, 1, 0, _("%1.0f pixels"),
                            8, 128, 4, 0, NULL, &(cfdata->cursor_size), 100);
   e_widget_framelist_object_append(of, ob);
   e_widget_check_widget_disable_on_unchecked_add(oc, ob);

   ob = e_widget_label_add(evas, _("Theme"));
   e_widget_framelist_object_append(of, ob);
   e_widget_check_widget_disable_on_unchecked_add(oc, ob);

   ob = e_widget_radio_add(evas, _("X"), 0, rg);
   e_widget_on_change_hook_set(ob, _use_e_cursor_cb_change, cfdata);
   e_widget_framelist_object_append(of, ob);
   e_widget_check_widget_disable_on_unchecked_add(oc, ob);

   ob = e_widget_radio_add(evas, _("Enlightenment"), 1, rg);
   e_widget_on_change_hook_set(ob, _use_e_cursor_cb_change, cfdata);
   e_widget_framelist_object_append(of, ob);
   e_widget_check_widget_disable_on_unchecked_add(oc, ob);
   e_widget_on_disable_hook_set(ob, _use_e_cursor_cb_change, cfdata);

   ob = e_widget_check_add(evas, _("Idle effects"),
                           &(cfdata->idle_cursor));
   e_widget_framelist_object_append(of, ob);
   cfdata->gui.idle_cursor = ob;

   e_widget_list_object_append(ol, of, 1, 0, 0.5);
   e_widget_toolbook_page_append(otb, NULL, _("Cursor"), ol, 
                                 1, 0, 1, 0, 0.5, 0.0);

   /* Mouse */
   /* TODO: Get all inputs and hide this tab if none is relative. */
   ol = e_widget_list_add(evas, 0, 0);

   of = e_widget_frametable_add(evas, _("Mouse Hand"), 0);
   rg = e_widget_radio_group_new(&(cfdata->mouse_hand));
   ob = e_widget_radio_icon_add(evas, NULL, "preferences-desktop-mouse-right", 48, 48, E_MOUSE_HAND_LEFT, rg);
   e_widget_frametable_object_append(of, ob, 0, 0, 1, 1, 1, 1, 1, 1);
   ob = e_widget_radio_icon_add(evas, NULL, "preferences-desktop-mouse-left", 48, 48, E_MOUSE_HAND_RIGHT, rg);
   e_widget_frametable_object_append(of, ob, 1, 0, 1, 1, 1, 1, 1, 1);
   e_widget_list_object_append(ol, of, 1, 0, 0.5);

   of = e_widget_framelist_add(evas, _("Mouse Acceleration"), 0);

   ob = e_widget_label_add(evas, _("Acceleration"));
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_slider_add(evas, 1, 0, _("%1.0f"), 1.0, 30.0, 1.0, 0,
			    &(cfdata->numerator), NULL, 100);
   e_widget_framelist_object_append(of, ob);

   ob = e_widget_label_add(evas, _("Threshold"));
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_slider_add(evas, 1, 0, _("%1.0f"), 1.0, 10.0, 1.0, 0,
			    &(cfdata->threshold), NULL, 100);
   e_widget_framelist_object_append(of, ob);

   e_widget_list_object_append(ol, of, 1, 0, 0.5);
   e_widget_toolbook_page_append(otb, NULL, _("Mouse"), ol, 
                                 1, 0, 1, 0, 0.5, 0.0);

   e_widget_toolbook_page_show(otb, 0);
   return otb;
}
