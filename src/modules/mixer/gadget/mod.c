#include "mixer.h"

EINTERN int _e_gemix_log_domain;

EINTERN void *
e_modapi_gadget_init(E_Module *m)
{
   _e_gemix_log_domain = eina_log_domain_register("mixer_gadget", EINA_COLOR_RED);

   e_gadget_type_add("Mixer", mixer_gadget_create, NULL);

   return m;
}

EINTERN int
e_modapi_gadget_shutdown(E_Module *m EINA_UNUSED)
{
   mixer_shutdown();

   e_gadget_type_del("Mixer");

   emix_shutdown();

   return 1;
}

EINTERN int
e_modapi_gadget_save(E_Module *m EINA_UNUSED)
{
   emix_config_save();
   return 1;
}

