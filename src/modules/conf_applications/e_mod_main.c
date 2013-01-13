#include "e.h"
#include "e_mod_main.h"

/* actual module specifics */

static E_Module *conf_module = NULL;

/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Settings - Applications"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   e_configure_registry_category_add("applications", 20, _("Apps"), NULL,
                                     "preferences-applications");
   e_configure_registry_item_add("applications/new_application", -1,
                                 _("Create Application Launcher"), NULL,
                                 "preferences-applications-add",
                                 e_int_config_apps_add);
   e_configure_registry_item_add("applications/personal_applications", 20,
                                 _("Personal Application Launchers"), NULL,
                                 "preferences-applications-personal",
                                 e_int_config_apps_personal);
   e_configure_registry_item_add("applications/favorite_applications", 30,
                                 _("Favorite Applications"), NULL,
                                 "user-bookmarks",
                                 e_int_config_apps_favs);
   e_configure_registry_item_add("applications/ibar_applications", 40,
                                 _("IBar Applications"), NULL,
                                 "preferences-applications-ibar",
                                 e_int_config_apps_ibar);
   e_configure_registry_item_add("applications/screen_lock_applications", 45,
                                 _("Screen Lock Applications"), NULL,
                                 "preferences-applications-screen-lock",
                                 e_int_config_apps_desk_lock);
   e_configure_registry_item_add("applications/screen_unlock_applications", 46,
                                 _("Screen Unlock Applications"), NULL,
                                 "preferences-applications-screen-unlock",
                                 e_int_config_apps_desk_unlock);
   e_configure_registry_item_add("applications/restart_applications", 50,
                                 _("Restart Applications"), NULL,
                                 "preferences-applications-restart",
                                 e_int_config_apps_restart);
   e_configure_registry_item_add("applications/startup_applications", 60,
                                 _("Startup Applications"), NULL,
                                 "preferences-applications-startup",
                                 e_int_config_apps_startup);
   e_configure_registry_item_add("applications/default_applications", 70,
                                 _("Default Applications"), NULL,
                                 "preferences-desktop-default-applications",
                                 e_int_config_defapps);
   e_configure_registry_item_add("applications/desktop_environments", 80,
                                 _("Desktop Environments"), NULL,
                                 "preferences-desktop-environments",
                                 e_int_config_deskenv);
   e_configure_registry_category_add("internal", -1, _("Internal"), NULL,
                                     "enlightenment/internal");
   e_configure_registry_item_add("internal/ibar_other", -1, _("IBar Other"),
                                 NULL, "preferences-system-windows",
                                 e_int_config_apps_ibar_other);

   {
      E_Configure_Option *co;

      e_configure_option_domain_current_set("conf_applications");

      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("new app launcher"), _("Create a new application launcher"), _("application"), _("exec"));
      co->info = eina_stringshare_add("applications/new_application");
      E_CONFIGURE_OPTION_ICON(co, "preferences-applications-add");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("apps"), _("Application launchers"), _("application"), _("exec"));
      co->info = eina_stringshare_add("applications/personal_applications");
      E_CONFIGURE_OPTION_ICON(co, "preferences-applications-personal");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("favorite apps"), _("Favorite applications"), _("application"), _("exec"));
      co->info = eina_stringshare_add("applications/favorite_applications");
      E_CONFIGURE_OPTION_ICON(co, "user-bookmarks");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("ibar apps"), _("Ibar applications"), _("application"), _("exec"));
      co->info = eina_stringshare_add("applications/ibar_applications");
      E_CONFIGURE_OPTION_ICON(co, "preferences-applications-ibar");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("desklock apps"), _("Desk lock applications"), _("application"), _("exec"), _("desklock"));
      co->info = eina_stringshare_add("applications/screen_lock_applications");
      E_CONFIGURE_OPTION_ICON(co, "preferences-applications-screen-lock");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("deskunlock apps"), _("Desk unlock applications"), _("application"), _("exec"), _("desklock"));
      co->info = eina_stringshare_add("applications/screen_unlock_applications");
      E_CONFIGURE_OPTION_ICON(co, "preferences-applications-screen-unlock");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("restart apps"), _("Enlightenment restart applications"), _("application"), _("exec"));
      co->info = eina_stringshare_add("applications/restart_applications");
      E_CONFIGURE_OPTION_ICON(co, "preferences-applications-restart");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("startup apps"), _("Enlightenment start applications"), _("application"), _("exec"), _("startup"));
      co->info = eina_stringshare_add("applications/startup_applications");
      E_CONFIGURE_OPTION_ICON(co, "preferences-applications-startup");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("default apps"), _("Enlightenment default applications"), _("application"), _("exec"));
      co->info = eina_stringshare_add("applications/default_applications");
      E_CONFIGURE_OPTION_ICON(co, "preferences-desktop-default-applications");
   }

   conf_module = m;
   e_module_delayed_set(m, 1);
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   E_Config_Dialog *cfd;

   while ((cfd = e_config_dialog_get("E", "internal/ibar_other")))
     e_object_del(E_OBJECT(cfd));
   e_configure_registry_item_del("internal/ibar_other");
   e_configure_registry_category_del("internal");
   e_configure_registry_item_del("applications/favorite_applications");
   e_configure_registry_item_del("applications/new_application");
   e_configure_registry_item_del("applications/personal_applications");
   e_configure_registry_item_del("applications/ibar_applications");
   e_configure_registry_item_del("applications/restart_applications");
   e_configure_registry_item_del("applications/startup_applications");
   e_configure_registry_item_del("applications/default_applications");
   e_configure_registry_item_del("applications/desktop_environments");
   e_configure_registry_category_del("applications");

   e_configure_option_domain_clear("conf_applications");
   conf_module = NULL;
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   return 1;
}

