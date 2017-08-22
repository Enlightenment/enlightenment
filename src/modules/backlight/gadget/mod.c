#include "backlight.h"

EINTERN void *
e_modapi_gadget_init(E_Module *m)
{
   e_gadget_type_add("Backlight", backlight_gadget_create, NULL);
   return m;
}

EINTERN int
e_modapi_gadget_shutdown(E_Module *m EINA_UNUSED)
{
   e_gadget_type_del("Backlight");
   return 1;
}

EINTERN int
e_modapi_gadget_save(E_Module *m EINA_UNUSED)
{
   return 1;
}

