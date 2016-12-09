#include "luncher.h"
Eina_List *luncher_instances = NULL;
E_Module *module = NULL;
Config *luncher_config = NULL;

EINTERN void
luncher_init(void)
{
   config_descriptor_init();
   luncher_config = e_config_domain_load("module.luncher", config_descriptor_get());

   if (!luncher_config)
     {
        Config_Item *ci;

        luncher_config = E_NEW(Config, 1);
        ci = E_NEW(Config_Item, 1);
        ci->id = 0;
        ci->style = eina_stringshare_add("default");
        ci->dir = eina_stringshare_add("default");
        luncher_config->items = eina_list_append(luncher_config->items, ci);
     }
   e_gadget_type_add("Luncher Bar", bar_create, NULL);
   e_gadget_type_add("Luncher Grid", grid_create, NULL);
}

EINTERN void
luncher_shutdown(void)
{
   if (luncher_config)
     {
        Config_Item *ci;
        EINA_LIST_FREE(luncher_config->items, ci)
          {
             eina_stringshare_del(ci->style);
             eina_stringshare_del(ci->dir);
             free(ci);
          }
        E_FREE(luncher_config);
     }

   e_gadget_type_del("Luncher Bar");
   e_gadget_type_del("Luncher Grid");

   config_descriptor_shutdown();
}

E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Luncher"
};

E_API void *
e_modapi_init(E_Module *m)
{
   luncher_init();

   module = m;
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   luncher_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.luncher", config_descriptor_get(), luncher_config);
   return 1;
}
