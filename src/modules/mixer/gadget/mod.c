#include "mixer.h"

int _e_gemix_log_domain;
E_Module *gm;

EINTERN void *
e_modapi_gadget_init(E_Module *m)
{
   Eina_Bool loaded = EINA_FALSE;

   _e_gemix_log_domain = eina_log_domain_register("mixer_gadget", EINA_COLOR_RED);

   EINA_SAFETY_ON_FALSE_RETURN_VAL(emix_init(), NULL);

   gm = m;

   loaded = mixer_init();
   if (!loaded)
     goto err;

   e_gadget_type_add("Mixer", mixer_gadget_create, NULL);

   return m;
err:
//   emix_config_shutdown();
   emix_shutdown();
   return NULL;
}

EINTERN int
e_modapi_gadget_shutdown(E_Module *m EINA_UNUSED)
{
   mixer_shutdown();

   e_gadget_type_del("Mixer");


   emix_shutdown();
//   emix_config_shutdown();
   return 1;
}

EINTERN int
e_modapi_gadget_save(E_Module *m EINA_UNUSED)
{
   emix_config_save();
   return 1;
}

