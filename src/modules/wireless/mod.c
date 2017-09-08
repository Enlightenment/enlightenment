#include "wireless.h"

EINTERN void wireless_gadget_init(void);
EINTERN void wireless_gadget_shutdown(void);

EINTERN void connman_init(void);
EINTERN void connman_shutdown(void);

EINTERN Eldbus_Connection *dbus_conn;
static E_Config_DD *edd;
EINTERN Wireless_Config *wireless_config;

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

   edd = E_CONFIG_DD_NEW("Wireless_Config", Wireless_Config);
#undef T
#undef D
#define T Wireless_Config
#define D edd

   E_CONFIG_VAL(D, T, disabled_types, UINT);
   wireless_config = e_config_domain_load("module.wireless", edd);
   if (!wireless_config) wireless_config = E_NEW(Wireless_Config, 1);
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   wireless_gadget_shutdown();
   connman_shutdown();
   E_FREE_FUNC(dbus_conn, eldbus_connection_unref);
   E_CONFIG_DD_FREE(edd);
   E_FREE(wireless_config);
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.wireless", edd, wireless_config);
   return 1;
}
