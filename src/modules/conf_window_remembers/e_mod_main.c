#include "e.h"
#include "e_mod_main.h"

static E_Module *conf_module = NULL;

EAPI E_Module_Api e_modapi = 
{
   E_MODULE_API_VERSION, "Settings - Window Remembers"
};

EAPI void *
e_modapi_init(E_Module *m) 
{
   e_configure_registry_category_add("windows", 50, _("Windows"), NULL, 
                                     "preferences-system-windows");
   e_configure_registry_item_add("windows/window_remembers", 40, 
                                 _("Window Remembers"), NULL, 
                                 "preferences-desktop-window-remember", 
                                 e_int_config_remembers);
   conf_module = m;
   e_module_delayed_set(m, 1);

   {
      E_Configure_Option *co;

      e_configure_option_domain_current_set("conf_window_remembers");

      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("window remembers"), _("Window remember settings"), _("border"), _("remember"));
      co->info = eina_stringshare_add("windows/window_remembers");
      E_CONFIGURE_OPTION_ICON(co, "preferences-desktop-window-remember");
   }

   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   E_Config_Dialog *cfd;

   while ((cfd = e_config_dialog_get("E", "windows/window_remembers")))
     e_object_del(E_OBJECT(cfd));
   e_configure_registry_item_del("windows/window_remembers");
   e_configure_registry_category_del("windows");

   e_configure_option_domain_clear("conf_window_remembers");
   conf_module = NULL;
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   return 1;
}
