#include "backlight.h"

E_Module *gm;

EINTERN void *
e_modapi_gadget_init(E_Module *m)
{
   gm = m;
   backlight_init();
   e_gadget_type_add("Backlight Gadget", backlight_gadget_create, NULL);
   return m;
}

EINTERN int
e_modapi_gadget_shutdown(E_Module *m EINA_UNUSED)
{
   backlight_shutdown();
   e_gadget_type_del("Backlight Gadget");
   return 1;
}

EINTERN int
e_modapi_gadget_save(E_Module *m EINA_UNUSED)
{
   return 1;
}

