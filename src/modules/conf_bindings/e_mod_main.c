#include "e.h"
#include "e_mod_main.h"

/* actual module specifics */
static E_Module *conf_module = NULL;

/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Settings - Input Controls"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   e_configure_registry_category_add("keyboard_and_mouse", 40, _("Input"),
                                     NULL, "preferences-behavior");

   e_configure_registry_item_add("keyboard_and_mouse/key_bindings", 10,
                                 _("Key Bindings"), NULL,
                                 "preferences-desktop-keyboard-shortcuts",
                                 e_int_config_keybindings);
   e_configure_registry_item_add("keyboard_and_mouse/mouse_bindings", 20,
                                 _("Mouse Bindings"), NULL,
                                 "preferences-desktop-mouse",
                                 e_int_config_mousebindings);
   e_configure_registry_item_add("keyboard_and_mouse/acpi_bindings", 30,
                                 _("ACPI Bindings"), NULL,
                                 "preferences-system-power-management",
                                 e_int_config_acpibindings);
   e_configure_registry_item_add("keyboard_and_mouse/edge_bindings", 10,
                                 _("Edge Bindings"), NULL,
                                 "preferences-desktop-edge-bindings",
                                 e_int_config_edgebindings);

   e_configure_registry_category_add("advanced", 80, _("Advanced"), NULL, "preferences-advanced");
   e_configure_registry_item_add("advanced/signal_bindings", 10,
                                 _("Signal Bindings"), NULL,
                                 "preferences-desktop-signal-bindings",
                                 e_int_config_signalbindings);
   conf_module = m;
   e_module_delayed_set(m, 1);

   {
      E_Configure_Option *co;

      e_configure_option_domain_current_set("conf_bindings");

      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("key bindings"), _("Key binding settings"), _("input"), _("key"), _("binding"));
      co->info = eina_stringshare_add("keyboard_and_mouse/key_bindings");
      E_CONFIGURE_OPTION_ICON(co, "preferences-desktop-keyboard-shortcuts");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("mouse bindings"), _("Mouse binding settings"), _("input"), _("mouse"), _("binding"));
      co->info = eina_stringshare_add("keyboard_and_mouse/mouse_bindings");
      E_CONFIGURE_OPTION_ICON(co, "preferences-desktop-mouse");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("ACPI bindings"), _("ACPI binding settings"), _("input"), _("acpi"), _("binding"));
      co->info = eina_stringshare_add("keyboard_and_mouse/acpi_bindings");
      E_CONFIGURE_OPTION_ICON(co, "preferences-system-power-management");

      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("edge bindings"), _("Screen edge binding settings"), _("input"), _("edge"), _("screen"), _("binding"));
      co->info = eina_stringshare_add("keyboard_and_mouse/edge_bindings");
      E_CONFIGURE_OPTION_ICON(co, "preferences-desktop-edge-bindings");
      E_CONFIGURE_OPTION_ADD_CUSTOM(co, _("signal bindings"), _("Edje signal binding settings"), _("input"), _("edje"), _("mouse"), _("binding"));
      co->info = eina_stringshare_add("advanced/signal_bindings");
      E_CONFIGURE_OPTION_ICON(co, "preferences-desktop-signal-bindings");
   }


   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   E_Config_Dialog *cfd;

   while ((cfd = e_config_dialog_get("E", "keyboard_and_mouse/acpi_bindings")))
     e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "keyboard_and_mouse/mouse_bindings")))
     e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "keyboard_and_mouse/key_bindings")))
     e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "keyboard_and_mouse/edge_bindings")))
     e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "advanced/signal_bindings")))
     e_object_del(E_OBJECT(cfd));

   e_configure_registry_item_del("keyboard_and_mouse/acpi_bindings");
   e_configure_registry_item_del("keyboard_and_mouse/mouse_bindings");
   e_configure_registry_item_del("keyboard_and_mouse/key_bindings");
   e_configure_registry_item_del("keyboard_and_mouse/edge_bindings");
   e_configure_registry_item_del("advanced/signal_bindings");
   e_configure_registry_category_del("keyboard_and_mouse");
   e_configure_registry_category_del("advanced");

   e_configure_option_domain_clear("conf_bindings");

   conf_module = NULL;
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   return 1;
}

