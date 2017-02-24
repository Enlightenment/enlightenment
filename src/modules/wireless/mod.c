#include "e.h"

EINTERN void wireless_gadget_init(void);
EINTERN void wireless_gadget_shutdown(void);

EINTERN void connman_init(void);
EINTERN void connman_shutdown(void);

EINTERN Eldbus_Connection *dbus_conn;

E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Wireless"
};

E_API void *
e_modapi_init(E_Module *m)
{
   dbus_conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   connman_init();
   wireless_gadget_init();
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   wireless_gadget_shutdown();
   connman_shutdown();
   E_FREE_FUNC(dbus_conn, eldbus_connection_unref);
   return 1;
}
