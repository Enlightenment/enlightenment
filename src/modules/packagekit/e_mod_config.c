#include "e_mod_config.h"
#include "e_mod_packagekit.h"


struct _E_Config_Dialog_Data
{
   int update_interval;
   char *manager_command;
   int show_description;
};

static void *
_cfg_data_create(E_Config_Dialog *cfd)
{
   E_PackageKit_Module_Context *ctxt = cfd->data;

   E_Config_Dialog_Data *cfdata = E_NEW(E_Config_Dialog_Data, 1);
   cfdata->update_interval = ctxt->config->update_interval;
   cfdata->show_description = ctxt->config->show_description;
   if (ctxt->config->manager_command)
      cfdata->manager_command = strdup(ctxt->config->manager_command);
   else
      cfdata->manager_command = strdup("");

   return cfdata;
}

static void
_cfg_data_free(E_Config_Dialog *cfd EINA_UNUSED, E_Config_Dialog_Data *cfdata)
{
   E_FREE(cfdata->manager_command);
   E_FREE(cfdata);
}

static Evas_Object *
_cfg_widgets_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata)
{
   Evas_Object *list, *of, *ob;
   E_Radio_Group *rg;

   list = e_widget_list_add(evas, 0, 0);

   of = e_widget_framelist_add(evas, _("Refresh Packages"), 0);
   rg = e_widget_radio_group_new(&(cfdata->update_interval));
   ob = e_widget_radio_add(evas, _("Never"), 0, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("Hourly"), 60, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("Daily"), 60 * 24, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("Weekly"), 60 * 24 * 7, rg);
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(list, of, 1, 1, 0.5);

   of = e_widget_framelist_add(evas, _("Package list"), 0);
   rg = e_widget_radio_group_new(&(cfdata->show_description));
   ob = e_widget_radio_add(evas, _("Show package name"), 0, rg);
   e_widget_framelist_object_append(of, ob);
   ob = e_widget_radio_add(evas, _("Show package description"), 1, rg);
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(list, of, 1, 0, 0.5);

   of = e_widget_framelist_add(evas, _("Package Manager"), 0);
   ob = e_widget_entry_add(cfd->dia->win, &(cfdata->manager_command), NULL, NULL, NULL);
   e_widget_framelist_object_append(of, ob);
   e_widget_list_object_append(list, of, 1, 0, 0.5);

   e_dialog_resizable_set(cfd->dia, 1);

   return list;
}

static int
_cfg_check_changed(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
   E_PackageKit_Module_Context *ctxt = cfd->data;

   if (ctxt->config->update_interval != cfdata->update_interval)
     return 1;

   if (ctxt->config->show_description != cfdata->show_description)
     return 1;

   if ((ctxt->config->manager_command) && (cfdata->manager_command) &&
       (strcmp(ctxt->config->manager_command, cfdata->manager_command)))
     return 1;

   if ((!ctxt->config->manager_command) && (cfdata->manager_command[0]))
     return 1;

   return 0;
}

static int
_cfg_data_apply(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata)
{
   E_PackageKit_Module_Context *ctxt = cfd->data;

   ctxt->config->update_interval = cfdata->update_interval;
   ctxt->config->show_description = cfdata->show_description;

   if (ctxt->config->manager_command)
      eina_stringshare_replace(&ctxt->config->manager_command, cfdata->manager_command);
   else
      ctxt->config->manager_command = eina_stringshare_add(cfdata->manager_command);

   return 1;
}

void
packagekit_config_show(E_PackageKit_Module_Context *ctxt)
{
   E_Config_Dialog_View *v;

   v = E_NEW(E_Config_Dialog_View, 1);
   v->create_cfdata = _cfg_data_create;
   v->free_cfdata = _cfg_data_free;
   v->basic.create_widgets = _cfg_widgets_create;
   v->basic.apply_cfdata = _cfg_data_apply;
   v->basic.check_changed = _cfg_check_changed;

   e_config_dialog_new(NULL, _("System Updates Settings"),
                       "E", "_e_mod_packagekit_dialog", NULL, 0, v, ctxt);
}
