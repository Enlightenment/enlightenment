#include <unistd.h>
#include "e.h"
#include "agent.h"
#include "e_mod_main.h"
#include "ebluez4.h"

typedef struct _Pair_Cb
{
   void (*cb)(void *, Eina_Bool, const char *);
   void *data;
} Pair_Cb;

Service services[] = {
   { HumanInterfaceDevice_UUID, INPUT },
   { AudioSource_UUID, AUDIO_SOURCE },
   { AudioSink_UUID, AUDIO_SINK },
   { NULL, NONE }
};

static int
_dev_addr_cmp(const void *d1, const void *d2)
{
   const Device *dev = d1;
   const char *addr = d2;

   return strcmp(dev->addr, addr);
}

int
_adap_path_cmp(const void *d1, const void *d2)
{
   const Adapter *adap = d1;
   const char *path = d2;

   return strcmp(eldbus_object_path_get(adap->obj), path);
}

static struct icon_type
{
   const char *icon;
   const char *type;
} icon_type_table[] = {
   { "audio-card", "Audio" },
   { "camera-photo", "Photo Camera" },
   { "camera-video", "Video Camera" },
   { "computer", "Computer" },
   { "input-gaming", "Game Controller" },
   { "input-keyboard", "Keyboard" },
   { "input-mouse", "Mouse" },
   { "input-tablet", "Tablet" },
   { "modem", "Modem" },
   { "network-wireless", "Wireless" },
   { "phone", "Phone" },
   { "printer", "Printer" },
   { NULL, NULL }
};

static const char *
_icon_to_type(const char *icon)
{
   struct icon_type *it;

   for (it = &icon_type_table[0]; it && it->icon; it++)
     {
        if (strcmp(icon, it->icon))
          return it->type;
     }
   return NULL;
}

static void
_free_dev(Device *dev)
{
   if (dev->proxy.input) eldbus_proxy_unref(dev->proxy.input);
   if (dev->proxy.audio_source) eldbus_proxy_unref(dev->proxy.audio_source);
   if (dev->proxy.audio_sink) eldbus_proxy_unref(dev->proxy.audio_sink);

   if (dev->obj)
     eldbus_object_unref(dev->obj);
   eina_stringshare_del(dev->addr);
   dev->addr = NULL;
   eina_stringshare_del(dev->name);
   dev->name = NULL;
   free(dev);
}

static void
_free_adap(Adapter *adap)
{
   eldbus_proxy_unref(adap->proxy);
   eldbus_object_unref(adap->obj);
   eina_stringshare_del(adap->name);
   adap->name = NULL;
   ebluez4_adapter_settings_del(adap->dialog);
   free(adap);
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
_free_adap_list(void)
{
   Adapter *adap;

   EINA_LIST_FREE(ctxt->adapters, adap)
     _free_adap(adap);
   ctxt->adapters = NULL;
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
_set_dev_services(Device *dev, Eldbus_Message_Iter *uuids)
{
   const char *uuid;

   while (eldbus_message_iter_get_and_next(uuids, 's', &uuid))
     switch (_uuid_to_profile(uuid))
       {
          case INPUT:
            if (!dev->proxy.input)
              dev->proxy.input = eldbus_proxy_get(dev->obj, INPUT_INTERFACE);
            break;
          case AUDIO_SOURCE:
            if (!dev->proxy.audio_source)
              dev->proxy.audio_source = eldbus_proxy_get(dev->obj,
                                                   AUDIO_SOURCE_INTERFACE);
            break;
          case AUDIO_SINK:
            if (!dev->proxy.audio_sink)
              dev->proxy.audio_sink = eldbus_proxy_get(dev->obj,
                                                    AUDIO_SINK_INTERFACE);
            break;
          default:
            break;
       }
}

static void
_retrieve_properties(Eldbus_Message_Iter *dict, const char **addr,
                     const char **name, const char **icon, Eina_Bool *paired,
                     Eina_Bool *connected, Eldbus_Message_Iter **uuids)
{
   Eldbus_Message_Iter *entry, *variant;
   const char *key;

   *icon = NULL;

   while (eldbus_message_iter_get_and_next(dict, 'e', &entry))
     {
        if (!eldbus_message_iter_arguments_get(entry, "sv", &key, &variant))
           return;

        if (!strcmp(key, "Address"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "s", addr))
               return;
          }
        else if (!strcmp(key, "Name"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "s", name))
               return;
          }
        else if (!strcmp(key, "Icon"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "s", icon))
               return;
          }
        else if (!strcmp(key, "Paired"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "b", paired))
               return;
          }
        else if (!strcmp(key, "Connected"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "b", connected))
               return;
          }
        else if (!strcmp(key, "UUIDs"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "as", uuids))
               return;
          }
     }
}

static void
_on_dev_property_changed(void *context, const Eldbus_Message *msg)
{
   const char *key, *name, *icon;
   char err_msg[4096];
   Eina_Bool paired, connected;
   Eldbus_Message_Iter *variant, *uuids;
   Device *dev = context;
   Device *found_dev = eina_list_search_unsorted(ctxt->found_devices,
                                                 _dev_addr_cmp, dev->addr);

   if (!eldbus_message_arguments_get(msg, "sv", &key, &variant))
     {
        snprintf(err_msg, sizeof(err_msg),
                 _("Property of %s changed, but could not be read"),
                 dev->name);
        ERR("%s", err_msg);
        ebluez4_show_error(_("Bluez Error"), err_msg);
        return;
     }

   if (!strcmp(key, "Name"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "s", &name))
          return;
        DBG("'%s' property of %s changed to %s", key, dev->name, name);
        eina_stringshare_replace(&dev->name, name);

        if (found_dev)
          {
             eina_stringshare_replace(&found_dev->name, name);
             ebluez4_update_instances(ctxt->found_devices);
          }
     }
   else if (!strcmp(key, "Icon"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "s", &icon))
          return;
        if (!found_dev) return;
        DBG("'%s' property of %s changed to %s", key, found_dev->name, icon);
        found_dev->type = _icon_to_type(icon);
        ebluez4_update_instances(ctxt->found_devices);
     }
   else if (!strcmp(key, "Paired"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "b", &paired))
          return;
        DBG("'%s' property of %s changed to %d", key, dev->name, paired);
        dev->paired = paired;

        if (found_dev)
          {
             found_dev->paired = paired;
             ebluez4_update_instances(ctxt->found_devices);
          }
     }
   else if (!strcmp(key, "Connected"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "b", &connected))
          return;
        DBG("'%s' property of %s changed to %d", key, dev->name, connected);
        dev->connected = connected;
     }
   else if (!strcmp(key, "UUIDs"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "as", &uuids))
          return;
        _set_dev_services(dev, uuids);
     }
}

static void
_on_connected(void *data EINA_UNUSED, const Eldbus_Message *msg,
              Eldbus_Pending *pending EINA_UNUSED)
{
   const char *err_name, *err_msg;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("%s: %s", err_name, err_msg);
        ebluez4_show_error(err_name, err_msg);
        return;
     }
}

static void
_try_to_connect(Eldbus_Proxy *proxy)
{
   if (proxy)
     eldbus_proxy_call(proxy, "Connect", _on_connected, NULL, -1, "");
}

static void
_on_disconnected(void *data EINA_UNUSED, const Eldbus_Message *msg,
                 Eldbus_Pending *pending EINA_UNUSED)
{
   const char *err_name, *err_msg;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("%s: %s", err_name, err_msg);
        ebluez4_show_error(err_name, err_msg);
        return;
     }
}

static void
_try_to_disconnect(Eldbus_Proxy *proxy)
{
   if (proxy)
     eldbus_proxy_call(proxy, "Disconnect", _on_disconnected, NULL, -1, "");
}

static void
_on_paired(void *data, const Eldbus_Message *msg,
           Eldbus_Pending *pending EINA_UNUSED)
{
   const char *err_name, *err_msg;
   Pair_Cb *d = data;
   Eina_Bool success = EINA_TRUE;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("%s: %s", err_name, err_msg);
        success = EINA_FALSE;
     }
   if (d->cb) d->cb(d->data, success, err_msg);
   free(d);
}

static void
_on_dev_properties(void *data, const Eldbus_Message *msg,
                   Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *dict, *uuids;
   const char *addr, *name, *icon;
   Eina_Bool paired;
   Eina_Bool connected;
   Device *dev = data;

   if (!eldbus_message_arguments_get(msg, "a{sv}", &dict))
     return;

   _retrieve_properties(dict, &addr, &name, &icon, &paired, &connected, &uuids);

   dev->addr = eina_stringshare_add(addr);
   dev->name = eina_stringshare_add(name);
   dev->paired = paired;
   dev->connected = connected;
   _set_dev_services(dev, uuids);
}

static void
_unset_dev(Device *dev, Eina_List **list)
{
   if (!dev || !list)
     return;

   *list = eina_list_remove(*list, dev);
   _free_dev(dev);
}

static void
_set_dev(const char *path)
{
   Device *dev = calloc(1, sizeof(Device));

   dev->obj = eldbus_object_get(ctxt->conn, BLUEZ_BUS, path);
   dev->proxy.dev = eldbus_proxy_get(dev->obj, DEVICE_INTERFACE);
   eldbus_proxy_call(dev->proxy.dev, "GetProperties", _on_dev_properties, dev,
                    -1, "");
   eldbus_proxy_signal_handler_add(dev->proxy.dev, "PropertyChanged",
                                  _on_dev_property_changed, dev);
   ctxt->devices = eina_list_append(ctxt->devices, dev);
}

static void
_on_removed(void *context EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *path;
   Device *dev, *fdev;

   if (!eldbus_message_arguments_get(msg, "o", &path))
     return;

   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, path);
   fdev = eina_list_search_unsorted(ctxt->found_devices, _dev_addr_cmp,
                                    dev->addr);

   _unset_dev(dev, &ctxt->devices);
   _unset_dev(fdev, &ctxt->found_devices);
}

static void
_on_created(void *context EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *path;

   if (!eldbus_message_arguments_get(msg, "o", &path))
     return;

   _set_dev(path);
}

static void
_on_device_found(void *context EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message_Iter *dict, *uuids;
   const char *addr, *name, *icon;
   Eina_Bool paired, connected;
   Device *dev;

   if (!eldbus_message_arguments_get(msg, "sa{sv}", &addr, &dict))
     return;

   if (eina_list_search_unsorted(ctxt->found_devices, _dev_addr_cmp, addr))
     return;

   _retrieve_properties(dict, &addr, &name, &icon, &paired, &connected, &uuids);

   dev = calloc(1, sizeof(Device));
   dev->addr = eina_stringshare_add(addr);
   dev->name = eina_stringshare_add(name);
   if (icon) dev->type = _icon_to_type(icon);
   dev->paired = paired;
   ctxt->found_devices = eina_list_append(ctxt->found_devices, dev);

   ebluez4_update_instances(ctxt->found_devices);
}

static void
_on_list_devices(void *data EINA_UNUSED, const Eldbus_Message *msg,
                 Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *array;
   const char *path;
   const char *err_msg = _("Error reading list of devices");

   if (!eldbus_message_arguments_get(msg, "ao", &array))
     {
        ERR("%s", err_msg);
        ebluez4_show_error(_("Bluez Error"), err_msg);
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'o', &path))
     _set_dev(path);
}

static void
_on_adap_property_changed(void *context, const Eldbus_Message *msg)
{
   const char *key, *name;
   char err_msg[1096];
   Eina_Bool visible, pairable, powered;
   Eldbus_Message_Iter *variant;
   Adapter *adap = context;

   if (!eldbus_message_arguments_get(msg, "sv", &key, &variant))
     {
        snprintf(err_msg, sizeof(err_msg),
                 _("Property of %s changed, but could not be read"), adap->name);
        ERR("%s", err_msg);
        ebluez4_show_error(_("Bluez Error"), err_msg);
        return;
     }

   if (!strcmp(key, "Name"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "s", &name))
          return;
        DBG("'%s' property of %s changed to %s", key, adap->name, name);
        eina_stringshare_del(adap->name);
        adap->name = eina_stringshare_add(name);
        ebluez4_update_instances(ctxt->adapters);
        return;
     }
   else if (!strcmp(key, "Discoverable"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "b", &visible))
          return;
        DBG("'%s' property of %s changed to %d", key, adap->name, visible);
        adap->visible = visible;
     }
   else if (!strcmp(key, "Pairable"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "b", &pairable))
          return;
        DBG("'%s' property of %s changed to %d", key, adap->name, pairable);
        adap->pairable = pairable;
     }
   else if (!strcmp(key, "Powered"))
     {
        if (!eldbus_message_iter_arguments_get(variant, "b", &powered))
          return;
        DBG("'%s' property of %s changed to %d", key, adap->name, powered);
        adap->powered = powered;
     }

   ebluez4_adapter_properties_update(adap);
}

static void
_on_adap_properties(void *data, const Eldbus_Message *msg,
                    Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *dict, *entry, *variant;
   const char *name, *key;
   Eina_Bool visible, pairable, powered;
   Adapter *adap = data;

   if (!eldbus_message_arguments_get(msg, "a{sv}", &dict))
     return;

   while (eldbus_message_iter_get_and_next(dict, 'e', &entry))
     {
        if (!eldbus_message_iter_arguments_get(entry, "sv", &key, &variant))
           return;

        else if (!strcmp(key, "Name"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "s", &name))
               return;
          }
        else if (!strcmp(key, "Discoverable"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "b", &visible))
               return;
          }
        else if (!strcmp(key, "Pairable"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "b", &pairable))
               return;
          }
        else if (!strcmp(key, "Powered"))
          {
             if (!eldbus_message_iter_arguments_get(variant, "b", &powered))
               return;
          }
     }

   adap->name = eina_stringshare_add(name);
   adap->visible = visible;
   adap->pairable = pairable;
   adap->powered = powered;
   ebluez4_update_instances(ctxt->adapters);
}

static void
_unset_default_adapter(void)
{
   DBG("Remove default adapter %s", eldbus_object_path_get(ctxt->adap_obj));
   _free_dev_list(&ctxt->devices);
   ctxt->devices = NULL;
   _free_dev_list(&ctxt->found_devices);
   ctxt->found_devices = NULL;
   eldbus_object_unref(ctxt->adap_obj);
   ctxt->adap_obj = NULL;
   ebluez4_update_all_gadgets_visibility();
}

static void
_unset_adapter(const char *path)
{
   Adapter *adap = eina_list_search_unsorted(ctxt->adapters, _adap_path_cmp,
                                             path);

   if (!adap)
     return;

   if (!strcmp(eldbus_object_path_get(ctxt->adap_obj), path))
     _unset_default_adapter();
   ctxt->adapters = eina_list_remove(ctxt->adapters, adap);
   _free_adap(adap);
   ebluez4_update_instances(ctxt->adapters);
}

static void
_set_adapter(const char *path)
{
   Adapter *adap = calloc(1, sizeof(Adapter));

   adap->obj = eldbus_object_get(ctxt->conn, BLUEZ_BUS, path);
   if (ctxt->adap_obj && adap->obj == ctxt->adap_obj)
     adap->is_default = EINA_TRUE;
   else
     adap->is_default = EINA_FALSE;
   adap->proxy = eldbus_proxy_get(adap->obj, ADAPTER_INTERFACE);
   eldbus_proxy_call(adap->proxy, "GetProperties", _on_adap_properties, adap, -1,
                    "");
   eldbus_proxy_signal_handler_add(adap->proxy, "PropertyChanged",
                                  _on_adap_property_changed, adap);
   ctxt->adapters = eina_list_append(ctxt->adapters, adap);
}

static void
_on_list_adapters(void *data EINA_UNUSED, const Eldbus_Message *msg,
                  Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *array;
   const char *path;
   const char *err_msg = _("Error reading list of adapters");

   if (!eldbus_message_arguments_get(msg, "ao", &array))
     {
        ERR("%s", err_msg);
        ebluez4_show_error(_("Bluez Error"), err_msg);
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'o', &path))
     _set_adapter(path);
}

static void
_set_default_adapter(const Eldbus_Message *msg)
{
   Adapter *adap;
   const char *adap_path;
   const char *err_msg = _("Error reading path of Default Adapter");

   if (!eldbus_message_arguments_get(msg, "o", &adap_path))
     {
        ERR("%s", err_msg);
        ebluez4_show_error(_("Bluez Error"), err_msg);
        return;
     }

   DBG("Setting default adapter to %s", adap_path);

   if (ctxt->adap_obj)
     _unset_default_adapter();

   adap = eina_list_search_unsorted(ctxt->adapters, _adap_path_cmp, adap_path);
   if (adap)
     adap->is_default = EINA_TRUE;

   ctxt->adap_obj = eldbus_object_get(ctxt->conn, BLUEZ_BUS, adap_path);
   ctxt->adap_proxy = eldbus_proxy_get(ctxt->adap_obj, ADAPTER_INTERFACE);

   eldbus_proxy_signal_handler_add(ctxt->adap_proxy, "DeviceFound",
                                  _on_device_found, NULL);
   eldbus_proxy_signal_handler_add(ctxt->adap_proxy, "DeviceCreated",
                                  _on_created, NULL);
   eldbus_proxy_signal_handler_add(ctxt->adap_proxy, "DeviceRemoved",
                                  _on_removed, NULL);
   eldbus_proxy_call(ctxt->adap_proxy, "ListDevices", _on_list_devices, NULL, -1,
                    "");
   eldbus_proxy_call(ctxt->adap_proxy, "RegisterAgent", NULL, NULL, -1, "os",
                    REMOTE_AGENT_PATH, "KeyboardDisplay");
   ebluez4_update_all_gadgets_visibility();
}

static void
_default_adapter_get(void *data EINA_UNUSED, const Eldbus_Message *msg,
                     Eldbus_Pending *pending EINA_UNUSED)
{
   const char *err_name, *err_msg;

   /*
    * If bluetoothd is starting up, we can fail here and wait for the
    * DefaultAdapterChanged signal later
    */
   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     return;

   if (!ctxt->adap_obj)
     _set_default_adapter(msg);
}

static void
_on_default_adapter_changed(void *context EINA_UNUSED, const Eldbus_Message *msg)
{
   _set_default_adapter(msg);
}

static void
_on_adapter_removed(void *context EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *adap_path;
   const char *err_msg = _("Error reading path of Removed Adapter");

   if (!eldbus_message_arguments_get(msg, "o", &adap_path))
     {
        ERR("%s", err_msg);
        ebluez4_show_error(_("Bluez Error"), err_msg);
        return;
     }

   _unset_adapter(adap_path);
}

static void
_on_adapter_added(void *context EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *adap_path;
   const char *err_msg = _("Error reading path of Added Adapter");

   if (!eldbus_message_arguments_get(msg, "o", &adap_path))
     {
        ERR("%s", err_msg);
        ebluez4_show_error(_("Bluez Error"), err_msg);
        return;
     }

   _set_adapter(adap_path);
}

static void
_bluez_monitor(void *data EINA_UNUSED, const char *bus EINA_UNUSED,
               const char *old_id, const char *new_id)
{
   if (!strcmp(old_id,"") && strcmp(new_id,""))
     {
        // Bluez up
        eldbus_proxy_call(ctxt->man_proxy, "DefaultAdapter",
                         _default_adapter_get, NULL, -1, "");
        eldbus_proxy_call(ctxt->man_proxy, "ListAdapters",
                         _on_list_adapters, NULL, -1, "");
     }
   else if (strcmp(old_id,"") && !strcmp(new_id,""))
     {
        // Bluez down
        _unset_default_adapter();
        _free_adap_list();
     }
}

/* Public Functions */
void
ebluez4_eldbus_init(void)
{
   Eldbus_Object *obj;

   ctxt = calloc(1, sizeof(Context));

   eldbus_init();

   ctxt->conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   obj = eldbus_object_get(ctxt->conn, BLUEZ_BUS, MANAGER_PATH);
   ctxt->man_proxy = eldbus_proxy_get(obj, MANAGER_INTERFACE);

   ebluez4_register_agent_interfaces(ctxt->conn);

   eldbus_proxy_signal_handler_add(ctxt->man_proxy, "DefaultAdapterChanged",
                                  _on_default_adapter_changed, NULL);
   eldbus_proxy_signal_handler_add(ctxt->man_proxy, "AdapterRemoved",
                                  _on_adapter_removed, NULL);
   eldbus_proxy_signal_handler_add(ctxt->man_proxy, "AdapterAdded",
                                  _on_adapter_added, NULL);

   eldbus_name_owner_changed_callback_add(ctxt->conn, BLUEZ_BUS, _bluez_monitor,
                                         NULL, EINA_TRUE);
}

void
ebluez4_eldbus_shutdown(void)
{
   if (ctxt->adap_obj)
     _unset_default_adapter();
   _free_adap_list();
   eldbus_object_unref(eldbus_proxy_object_get(ctxt->man_proxy));
   eldbus_connection_unref(ctxt->conn);
   free(ctxt);

   eldbus_shutdown();
}

void
ebluez4_start_discovery(void)
{
   _free_dev_list(&ctxt->found_devices);
   ebluez4_update_instances(ctxt->found_devices);
   eldbus_proxy_call(ctxt->adap_proxy, "StartDiscovery", NULL, NULL, -1, "");
}

void
ebluez4_stop_discovery(void)
{
   eldbus_proxy_call(ctxt->adap_proxy, "StopDiscovery", NULL, NULL, -1, "");
}

void
ebluez4_connect_to_device(Device *dev)
{
   _try_to_connect(dev->proxy.input);
   _try_to_connect(dev->proxy.audio_source);
   _try_to_connect(dev->proxy.audio_sink);
}

void
ebluez4_disconnect_device(Device *dev)
{
   _try_to_disconnect(dev->proxy.input);
   _try_to_disconnect(dev->proxy.audio_source);
   _try_to_disconnect(dev->proxy.audio_sink);
}

void
ebluez4_pair_with_device(const char *addr, void (*cb)(void *, Eina_Bool, const char *), void *data)
{
   Pair_Cb *d = malloc(sizeof(Pair_Cb));
   EINA_SAFETY_ON_NULL_RETURN(d);
   d->cb = cb;
   d->data = data;
   eldbus_proxy_call(ctxt->adap_proxy, "CreatePairedDevice", _on_paired, d,
                    -1, "sos", addr, AGENT_PATH, "KeyboardDisplay");
}

void
ebluez4_remove_device(Eldbus_Object *obj)
{
   eldbus_proxy_call(ctxt->adap_proxy, "RemoveDevice", NULL, NULL, -1, "o",
                    eldbus_object_path_get(obj));
}

int
ebluez4_dev_path_cmp(const void *d1, const void *d2)
{
   const Device *dev = d1;
   const char *path = d2;

   return strcmp(eldbus_object_path_get(dev->obj), path);
}

void
ebluez4_adapter_property_set(Adapter *adap, const char *prop_name, Eina_Bool value)
{
   Eldbus_Message_Iter *variant, *iter;
   Eldbus_Message *new_msg;

   if (!adap) return;
   if (!adap->obj) return;
   new_msg = eldbus_proxy_method_call_new(adap->proxy, "SetProperty");
   iter = eldbus_message_iter_get(new_msg);
   eldbus_message_iter_basic_append(iter, 's', prop_name);
   variant = eldbus_message_iter_container_new(iter, 'v', "b");
   eldbus_message_iter_basic_append(variant, 'b', value);
   eldbus_message_iter_container_close(iter, variant);
   eldbus_proxy_send(adap->proxy, new_msg, NULL, NULL, -1);
}
