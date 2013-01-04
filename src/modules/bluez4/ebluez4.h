#include "EDBus.h"

#define BLUEZ_BUS "org.bluez"
#define MANAGER_PATH "/"
#define ADAPTER_INTERFACE "org.bluez.Adapter"
#define MANAGER_INTERFACE "org.bluez.Manager"

typedef struct _Device
{
   const char *addr;
   const char *name;
} Device;

typedef struct _Context
{
   EDBus_Connection *conn;
   const char *default_adap_path;
   EDBus_Object *adap_obj;
   EDBus_Proxy *man_proxy;
   EDBus_Proxy *adap_proxy;
   Eina_List *devices;
} Context;

void ebluez4_edbus_init();
void ebluez4_edbus_shutdown();
void ebluez4_start_discovery();
void ebluez4_stop_discovery();
