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

   free(ap->ssid);
   eina_stringshare_del(ap->path);

   if (ap->proxy)
     {
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

   if (eldbus_message_error_get(msg, &name, &text))
     {
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
   eldbus_proxy_signal_handler_add(ap->proxy, "PropertiesChanged",
                                   _ap_prop_changed, ap);

   /* Fetch all AP properties */
   eldbus_proxy_call(ap->proxy, "GetAll", _ap_get_props_cb, ap, -1,
                     "s", NM_IFACE_AP);

   return ap;
}

/* -------------------------------------------------------------------------- */
/* Device                                                                      */
/* -------------------------------------------------------------------------- */

static void _device_free(struct NM_Device *dev);
static struct NM_Device *_device_new(const char *path);

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
     }

   /* If this is a WiFi device, get wireless proxy and fetch APs */
   if (dev->type == NM_DEVICE_TYPE_WIFI && !dev->wireless_proxy)
     {
        Eldbus_Object *obj = eldbus_proxy_object_get(dev->proxy);

        /* Re-get object to get the wireless interface proxy */
        dev->wireless_proxy = eldbus_proxy_get(obj, NM_IFACE_WIFI);

        eldbus_proxy_signal_handler_add(dev->wireless_proxy, "AccessPointAdded",
                                        _device_ap_added, dev);
        eldbus_proxy_signal_handler_add(dev->wireless_proxy,
                                        "AccessPointRemoved",
                                        _device_ap_removed, dev);

        dev->pending.get_aps = eldbus_proxy_call(dev->wireless_proxy,
                                                  "GetAccessPoints",
                                                  _device_get_aps_cb, dev,
                                                  -1, "");
     }
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

   while (dev->access_points)
     {
        ap = EINA_INLIST_CONTAINER_GET(dev->access_points,
                                       struct NM_Access_Point);
        dev->access_points = eina_inlist_remove(dev->access_points,
                                                dev->access_points);
        _ap_free(ap);
     }

   free(dev->interface);

   if (dev->wireless_proxy)
     eldbus_proxy_unref(dev->wireless_proxy);

   if (dev->proxy)
     {
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

static void
_manager_active_conn_watch_free(struct NM_Manager *nm)
{
   if (nm->pending.active_conn)
     {
        eldbus_pending_cancel(nm->pending.active_conn);
        nm->pending.active_conn = NULL;
     }
   if (nm->active_conn_proxy)
     {
        eldbus_proxy_unref(nm->active_conn_proxy);
        eldbus_object_unref(nm->active_conn_obj);
        nm->active_conn_proxy = NULL;
        nm->active_conn_obj = NULL;
     }
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
                  nm->active_ap_path = eina_stringshare_add(ap_path);
                  enm_mod_manager_update(nm);
                  enm_mod_aps_changed(nm);
               }
          }
     }
}

static void
_active_conn_get_props_cb(void *data, const Eldbus_Message *msg,
                          Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_Manager *nm = data;
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;

   nm->pending.active_conn = NULL;

   if (eldbus_message_error_get(msg, &name, &text))
     {
        WRN("ActiveConnection GetAll failed: %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        WRN("ActiveConnection GetAll: cannot parse a{sv}");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;

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
                  DBG("ActiveConn SpecificObject=%s", ap_path);
                  eina_stringshare_del(nm->active_ap_path);
                  nm->active_ap_path = eina_stringshare_add(ap_path);
               }
          }
     }

   /* Proxy stays alive — updates arrive via _active_conn_prop_changed */
   DBG("ActiveConn done: active_ap=%s", nm->active_ap_path ?: "(null)");
   enm_mod_manager_update(nm);
   enm_mod_aps_changed(nm);
}

static void
_manager_watch_active_conn(struct NM_Manager *nm,
                            const char *active_conn_path)
{
   Eldbus_Object *obj;
   Eldbus_Proxy *props;

   DBG("_manager_watch_active_conn path=%s", active_conn_path ?: "(null)");
   if (!active_conn_path || !strcmp(active_conn_path, "/")) return;

   /* Skip if already watching this exact connection */
   if (nm->active_connection_path &&
       !strcmp(nm->active_connection_path, active_conn_path))
     return;

   eina_stringshare_del(nm->active_connection_path);
   nm->active_connection_path = eina_stringshare_add(active_conn_path);

   /* Tear down previous watchers (active conn + ip4) */
   _manager_active_conn_watch_free(nm);
   _manager_ip4_watch_free(nm);

   obj = eldbus_object_get(conn, NM_BUS_NAME, active_conn_path);
   props = eldbus_proxy_get(obj, NM_IFACE_PROPS);

   nm->active_conn_proxy = props;
   nm->active_conn_obj = obj;

   /* Subscribe to property changes — keeps proxy alive for signals */
   eldbus_proxy_signal_handler_add(props, "PropertiesChanged",
                                   _active_conn_prop_changed, nm);

   /* Initial fetch */
   nm->pending.active_conn = eldbus_proxy_call(props, "GetAll",
                                               _active_conn_get_props_cb, nm,
                                               -1, "s", NM_IFACE_ACONN);
}

/* -------------------------------------------------------------------------- */
/* Manager                                                                     */
/* -------------------------------------------------------------------------- */

static void _manager_free(struct NM_Manager *nm);
static struct NM_Manager *_manager_new(void);

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

             /* Use first active connection */
             if (eldbus_message_iter_get_and_next(conn_array, 'o', &aconn_path))
               _manager_watch_active_conn(nm, aconn_path);
             else
               {
                  /* No active connections — tear down watchers */
                  _manager_active_conn_watch_free(nm);
                  _manager_ip4_watch_free(nm);
                  eina_stringshare_del(nm->active_ap_path);
                  nm->active_ap_path = NULL;
                  eina_stringshare_del(nm->active_connection_path);
                  nm->active_connection_path = NULL;
                  free(nm->ip_address);
                  nm->ip_address = NULL;
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

             if (eldbus_message_iter_get_and_next(conn_array, 'o', &aconn_path))
               {
                  DBG("ActiveConnections: first path=%s", aconn_path);
                  _manager_watch_active_conn(nm, aconn_path);
               }
             else
               {
                  DBG("ActiveConnections: empty array");
                  eina_stringshare_del(nm->active_ap_path);
                  nm->active_ap_path = NULL;
                  eina_stringshare_del(nm->active_connection_path);
                  nm->active_connection_path = NULL;
                  free(nm->ip_address);
                  nm->ip_address = NULL;
               }
          }
     }

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
   eldbus_proxy_signal_handler_add(nm->props_proxy, "PropertiesChanged",
                                   _manager_prop_changed, nm);

   /* DeviceAdded / DeviceRemoved on NM iface */
   eldbus_proxy_signal_handler_add(nm->proxy, "DeviceAdded",
                                   _manager_device_added, nm);
   eldbus_proxy_signal_handler_add(nm->proxy, "DeviceRemoved",
                                   _manager_device_removed, nm);

   nm->pending.get_props =
      eldbus_proxy_call(nm->props_proxy, "GetAll",
                        _manager_get_props_cb, nm, -1,
                        "s", NM_IFACE_MGR);

   nm->pending.get_devices =
      eldbus_proxy_call(nm->proxy, "GetDevices",
                        _manager_get_devices_cb, nm, -1, "");

   return nm;
}

static void
_manager_free(struct NM_Manager *nm)
{
   struct NM_Device *dev;
   Eldbus_Object *obj;

   if (!nm) return;

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

   free(nm->ip_address);
   eina_stringshare_del(nm->active_ap_path);
   eina_stringshare_del(nm->active_connection_path);

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

void
enm_ap_connect(struct NM_Manager *nm, struct NM_Device *dev,
               struct NM_Access_Point *ap)
{
   struct connection_cb_data *cd;

   EINA_SAFETY_ON_NULL_RETURN(nm);
   EINA_SAFETY_ON_NULL_RETURN(dev);
   EINA_SAFETY_ON_NULL_RETURN(ap);

   cd = calloc(1, sizeof(*cd));
   EINA_SAFETY_ON_NULL_RETURN(cd);

   cd->nm  = nm;
   cd->dev = dev;
   cd->ap  = ap;

   /* ActivateConnection("/" means auto-select saved connection or create one,
    * device path, AP path) */
   eldbus_proxy_call(nm->proxy, "ActivateConnection",
                     _activate_cb, cd, NM_CONNECTION_TIMEOUT,
                     "ooo", "/", dev->path, ap->path);
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
