#include "e.h"
#include "e_mod_main.h"
#include "e_int_config_randr2.h"

EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION, "Settings - Screen Setup"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   /* create Screen configuration category
    * 
    * NB: If the category already exists, this function just returns */
   e_configure_registry_category_add("screen", 30, _("Screen"),
                                     NULL, "preferences-desktop-display");

   /* add the randr dialog to the screen category and provide 
    * the configure category with the function to call */
   e_configure_registry_item_add("screen/screen_setup", 20, _("Screen Setup"),
                                 NULL, "preferences-system-screen-resolution",
                                 e_int_config_randr2);
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   E_Config_Dialog *cfd;

   while ((cfd = e_config_dialog_get("E", "screen/screen_setup")))
     e_object_del(E_OBJECT(cfd));

   e_configure_registry_item_del("screen/screen_setup");
   e_configure_registry_category_del("screen");
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   return 1;
}
