#include "xkbswitch.h"

Xkbg _xkbg = { NULL, NULL, NULL };

EINTERN void *
e_modapi_gadget_init(E_Module *m)
{
   _xkbg.module = m;
   xkbg_init();
   e_gadget_type_add("Xkbswitch Gadget", xkbg_gadget_create, NULL);
   return m;
}

EINTERN int
e_modapi_gadget_shutdown(E_Module *m EINA_UNUSED)
{
   xkbg_shutdown();
   _xkbg.module = NULL;
   e_gadget_type_del("Xkbswitch Gadget");

   return 1;
}

EINTERN int
e_modapi_gadget_save(E_Module *m EINA_UNUSED)
{
   return 1;
}
