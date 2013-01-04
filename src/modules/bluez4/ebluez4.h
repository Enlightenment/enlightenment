#include "EDBus.h"

#define BLUEZ_BUS "org.bluez"
#define MANAGER_PATH "/"
#define INPUT_INTERFACE "org.bluez.Input"
#define AUDIO_SOURCE_INTERFACE "org.bluez.AudioSource"
#define AUDIO_SINK_INTERFACE "org.bluez.AudioSink"
#define DEVICE_INTERFACE "org.bluez.Device"
#define ADAPTER_INTERFACE "org.bluez.Adapter"
#define MANAGER_INTERFACE "org.bluez.Manager"

#define HumanInterfaceDevice_UUID "00001124-0000-1000-8000-00805f9b34fb"
#define AudioSource_UUID "0000110a-0000-1000-8000-00805f9b34fb"
#define AudioSink_UUID "0000110b-0000-1000-8000-00805f9b34fb"

typedef enum _Profile
{
   INPUT,
   AUDIO_SOURCE,
   AUDIO_SINK,
   NONE
} Profile;

typedef struct _Service
{
   const char *uuid;
   Profile profile;
} Service;

typedef struct _Device
{
   const char *addr;
   const char *name;
   Eina_Bool paired;
   Eina_Bool connected;
   EDBus_Object *obj;
   struct
     {
        EDBus_Proxy *dev;
        EDBus_Proxy *input;
        EDBus_Proxy *audio_sink;
        EDBus_Proxy *audio_source;
     } proxy;
} Device;

typedef struct _Adapter
{
   const char *name;
   Eina_Bool visible;
   Eina_Bool pairable;
   Eina_Bool powered;
   int visible_checked;
   int pairable_checked;
   int powered_checked;
   Eina_Bool is_default;
   E_Dialog *dialog;
   EDBus_Object *obj;
   EDBus_Proxy *proxy;
} Adapter;

typedef struct _Context
{
   EDBus_Connection *conn;
   EDBus_Object *adap_obj;
   EDBus_Proxy *man_proxy;
   EDBus_Proxy *adap_proxy;
   Eina_List *devices;
   Eina_List *found_devices;
   Eina_List *adapters;
} Context;

Context *ctxt;

void ebluez4_edbus_init(void);
void ebluez4_edbus_shutdown(void);
void ebluez4_start_discovery(void);
void ebluez4_stop_discovery(void);
void ebluez4_connect_to_device(Device *dev);
void ebluez4_disconnect_device(Device *dev);
void ebluez4_pair_with_device(const char *addr, void (*cb)(void *, Eina_Bool, const char *), void *data);
void ebluez4_remove_device(EDBus_Object *obj);
int ebluez4_dev_path_cmp(const void *d1, const void *d2);
void ebluez4_adapter_property_set(Adapter *adap, const char *prop_name, Eina_Bool value);
