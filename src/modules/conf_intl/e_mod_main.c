#include "e.h"
#include "e_mod_main.h"

/* actual module specifics */
static E_Module *conf_module = NULL;

/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Settings - Language"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   e_configure_registry_category_add("language", 70, _("Language"), NULL,
                                     "preferences-desktop-locale");
   e_configure_registry_item_add("language/language_settings", 10,
                                 _("Language Settings"), NULL,
                                 "preferences-desktop-locale",
                                 e_int_config_intl);
   e_configure_registry_item_add("language/desklock_language_settings", 10,
                                 _("Desklock Language Settings"), NULL,
                                 "preferences-desklock-locale",
                                 e_int_config_desklock_intl);
   e_configure_registry_item_add("language/input_method_settings", 20,
                                 _("Input Method Settings"), NULL,
                                 "preferences-imc", e_int_config_imc);
   conf_module = m;
   e_module_delayed_set(m, 1);

   {
      E_Configure_Option *co;

      e_configure_option_domain_current_set("conf_intl");

      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("language"), _("Language settings"), _("language"));
      co->info = eina_stringshare_add("language/language_settings");
      E_CONFIGURE_OPTION_ICON(co, "preferences-desktop-locale");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("desklock language"), _("Desklock language settings"), _("desklock"), _("language"));
      co->info = eina_stringshare_add("language/desklock_language_settings");
      E_CONFIGURE_OPTION_ICON(co, "preferences-desklock-locale");

      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("input method"), _("Input method settings"), _("input"), _("language"));
      co->info = eina_stringshare_add("language/input_method_settings");
      E_CONFIGURE_OPTION_ICON(co, "preferences-imc");
   }

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   E_Config_Dialog *cfd;

   while ((cfd = e_config_dialog_get("E", "language/input_method_settings")))
     e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "language/language_settings")))
     e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "language/desklock_language_settings")))
     e_object_del(E_OBJECT(cfd));
   e_configure_registry_item_del("language/input_method_settings");
   e_configure_registry_item_del("language/desklock_language_settings");
   e_configure_registry_item_del("language/language_settings");
   e_configure_registry_category_del("language");

   e_configure_option_domain_clear("conf_intl");
   conf_module = NULL;
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   return 1;
}

