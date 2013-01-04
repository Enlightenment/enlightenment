#include "EDBus.h"

#define BLUEZ_BUS "org.bluez"
#define MANAGER_PATH "/"
#define INPUT_INTERFACE "org.bluez.Input"
#define DEVICE_INTERFACE "org.bluez.Device"
#define ADAPTER_INTERFACE "org.bluez.Adapter"
#define MANAGER_INTERFACE "org.bluez.Manager"

typedef struct _Device
{
   const char *addr;
   const char *name;
   Eina_Bool paired;
   Eina_Bool connected;
   EDBus_Object *obj;
   EDBus_Proxy *prof_proxy;
   EDBus_Proxy *dev_proxy;
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
void ebluez4_connect_to_device(const char *addr);
