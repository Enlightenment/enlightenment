#include "e_mod_main.h"

/* -------------------------------------------------------------------------- */
/* D-Bus interface constants                                                   */
/* -------------------------------------------------------------------------- */

#define NM_BUS_NAME    "org.freedesktop.NetworkManager"
#define NM_OBJ_PATH    "/org/freedesktop/NetworkManager"

#define NM_IFACE_MGR   "org.freedesktop.NetworkManager"
#define NM_IFACE_DEV   "org.freedesktop.NetworkManager.Device"
#define NM_IFACE_WIFI  "org.freedesktop.NetworkManager.Device.Wireless"
#define NM_IFACE_AP    "org.freedesktop.NetworkManager.AccessPoint"
#define NM_IFACE_ACONN "org.freedesktop.NetworkManager.Connection.Active"
#define NM_IFACE_IP4   "org.freedesktop.NetworkManager.IP4Config"
#define NM_IFACE_PROPS "org.freedesktop.DBus.Properties"
#define NM_IFACE_AGENT_MGR "org.freedesktop.NetworkManager.AgentManager"
#define NM_IFACE_SETTINGS  "org.freedesktop.NetworkManager.Settings"
#define NM_IFACE_SCONN     "org.freedesktop.NetworkManager.Settings.Connection"
#define NM_SETTINGS_PATH   "/org/freedesktop/NetworkManager/Settings"

#define NM_CONNECTION_TIMEOUT (60 * 1000)

/* -------------------------------------------------------------------------- */
/* Module-level globals                                                        */
/* -------------------------------------------------------------------------- */

static unsigned int       init_count;
static Eldbus_Connection *conn;
static struct NM_Manager *nm_manager;
static E_NM_Agent        *agent;

E_API int E_NM_EVENT_MANAGER_IN;
E_API int E_NM_EVENT_MANAGER_OUT;

/* -------------------------------------------------------------------------- */
/* Utility                                                                     */
/* -------------------------------------------------------------------------- */

const char *
enm_state_to_str(enum NM_State state)
{
   switch (state)
     {
      case NM_STATE_ASLEEP:           return "asleep";
      case NM_STATE_DISCONNECTED:     return "disconnected";
      case NM_STATE_DISCONNECTING:    return "disconnecting";
      case NM_STATE_CONNECTING:       return "connecting";
      case NM_STATE_CONNECTED_LOCAL:  return "connected-local";
      case NM_STATE_CONNECTED_SITE:   return "connected-site";
      case NM_STATE_CONNECTED_GLOBAL: return "connected-global";
      case NM_STATE_UNKNOWN:
      default:
         break;
     }
   return "unknown";
}

const char *
enm_device_type_to_str(enum NM_Device_Type type)
{
   switch (type)
     {
      case NM_DEVICE_TYPE_ETHERNET:  return "ethernet";
      case NM_DEVICE_TYPE_WIFI:      return "wifi";
      case NM_DEVICE_TYPE_BLUETOOTH: return "bluetooth";
      case NM_DEVICE_TYPE_MODEM:     return "modem";
      case NM_DEVICE_TYPE_UNKNOWN:
      default:
         break;
     }
   return "unknown";
}

const char *
enm_ap_security_to_str(uint32_t wpa_flags, uint32_t rsn_flags)
{
   if (rsn_flags & NM_AP_SEC_KEY_MGMT_SAE)   return "sae";
   if (rsn_flags & NM_AP_SEC_KEY_MGMT_802_1X) return "802.1x";
   if (rsn_flags & NM_AP_SEC_KEY_MGMT_PSK)    return "wpa2";
   if (wpa_flags & NM_AP_SEC_KEY_MGMT_PSK)    return "wpa";
   if (wpa_flags & NM_AP_SEC_PAIR_WEP40 ||
       wpa_flags & NM_AP_SEC_PAIR_WEP104)     return "wep";
   return "open";
}

/* -------------------------------------------------------------------------- */
/* Access Point                                                                */
/* -------------------------------------------------------------------------- */

static void
_ap_free(struct NM_Access_Point *ap)
{
   Eldbus_Object *obj;

   if (!ap) return;

   if (ap->pending_get_props)
     {
        eldbus_pending_cancel(ap->pending_get_props);
        ap->pending_get_props = NULL;
     }

   free(ap->ssid);
   eina_stringshare_del(ap->path);

   if (ap->proxy)
     {
        if (ap->prop_changed_handler)
          {
             eldbus_signal_handler_del(ap->prop_changed_handler);
             ap->prop_changed_handler = NULL;
          }
        obj = eldbus_proxy_object_get(ap->proxy);
        eldbus_proxy_unref(ap->proxy);
        eldbus_object_unref(obj);
     }

   free(ap);
}

static void
_ap_get_props_cb(void *data, const Eldbus_Message *msg,
                 Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_Access_Point *ap = data;
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;

   ap->pending_get_props = NULL;

   if (eldbus_message_error_get(msg, &name, &text))
     {
        if (strcmp(name, ELDBUS_ERROR_PENDING_CANCELED) != 0)
          WRN("Could not get AP properties. %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        WRN("AP GetAll: error getting arguments");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;

        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "Ssid"))
          {
             /* SSID is ay (byte array) */
             Eldbus_Message_Iter *bytes;
             unsigned char b;
             Eina_Strbuf *buf;

             if (!eldbus_message_iter_arguments_get(var, "ay", &bytes))
               continue;

             buf = eina_strbuf_new();
             while (eldbus_message_iter_get_and_next(bytes, 'y', &b))
               eina_strbuf_append_char(buf, (char)b);

             free(ap->ssid);
             ap->ssid = strdup(eina_strbuf_string_get(buf));
             eina_strbuf_free(buf);
          }
        else if (!strcmp(key, "Strength"))
          {
             uint8_t strength;
             if (eldbus_message_iter_arguments_get(var, "y", &strength))
               ap->strength = strength;
          }
        else if (!strcmp(key, "WpaFlags"))
          {
             uint32_t flags;
             if (eldbus_message_iter_arguments_get(var, "u", &flags))
               ap->wpa_flags = flags;
          }
        else if (!strcmp(key, "RsnFlags"))
          {
             uint32_t flags;
             if (eldbus_message_iter_arguments_get(var, "u", &flags))
               ap->rsn_flags = flags;
          }
        else if (!strcmp(key, "Frequency"))
          {
             uint32_t freq;
             if (eldbus_message_iter_arguments_get(var, "u", &freq))
               ap->frequency = freq;
          }
     }

   DBG("AP %s ssid=%s strength=%d", ap->path, ap->ssid ?: "(hidden)",
       ap->strength);
}

static void
_ap_prop_changed(void *data, const Eldbus_Message *msg)
{
   struct NM_Access_Point *ap = data;
   Eldbus_Message_Iter *changed_props, *invalidated;
   const char *iface;
   Eldbus_Message_Iter *dict, *var;
   const char *key;

   /* PropertiesChanged(s interface, a{sv} changed, as invalidated) */
   if (!eldbus_message_arguments_get(msg, "sa{sv}as",
                                     &iface, &changed_props, &invalidated))
     return;

   while (eldbus_message_iter_get_and_next(changed_props, 'e', &dict))
     {
        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "Strength"))
          {
             uint8_t strength;
             if (eldbus_message_iter_arguments_get(var, "y", &strength))
               ap->strength = strength;
          }
        else if (!strcmp(key, "WpaFlags"))
          {
             uint32_t flags;
             if (eldbus_message_iter_arguments_get(var, "u", &flags))
               ap->wpa_flags = flags;
          }
        else if (!strcmp(key, "RsnFlags"))
          {
             uint32_t flags;
             if (eldbus_message_iter_arguments_get(var, "u", &flags))
               ap->rsn_flags = flags;
          }
     }

   enm_mod_manager_update(nm_manager);
}

static struct NM_Access_Point *
_ap_new(const char *path)
{
   struct NM_Access_Point *ap;
   Eldbus_Object *obj;

   EINA_SAFETY_ON_NULL_RETURN_VAL(path, NULL);

   ap = calloc(1, sizeof(*ap));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ap, NULL);

   ap->path = eina_stringshare_add(path);

   obj = eldbus_object_get(conn, NM_BUS_NAME, path);
   ap->proxy = eldbus_proxy_get(obj, NM_IFACE_PROPS);

   /* Subscribe to PropertiesChanged on the AP object */
   ap->prop_changed_handler =
      eldbus_proxy_signal_handler_add(ap->proxy, "PropertiesChanged",
                                      _ap_prop_changed, ap);

   /* Fetch all AP properties */
   ap->pending_get_props =
      eldbus_proxy_call(ap->proxy, "GetAll", _ap_get_props_cb, ap, -1,
                        "s", NM_IFACE_AP);

   return ap;
}

/* -------------------------------------------------------------------------- */
/* Device                                                                      */
/* -------------------------------------------------------------------------- */

static void _device_free(struct NM_Device *dev);
static struct NM_Device *_device_new(const char *path);
static void _device_wifi_props_cb(void *data, const Eldbus_Message *msg,
                                  Eldbus_Pending *pending);
static void _active_conn_prop_changed(void *data, const Eldbus_Message *msg);
static void _manager_active_conn_watch_free(struct NM_Manager *nm);
static void _manager_ip4_watch_free(struct NM_Manager *nm);
static void _manager_watch_ip4(struct NM_Manager *nm,
                               const char *ip4config_path);

static void
_device_get_aps_cb(void *data, const Eldbus_Message *msg,
                   Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_Device *dev = data;
   Eldbus_Message_Iter *array;
   const char *name, *text, *ap_path;

   dev->pending.get_aps = NULL;

   if (eldbus_message_error_get(msg, &name, &text))
     {
        WRN("GetAccessPoints failed: %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "ao", &array))
     {
        WRN("GetAccessPoints: cannot parse reply");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'o', &ap_path))
     {
        struct NM_Access_Point *ap = _ap_new(ap_path);
        if (!ap) continue;
        dev->access_points = eina_inlist_append(dev->access_points,
                                                EINA_INLIST_GET(ap));
        DBG("Added AP %s to device %s", ap_path, dev->path);
     }

   enm_mod_aps_changed(nm_manager);
}

static void
_device_ap_added(void *data, const Eldbus_Message *msg)
{
   struct NM_Device *dev = data;
   const char *ap_path;

   if (!eldbus_message_arguments_get(msg, "o", &ap_path))
     return;

   DBG("AP added: %s on device %s", ap_path, dev->path);

   /* Check not already tracked */
   struct NM_Access_Point *ap;
   const char *shared = eina_stringshare_add(ap_path);
   EINA_INLIST_FOREACH(dev->access_points, ap)
     if (ap->path == shared)
       {
          eina_stringshare_del(shared);
          return;
       }
   eina_stringshare_del(shared);

   ap = _ap_new(ap_path);
   if (!ap) return;
   dev->access_points = eina_inlist_append(dev->access_points,
                                           EINA_INLIST_GET(ap));
   enm_mod_aps_changed(nm_manager);
}

static void
_device_ap_removed(void *data, const Eldbus_Message *msg)
{
   struct NM_Device *dev = data;
   const char *ap_path;
   struct NM_Access_Point *ap;
   const char *shared;

   if (!eldbus_message_arguments_get(msg, "o", &ap_path))
     return;

   DBG("AP removed: %s on device %s", ap_path, dev->path);

   shared = eina_stringshare_add(ap_path);
   EINA_INLIST_FOREACH(dev->access_points, ap)
     if (ap->path == shared)
       {
          dev->access_points = eina_inlist_remove(dev->access_points,
                                                  EINA_INLIST_GET(ap));
          _ap_free(ap);
          break;
       }
   eina_stringshare_del(shared);
   enm_mod_aps_changed(nm_manager);
}

static void
_device_prop_changed(void *data, const Eldbus_Message *msg)
{
   struct NM_Device *dev = data;
   Eldbus_Message_Iter *changed_props, *invalidated, *dict, *var;
   const char *iface, *key;

   if (!eldbus_message_arguments_get(msg, "sa{sv}as",
                                     &iface, &changed_props, &invalidated))
     return;

   while (eldbus_message_iter_get_and_next(changed_props, 'e', &dict))
     {
        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "State"))
          {
             uint32_t state;
             if (eldbus_message_iter_arguments_get(var, "u", &state))
               {
                  dev->state = state;
                  DBG("Device %s state -> %u", dev->path, state);
                  enm_mod_manager_update(nm_manager);
               }
          }
     }
}

static void
_device_get_props_cb(void *data, const Eldbus_Message *msg,
                     Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_Device *dev = data;
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;

   dev->pending.get_props = NULL;

   if (eldbus_message_error_get(msg, &name, &text))
     {
        WRN("Device GetAll failed: %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        WRN("Device GetAll: cannot parse reply");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;

        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "Interface"))
          {
             const char *iface;
             if (eldbus_message_iter_arguments_get(var, "s", &iface))
               {
                  free(dev->interface);
                  dev->interface = strdup(iface);
               }
          }
        else if (!strcmp(key, "DeviceType"))
          {
             uint32_t dtype;
             if (eldbus_message_iter_arguments_get(var, "u", &dtype))
               {
                  dev->type = (enum NM_Device_Type)dtype;
                  DBG("Device %s type=%u", dev->path, dtype);
               }
          }
        else if (!strcmp(key, "State"))
          {
             uint32_t state;
             if (eldbus_message_iter_arguments_get(var, "u", &state))
               dev->state = state;
          }
        else if (!strcmp(key, "ActiveConnection"))
          {
             const char *aconn_path;
             if (eldbus_message_iter_arguments_get(var, "o", &aconn_path) &&
                 aconn_path && strcmp(aconn_path, "/") != 0)
               {
                  free(dev->active_conn_path);
                  dev->active_conn_path = strdup(aconn_path);
               }
          }
        else if (!strcmp(key, "Ip4Config"))
          {
             const char *ip4;
             if (eldbus_message_iter_arguments_get(var, "o", &ip4) &&
                 ip4 && strcmp(ip4, "/") != 0)
               {
                  free(dev->ip4_path);
                  dev->ip4_path = strdup(ip4);
               }
          }
     }

   /* If this is a WiFi device, get wireless proxy and fetch APs */
   if (dev->type == NM_DEVICE_TYPE_WIFI && !dev->wireless_proxy)
     {
        Eldbus_Object *obj = eldbus_proxy_object_get(dev->proxy);

        /* Re-get object to get the wireless interface proxy */
        dev->wireless_proxy = eldbus_proxy_get(obj, NM_IFACE_WIFI);

        dev->ap_added_handler =
           eldbus_proxy_signal_handler_add(dev->wireless_proxy, "AccessPointAdded",
                                           _device_ap_added, dev);
        dev->ap_removed_handler =
           eldbus_proxy_signal_handler_add(dev->wireless_proxy,
                                           "AccessPointRemoved",
                                           _device_ap_removed, dev);

        dev->pending.get_aps = eldbus_proxy_call(dev->wireless_proxy,
                                                  "GetAccessPoints",
                                                  _device_get_aps_cb, dev,
                                                  -1, "");

        /* Also fetch WiFi-specific props (ActiveAccessPoint) in parallel
         * with GetAccessPoints — this avoids the ActiveConnection.GetAll
         * round-trip on the critical startup path. */
        dev->pending.get_wifi_props =
           eldbus_proxy_call(dev->wireless_proxy, "GetAll",
                             _device_wifi_props_cb, dev,
                             -1, "s", NM_IFACE_WIFI);
     }
   else if (dev->type == NM_DEVICE_TYPE_ETHERNET && dev->state >= 100 &&
            dev->active_conn_path)
     {
        /* Guard: nm_manager may have been freed if NM exited while this
         * D-Bus reply was in flight. */
        if (!nm_manager) return;

        /* First active device wins — skip if another already adopted. */
        if (nm_manager->active_connection_path) return;

        /* Ethernet device is active — set manager state directly without
         * an extra ActiveConnection.GetAll round-trip. */
        DBG("Ethernet device %s active (state=%u), adopting connection %s",
            dev->path, dev->state, dev->active_conn_path);

        nm_manager->probe_generation++;
        _manager_active_conn_watch_free(nm_manager);
        _manager_ip4_watch_free(nm_manager);

        eina_stringshare_del(nm_manager->active_connection_path);
        nm_manager->active_connection_path =
           eina_stringshare_add(dev->active_conn_path);

        nm_manager->active_conn_type = NM_DEVICE_TYPE_ETHERNET;

        eina_stringshare_del(nm_manager->active_ap_path);
        nm_manager->active_ap_path = NULL;

        /* Set up persistent watcher on the active connection object */
        {
           Eldbus_Object *aobj =
              eldbus_object_get(conn, NM_BUS_NAME, dev->active_conn_path);
           nm_manager->active_conn_obj = aobj;
           nm_manager->active_conn_proxy = eldbus_proxy_get(aobj,
                                                            NM_IFACE_PROPS);
           nm_manager->active_conn_signal_handler =
              eldbus_proxy_signal_handler_add(nm_manager->active_conn_proxy,
                                              "PropertiesChanged",
                                              _active_conn_prop_changed,
                                              nm_manager);
        }

        if (dev->ip4_path)
          _manager_watch_ip4(nm_manager, dev->ip4_path);

        enm_mod_manager_update(nm_manager);
        /* Saved connections are WiFi-specific — skip for Ethernet */
     }
}

static void
_device_wifi_props_cb(void *data, const Eldbus_Message *msg,
                      Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_Device *dev = data;
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;
   const char *ap_path = NULL;

   /* Guard: nm_manager may have been freed if NM exited while this D-Bus
    * reply was in flight (e.g. eldbus_pending_cancel called synchronously
    * from _device_free during _e_nm_system_name_owner_exit). */
   if (!nm_manager) return;

   dev->pending.get_wifi_props = NULL;

   if (eldbus_message_error_get(msg, &name, &text))
     {
        WRN("Device Wireless GetAll failed: %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        WRN("Device Wireless GetAll: cannot parse reply");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;

        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "ActiveAccessPoint"))
          {
             const char *path;
             if (eldbus_message_iter_arguments_get(var, "o", &path) &&
                 path && strcmp(path, "/") != 0)
               ap_path = path;
          }
     }

   DBG("Device %s ActiveAccessPoint=%s state=%u",
       dev->path, ap_path ?: "(none)", dev->state);

   /* Only adopt this device as the active connection if it is in activated
    * state (>= 100) and has an active AP. */
   if (dev->state < 100 || !ap_path || !dev->active_conn_path)
     return;

   /* First active device wins — skip if another already adopted. */
   if (nm_manager->active_connection_path)
     return;

   nm_manager->probe_generation++;
   _manager_active_conn_watch_free(nm_manager);
   _manager_ip4_watch_free(nm_manager);

   nm_manager->active_conn_type = NM_DEVICE_TYPE_WIFI;

   eina_stringshare_del(nm_manager->active_ap_path);
   nm_manager->active_ap_path = eina_stringshare_add(ap_path);

   eina_stringshare_del(nm_manager->active_connection_path);
   nm_manager->active_connection_path =
      eina_stringshare_add(dev->active_conn_path);

   /* Set up persistent watcher on the active connection object */
   {
      Eldbus_Object *aobj =
         eldbus_object_get(conn, NM_BUS_NAME, dev->active_conn_path);
      nm_manager->active_conn_obj = aobj;
      nm_manager->active_conn_proxy = eldbus_proxy_get(aobj, NM_IFACE_PROPS);
      nm_manager->active_conn_signal_handler =
         eldbus_proxy_signal_handler_add(nm_manager->active_conn_proxy,
                                         "PropertiesChanged",
                                         _active_conn_prop_changed,
                                         nm_manager);
   }

   if (dev->ip4_path)
     _manager_watch_ip4(nm_manager, dev->ip4_path);

   enm_mod_manager_update(nm_manager);
   enm_saved_connections_get(nm_manager);
}

static struct NM_Device *
_device_new(const char *path)
{
   struct NM_Device *dev;
   Eldbus_Object *obj;

   EINA_SAFETY_ON_NULL_RETURN_VAL(path, NULL);

   dev = calloc(1, sizeof(*dev));
   EINA_SAFETY_ON_NULL_RETURN_VAL(dev, NULL);

   dev->path = eina_stringshare_add(path);

   obj = eldbus_object_get(conn, NM_BUS_NAME, path);
   dev->proxy = eldbus_proxy_get(obj, NM_IFACE_PROPS);

   dev->prop_changed_handler =
      eldbus_proxy_signal_handler_add(dev->proxy, "PropertiesChanged",
                                      _device_prop_changed, dev);

   dev->pending.get_props = eldbus_proxy_call(dev->proxy, "GetAll",
                                              _device_get_props_cb, dev,
                                              -1, "s", NM_IFACE_DEV);
   return dev;
}

static void
_device_free(struct NM_Device *dev)
{
   struct NM_Access_Point *ap;
   Eldbus_Object *obj;

   if (!dev) return;

   if (dev->pending.get_props)
     eldbus_pending_cancel(dev->pending.get_props);
   if (dev->pending.get_aps)
     eldbus_pending_cancel(dev->pending.get_aps);
   if (dev->pending.get_wifi_props)
     eldbus_pending_cancel(dev->pending.get_wifi_props);

   while (dev->access_points)
     {
        ap = EINA_INLIST_CONTAINER_GET(dev->access_points,
                                       struct NM_Access_Point);
        dev->access_points = eina_inlist_remove(dev->access_points,
                                                dev->access_points);
        _ap_free(ap);
     }

   free(dev->interface);
   free(dev->active_conn_path);
   free(dev->ip4_path);

   if (dev->wireless_proxy)
     {
        Eldbus_Object *wobj = eldbus_proxy_object_get(dev->wireless_proxy);
        if (dev->ap_added_handler)
          {
             eldbus_signal_handler_del(dev->ap_added_handler);
             dev->ap_added_handler = NULL;
          }
        if (dev->ap_removed_handler)
          {
             eldbus_signal_handler_del(dev->ap_removed_handler);
             dev->ap_removed_handler = NULL;
          }
        eldbus_proxy_unref(dev->wireless_proxy);
        if (wobj) eldbus_object_unref(wobj);
     }

   if (dev->proxy)
     {
        if (dev->prop_changed_handler)
          {
             eldbus_signal_handler_del(dev->prop_changed_handler);
             dev->prop_changed_handler = NULL;
          }
        obj = eldbus_proxy_object_get(dev->proxy);
        eldbus_proxy_unref(dev->proxy);
        eldbus_object_unref(obj);
     }

   eina_stringshare_del(dev->path);
   free(dev);
}

/* -------------------------------------------------------------------------- */
/* IP tracking (persistent watcher)                                            */
/* -------------------------------------------------------------------------- */

static void
_manager_ip4_watch_free(struct NM_Manager *nm)
{
   if (nm->pending.ip4config)
     {
        eldbus_pending_cancel(nm->pending.ip4config);
        nm->pending.ip4config = NULL;
     }
   if (nm->ip4_proxy)
     {
        if (nm->ip4_prop_handler)
          {
             eldbus_signal_handler_del(nm->ip4_prop_handler);
             nm->ip4_prop_handler = NULL;
          }
        eldbus_proxy_unref(nm->ip4_proxy);
        eldbus_object_unref(nm->ip4_obj);
        nm->ip4_proxy = NULL;
        nm->ip4_obj = NULL;
     }
   eina_stringshare_del(nm->ip4_path);
   nm->ip4_path = NULL;
}

/* Parse AddressData from a variant containing aa{sv} */
static void
_ip4_parse_address_data(struct NM_Manager *nm, Eldbus_Message_Iter *var)
{
   Eldbus_Message_Iter *addr_array, *addr_dict, *entry;
   const char *entry_key;
   Eldbus_Message_Iter *entry_var;

   if (!eldbus_message_iter_arguments_get(var, "aa{sv}", &addr_array))
     return;

   /* Take the first address */
   if (!eldbus_message_iter_get_and_next(addr_array, 'a', &addr_dict))
     return;

   while (eldbus_message_iter_get_and_next(addr_dict, 'e', &entry))
     {
        if (!eldbus_message_iter_arguments_get(entry, "sv",
                                               &entry_key, &entry_var))
          continue;
        if (!strcmp(entry_key, "address"))
          {
             const char *addr;
             if (eldbus_message_iter_arguments_get(entry_var, "s", &addr))
               {
                  free(nm->ip_address);
                  nm->ip_address = strdup(addr);
                  DBG("IP address: %s", nm->ip_address);
               }
          }
     }
}

static void
_ip4_prop_changed(void *data, const Eldbus_Message *msg)
{
   struct NM_Manager *nm = data;
   Eldbus_Message_Iter *changed_props, *invalidated;
   const char *iface;
   Eldbus_Message_Iter *dict, *var;
   const char *key;

   if (!eldbus_message_arguments_get(msg, "sa{sv}as",
                                     &iface, &changed_props, &invalidated))
     return;

   while (eldbus_message_iter_get_and_next(changed_props, 'e', &dict))
     {
        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "AddressData"))
          {
             _ip4_parse_address_data(nm, var);
             enm_mod_manager_update(nm);
          }
     }
}

static void
_ip4config_get_props_cb(void *data, const Eldbus_Message *msg,
                        Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_Manager *nm = data;
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;

   nm->pending.ip4config = NULL;

   if (eldbus_message_error_get(msg, &name, &text))
     {
        WRN("IP4Config GetAll failed: %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     return;

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;

        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "AddressData"))
          _ip4_parse_address_data(nm, var);
     }

   /* Proxy stays alive — updates arrive via _ip4_prop_changed signal */
   enm_mod_manager_update(nm);
}

static void
_manager_watch_ip4(struct NM_Manager *nm, const char *ip4config_path)
{
   Eldbus_Object *obj;
   Eldbus_Proxy *props;

   if (!ip4config_path || !strcmp(ip4config_path, "/")) return;

   /* Skip if already watching this exact path */
   if (nm->ip4_path && !strcmp(nm->ip4_path, ip4config_path))
     return;

   /* Tear down previous watcher */
   _manager_ip4_watch_free(nm);

   obj = eldbus_object_get(conn, NM_BUS_NAME, ip4config_path);
   props = eldbus_proxy_get(obj, NM_IFACE_PROPS);

   nm->ip4_proxy = props;
   nm->ip4_obj = obj;
   nm->ip4_path = eina_stringshare_add(ip4config_path);

   /* Subscribe to property changes — keeps proxy alive for signals */
   nm->ip4_prop_handler =
      eldbus_proxy_signal_handler_add(props, "PropertiesChanged",
                                      _ip4_prop_changed, nm);

   /* Initial fetch */
   nm->pending.ip4config = eldbus_proxy_call(props, "GetAll",
                                             _ip4config_get_props_cb, nm,
                                             -1, "s", NM_IFACE_IP4);
}

/* -------------------------------------------------------------------------- */
/* Active connection tracking (persistent watcher)                             */
/* -------------------------------------------------------------------------- */

static enum NM_Device_Type
_nm_conn_type_parse(const char *type)
{
   if (!type) return NM_DEVICE_TYPE_UNKNOWN;
   if (!strcmp(type, "802-3-ethernet")) return NM_DEVICE_TYPE_ETHERNET;
   if (!strcmp(type, "802-11-wireless")) return NM_DEVICE_TYPE_WIFI;
   if (!strcmp(type, "bluetooth")) return NM_DEVICE_TYPE_BLUETOOTH;
   if (!strcmp(type, "gsm") || !strcmp(type, "cdma")) return NM_DEVICE_TYPE_MODEM;
   return NM_DEVICE_TYPE_UNKNOWN;
}

static void
_manager_active_conn_watch_free(struct NM_Manager *nm)
{
   if (nm->active_conn_signal_handler)
     {
        eldbus_signal_handler_del(nm->active_conn_signal_handler);
        nm->active_conn_signal_handler = NULL;
     }
   if (nm->active_conn_proxy)
     {
        eldbus_proxy_unref(nm->active_conn_proxy);
        eldbus_object_unref(nm->active_conn_obj);
        nm->active_conn_proxy = NULL;
        nm->active_conn_obj = NULL;
     }
   eina_stringshare_del(nm->active_connection_path);
   nm->active_connection_path = NULL;
   eina_stringshare_del(nm->active_ap_path);
   nm->active_ap_path = NULL;
}

static void
_active_conn_prop_changed(void *data, const Eldbus_Message *msg)
{
   struct NM_Manager *nm = data;
   Eldbus_Message_Iter *changed_props, *invalidated;
   const char *iface;
   Eldbus_Message_Iter *dict, *var;
   const char *key;

   if (!eldbus_message_arguments_get(msg, "sa{sv}as",
                                     &iface, &changed_props, &invalidated))
     return;

   while (eldbus_message_iter_get_and_next(changed_props, 'e', &dict))
     {
        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "Ip4Config"))
          {
             const char *ip4path;
             if (eldbus_message_iter_arguments_get(var, "o", &ip4path))
               _manager_watch_ip4(nm, ip4path);
          }
        else if (!strcmp(key, "SpecificObject"))
          {
             const char *ap_path;
             if (eldbus_message_iter_arguments_get(var, "o", &ap_path))
               {
                  DBG("ActiveConn SpecificObject changed: %s", ap_path);
                  eina_stringshare_del(nm->active_ap_path);
                  nm->active_ap_path = (ap_path && strcmp(ap_path, "/"))
                                       ? eina_stringshare_add(ap_path) : NULL;
                  enm_mod_manager_update(nm);
                  enm_mod_aps_changed(nm);
               }
          }
     }
}

/* ------ Active connection probe ------ */
/* We probe each active connection with a lightweight GetAll.  Only when the
 * callback discovers a wifi/ethernet type do we "promote" that probe to the
 * persistent watcher on NM_Manager.  Others (bridge, vpn, …) are freed. */

struct _Active_Conn_Probe
{
   struct NM_Manager *nm;
   Eldbus_Proxy      *proxy;
   Eldbus_Object     *obj;
   const char        *path;       /* stringshare */
   unsigned int       generation; /* snapshot of nm->probe_generation at creation */
};

static void
_active_conn_probe_free(struct _Active_Conn_Probe *probe)
{
   if (!probe) return;
   if (probe->proxy) eldbus_proxy_unref(probe->proxy);
   if (probe->obj)   eldbus_object_unref(probe->obj);
   eina_stringshare_del(probe->path);
   free(probe);
}

static void
_active_conn_probe_cb(void *data, const Eldbus_Message *msg,
                      Eldbus_Pending *pending EINA_UNUSED)
{
   struct _Active_Conn_Probe *probe = data;
   struct NM_Manager *nm = probe->nm;
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;
   const char *ap_path = NULL, *ip4path = NULL, *type_str = NULL;
   enum NM_Device_Type conn_type;

   /* Discard stale probes that were superseded by a newer batch */
   if (probe->generation != nm->probe_generation)
     {
        DBG("Discarding stale probe (gen %u vs current %u) path=%s",
            probe->generation, nm->probe_generation, probe->path ?: "?");
        _active_conn_probe_free(probe);
        return;
     }

   if (eldbus_message_error_get(msg, &name, &text))
     {
        WRN("ActiveConnection GetAll failed: %s: %s", name, text);
        _active_conn_probe_free(probe);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        WRN("ActiveConnection GetAll: cannot parse a{sv}");
        _active_conn_probe_free(probe);
        return;
     }

   /* Parse into locals first so we can check the type before committing */
   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;

        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "Ip4Config"))
          eldbus_message_iter_arguments_get(var, "o", &ip4path);
        else if (!strcmp(key, "SpecificObject"))
          eldbus_message_iter_arguments_get(var, "o", &ap_path);
        else if (!strcmp(key, "Type"))
          eldbus_message_iter_arguments_get(var, "s", &type_str);
     }

   conn_type = _nm_conn_type_parse(type_str);

   /* NM uses "/" as the null/none sentinel for object paths */
   if (ap_path && !strcmp(ap_path, "/")) ap_path = NULL;

   /* Skip non-network types (bridge, vpn, etc.) — they don't have
    * useful SpecificObject or signal strength info. */
   if (conn_type != NM_DEVICE_TYPE_WIFI &&
       conn_type != NM_DEVICE_TYPE_ETHERNET)
     {
        DBG("ActiveConn type=%s (%d) — skipping", type_str ?: "?", conn_type);
        _active_conn_probe_free(probe);
        return;
     }

   /* This is a useful connection — promote probe to persistent watcher */
   DBG("ActiveConn type=%s (%d) — adopting path=%s",
       type_str ?: "?", conn_type, probe->path);

   /* Tear down any previous persistent watcher */
   _manager_active_conn_watch_free(nm);
   _manager_ip4_watch_free(nm);

   /* Transfer ownership from probe to nm */
   eina_stringshare_del(nm->active_connection_path);
   nm->active_connection_path = probe->path;
   probe->path = NULL; /* transferred */

   nm->active_conn_proxy = probe->proxy;
   nm->active_conn_obj = probe->obj;
   probe->proxy = NULL; /* transferred */
   probe->obj = NULL;   /* transferred */

   nm->active_conn_type = conn_type;

   eina_stringshare_del(nm->active_ap_path);
   nm->active_ap_path = ap_path ? eina_stringshare_add(ap_path) : NULL;

   /* Subscribe to property changes on the now-persistent proxy */
   nm->active_conn_signal_handler =
      eldbus_proxy_signal_handler_add(nm->active_conn_proxy, "PropertiesChanged",
                                      _active_conn_prop_changed, nm);

   if (ip4path)
     _manager_watch_ip4(nm, ip4path);

   free(probe); /* fields already transferred or NULL */

   enm_mod_manager_update(nm);
   enm_mod_aps_changed(nm);
}

static void
_manager_probe_active_conn(struct NM_Manager *nm,
                           const char *active_conn_path)
{
   struct _Active_Conn_Probe *probe;

   DBG("_manager_probe_active_conn path=%s", active_conn_path ?: "(null)");
   if (!active_conn_path || !strcmp(active_conn_path, "/")) return;

   /* Skip if already watching this exact connection */
   if (nm->active_connection_path &&
       !strcmp(nm->active_connection_path, active_conn_path))
     return;

   probe = calloc(1, sizeof(*probe));
   if (!probe) return;

   probe->nm = nm;
   probe->generation = nm->probe_generation;
   probe->path = eina_stringshare_add(active_conn_path);
   probe->obj = eldbus_object_get(conn, NM_BUS_NAME, active_conn_path);
   probe->proxy = eldbus_proxy_get(probe->obj, NM_IFACE_PROPS);

   eldbus_proxy_call(probe->proxy, "GetAll",
                     _active_conn_probe_cb, probe,
                     -1, "s", NM_IFACE_ACONN);
}

/* -------------------------------------------------------------------------- */
/* Manager                                                                     */
/* -------------------------------------------------------------------------- */

static void _manager_free(struct NM_Manager *nm);
static struct NM_Manager *_manager_new(void);
static void _settings_conn_removed_cb(void *data, const Eldbus_Message *msg);

static void
_manager_prop_changed(void *data, const Eldbus_Message *msg)
{
   struct NM_Manager *nm = data;
   Eldbus_Message_Iter *changed_props, *invalidated;
   const char *iface;
   Eldbus_Message_Iter *dict, *var;
   const char *key;

   /* NM uses org.freedesktop.DBus.Properties.PropertiesChanged */
   if (!eldbus_message_arguments_get(msg, "sa{sv}as",
                                     &iface, &changed_props, &invalidated))
     return;

   while (eldbus_message_iter_get_and_next(changed_props, 'e', &dict))
     {
        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "State"))
          {
             uint32_t state;
             if (eldbus_message_iter_arguments_get(var, "u", &state))
               {
                  nm->state = (enum NM_State)state;
                  DBG("NM state changed: %u (%s)", state,
                      enm_state_to_str(nm->state));
                  enm_mod_manager_update(nm);
               }
          }
        else if (!strcmp(key, "WirelessEnabled"))
          {
             Eina_Bool enabled;
             if (eldbus_message_iter_arguments_get(var, "b", &enabled))
               {
                  nm->wireless_enabled = enabled;
                  enm_mod_manager_update(nm);
               }
          }
        else if (!strcmp(key, "ActiveConnections"))
          {
             Eldbus_Message_Iter *conn_array;
             const char *aconn_path;

             if (!eldbus_message_iter_arguments_get(var, "ao", &conn_array))
               continue;

             /* Advance generation so any in-flight probes from the previous
              * batch are discarded in their callbacks.  Then clear the old
              * watchers so we don't display a stale connection while the new
              * probes are in flight. */
             nm->probe_generation++;
             _manager_active_conn_watch_free(nm);
             _manager_ip4_watch_free(nm);

             /* Try each active connection — _active_conn_probe_cb
              * filters out bridge/vpn types and only adopts wifi/ethernet */
             if (eldbus_message_iter_get_and_next(conn_array, 'o', &aconn_path))
               {
                  do
                    {
                       _manager_probe_active_conn(nm, aconn_path);
                    }
                  while (eldbus_message_iter_get_and_next(conn_array, 'o', &aconn_path));
               }
             else
               {
                  /* No active connections — tear down already done above */
                  eina_stringshare_del(nm->active_ap_path);
                  nm->active_ap_path = NULL;
                  eina_stringshare_del(nm->active_connection_path);
                  nm->active_connection_path = NULL;
                  free(nm->ip_address);
                  nm->ip_address = NULL;
                  nm->active_conn_type = NM_DEVICE_TYPE_UNKNOWN;
                  enm_mod_manager_update(nm);
               }
          }
     }
}

static void
_manager_get_props_cb(void *data, const Eldbus_Message *msg,
                      Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_Manager *nm = data;
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;

   nm->pending.get_props = NULL;

   if (eldbus_message_error_get(msg, &name, &text))
     {
        WRN("NM GetAll failed: %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        WRN("NM GetAll: cannot parse reply");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;

        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          continue;

        if (!strcmp(key, "State"))
          {
             uint32_t state;
             if (eldbus_message_iter_arguments_get(var, "u", &state))
               nm->state = (enum NM_State)state;
          }
        else if (!strcmp(key, "WirelessEnabled"))
          {
             Eina_Bool enabled;
             if (eldbus_message_iter_arguments_get(var, "b", &enabled))
               nm->wireless_enabled = enabled;
          }
        else if (!strcmp(key, "ActiveConnections"))
          {
             Eldbus_Message_Iter *conn_array;
             const char *aconn_path;

             if (!eldbus_message_iter_arguments_get(var, "ao", &conn_array))
               {
                  WRN("ActiveConnections: cannot parse ao from variant");
                  continue;
               }

             /* On startup we do NOT launch ActiveConnection.GetAll probes
              * here — the device callbacks (_device_wifi_props_cb for WiFi,
              * _device_get_props_cb for Ethernet) derive all needed info from
              * Device.GetAll / Device.Wireless.GetAll which are already
              * running in parallel. */
             while (eldbus_message_iter_get_and_next(conn_array, 'o', &aconn_path))
               DBG("ActiveConnections (startup): path=%s — device probes pending",
                   aconn_path);
          }
     }

   DBG("manager_get_props done: state=%d active_ap=%s conn_type=%d",
       nm->state, nm->active_ap_path ?: "(null)", nm->active_conn_type);

   /* Always update the UI now so that disconnected/VPN-only/unassociated
    * states are shown immediately.  Device callbacks will call
    * enm_mod_manager_update again once they have full type and AP info. */
   enm_mod_manager_update(nm);
}

static void
_manager_get_devices_cb(void *data, const Eldbus_Message *msg,
                        Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_Manager *nm = data;
   Eldbus_Message_Iter *array;
   const char *name, *text, *dev_path;

   nm->pending.get_devices = NULL;

   if (eldbus_message_error_get(msg, &name, &text))
     {
        WRN("GetDevices failed: %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "ao", &array))
     {
        WRN("GetDevices: cannot parse reply");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'o', &dev_path))
     {
        struct NM_Device *dev = _device_new(dev_path);
        if (!dev) continue;
        nm->devices = eina_inlist_append(nm->devices, EINA_INLIST_GET(dev));
        DBG("Added device %s", dev_path);
     }
}

static void
_manager_device_added(void *data, const Eldbus_Message *msg)
{
   struct NM_Manager *nm = data;
   const char *dev_path;
   struct NM_Device *dev;
   const char *shared;

   if (!eldbus_message_arguments_get(msg, "o", &dev_path))
     return;

   DBG("Device added: %s", dev_path);

   shared = eina_stringshare_add(dev_path);
   EINA_INLIST_FOREACH(nm->devices, dev)
     if (dev->path == shared)
       {
          eina_stringshare_del(shared);
          return;
       }
   eina_stringshare_del(shared);

   dev = _device_new(dev_path);
   if (!dev) return;
   nm->devices = eina_inlist_append(nm->devices, EINA_INLIST_GET(dev));
}

static void
_manager_device_removed(void *data, const Eldbus_Message *msg)
{
   struct NM_Manager *nm = data;
   const char *dev_path;
   struct NM_Device *dev;
   const char *shared;

   if (!eldbus_message_arguments_get(msg, "o", &dev_path))
     return;

   DBG("Device removed: %s", dev_path);

   shared = eina_stringshare_add(dev_path);
   EINA_INLIST_FOREACH(nm->devices, dev)
     if (dev->path == shared)
       {
          nm->devices = eina_inlist_remove(nm->devices, EINA_INLIST_GET(dev));
          _device_free(dev);
          break;
       }
   eina_stringshare_del(shared);
}

/* -------------------------------------------------------------------------- */
/* Saved connections (for forget button)                                       */
/* -------------------------------------------------------------------------- */

struct _Saved_Conn_Ctx
{
   struct NM_Manager *nm;
   const char        *path;       /* stringshare: connection D-Bus object path */
   Eldbus_Proxy      *proxy;      /* Settings.Connection proxy — unref in callback */
   Eldbus_Object     *obj;        /* connection object — unref in callback */
   unsigned int       generation; /* snapshot of nm->saved_conn_generation */
};

static void
_saved_conn_settings_cb(void *data, const Eldbus_Message *msg,
                        Eldbus_Pending *pending EINA_UNUSED)
{
   struct _Saved_Conn_Ctx *ctx = data;
   Eldbus_Message_Iter *settings, *dict_entry, *setting_dict, *variant;
   const char *setting_name, *key;
   char ssid_str[256];

   /* Bail out if the manager was freed or a new batch was started while
    * this D-Bus call was in flight — avoids writing to freed/stale state. */
   if (ctx->generation != ctx->nm->saved_conn_generation)
     {
        DBG("_saved_conn_settings_cb: stale generation %u vs %u, discarding",
            ctx->generation, ctx->nm->saved_conn_generation);
        goto done;
     }

   {
      const char *err_name = NULL, *err_msg = NULL;
      if (eldbus_message_error_get(msg, &err_name, &err_msg))
        {
           ERR("GetSettings failed: %s %s", err_name ?: "", err_msg ?: "");
           goto done;
        }
   }
   if (!eldbus_message_arguments_get(msg, "a{sa{sv}}", &settings))
     {
        ERR("GetSettings: failed to parse reply arguments");
        goto done;
     }

   while (eldbus_message_iter_get_and_next(settings, 'e', &dict_entry))
     {
        Eldbus_Message_Iter *inner_entry;

        if (!eldbus_message_iter_arguments_get(dict_entry, "sa{sv}",
                                               &setting_name, &setting_dict))
          continue;

        if (strcmp(setting_name, "802-11-wireless") != 0)
          continue;

        while (eldbus_message_iter_get_and_next(setting_dict, 'e', &inner_entry))
          {
             if (!eldbus_message_iter_arguments_get(inner_entry, "sv",
                                                    &key, &variant))
               continue;

             if (!strcmp(key, "ssid"))
               {
                  Eldbus_Message_Iter *ssid_iter;
                  unsigned char byte;
                  int i = 0;

                  if (!eldbus_message_iter_arguments_get(variant, "ay", &ssid_iter))
                    continue;

                  while (eldbus_message_iter_get_and_next(ssid_iter, 'y', &byte)
                         && i < (int)(sizeof(ssid_str) - 1))
                    ssid_str[i++] = (char)byte;
                  ssid_str[i] = '\0';

                  if (i > 0 && ctx->nm->saved_connections)
                    {
                       /* Remove old entry first to avoid leaking stringshare */
                       eina_hash_del_by_key(ctx->nm->saved_connections, ssid_str);
                       eina_hash_add(ctx->nm->saved_connections, ssid_str,
                                     eina_stringshare_add(ctx->path));
                       INF("saved_conn: added '%s' -> %s", ssid_str, ctx->path);
                    }
                  goto done;
               }
          }
     }

done:
   eldbus_proxy_unref(ctx->proxy);
   eldbus_object_unref(ctx->obj);
   eina_stringshare_del(ctx->path);
   /* Refresh popup once all GetSettings replies have arrived */
   if (ctx->nm->saved_conn_pending > 0)
     ctx->nm->saved_conn_pending--;
   if (ctx->nm->saved_conn_pending == 0)
     enm_mod_aps_changed(ctx->nm);
   free(ctx);
}

struct _Settings_List_Ctx
{
   struct NM_Manager *nm;
   Eldbus_Proxy      *proxy;
   Eldbus_Object     *obj;
   unsigned int       generation; /* snapshot of nm->saved_conn_generation */
};

static void
_saved_conn_list_cb(void *data, const Eldbus_Message *msg,
                    Eldbus_Pending *pending EINA_UNUSED)
{
   struct _Settings_List_Ctx *sctx = data;
   struct NM_Manager *nm = sctx->nm;
   unsigned int generation = sctx->generation;
   Eldbus_Message_Iter *conn_array;
   const char *conn_path;

   /* Free the Settings proxy/obj that were kept alive for this call */
   eldbus_proxy_unref(sctx->proxy);
   eldbus_object_unref(sctx->obj);
   free(sctx);

   /* Bail out if a newer batch was started or manager was freed. */
   if (generation != nm->saved_conn_generation)
     {
        DBG("_saved_conn_list_cb: stale generation %u vs %u, discarding",
            generation, nm->saved_conn_generation);
        return;
     }

   {
      const char *err_name = NULL, *err_msg = NULL;
      if (eldbus_message_error_get(msg, &err_name, &err_msg))
        {
           ERR("ListConnections failed: %s %s", err_name ?: "", err_msg ?: "");
           return;
        }
   }
   if (!eldbus_message_arguments_get(msg, "ao", &conn_array))
     {
        ERR("ListConnections: failed to parse reply");
        return;
     }

   nm->saved_conn_pending = 0;

   while (eldbus_message_iter_get_and_next(conn_array, 'o', &conn_path))
     {
        struct _Saved_Conn_Ctx *ctx;

        ctx = malloc(sizeof(*ctx));
        if (!ctx) continue;

        ctx->nm = nm;
        ctx->generation = generation;
        ctx->path = eina_stringshare_add(conn_path);
        ctx->obj = eldbus_object_get(conn, NM_BUS_NAME, conn_path);
        ctx->proxy = eldbus_proxy_get(ctx->obj, NM_IFACE_SCONN);

        nm->saved_conn_pending++;
        eldbus_proxy_call(ctx->proxy, "GetSettings",
                          _saved_conn_settings_cb, ctx, -1, "");
        /* ctx, proxy, and obj are freed/unref'd inside _saved_conn_settings_cb */
     }

   /* If no connections found, trigger refresh immediately */
   if (nm->saved_conn_pending == 0)
     enm_mod_aps_changed(nm);
}

static void
_saved_connections_free_cb(void *data)
{
   eina_stringshare_del(data);
}

void
enm_saved_connections_get(struct NM_Manager *nm)
{
   EINA_SAFETY_ON_NULL_RETURN(nm);

   {
      struct _Settings_List_Ctx *sctx = malloc(sizeof(*sctx));
      EINA_SAFETY_ON_NULL_RETURN(sctx);

      /* Increment generation so any in-flight ListConnections/GetSettings
       * callbacks from the previous batch will detect staleness and abort. */
      nm->saved_conn_generation++;

      /* Swap pattern: assign new hash before freeing old to avoid a window
       * where saved_connections is NULL (in-flight callbacks check it).
       * Only performed after malloc succeeds so OOM cannot corrupt the hash. */
      {
         Eina_Hash *old = nm->saved_connections;
         nm->saved_connections = eina_hash_string_superfast_new(
                                    _saved_connections_free_cb);
         if (old) eina_hash_free(old);
      }

      sctx->nm = nm;
      sctx->generation = nm->saved_conn_generation;
      sctx->obj = eldbus_object_get(conn, NM_BUS_NAME, NM_SETTINGS_PATH);
      sctx->proxy = eldbus_proxy_get(sctx->obj, NM_IFACE_SETTINGS);
      eldbus_proxy_call(sctx->proxy, "ListConnections",
                        _saved_conn_list_cb, sctx, -1, "");
      /* proxy and obj are kept alive until _saved_conn_list_cb fires */
   }
}

static void
_connection_delete_cb(void *data, const Eldbus_Message *msg,
                      Eldbus_Pending *pending EINA_UNUSED)
{
   struct _Saved_Conn_Ctx *ctx = data;
   const char *err_name, *err_msg;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("Failed to delete connection %s: %s %s",
            ctx->path, err_name, err_msg);
        /* Recover: the optimistic UI removal already hid the forget button.
         * Refresh saved_connections so the button reappears if the delete
         * failed (e.g. stale connection path). */
        enm_saved_connections_get(ctx->nm);
        goto done;
     }

   INF("Connection %s deleted successfully — awaiting ConnectionRemoved signal",
       ctx->path);
   /* Do NOT call enm_saved_connections_get here: NM may not have removed the
    * connection from its internal list yet, so ListConnections would still
    * return the deleted path and the SSID would be re-added to the hash.
    * The ConnectionRemoved signal (_settings_conn_removed_cb) fires once NM
    * has actually removed the object and will trigger the authoritative
    * refresh. */

done:
   eldbus_proxy_unref(ctx->proxy);
   eldbus_object_unref(ctx->obj);
   eina_stringshare_del(ctx->path);
   free(ctx);
}

void
enm_connection_delete(struct NM_Manager *nm, const char *connection_path)
{
   struct _Saved_Conn_Ctx *ctx;

   EINA_SAFETY_ON_NULL_RETURN(nm);
   EINA_SAFETY_ON_NULL_RETURN(connection_path);

   ctx = malloc(sizeof(*ctx));
   EINA_SAFETY_ON_NULL_RETURN(ctx);

   ctx->nm = nm;
   ctx->path = eina_stringshare_add(connection_path);
   ctx->obj = eldbus_object_get(conn, NM_BUS_NAME, connection_path);
   ctx->proxy = eldbus_proxy_get(ctx->obj, NM_IFACE_SCONN);

   eldbus_proxy_call(ctx->proxy, "Delete",
                     _connection_delete_cb, ctx, -1, "");
   /* ctx, proxy, and obj are freed/unref'd inside _connection_delete_cb */
}

/* Called when NM removes a saved connection — refresh the hash so the
 * forget button disappears without a visible race window. */
static void
_settings_conn_removed_cb(void *data, const Eldbus_Message *msg EINA_UNUSED)
{
   struct NM_Manager *nm = data;
   if (!nm) return;
   INF("ConnectionRemoved signal: refreshing saved connections");
   enm_saved_connections_get(nm);
}

/* Called when NM adds a new connection (e.g. via AddAndActivateConnection) —
 * refresh the hash so the forget button appears for the newly saved network. */
static void
_settings_conn_added_cb(void *data, const Eldbus_Message *msg EINA_UNUSED)
{
   struct NM_Manager *nm = data;
   if (!nm) return;
   INF("NewConnection signal: refreshing saved connections");
   enm_saved_connections_get(nm);
}

static struct NM_Manager *
_manager_new(void)
{
   struct NM_Manager *nm;
   Eldbus_Object *obj;

   nm = calloc(1, sizeof(*nm));
   EINA_SAFETY_ON_NULL_RETURN_VAL(nm, NULL);

   obj = eldbus_object_get(conn, NM_BUS_NAME, NM_OBJ_PATH);
   nm->proxy       = eldbus_proxy_get(obj, NM_IFACE_MGR);
   nm->props_proxy = eldbus_proxy_get(obj, NM_IFACE_PROPS);

   /* PropertiesChanged on the org.freedesktop.DBus.Properties iface */
   nm->prop_changed_handler =
      eldbus_proxy_signal_handler_add(nm->props_proxy, "PropertiesChanged",
                                      _manager_prop_changed, nm);

   /* DeviceAdded / DeviceRemoved on NM iface */
   nm->device_added_handler =
      eldbus_proxy_signal_handler_add(nm->proxy, "DeviceAdded",
                                      _manager_device_added, nm);
   nm->device_removed_handler =
      eldbus_proxy_signal_handler_add(nm->proxy, "DeviceRemoved",
                                      _manager_device_removed, nm);

   nm->pending.get_props =
      eldbus_proxy_call(nm->props_proxy, "GetAll",
                        _manager_get_props_cb, nm, -1,
                        "s", NM_IFACE_MGR);

   nm->pending.get_devices =
      eldbus_proxy_call(nm->proxy, "GetDevices",
                        _manager_get_devices_cb, nm, -1, "");

   /* Persistent Settings object for ConnectionRemoved signal.
    * This avoids the race in _connection_delete_cb where ListConnections
    * may still return the just-deleted path before NM's internal list is
    * updated.  Instead we react to the authoritative signal. */
   nm->settings_obj = eldbus_object_get(conn, NM_BUS_NAME, NM_SETTINGS_PATH);
   nm->settings_proxy = eldbus_proxy_get(nm->settings_obj, NM_IFACE_SETTINGS);
   nm->conn_removed_handler =
      eldbus_proxy_signal_handler_add(nm->settings_proxy, "ConnectionRemoved",
                                      _settings_conn_removed_cb, nm);
   nm->conn_added_handler =
      eldbus_proxy_signal_handler_add(nm->settings_proxy, "NewConnection",
                                      _settings_conn_added_cb, nm);

   return nm;
}

static void
_manager_free(struct NM_Manager *nm)
{
   struct NM_Device *dev;
   Eldbus_Object *obj;

   if (!nm) return;

   /* Invalidate in-flight saved-connection probes before freeing anything.
    * Both _saved_conn_list_cb and _saved_conn_settings_cb check this field
    * and will bail out cleanly without touching the freed NM_Manager. */
   nm->saved_conn_generation++;

   if (nm->pending.get_props)
     eldbus_pending_cancel(nm->pending.get_props);
   if (nm->pending.get_devices)
     eldbus_pending_cancel(nm->pending.get_devices);

   _manager_active_conn_watch_free(nm);
   _manager_ip4_watch_free(nm);

   while (nm->devices)
     {
        dev = EINA_INLIST_CONTAINER_GET(nm->devices, struct NM_Device);
        nm->devices = eina_inlist_remove(nm->devices, nm->devices);
        _device_free(dev);
     }

   if (nm->saved_connections)
     {
        eina_hash_free(nm->saved_connections);
        nm->saved_connections = NULL;
     }

   free(nm->ip_address);
   /* active_ap_path and active_connection_path already freed+NULLed
    * by _manager_active_conn_watch_free() above */

   /* Tear down the Settings signal subscription */
   if (nm->conn_removed_handler)
     {
        eldbus_signal_handler_del(nm->conn_removed_handler);
        nm->conn_removed_handler = NULL;
     }
   if (nm->conn_added_handler)
     {
        eldbus_signal_handler_del(nm->conn_added_handler);
        nm->conn_added_handler = NULL;
     }
   if (nm->settings_proxy)
     {
        eldbus_proxy_unref(nm->settings_proxy);
        nm->settings_proxy = NULL;
     }
   if (nm->settings_obj)
     {
        eldbus_object_unref(nm->settings_obj);
        nm->settings_obj = NULL;
     }

   if (nm->prop_changed_handler)
     {
        eldbus_signal_handler_del(nm->prop_changed_handler);
        nm->prop_changed_handler = NULL;
     }
   if (nm->device_added_handler)
     {
        eldbus_signal_handler_del(nm->device_added_handler);
        nm->device_added_handler = NULL;
     }
   if (nm->device_removed_handler)
     {
        eldbus_signal_handler_del(nm->device_removed_handler);
        nm->device_removed_handler = NULL;
     }

   obj = eldbus_proxy_object_get(nm->proxy);
   eldbus_proxy_unref(nm->proxy);
   eldbus_proxy_unref(nm->props_proxy);
   eldbus_object_unref(obj);

   free(nm);
}

/* -------------------------------------------------------------------------- */
/* Connection actions                                                          */
/* -------------------------------------------------------------------------- */

struct connection_cb_data
{
   struct NM_Manager      *nm;
   struct NM_Device       *dev;
   struct NM_Access_Point *ap;
};

static void
_activate_cb(void *data, const Eldbus_Message *msg,
             Eldbus_Pending *pending EINA_UNUSED)
{
   struct connection_cb_data *cd = data;
   const char *name, *text;

   if (eldbus_message_error_get(msg, &name, &text))
     ERR("ActivateConnection failed: %s: %s", name, text);
   else
     INF("ActivateConnection succeeded");

   free(cd);
}

static void
_add_activate_cb(void *data, const Eldbus_Message *msg,
                 Eldbus_Pending *pending EINA_UNUSED)
{
   struct connection_cb_data *cd = data;
   const char *name, *text;

   if (eldbus_message_error_get(msg, &name, &text))
     ERR("AddAndActivateConnection failed: %s: %s", name, text);
   else
     INF("AddAndActivateConnection succeeded");

   free(cd);
}

void
enm_ap_connect(struct NM_Manager *nm, struct NM_Device *dev,
               struct NM_Access_Point *ap)
{
   struct connection_cb_data *cd;
   const char *conn_path = NULL;

   EINA_SAFETY_ON_NULL_RETURN(nm);
   EINA_SAFETY_ON_NULL_RETURN(dev);
   EINA_SAFETY_ON_NULL_RETURN(ap);

   cd = calloc(1, sizeof(*cd));
   EINA_SAFETY_ON_NULL_RETURN(cd);

   cd->nm  = nm;
   cd->dev = dev;
   cd->ap  = ap;

   if (nm->saved_connections && ap->ssid)
     conn_path = eina_hash_find(nm->saved_connections, ap->ssid);

   if (conn_path)
     {
        /* Known saved profile — activate it directly */
        INF("ActivateConnection: saved profile %s for ssid=%s",
            conn_path, ap->ssid);
        eldbus_proxy_call(nm->proxy, "ActivateConnection",
                          _activate_cb, cd, NM_CONNECTION_TIMEOUT,
                          "ooo", conn_path, dev->path, ap->path);
     }
   else
     {
        /* No saved profile (new network or just forgotten) — let NM create
         * a fresh profile and call our agent for credentials. */
        Eldbus_Message *msg_call;
        Eldbus_Message_Iter *iter, *empty_dict;

        INF("AddAndActivateConnection: no saved profile for ssid=%s",
            ap->ssid ?: "(null)");

        msg_call = eldbus_proxy_method_call_new(nm->proxy,
                                                "AddAndActivateConnection");
        iter = eldbus_message_iter_get(msg_call);
        /* Empty a{sa{sv}} connection dict — NM fills in security from the AP */
        eldbus_message_iter_arguments_append(iter, "a{sa{sv}}", &empty_dict);
        eldbus_message_iter_container_close(iter, empty_dict);
        eldbus_message_iter_basic_append(iter, 'o', dev->path);
        eldbus_message_iter_basic_append(iter, 'o', ap->path);
        eldbus_proxy_send(nm->proxy, msg_call, _add_activate_cb, cd,
                          NM_CONNECTION_TIMEOUT);
     }
}

static void
_deactivate_cb(void *data EINA_UNUSED, const Eldbus_Message *msg,
               Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;

   if (eldbus_message_error_get(msg, &name, &text))
     ERR("DeactivateConnection failed: %s: %s", name, text);
   else
     INF("DeactivateConnection succeeded");
}

void
enm_ap_disconnect(struct NM_Manager *nm)
{
   EINA_SAFETY_ON_NULL_RETURN(nm);

   if (!nm->active_connection_path ||
       !strcmp(nm->active_connection_path, "/"))
     {
        WRN("No active connection to disconnect");
        return;
     }

   eldbus_proxy_call(nm->proxy, "DeactivateConnection",
                     _deactivate_cb, NULL, -1,
                     "o", nm->active_connection_path);
}

void
enm_wireless_enabled_set(struct NM_Manager *nm, Eina_Bool enabled)
{
   Eldbus_Message *msg;
   Eldbus_Message_Iter *main_iter, *var;

   EINA_SAFETY_ON_NULL_RETURN(nm);

   msg = eldbus_proxy_method_call_new(nm->props_proxy, "Set");
   main_iter = eldbus_message_iter_get(msg);
   eldbus_message_iter_basic_append(main_iter, 's', NM_IFACE_MGR);
   eldbus_message_iter_basic_append(main_iter, 's', "WirelessEnabled");
   var = eldbus_message_iter_container_new(main_iter, 'v', "b");
   eldbus_message_iter_basic_append(var, 'b', enabled ? EINA_TRUE : EINA_FALSE);
   eldbus_message_iter_container_close(main_iter, var);

   eldbus_proxy_send(nm->props_proxy, msg, NULL, NULL, -1);
}

void
e_nm_scan(struct NM_Manager *nm)
{
   struct NM_Device *dev;

   EINA_SAFETY_ON_NULL_RETURN(nm);

   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        Eldbus_Message *msg;
        Eldbus_Message_Iter *iter, *opts;

        if (dev->type != NM_DEVICE_TYPE_WIFI) continue;
        if (!dev->wireless_proxy) continue;

        /* RequestScan takes a{sv} options dict — pass empty dict */
        msg = eldbus_proxy_method_call_new(dev->wireless_proxy,
                                           "RequestScan");
        iter = eldbus_message_iter_get(msg);
        eldbus_message_iter_arguments_append(iter, "a{sv}", &opts);
        eldbus_message_iter_container_close(iter, opts);

        eldbus_proxy_send(dev->wireless_proxy, msg, NULL, NULL, -1);
     }
}

/* -------------------------------------------------------------------------- */
/* Find AP across all devices                                                  */
/* -------------------------------------------------------------------------- */

struct NM_Access_Point *
enm_manager_find_ap(struct NM_Manager *nm, const char *path)
{
   struct NM_Device *dev;
   struct NM_Access_Point *ap;
   const char *shared;

   EINA_SAFETY_ON_NULL_RETURN_VAL(nm, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(path, NULL);

   shared = eina_stringshare_add(path);
   EINA_INLIST_FOREACH(nm->devices, dev)
     EINA_INLIST_FOREACH(dev->access_points, ap)
       if (ap->path == shared)
         {
            eina_stringshare_del(shared);
            return ap;
         }

   eina_stringshare_del(shared);
   return NULL;
}

/* -------------------------------------------------------------------------- */
/* NM name owner lifecycle                                                     */
/* -------------------------------------------------------------------------- */

static void
_e_nm_system_name_owner_exit(Eina_Bool shutdown)
{
   if (!nm_manager) return;

   enm_mod_manager_inout(NULL);
   _manager_free(nm_manager);
   nm_manager = NULL;

   ecore_event_add(E_NM_EVENT_MANAGER_OUT, NULL, NULL, NULL);

   if (!shutdown)
     e_util_dialog_show(_("NetworkManager Service Missing"),
                        _("The NetworkManager service is not available.<br>"
                          "Is <b>NetworkManager</b> daemon running?"));
}

static void
_e_nm_system_name_owner_enter(const char *owner EINA_UNUSED)
{
   nm_manager = _manager_new();
   ecore_event_add(E_NM_EVENT_MANAGER_IN, NULL, NULL, NULL);
   enm_mod_manager_inout(nm_manager);
}

static void
_e_nm_system_name_owner_changed(void *data EINA_UNUSED,
                                 const char *bus EINA_UNUSED,
                                 const char *from EINA_UNUSED,
                                 const char *to)
{
   if (to[0])
     _e_nm_system_name_owner_enter(to);
   else
     _e_nm_system_name_owner_exit(EINA_FALSE);
}

/* -------------------------------------------------------------------------- */
/* Public lifecycle                                                            */
/* -------------------------------------------------------------------------- */

unsigned int
e_nm_system_init(Eldbus_Connection *eldbus_conn)
{
   init_count++;
   if (init_count > 1) return init_count;

   E_NM_EVENT_MANAGER_IN  = ecore_event_type_new();
   E_NM_EVENT_MANAGER_OUT = ecore_event_type_new();

   conn = eldbus_conn;
   eldbus_name_owner_changed_callback_add(conn, NM_BUS_NAME,
                                          _e_nm_system_name_owner_changed,
                                          NULL, EINA_TRUE);
   agent = enm_agent_new(eldbus_conn);

   return init_count;
}

unsigned int
e_nm_system_shutdown(void)
{
   if (init_count == 0)
     {
        ERR("networkmanager system already shut down.");
        return 0;
     }

   init_count--;
   if (init_count > 0) return init_count;

   eldbus_name_owner_changed_callback_del(conn, NM_BUS_NAME,
                                          _e_nm_system_name_owner_changed,
                                          NULL);
   _e_nm_system_name_owner_exit(EINA_TRUE);

   if (agent) enm_agent_del(agent);
   if (conn) eldbus_connection_unref(conn);

   agent = NULL;
   conn  = NULL;

   E_NM_EVENT_MANAGER_OUT = 0;
   E_NM_EVENT_MANAGER_IN  = 0;

   return init_count;
}
