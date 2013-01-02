#include "e.h"
#include "e_mod_main.h"

#define BUS "org.freedesktop.UPower"
#define PATH "/org/freedesktop/UPower"
#define IFACE "org.freedesktop.UPower"

extern Eina_List *device_batteries;
extern Eina_List *device_ac_adapters;
extern double init_time;

static EDBus_Connection *conn;
static EDBus_Proxy *upower_proxy;

static void
_battery_free(Battery *bat)
{
   EDBus_Object *obj = edbus_proxy_object_get(bat->proxy);
   edbus_proxy_unref(bat->proxy);
   edbus_object_unref(obj);

   device_batteries = eina_list_remove(device_batteries, bat);
   eina_stringshare_del(bat->udi);
   if (bat->model)
     eina_stringshare_del(bat->model);
   if (bat->vendor)
     eina_stringshare_del(bat->vendor);
   free(bat);
}

static void
_ac_free(Ac_Adapter *ac)
{
   EDBus_Object *obj = edbus_proxy_object_get(ac->proxy);
   edbus_proxy_unref(ac->proxy);
   edbus_object_unref(obj);

   device_ac_adapters = eina_list_remove(device_ac_adapters, ac);
   eina_stringshare_del(ac->udi);
   free(ac);
}

static void
_ac_get_all_cb(void *data, const EDBus_Message *msg, EDBus_Pending *pending __UNUSED__)
{
   Ac_Adapter *ac = data;
   EDBus_Message_Iter *array, *dict, *variant;

   if (!edbus_message_arguments_get(msg, "a{sv}", &array))
     return;

   while (edbus_message_iter_get_and_next(array, 'e', &dict))
     {
        const char *key;

        if (!edbus_message_iter_arguments_get(dict, "sv", &key, &variant))
          continue;

        if (!strcmp(key, "Online"))
          {
             Eina_Bool b;
             edbus_message_iter_arguments_get(variant, "b", &b);
             ac->present = b;
             break;
          }
     }
   _battery_device_update();
}

static void
_ac_changed_cb(void *data, const EDBus_Message *msg __UNUSED__)
{
   Ac_Adapter *ac = data;
   edbus_proxy_property_get_all(ac->proxy, _ac_get_all_cb, ac);
}

static void
_process_ac(EDBus_Proxy *proxy)
{
   Ac_Adapter *ac;
   ac = E_NEW(Ac_Adapter, 1);
   if (!ac) goto error;
   ac->proxy = proxy;
   ac->udi = eina_stringshare_add(edbus_object_path_get(edbus_proxy_object_get(proxy)));
   edbus_proxy_property_get_all(proxy, _ac_get_all_cb, ac);
   edbus_proxy_signal_handler_add(proxy, "Changed", _ac_changed_cb, ac);
   device_ac_adapters = eina_list_append(device_ac_adapters, ac);
   return;

error:
   edbus_object_unref(edbus_proxy_object_get(proxy));
   edbus_proxy_unref(proxy);
   return;
}

static const char *bat_techologies[] = {
   "Unknown",
   "Lithium ion",
   "Lithium polymer",
   "Lithium iron phosphate",
   "Lead acid",
   "Nickel cadmium",
   "Nickel metal hydride"
};

static void
_bat_get_all_cb(void *data, const EDBus_Message *msg, EDBus_Pending *pending __UNUSED__)
{
   Battery *bat = data;
   EDBus_Message_Iter *array, *dict, *variant;

   bat->got_prop = EINA_TRUE;
   if (!edbus_message_arguments_get(msg, "a{sv}", &array))
     return;
   while (edbus_message_iter_get_and_next(array, 'e', &dict))
     {
        const char *key;
        union
          {
             Eina_Bool b;
             int64_t i64;
             unsigned u;
             double d;
             const char *s;
          } val;

        if (!edbus_message_iter_arguments_get(dict, "sv", &key, &variant))
          continue;
        if (!strcmp(key, "IsPresent"))
           {
              edbus_message_iter_arguments_get(variant, "b", &val.b);
              bat->present = val.b;
           }
        else if (!strcmp(key, "TimeToEmpty"))
          {
             edbus_message_iter_arguments_get(variant, "x", &val.i64);
             bat->time_left = (int) val.i64;
             if (bat->time_left > 0)
               bat->charging = EINA_FALSE;
             else
               bat->charging = EINA_TRUE;
          }
        else if (!strcmp(key, "Percentage"))
          {
             edbus_message_iter_arguments_get(variant, "d", &val.d);
             bat->percent = (int) val.d;
          }
        else if (!strcmp(key, "Energy"))
          {
             edbus_message_iter_arguments_get(variant, "d", &val.d);
             bat->current_charge = (int) val.d;
          }
        else if (!strcmp(key, "EnergyFullDesign"))
          {
             edbus_message_iter_arguments_get(variant, "d", &val.d);
             bat->design_charge = (int) val.d;
          }
        else if (!strcmp(key, "EnergyFull"))
          {
             edbus_message_iter_arguments_get(variant, "d", &val.d);
             bat->last_full_charge = (int) val.d;
          }
        else if (!strcmp(key, "TimeToFull"))
          {
             edbus_message_iter_arguments_get(variant, "x", &val.i64);
             bat->time_full = (int) val.i64;
          }
        else if (!strcmp(key, "Technology"))
          {
             edbus_message_iter_arguments_get(variant, "u", &val.u);
             if (val.u > EINA_C_ARRAY_LENGTH(bat_techologies))
               val.u = 0;
             bat->technology = bat_techologies[val.u];
          }
        else if (!strcmp(key, "Model"))
          {
             if (!edbus_message_iter_arguments_get(variant, "s", &val.s))
               continue;
             eina_stringshare_replace(&bat->model, val.s);
          }
        else if (!strcmp(key, "Vendor"))
          {
             if (!edbus_message_iter_arguments_get(variant, "s", &val.s))
               continue;
             if (bat->vendor)
               eina_stringshare_del(bat->vendor);
             bat->vendor = eina_stringshare_add(val.s);
          }
     }
   _battery_device_update();
}

static void
_bat_changed_cb(void *data, const EDBus_Message *msg __UNUSED__)
{
   Battery *bat = data;
   edbus_proxy_property_get_all(bat->proxy, _bat_get_all_cb, bat);
}

static void
_process_battery(EDBus_Proxy *proxy)
{
   Battery *bat;

   bat = E_NEW(Battery, 1);
   if (!bat)
     {
        edbus_object_unref(edbus_proxy_object_get(proxy));
        edbus_proxy_unref(proxy);
        return;
     }

   bat->proxy = proxy;
   bat->udi = eina_stringshare_add(edbus_object_path_get(edbus_proxy_object_get(proxy)));
   edbus_proxy_property_get_all(proxy, _bat_get_all_cb, bat);
   edbus_proxy_signal_handler_add(proxy, "Changed", _bat_changed_cb, bat);
   device_batteries = eina_list_append(device_batteries, bat);
   _battery_device_update();
}

static void
_device_type_cb(void *data, const EDBus_Message *msg, EDBus_Pending *pending __UNUSED__)
{
   EDBus_Proxy *proxy = data;
   EDBus_Message_Iter *variant;
   EDBus_Object *obj;
   unsigned int type;

   if (!edbus_message_arguments_get(msg, "v", &variant))
     goto error;

   edbus_message_iter_arguments_get(variant, "u", &type);
   if (type == 1)
     _process_ac(proxy);
   else if (type == 2)
     _process_battery(proxy);
   else
     goto error;

   return;

error:
   obj = edbus_proxy_object_get(proxy);
   edbus_proxy_unref(proxy);
   edbus_object_unref(obj);
   return;
}

static void
_process_enumerate_path(const char *path)
{
   EDBus_Object *obj;
   EDBus_Proxy *proxy;

   obj = edbus_object_get(conn, BUS, path);
   EINA_SAFETY_ON_FALSE_RETURN(obj);
   proxy = edbus_proxy_get(obj, "org.freedesktop.UPower.Device");
   edbus_proxy_property_get(proxy, "Type", _device_type_cb, proxy);
}

static void
_enumerate_cb(void *data __UNUSED__, const EDBus_Message *msg, EDBus_Pending *pending __UNUSED__)
{
   const char *path;
   EDBus_Message_Iter *array;

   if (!edbus_message_arguments_get(msg, "ao", &array))
     return;

   while (edbus_message_iter_get_and_next(array, 'o', &path))
     _process_enumerate_path(path);
}

static void
_device_added_cb(void *data __UNUSED__, const EDBus_Message *msg)
{
   const char *path;

   if (!edbus_message_arguments_get(msg, "o", &path))
     return;
   _process_enumerate_path(path);
}

static void
_device_removed_cb(void *data __UNUSED__, const EDBus_Message *msg)
{
   Battery *bat;
   Ac_Adapter *ac;
   const char *path;

   if (!edbus_message_arguments_get(msg, "o", &path))
     return;
   bat = _battery_battery_find(path);
   if (bat)
     {
         _battery_free(bat);
        _battery_device_update();
        return;
     }
   ac = _battery_ac_adapter_find(path);
   if (ac)
     {
        _ac_free(ac);
        _battery_device_update();
     }
}

int
_battery_upower_start(void)
{
   EDBus_Object *obj;

   edbus_init();
   conn = edbus_connection_get(EDBUS_CONNECTION_TYPE_SYSTEM);
   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, 0);

   obj = edbus_object_get(conn, BUS, PATH);
   EINA_SAFETY_ON_NULL_GOTO(obj, obj_error);
   upower_proxy = edbus_proxy_get(obj, IFACE);
   EINA_SAFETY_ON_NULL_GOTO(upower_proxy, proxy_error);

   edbus_proxy_call(upower_proxy, "EnumerateDevices", _enumerate_cb, NULL, -1, "");
   edbus_proxy_signal_handler_add(upower_proxy, "DeviceAdded", _device_added_cb, NULL);
   edbus_proxy_signal_handler_add(upower_proxy, "DeviceRemoved", _device_removed_cb, NULL);
   return 1;

proxy_error:
   edbus_object_unref(obj);
obj_error:
   edbus_connection_unref(conn);
   return 0;
}

void
_battery_upower_stop(void)
{
   Eina_List *list, *list2;
   Battery *bat;
   Ac_Adapter *ac;
   EDBus_Object *obj;

   EINA_LIST_FOREACH_SAFE(device_batteries, list, list2, bat)
     _battery_free(bat);
   EINA_LIST_FOREACH_SAFE(device_ac_adapters, list, list2, ac)
     _ac_free(ac);

   obj = edbus_proxy_object_get(upower_proxy);
   edbus_proxy_unref(upower_proxy);
   edbus_object_unref(obj);
   edbus_connection_unref(conn);
   edbus_shutdown();
}
