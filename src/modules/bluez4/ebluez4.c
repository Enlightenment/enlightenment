#include <unistd.h>
#include "e.h"
#include "agent.h"
#include "e_mod_main.h"
#include "ebluez4.h"

Service services[] = {
   { HumanInterfaceDevice_UUID, INPUT },
   { AudioSource_UUID, AUDIO_SOURCE },
   { AudioSink_UUID, AUDIO_SINK },
   { NULL, NONE}
};

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
   _free_dev_list(&ctxt->found_devices);
   ctxt->found_devices = NULL;
   edbus_object_unref(ctxt->adap_obj);
   ctxt->adap_obj = NULL;
   ebluez4_update_instances(ctxt->devices, LIST_TYPE_CREATED_DEVICES);
   ebluez4_update_all_gadgets_visibility();
}

static Profile
_uuid_to_profile(const char *uuid)
{
   Service *service;

   for (service = (Service *)services; service && service->uuid; service++)
     if (!strcmp(service->uuid, uuid))
       return service->profile;
   return NONE;
}

static void
_set_dev_services(Device *dev, EDBus_Message_Iter *uuids)
{
   const char *uuid;

   while (edbus_message_iter_get_and_next(uuids, 's', &uuid))
     switch (_uuid_to_profile(uuid))
       {
          case INPUT:
            if (!dev->proxy.input)
              dev->proxy.input = edbus_proxy_get(dev->obj, INPUT_INTERFACE);
            break;
          case AUDIO_SOURCE:
            if (!dev->proxy.audio_source)
              dev->proxy.audio_source = edbus_proxy_get(dev->obj,
                                                   AUDIO_SOURCE_INTERFACE);
            break;
          case AUDIO_SINK:
            if (!dev->proxy.audio_sink)
              dev->proxy.audio_sink = edbus_proxy_get(dev->obj,
                                                    AUDIO_SINK_INTERFACE);
            break;
          default:
            break;
       }
}

static void
_retrieve_properties(EDBus_Message_Iter *dict, const char **addr,
                     const char **name, Eina_Bool *paired, Eina_Bool *connected,
                     EDBus_Message_Iter **uuids)
{
   EDBus_Message_Iter *entry, *variant;
   const char *key;

   while (edbus_message_iter_get_and_next(dict, 'e', &entry))
     {
        if(!edbus_message_iter_arguments_get(entry, "sv", &key, &variant))
           return;

        if (!strcmp(key, "Address"))
          {
             if(!edbus_message_iter_arguments_get(variant, "s", addr))
               return;
          }
        else if (!strcmp(key, "Name"))
          {
             if(!edbus_message_iter_arguments_get(variant, "s", name))
               return;
          }
        else if (!strcmp(key, "Paired"))
          {
             if(!edbus_message_iter_arguments_get(variant, "b", paired))
               return;
          }
        else if (!strcmp(key, "Connected"))
          {
             if(!edbus_message_iter_arguments_get(variant, "b", connected))
               return;
          }
        else if (!strcmp(key, "UUIDs"))
          {
             if(!edbus_message_iter_arguments_get(variant, "as", uuids))
               return;
          }
     }
}

static void
_on_prop_changed(void *context, const EDBus_Message *msg)
{
   const char *key, *name;
   Eina_Bool paired, connected;
   EDBus_Message_Iter *variant, *uuids;
   Device *dev = context;
   Device *found_dev = eina_list_search_unsorted(ctxt->found_devices, _addr_cmp,
                                                 dev->addr);

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

        if (found_dev)
          {
             eina_stringshare_del(found_dev->name);
             found_dev->name = eina_stringshare_add(name);
          }
     }
   else if (!strcmp(key, "Paired"))
     {
        if(!edbus_message_iter_arguments_get(variant, "b", &paired))
          return;
        DBG("'%s' property of %s changed to %d", key, dev->name, paired);
        dev->paired = paired;

        if (found_dev)
          found_dev->paired = paired;
     }
   else if (!strcmp(key, "Connected"))
     {
        if(!edbus_message_iter_arguments_get(variant, "b", &connected))
          return;
        DBG("'%s' property of %s changed to %d", key, dev->name, connected);
        dev->connected = connected;
     }
   else if (!strcmp(key, "UUIDs"))
     {
        if(!edbus_message_iter_arguments_get(variant, "as", &uuids))
          return;
        _set_dev_services(dev, uuids);
        return;
     }

   ebluez4_update_instances(ctxt->found_devices, LIST_TYPE_FOUND_DEVICES);
   ebluez4_update_instances(ctxt->devices, LIST_TYPE_CREATED_DEVICES);
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
_try_to_connect(EDBus_Proxy *proxy)
{
   if (proxy)
     edbus_proxy_call(proxy, "Connect", _on_connected, NULL, -1, "");
}

static void
_on_paired(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   const char *err_name, *err_msg;

   if (edbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("%s: %s", err_name, err_msg);
        return;
     }
}

static void
_on_dev_properties(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   EDBus_Message_Iter *dict, *uuids;
   const char *addr, *name;
   Eina_Bool paired;
   Eina_Bool connected;
   Device *dev = data;

   if (!edbus_message_arguments_get(msg, "a{sv}", &dict))
     return;

   _retrieve_properties(dict, &addr, &name, &paired, &connected, &uuids);

   dev->addr = eina_stringshare_add(addr);
   dev->name = eina_stringshare_add(name);
   dev->paired = paired;
   dev->connected = connected;
   _set_dev_services(dev, uuids);
   ebluez4_append_to_instances(dev, LIST_TYPE_CREATED_DEVICES);
}

static void
_unset_dev(const char *path)
{
   Device *dev = eina_list_search_unsorted(ctxt->devices, ebluez4_path_cmp,
                                           path);

   if (!dev)
     return;
   ctxt->devices = eina_list_remove(ctxt->devices, dev);
   ebluez4_update_instances(ctxt->devices, LIST_TYPE_CREATED_DEVICES);
   _free_dev(dev);
}

static void
_set_dev(const char *path)
{
   Device *dev = calloc(1, sizeof(Device));

   dev->obj = edbus_object_get(ctxt->conn, BLUEZ_BUS, path);
   dev->proxy.dev = edbus_proxy_get(dev->obj, DEVICE_INTERFACE);
   edbus_proxy_call(dev->proxy.dev, "GetProperties", _on_dev_properties, dev,
                    -1, "");
   edbus_proxy_signal_handler_add(dev->proxy.dev, "PropertyChanged",
                                  _on_prop_changed, dev);
   ctxt->devices = eina_list_append(ctxt->devices, dev);
}

static void
_on_removed(void *context, const EDBus_Message *msg)
{
   const char *path;

   if (!edbus_message_arguments_get(msg, "o", &path))
     return;

   _unset_dev(path);
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
   EDBus_Message_Iter *dict, *uuids;
   const char *addr, *name;
   Eina_Bool paired, connected;
   Device *dev;

   if (!edbus_message_arguments_get(msg, "sa{sv}", &addr, &dict))
     return;

   if(eina_list_search_unsorted(ctxt->found_devices, _addr_cmp, addr))
     return;

   if (!edbus_message_arguments_get(msg, "a{sv}", &dict))
     return;

   _retrieve_properties(dict, &addr, &name, &paired, &connected, &uuids);

   dev = calloc(1, sizeof(Device));
   dev->addr = eina_stringshare_add(addr);
   dev->name = eina_stringshare_add(name);
   dev->paired = paired;
   ctxt->found_devices = eina_list_append(ctxt->found_devices, dev);

   ebluez4_append_to_instances(dev, LIST_TYPE_FOUND_DEVICES);
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

   edbus_proxy_signal_handler_add(ctxt->adap_proxy, "DeviceFound",
                                  _on_device_found, NULL);
   edbus_proxy_signal_handler_add(ctxt->adap_proxy, "DeviceCreated",
                                  _on_created, NULL);
   edbus_proxy_signal_handler_add(ctxt->adap_proxy, "DeviceRemoved",
                                  _on_removed, NULL);
   edbus_proxy_call(ctxt->adap_proxy, "ListDevices", _on_list, NULL, -1, "");
   edbus_proxy_call(ctxt->adap_proxy, "RegisterAgent", NULL, NULL, -1, "os",
                    REMOTE_AGENT_PATH, "KeyboardDisplay");
   ebluez4_update_all_gadgets_visibility();
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
   _free_dev_list(&ctxt->found_devices);
   edbus_connection_unref(ctxt->conn);
   free(ctxt);

   edbus_shutdown();
}

void
ebluez4_start_discovery()
{
   _free_dev_list(&ctxt->found_devices);
   ebluez4_update_instances(ctxt->found_devices, LIST_TYPE_FOUND_DEVICES);
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
   _try_to_connect(dev->proxy.input);
   _try_to_connect(dev->proxy.audio_source);
   _try_to_connect(dev->proxy.audio_sink);
}

void
ebluez4_pair_with_device(const char *addr)
{
   edbus_proxy_call(ctxt->adap_proxy, "CreatePairedDevice", _on_paired, NULL,
                    -1, "sos", addr, AGENT_PATH, "KeyboardDisplay");
}

void
ebluez4_remove_device(const char *addr)
{
   Device *dev = eina_list_search_unsorted(ctxt->devices, _addr_cmp, addr);
   edbus_proxy_call(ctxt->adap_proxy, "RemoveDevice", NULL, NULL, -1, "o",
                    edbus_object_path_get(dev->obj));
}

int
ebluez4_path_cmp(const void *d1, const void *d2)
{
   const Device *dev = d1;
   const char *path = d2;

   return strcmp(edbus_object_path_get(dev->obj), path);
}
