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
_on_device_found(void *context, const EDBus_Message *msg)
{
   EDBus_Message_Iter *dict, *entry, *variant;
   const char *addr, *key, *name;
   Device *dev;

   if (!edbus_message_arguments_get(msg, "sa{sv}", &addr, &dict))
     {
        ERR("Error reading device address");
        return;
     }

   if(eina_list_search_unsorted(ctxt->devices, _addr_cmp, addr))
     return;

   while (edbus_message_iter_get_and_next(dict,'e', &entry))
     {
        if(!edbus_message_iter_arguments_get(entry, "sv", &key, &variant))
          {
             ERR("Error reading device property");
             return;
          }

        if(strcmp(key,"Name"))
          continue;

        if(!edbus_message_iter_arguments_get(variant, "s", &name))
          {
             ERR("Error reading device name");
             return;
          }

        DBG("Device Found --- Name: %s", name);
        dev = malloc(sizeof(Device));
        dev->addr = eina_stringshare_add(addr);
        dev->name = eina_stringshare_add(name);
        ctxt->devices = eina_list_append(ctxt->devices, dev);
        ebluez4_append_to_instances(addr, name);
     }
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
   _free_dev_list(&ctxt->devices);
   ebluez4_update_instances(ctxt->devices);
   edbus_proxy_call(ctxt->adap_proxy, "StartDiscovery", NULL, NULL, -1, "");
}

void
ebluez4_stop_discovery()
{
   edbus_proxy_call(ctxt->adap_proxy, "StopDiscovery", NULL, NULL, -1, "");
}
