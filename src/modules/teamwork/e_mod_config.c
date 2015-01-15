#include "e_mod_main.h"

struct _E_Config_Dialog_Data
{
   int disable_media_fetch;
   int disable_video;
   double allowed_media_size;
   double allowed_media_fetch_size;
   double allowed_media_age;

   double mouse_out_delay;
   double popup_size;
   double popup_opacity;
};

static void *
_create_data(E_Config_Dialog *cfd EINA_UNUSED)
{
   E_Config_Dialog_Data *cfdata;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);
#define SET(X) \
  cfdata->X = tw_config->X
   SET(disable_media_fetch);
   SET(disable_video);
   SET(allowed_media_size);
   SET(allowed_media_fetch_size);
   SET(allowed_media_age);

   SET(mouse_out_delay);
   SET(popup_size);
   SET(popup_opacity);
#undef SET
   return cfdata;
}

static void
_free_data(E_Config_Dialog *cfd  EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   tw_mod->cfd = NULL;
   free(cfdata);
}

static int
_basic_check_changed(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
#define CHECK(X) \
   if (cfdata->X != tw_config->X) return 1

   CHECK(disable_media_fetch);
   CHECK(disable_video);
   if (lround(cfdata->allowed_media_age) != tw_config->allowed_media_age) return 1;
   if (lround(cfdata->allowed_media_size) != tw_config->allowed_media_size) return 1;
   if (lround(cfdata->allowed_media_fetch_size) != tw_config->allowed_media_fetch_size) return 1;

   if (fabs(cfdata->mouse_out_delay - tw_config->mouse_out_delay) > 0.45) return 1;

   if (fabs(cfdata->popup_size - tw_config->popup_size) > 0.9) return 1;
   if (fabs(cfdata->popup_opacity - tw_config->popup_opacity) > 0.9) return 1;

#undef CHECK
   return 0;
}

static Evas_Object *
_basic_create_widgets(E_Config_Dialog *cfd EINA_UNUSED,
                      Evas *evas,
                      E_Config_Dialog_Data *cfdata)
{
   Evas_Object *ob, *ol, *otb, *tab, *oc;

   tab = e_widget_table_add(evas, 0);

   otb = e_widget_toolbook_add(evas, 48 * e_scale, 48 * e_scale);

   ///////////////////////////////////////////

   ol = e_widget_list_add(evas, 0, 0);

   ob = e_widget_check_add(evas, _("Disable remote media fetching"), &cfdata->disable_media_fetch);
   e_widget_list_object_append(ol, ob, 1, 0, 0.5);

   oc = e_widget_label_add(evas, _("Maximum media size to fetch"));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   oc = e_widget_slider_add(evas, 1, 0, _("%2.0f MiB"), 1, 50, 1, 0, &cfdata->allowed_media_fetch_size, NULL, 150);
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   e_widget_check_widget_disable_on_checked_add(ob, oc);

   oc = e_widget_label_add(evas, _("Maximum media cache size in RAM"));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   oc = e_widget_slider_add(evas, 1, 0, _("%4.0f MiB"), 0, 1024, 16, 0, &cfdata->allowed_media_size, NULL, 150);
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   e_widget_check_widget_disable_on_checked_add(ob, oc);

   oc = e_widget_label_add(evas, _("Maximum media cache age on disk"));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   oc = e_widget_slider_add(evas, 1, 0, _("%3.0f Days"), -1, 180, 1, 0, &cfdata->allowed_media_age, NULL, 150);
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   e_widget_check_widget_disable_on_checked_add(ob, oc);

   e_widget_toolbook_page_append(otb, NULL, _("Cache"), ol, 1, 1, 1, 1, 0.5, 0.5);

   ///////////////////////////////////////////

   ol = e_widget_list_add(evas, 0, 0);

   ob = e_widget_check_add(evas, _("Disable video previews"), &cfdata->disable_video);
   e_widget_list_object_append(ol, ob, 1, 0, 0.5);

   oc = e_widget_label_add(evas, _("Mouse-out hide delay"));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   oc = e_widget_slider_add(evas, 1, 0, _("%1.1f seconds"), 0, 5, 0.5, 0, &cfdata->mouse_out_delay, NULL, 150);
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   e_widget_check_widget_disable_on_checked_add(ob, oc);

   oc = e_widget_label_add(evas, _("Maximum size (Percentage of screens size)"));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   oc = e_widget_slider_add(evas, 1, 0, _("%3.0f"), 10, 100, 1, 0, &cfdata->popup_size, NULL, 150);
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   e_widget_check_widget_disable_on_checked_add(ob, oc);

   oc = e_widget_label_add(evas, _("Opacity"));
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   oc = e_widget_slider_add(evas, 1, 0, _("%3.0f"), 10, 100, 1, 0, &cfdata->popup_opacity, NULL, 150);
   e_widget_list_object_append(ol, oc, 1, 1, 0.5);
   e_widget_check_widget_disable_on_checked_add(ob, oc);

   e_widget_toolbook_page_append(otb, NULL, _("Popups"), ol, 1, 1, 1, 1, 0.5, 0.5);

   e_widget_toolbook_page_show(otb, 0);

   e_widget_table_object_append(tab, otb, 0, 0, 1, 1, 1, 1, 1, 1);
   return tab;
}


static int
_basic_apply_data(E_Config_Dialog *cfd  EINA_UNUSED,
                  E_Config_Dialog_Data *cfdata)
{

#define SET(X) tw_config->X = cfdata->X
   SET(disable_media_fetch);
   SET(disable_video);
   SET(allowed_media_size);
   SET(allowed_media_fetch_size);
   SET(allowed_media_age);

   SET(mouse_out_delay);
   SET(popup_size);
   if (fabs(cfdata->popup_opacity - tw_config->popup_opacity) > 0.9)
     {
        SET(popup_opacity);
        tw_popup_opacity_set();
     }

   e_config_save_queue();
   return 1;
}

EINTERN E_Config_Dialog *
e_int_config_teamwork_module(Evas_Object *parent EINA_UNUSED, const char *params EINA_UNUSED)
{
   E_Config_Dialog *cfd;
   E_Config_Dialog_View *v;
   char buf[4096];

   if (tw_mod->cfd) return NULL;
   snprintf(buf, sizeof(buf), "%s/e-module-teamwork.edj", e_module_dir_get(tw_mod->module));
   v = E_NEW(E_Config_Dialog_View, 1);

   v->create_cfdata = _create_data;
   v->free_cfdata = _free_data;
   v->basic.apply_cfdata = _basic_apply_data;
   v->basic.create_widgets = _basic_create_widgets;
   v->basic.check_changed = _basic_check_changed;

   cfd = e_config_dialog_new(NULL, _("Teamwork Settings"),
                             "E", "applications/teamwork", buf, 32, v, tw_mod);
   tw_mod->cfd = cfd;
   return cfd;
}
