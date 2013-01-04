#include <unistd.h>
#include "e.h"
#include "agent.h"
#include "e_mod_main.h"
#include "ebluez4.h"

/* Global Module Context */
Context *ctxt;

static int
_addr_cmp(const void *d1, const void *d2)
{
   const Device *dev = d1;
   const char *addr = d2;

   return strcmp(dev->addr, addr);
}

static void
_free_dev(Device *dev)
{
   if (dev->obj)
     edbus_object_unref(dev->obj);
   eina_stringshare_del(dev->addr);
   dev->addr = NULL;
   eina_stringshare_del(dev->name);
   dev->name = NULL;
   free(dev);
}

static void
_free_dev_list(Eina_List **list)
{
   Device *dev;

   EINA_LIST_FREE(*list, dev)
     _free_dev(dev);
   *list = NULL;
}

static void
_unset_adapter()
{
   if (!ctxt->adap_obj)
     return;

   DBG("Remove adapter %s", edbus_object_path_get(ctxt->adap_obj));

   _free_dev_list(&ctxt->devices);
   ctxt->devices = NULL;
   edbus_object_unref(ctxt->adap_obj);
   ctxt->adap_obj = NULL;
   ebluez4_disabled_set_all_search_buttons(EINA_TRUE);
   ebluez4_update_instances(ctxt->devices);
}

static void
_on_prop_changed(void *context, const EDBus_Message *msg)
{
   const char *key, *name;
   Eina_Bool paired, connected;
   EDBus_Message_Iter *variant;
   Device *dev = context;

   if (!edbus_message_arguments_get(msg, "sv", &key, &variant))
     {
        ERR("Property of %s changed, but could not be read", dev->name);
        return;
     }

   if (!strcmp(key, "Name"))
     {
        if(!edbus_message_iter_arguments_get(variant, "s", &name))
          return;
        DBG("'%s' property of %s changed to %s", key, dev->name, name);
        eina_stringshare_del(dev->name);
        dev->name = eina_stringshare_add(name);
        ebluez4_update_instances(ctxt->devices);
     }
   else if (!strcmp(key, "Paired"))
     {
        if(!edbus_message_iter_arguments_get(variant, "b", &paired))
          return;
        DBG("'%s' property of %s changed to %d", key, dev->name, paired);
        dev->paired = paired;
     }
   else if (!strcmp(key, "Connected"))
     {
        if(!edbus_message_iter_arguments_get(variant, "b", &connected))
          return;
        DBG("'%s' property of %s changed to %d", key, dev->name, connected);
        dev->connected = connected;
        ebluez4_update_instances(ctxt->devices);
     }
}

static void
_on_connected(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   const char *err_name, *err_msg;

   if (edbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("%s: %s", err_name, err_msg);
        return;
     }
}

static void
_on_paired(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   const char *path;
   Device *dev = data;

   const char *err_name, *err_msg;

   if (edbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("%s: %s", err_name, err_msg);
        return;
     }

   if (!edbus_message_arguments_get(msg, "o", &path))
     {
        ERR("Error reading device object path");
        return;
     }

   //FIXME: recognize device profile to allow connection to all devices
   dev->prof_proxy = edbus_proxy_get(dev->obj, INPUT_INTERFACE);
   edbus_proxy_call(dev->prof_proxy, "Connect", _on_connected, NULL, -1, "");
}

static void
_on_dev_properties(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   EDBus_Message_Iter *dict, *entry, *variant;
   const char *key, *addr, *name;
   Eina_Bool paired;
   Eina_Bool connected;
   Device *dev = data;

   if (!edbus_message_arguments_get(msg, "a{sv}", &dict))
     return;

   while (edbus_message_iter_get_and_next(dict,'e', &entry))
     {
        if(!edbus_message_iter_arguments_get(entry, "sv", &key, &variant))
           return;

        if (!strcmp(key, "Address"))
          {
             if(!edbus_message_iter_arguments_get(variant, "s", &addr))
               return;
          }
        else if (!strcmp(key, "Name"))
          {
             if(!edbus_message_iter_arguments_get(variant, "s", &name))
               return;
          }
        else if (!strcmp(key, "Paired"))
          {
             if(!edbus_message_iter_arguments_get(variant, "b", &paired))
               return;
          }
        else if (!strcmp(key, "Connected"))
          {
             if(!edbus_message_iter_arguments_get(variant, "b", &connected))
               return;
          }
     }

   dev->addr = eina_stringshare_add(addr);
   dev->name = eina_stringshare_add(name);
   dev->paired = paired;
   dev->connected = connected;
   ebluez4_append_to_instances(addr, name);
}

static void
_set_dev(const char *path)
{
   Device *dev = calloc(1, sizeof(Device));

   dev->obj = edbus_object_get(ctxt->conn, BLUEZ_BUS, path);
   dev->dev_proxy = edbus_proxy_get(dev->obj, DEVICE_INTERFACE);
   edbus_proxy_call(dev->dev_proxy, "GetProperties", _on_dev_properties, dev,
                    -1, "");
   edbus_proxy_signal_handler_add(dev->dev_proxy, "PropertyChanged",
                                  _on_prop_changed, dev);
   ctxt->devices = eina_list_append(ctxt->devices, dev);
}

static void
_on_created(void *context, const EDBus_Message *msg)
{
   const char *path;

   if (!edbus_message_arguments_get(msg, "o", &path))
     return;

   _set_dev(path);
}

static void
_on_device_found(void *context, const EDBus_Message *msg)
{
   EDBus_Message_Iter *dict;
   const char *addr;

   if (!edbus_message_arguments_get(msg, "sa{sv}", &addr, &dict))
     return;

   if(eina_list_search_unsorted(ctxt->devices, _addr_cmp, addr))
     return;

   edbus_proxy_call(ctxt->adap_proxy, "CreateDevice", NULL, NULL, -1, "s",
                    addr);
}

static void
_on_list(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   EDBus_Message_Iter *array;
   const char *path;

   if (!edbus_message_arguments_get(msg, "ao", &array))
     {
        ERR("Error reading list of devices");
       return;
     }

   while (edbus_message_iter_get_and_next(array, 'o', &path))
     _set_dev(path);
}

static void
_set_adapter(const EDBus_Message *msg)
{
   const char *adap_path;

   if (!edbus_message_arguments_get(msg, "o", &adap_path))
     {
        ERR("Error reading path of Default Adapter");
        return;
     }

   DBG("Setting adapter to %s", adap_path);

   if (ctxt->adap_obj)
     _unset_adapter();

   ctxt->adap_obj = edbus_object_get(ctxt->conn, BLUEZ_BUS, adap_path);
   ctxt->adap_proxy = edbus_proxy_get(ctxt->adap_obj, ADAPTER_INTERFACE);

   ebluez4_disabled_set_all_search_buttons(EINA_FALSE);
   edbus_proxy_signal_handler_add(ctxt->adap_proxy, "DeviceFound",
                                  _on_device_found, NULL);
   edbus_proxy_signal_handler_add(ctxt->adap_proxy, "DeviceCreated",
                                  _on_created, NULL);
   edbus_proxy_call(ctxt->adap_proxy, "ListDevices", _on_list, NULL, -1, "");
   edbus_proxy_call(ctxt->adap_proxy, "RegisterAgent", NULL, NULL, -1, "os",
                    REMOTE_AGENT_PATH, "KeyboardDisplay");
}

static void
_default_adapter_get(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   const char *err_name, *err_msg;

   /*
    * If bluetoothd is starting up, we can fail here and wait for the
    * DefaultAdapterChanged signal later
    */
   if (edbus_message_error_get(msg, &err_name, &err_msg))
     return;

   if (!ctxt->adap_obj)
     _set_adapter(msg);
}

static void
_on_adapter_changed(void *context, const EDBus_Message *msg)
{
   _set_adapter(msg);
}

static void
_on_adapter_removed(void *context, const EDBus_Message *msg)
{
   const char *adap_path;

   if (!edbus_message_arguments_get(msg, "o", &adap_path))
     {
        ERR("Error reading path of Removed Adapter");
        return;
     }

   if (!strcmp(edbus_object_path_get(ctxt->adap_obj), adap_path))
     _unset_adapter();
}

static void
_bluez_monitor(void *data, const char *bus, const char *old_id, const char *new_id)
{
   if (!strcmp(old_id,"") && strcmp(new_id,""))
     // Bluez up
     edbus_proxy_call(ctxt->man_proxy, "DefaultAdapter", _default_adapter_get,
                      NULL, -1, "");
   else if (strcmp(old_id,"") && !strcmp(new_id,""))
     // Bluez down
     _unset_adapter();
}

/* Public Functions */
void
ebluez4_edbus_init()
{
   EDBus_Object *obj;

   ctxt = calloc(1, sizeof(Context));

   edbus_init();

   ctxt->conn = edbus_connection_get(EDBUS_CONNECTION_TYPE_SYSTEM);
   obj = edbus_object_get(ctxt->conn, BLUEZ_BUS, MANAGER_PATH);
   ctxt->man_proxy = edbus_proxy_get(obj, MANAGER_INTERFACE);

   ebluez4_register_agent_interfaces(ctxt->conn);

   edbus_proxy_signal_handler_add(ctxt->man_proxy,
                            "DefaultAdapterChanged", _on_adapter_changed, NULL);
   edbus_proxy_signal_handler_add(ctxt->man_proxy, "AdapterRemoved",
                                  _on_adapter_removed, NULL);

   edbus_name_owner_changed_callback_add(ctxt->conn, BLUEZ_BUS, _bluez_monitor,
                                         NULL, EINA_TRUE);
}

void
ebluez4_edbus_shutdown()
{
   _free_dev_list(&ctxt->devices);
   edbus_connection_unref(ctxt->conn);
   free(ctxt);

   edbus_shutdown();
}

void
ebluez4_start_discovery()
{
   ebluez4_update_instances(ctxt->devices);
   edbus_proxy_call(ctxt->adap_proxy, "StartDiscovery", NULL, NULL, -1, "");
}

void
ebluez4_stop_discovery()
{
   edbus_proxy_call(ctxt->adap_proxy, "StopDiscovery", NULL, NULL, -1, "");
}

void
ebluez4_connect_to_device(const char *addr)
{
   Device *dev = eina_list_search_unsorted(ctxt->devices, _addr_cmp, addr);
   //FIXME: recognize device profile to allow connection to all devices
   dev->prof_proxy = edbus_proxy_get(dev->obj, INPUT_INTERFACE);
   if (dev->paired)
     edbus_proxy_call(dev->prof_proxy, "Connect", _on_connected, NULL, -1, "");
   else
     edbus_proxy_call(ctxt->adap_proxy, "CreatePairedDevice", _on_paired, dev,
                      -1, "sos", dev->addr, AGENT_PATH, "KeyboardDisplay");
}
