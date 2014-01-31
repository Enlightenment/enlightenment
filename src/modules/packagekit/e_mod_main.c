#include <e.h>
#include <Eldbus.h>
#include "e_mod_main.h"
#include "e_mod_packagekit.h"

#define PACKAGEKIT_DOMAIN "module.packagekit"

static E_Module *packagekit_mod = NULL;


/* Module Configuration */
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
   ob = e_widget_entry_add(evas, &(cfdata->manager_command), NULL, NULL, NULL);
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

static void
_cfg_menu_cb(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   E_Config_Dialog_View *v;

   v = E_NEW(E_Config_Dialog_View, 1);
   v->create_cfdata = _cfg_data_create;
   v->free_cfdata = _cfg_data_free;
   v->basic.create_widgets = _cfg_widgets_create;
   v->basic.apply_cfdata = _cfg_data_apply;
   v->basic.check_changed = _cfg_check_changed;

   e_config_dialog_new(e_util_comp_current_get(), _("System Updates Settings"),
                       "E", "_e_mod_packagekit_dialog", NULL, 0, v, data);
}

void
packagekit_mouse_down_cb(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_PackageKit_Instance *inst = data;
   E_PackageKit_Module_Context *ctxt = packagekit_mod->data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->button == 1)
     {
        if (inst->popup)
          packagekit_popup_del(inst);
        else
          packagekit_popup_new(inst);
     }
   else if (ev->button == 2)
     {
        packagekit_create_transaction_and_exec(ctxt, packagekit_get_updates);
     }
   else if (ev->button == 3)
     {
        E_Menu *m;
        E_Menu_Item *mi;
        E_Zone *zone = e_util_zone_current_get(e_manager_current_get());
        int x, y;

        if (inst->popup)
          packagekit_popup_del(inst);

        m = e_menu_new();
        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _cfg_menu_cb, inst->ctxt);

        m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);

        e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);
        e_menu_activate_mouse(m, zone, (x + ev->output.x),(y + ev->output.y),
                              1, 1, E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
        evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button,
                                 EVAS_BUTTON_NONE, ev->timestamp, NULL);
     }
}


/* Gadcon Api Functions */
static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   E_PackageKit_Instance *inst;
   E_PackageKit_Module_Context *ctxt = packagekit_mod->data;

   inst = E_NEW(E_PackageKit_Instance, 1);
   inst->ctxt = ctxt;
   inst->gadget = edje_object_add(gc->evas);
   e_theme_edje_object_set(inst->gadget, "base/theme/modules/packagekit",
                                         "e/modules/packagekit/main");
   inst->gcc = e_gadcon_client_new(gc, name, id, style, inst->gadget);
   inst->gcc->data = inst;
   evas_object_event_callback_add(inst->gadget, EVAS_CALLBACK_MOUSE_DOWN,
                                  packagekit_mouse_down_cb, inst);
   ctxt->instances = eina_list_append(ctxt->instances, inst);
   return inst->gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   E_PackageKit_Instance *inst = gcc->data;
   E_PackageKit_Module_Context *ctxt = packagekit_mod->data;

   E_FREE_FUNC(inst->gadget, evas_object_del);
   if (inst->popup) packagekit_popup_del(inst);
   ctxt->instances = eina_list_remove(ctxt->instances, inst);
   free(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   e_gadcon_client_aspect_set(gcc, 16, 16);
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _("System Updates");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   char buf[PATH_MAX];
   Evas_Object *o;

   EINA_SAFETY_ON_NULL_RETURN_VAL(packagekit_mod, NULL);
   snprintf(buf, sizeof(buf), "%s/e-module-packagekit.edj",
            e_module_dir_get(packagekit_mod));
   o = edje_object_add(evas);
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   E_PackageKit_Module_Context *ctxt = packagekit_mod->data;
   static char buf[64];
   snprintf(buf, sizeof(buf), "packagekit.%d", eina_list_count(ctxt->instances));
   return buf;
}

static const E_Gadcon_Client_Class _gc_class =
{
   GADCON_CLIENT_CLASS_VERSION, "PackageKit",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};


/* E Module Api Functions */
EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "PackageKit" };

Eina_Bool
_timer_cb(void *data)
{
   E_PackageKit_Module_Context *ctxt = data;
   double elapsed;

   if (ctxt->config->update_interval == 0)
     return ECORE_CALLBACK_RENEW;

   elapsed = (ecore_time_unix_get() - ctxt->config->last_update) / 60;
   if (elapsed > ctxt->config->update_interval)
     {
        ctxt->config->last_update = ecore_time_unix_get();
        packagekit_create_transaction_and_exec(ctxt, packagekit_refresh_cache);
     }
   return ECORE_CALLBACK_RENEW;
}

EAPI void *
e_modapi_init(E_Module *m)
{
   E_PackageKit_Module_Context *ctxt;

   ctxt = E_NEW(E_PackageKit_Module_Context, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctxt, NULL);
   ctxt->v_maj = ctxt->v_min = ctxt->v_mic = -1;

   ctxt->conf_edd = E_CONFIG_DD_NEW("packagekit_config", PackageKit_Config);
   #undef T
   #undef D
   #define T PackageKit_Config
   #define D ctxt->conf_edd
   E_CONFIG_VAL(D, T, update_interval, INT);
   E_CONFIG_VAL(D, T, last_update, INT);
   E_CONFIG_VAL(D, T, manager_command, STR);
   E_CONFIG_VAL(D, T, show_description, INT);
   ctxt->config = e_config_domain_load(PACKAGEKIT_DOMAIN, ctxt->conf_edd);
   if (!ctxt->config)
     ctxt->config = E_NEW(PackageKit_Config, 1);

   ctxt->module = m;
   packagekit_mod = m;
   e_gadcon_provider_register(&_gc_class);
   packagekit_dbus_connect(ctxt);
   ctxt->refresh_timer = ecore_timer_add(60.0, _timer_cb, ctxt);
   return ctxt;
}

EAPI int
e_modapi_shutdown(E_Module *m)
{
   E_PackageKit_Module_Context *ctxt = m->data;

   packagekit_dbus_disconnect(ctxt);

   E_FREE_FUNC(ctxt->refresh_timer, ecore_timer_del);
   E_FREE_FUNC(ctxt->error, eina_stringshare_del);

   E_FREE_FUNC(ctxt->config->manager_command, eina_stringshare_del);
   E_FREE(ctxt->config);
   E_CONFIG_DD_FREE(ctxt->conf_edd);

   e_gadcon_provider_unregister(&_gc_class);

   E_PackageKit_Package *pkg;
   EINA_LIST_FREE(ctxt->packages, pkg)
     {
        E_FREE_FUNC(pkg->name, eina_stringshare_del);
        E_FREE_FUNC(pkg->version, eina_stringshare_del);
        E_FREE_FUNC(pkg->summary, eina_stringshare_del);
     }

   E_FREE(ctxt);
   packagekit_mod = NULL;

   return 1;
}

EAPI int
e_modapi_save(E_Module *m)
{
   E_PackageKit_Module_Context *ctxt = m->data;
   e_config_domain_save(PACKAGEKIT_DOMAIN, ctxt->conf_edd, ctxt->config);
   return 1;
}


