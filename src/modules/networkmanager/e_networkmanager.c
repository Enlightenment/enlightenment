#include "e_mod_main.h"
#include "e_networkmanager_vpn.h"
#include <ctype.h>

/* -------------------------------------------------------------------------- */
/* D-Bus interface constants                                                   */
/* -------------------------------------------------------------------------- */

#define NM_OBJ_PATH    "/org/freedesktop/NetworkManager"

#define NM_IFACE_MGR   "org.freedesktop.NetworkManager"
#define NM_IFACE_DEV   "org.freedesktop.NetworkManager.Device"
#define NM_IFACE_WIFI  "org.freedesktop.NetworkManager.Device.Wireless"
#define NM_IFACE_AP    "org.freedesktop.NetworkManager.AccessPoint"
/* NM_IFACE_ACTIVE_CONN is defined in e_networkmanager.h (shared with VPN sub-module) */
#define NM_IFACE_IP4   "org.freedesktop.NetworkManager.IP4Config"
#define NM_IFACE_PROPS "org.freedesktop.DBus.Properties"
#define NM_IFACE_AGENT_MGR "org.freedesktop.NetworkManager.AgentManager"
#define NM_IFACE_SETTINGS  "org.freedesktop.NetworkManager.Settings"
/* NM_IFACE_SCONN is defined in e_networkmanager.h (shared with VPN sub-module) */
#define NM_SETTINGS_PATH   "/org/freedesktop/NetworkManager/Settings"

#define NM_CONNECTION_TIMEOUT (60 * 1000)

/* -------------------------------------------------------------------------- */
/* Module-level globals                                                        */
/* -------------------------------------------------------------------------- */

static unsigned int       init_count;
static Eldbus_Connection *conn;
static struct NM_Manager *nm_manager;
static E_NM_Agent        *agent;

static Eina_Bool          suspended;         /* system is in suspend/sleep */
static Eina_Bool          dialog_open;       /* "service missing" dialog shown */
static E_Dialog          *nm_missing_dialog; /* live dialog pointer, or NULL */
static Ecore_Event_Handler *suspend_handler;
static Ecore_Event_Handler *resume_handler;

E_API int E_NM_EVENT_MANAGER_IN;
E_API int E_NM_EVENT_MANAGER_OUT;

/* -------------------------------------------------------------------------- */
/* Module callback indirection                                                 */
/*                                                                             */
/* The data layer never calls the UI layer directly; it invokes the callbacks */
/* registered by the module at init time.  This keeps e_networkmanager.c free */
/* of cross-file symbol references to e_mod_main.c.                           */
/* -------------------------------------------------------------------------- */

static const E_NM_Mod_Callbacks *_mod_cbs = NULL;

void
e_nm_module_callbacks_set(const E_NM_Mod_Callbacks *cbs)
{
   _mod_cbs = cbs;
}

static inline void
_notify_aps_changed(struct NM_Manager *nm)
{
   if (_mod_cbs && _mod_cbs->aps_changed) _mod_cbs->aps_changed(nm);
}

static inline void
_notify_manager_update(struct NM_Manager *nm)
{
   if (_mod_cbs && _mod_cbs->manager_update) _mod_cbs->manager_update(nm);
}

static inline void
_notify_manager_inout(struct NM_Manager *nm)
{
   if (_mod_cbs && _mod_cbs->manager_inout) _mod_cbs->manager_inout(nm);
}

/* -------------------------------------------------------------------------- */
/* Internal accessors used by e_networkmanager_vpn.c                          */
/* -------------------------------------------------------------------------- */

_mod_cb_vpn_changed_t
_mod_cbs_vpn_changed_get(void)
{
   return _mod_cbs ? _mod_cbs->vpn_changed : NULL;
}

_mod_cb_vpn_active_t
_mod_cbs_vpn_active_changed_get(void)
{
   return _mod_cbs ? _mod_cbs->vpn_active_changed : NULL;
}

Eldbus_Connection *
_enm_dbus_conn_get(void)
{
   return conn;
}

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

const char *
enm_vpn_type_label(const char *conn_type, const char *service_type)
{
   const char *prefix = "org.freedesktop.NetworkManager.";
   const char *short_name;

   if (conn_type && !strcmp(conn_type, "wireguard")) return "WireGuard";
   if (!service_type) return "VPN";

   /* Strip the org.freedesktop.NetworkManager. prefix if present. */
   short_name = service_type;
   if (!strncmp(service_type, prefix, strlen(prefix)))
     short_name = service_type + strlen(prefix);

   if (!strcmp(short_name, "openvpn"))     return "OpenVPN";
   if (!strcmp(short_name, "openconnect")) return "OpenConnect";
   if (!strcmp(short_name, "vpnc"))        return "VPNC";
   if (!strcmp(short_name, "pptp"))        return "PPTP";
   if (!strcmp(short_name, "l2tp"))        return "L2TP";
   if (!strcmp(short_name, "libreswan"))   return "Libreswan";
   if (!strcmp(short_name, "strongswan"))  return "strongSwan";
   if (!strcmp(short_name, "fortisslvpn")) return "Fortinet";
   if (!strcmp(short_name, "iodine"))      return "Iodine";
   if (!strcmp(short_name, "sstp"))        return "SSTP";
   return "VPN";
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

   /* Refresh gadget now that AP data (strength, security, frequency) is
    * available — avoids leaving stale strength=0/no-lock state if this
    * callback races ahead of the active-conn probe. */
   _notify_manager_update(nm_manager);
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

   _notify_manager_update(nm_manager);
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
static void _try_wifi_adopt(struct NM_Device *dev);
static void _try_ethernet_adopt(struct NM_Device *dev);
static void _try_bluetooth_adopt(struct NM_Device *dev);

static char *
_nm_bt_addr_normalize(const char *addr)
{
   char buf[18];
   int i = 0, j = 0;

   if (!addr) return NULL;

   while (addr[i] && j < 17)
     {
        if (isxdigit((unsigned char)addr[i]))
          {
             buf[j++] = toupper((unsigned char)addr[i]);
             if ((j % 3) == 2 && j < 17) buf[j++] = ':';
          }
        i++;
     }

   if (j != 17) return NULL;
   buf[17] = '\0';
   return strdup(buf);
}

static char *
_nm_bt_addr_from_iter(Eldbus_Message_Iter *variant)
{
   Eldbus_Message_Iter *bytes;
   unsigned char b;
   char buf[18];
   int i = 0;

   if (!variant) return NULL;
   if (!eldbus_message_iter_arguments_get(variant, "ay", &bytes))
     return NULL;

   while (eldbus_message_iter_get_and_next(bytes, 'y', &b))
     {
        if (i >= 6) break;
        snprintf(buf + (i * 3), sizeof(buf) - (i * 3),
                 (i < 5) ? "%02X:" : "%02X", b);
        i++;
     }

   if (i != 6) return NULL;
   buf[17] = '\0';
   return strdup(buf);
}

static void
_nm_wired_connection_free(struct NM_Wired_Connection *wc)
{
   if (!wc) return;
   eina_stringshare_del(wc->path);
   free(wc->name);
   free(wc->interface_name);
   free(wc->hw_address);
   free(wc);
}

static void
_nm_wired_connections_clear(struct NM_Manager *nm)
{
   struct NM_Wired_Connection *wc;

   if (!nm) return;
   while (nm->wired_connections)
     {
        wc = EINA_INLIST_CONTAINER_GET(nm->wired_connections,
                                       struct NM_Wired_Connection);
        nm->wired_connections =
           eina_inlist_remove(nm->wired_connections,
                              nm->wired_connections);
        _nm_wired_connection_free(wc);
     }
}

static struct NM_Wired_Connection *
_nm_wired_connection_add(struct NM_Manager *nm, const char *path,
                         const char *name, const char *interface_name,
                         const char *hw_address)
{
   struct NM_Wired_Connection *wc;

   wc = calloc(1, sizeof(*wc));
   if (!wc) return NULL;

   wc->path = eina_stringshare_add(path);
   wc->name = name ? strdup(name) : NULL;
   wc->interface_name = interface_name ? strdup(interface_name) : NULL;
   wc->hw_address = hw_address ? strdup(hw_address) : NULL;
   nm->wired_connections =
      eina_inlist_append(nm->wired_connections, EINA_INLIST_GET(wc));
   return wc;
}

static struct NM_Wired_Connection *
_nm_wired_connection_find_for_device(struct NM_Manager *nm, struct NM_Device *dev)
{
   struct NM_Wired_Connection *wc, *iface_match = NULL, *fallback = NULL;

   if (!nm || !dev) return NULL;

   EINA_INLIST_FOREACH(nm->wired_connections, wc)
     {
        if (wc->hw_address && dev->hw_address &&
            !strcmp(wc->hw_address, dev->hw_address))
          return wc;

        if (!iface_match && wc->interface_name && dev->interface &&
            !strcmp(wc->interface_name, dev->interface))
          iface_match = wc;

        if (!fallback && !wc->hw_address && !wc->interface_name)
          fallback = wc;
     }

   if (iface_match) return iface_match;
   return fallback;
}

static void
_nm_bluetooth_connection_free(struct NM_Bluetooth_Connection *bc)
{
   if (!bc) return;
   eina_stringshare_del(bc->path);
   free(bc->name);
   free(bc->bdaddr);
   free(bc);
}

static void
_nm_bluetooth_connections_clear(struct NM_Manager *nm)
{
   struct NM_Bluetooth_Connection *bc;

   if (!nm) return;
   while (nm->bluetooth_connections)
     {
        bc = EINA_INLIST_CONTAINER_GET(nm->bluetooth_connections,
                                       struct NM_Bluetooth_Connection);
        nm->bluetooth_connections =
           eina_inlist_remove(nm->bluetooth_connections,
                              nm->bluetooth_connections);
        _nm_bluetooth_connection_free(bc);
     }
}

static struct NM_Bluetooth_Connection *
_nm_bluetooth_connection_add(struct NM_Manager *nm, const char *path,
                             const char *name, const char *bdaddr)
{
   struct NM_Bluetooth_Connection *bc;

   bc = calloc(1, sizeof(*bc));
   if (!bc) return NULL;

   bc->path = eina_stringshare_add(path);
   bc->name = name ? strdup(name) : NULL;
   bc->bdaddr = bdaddr ? strdup(bdaddr) : NULL;
   nm->bluetooth_connections =
      eina_inlist_append(nm->bluetooth_connections, EINA_INLIST_GET(bc));
   return bc;
}

struct NM_Bluetooth_Connection *
enm_bluetooth_connection_find(struct NM_Manager *nm, const char *connection_path)
{
   struct NM_Bluetooth_Connection *bc;

   if (!nm || !connection_path) return NULL;
   EINA_INLIST_FOREACH(nm->bluetooth_connections, bc)
     if (bc->path && !strcmp(bc->path, connection_path))
       return bc;
   return NULL;
}

struct NM_Device *
enm_bluetooth_connection_device_find(struct NM_Manager *nm,
                                     struct NM_Bluetooth_Connection *bc)
{
   struct NM_Device *dev, *fallback = NULL;

   if (!nm || !bc) return NULL;

   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        if (dev->type != NM_DEVICE_TYPE_BLUETOOTH) continue;
        if (!fallback && !bc->bdaddr) fallback = dev;
        if (bc->bdaddr && dev->hw_address &&
            !strcmp(bc->bdaddr, dev->hw_address))
          return dev;
     }

   return fallback;
}

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

   /* Guard: nm_manager may have been freed if NM exited while GetAccessPoints
    * was in flight.  Mirror the pattern in _device_wifi_props_cb. */
   if (!nm_manager) return;

   _notify_aps_changed(nm_manager);

   /* If the manager already has an active AP, the gadget should be refreshed
    * now that AP properties (strength, security, frequency) are populated.
    * This covers the secondary race where AP-list completion races ahead of
    * the adoption probe. */
   if (nm_manager->active_ap_path)
     _notify_manager_update(nm_manager);
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
   _notify_aps_changed(nm_manager);
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
   _notify_aps_changed(nm_manager);
}

static void
_device_prop_changed(void *data, const Eldbus_Message *msg)
{
   struct NM_Device *dev = data;
   Eldbus_Message_Iter *changed_props, *invalidated, *dict, *var;
   const char *iface, *key;
   Eina_Bool try_adopt = EINA_FALSE;

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
                  _notify_manager_update(nm_manager);
                  try_adopt = EINA_TRUE;
               }
          }
        else if (!strcmp(key, "ActiveConnection"))
          {
             /* On cold start NM may report ActiveConnection="/" in the initial
              * Device.GetAll and only populate it later via PropertiesChanged.
              * Capture it here so that _try_*_adopt can succeed on retry. */
             const char *aconn_path;
             if (eldbus_message_iter_arguments_get(var, "o", &aconn_path))
               {
                  if (aconn_path && strcmp(aconn_path, "/") != 0)
                    {
                       free(dev->active_conn_path);
                       dev->active_conn_path = strdup(aconn_path);
                       DBG("Device %s ActiveConnection -> %s", dev->path, aconn_path);
                    }
                  else
                    {
                       free(dev->active_conn_path);
                       dev->active_conn_path = NULL;
                    }
                  try_adopt = EINA_TRUE;
               }
          }
     }

   /* Re-attempt adoption when state or active-connection path changes.
    * This closes the cold-start race where the initial Device.GetAll returned
    * ActiveConnection="/" before NM had settled, so the one-shot
    * _device_wifi_props_cb / _device_get_props_cb adoption attempt failed.
    *
    * Skip if nm_manager already has an adopted connection — the "first wins"
    * guard in the helpers would no-op anyway, but this avoids a redundant
    * WiFi GetAll on every State change during normal operation. */
   if (try_adopt && nm_manager && !nm_manager->active_connection_path &&
       dev->state >= 100 && dev->active_conn_path)
     {
        if (dev->type == NM_DEVICE_TYPE_ETHERNET)
          _try_ethernet_adopt(dev);
        else if (dev->type == NM_DEVICE_TYPE_BLUETOOTH)
          _try_bluetooth_adopt(dev);
        else if (dev->type == NM_DEVICE_TYPE_WIFI)
          {
             /* Re-issue WiFi GetAll only if one is not already pending.
              * _device_wifi_props_cb stores the result in dev->active_ap_path
              * and calls _try_wifi_adopt(). */
             if (!dev->pending.get_wifi_props)
               dev->pending.get_wifi_props =
                  eldbus_proxy_call(dev->proxy, "GetAll",
                                    _device_wifi_props_cb, dev,
                                    -1, "s", NM_IFACE_WIFI);
          }
     }
}

/* -------------------------------------------------------------------------- */
/* Active-connection adoption helpers                                          */
/*                                                                             */
/* Factored out of _device_get_props_cb (Ethernet) and _device_wifi_props_cb  */
/* (WiFi) so that _device_prop_changed can re-attempt adoption when a         */
/* cold-start race caused the initial one-shot attempt to fail.               */
/* -------------------------------------------------------------------------- */

static void
_try_ethernet_adopt(struct NM_Device *dev)
{
   if (!nm_manager) return;
   if (dev->state < 100 || !dev->active_conn_path) return;

   /* First active device wins — skip if another already adopted. */
   if (nm_manager->active_connection_path) return;

   DBG("Ethernet device %s adopting connection %s (state=%u)",
       dev->path, dev->active_conn_path, dev->state);

   nm_manager->probe_generation++;
   _manager_active_conn_watch_free(nm_manager);
   _manager_ip4_watch_free(nm_manager);

   eina_stringshare_replace(&nm_manager->active_connection_path,
                            dev->active_conn_path);
   nm_manager->active_conn_type = NM_DEVICE_TYPE_ETHERNET;
   eina_stringshare_replace(&nm_manager->active_ap_path, NULL);

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

   _notify_manager_update(nm_manager);
}

static void
_try_wifi_adopt(struct NM_Device *dev)
{
   if (!nm_manager) return;
   if (dev->state < 100 || !dev->active_ap_path || !dev->active_conn_path)
     return;

   /* First active device wins — skip if another already adopted. */
   if (nm_manager->active_connection_path) return;

   DBG("WiFi device %s adopting conn=%s ap=%s (state=%u)",
       dev->path, dev->active_conn_path, dev->active_ap_path, dev->state);

   nm_manager->probe_generation++;
   _manager_active_conn_watch_free(nm_manager);
   _manager_ip4_watch_free(nm_manager);

   nm_manager->active_conn_type = NM_DEVICE_TYPE_WIFI;
   eina_stringshare_replace(&nm_manager->active_ap_path, dev->active_ap_path);
   eina_stringshare_replace(&nm_manager->active_connection_path,
                            dev->active_conn_path);

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

   _notify_manager_update(nm_manager);
   enm_saved_connections_get(nm_manager);
}

static void
_try_bluetooth_adopt(struct NM_Device *dev)
{
   if (!nm_manager) return;
   if (dev->state < 100 || !dev->active_conn_path) return;

   if (nm_manager->active_connection_path) return;

   DBG("Bluetooth device %s adopting connection %s (state=%u)",
       dev->path, dev->active_conn_path, dev->state);

   nm_manager->probe_generation++;
   _manager_active_conn_watch_free(nm_manager);
   _manager_ip4_watch_free(nm_manager);

   eina_stringshare_replace(&nm_manager->active_connection_path,
                            dev->active_conn_path);
   nm_manager->active_conn_type = NM_DEVICE_TYPE_BLUETOOTH;
   eina_stringshare_replace(&nm_manager->active_ap_path, NULL);

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

   _notify_manager_update(nm_manager);
}

/* Handle PropertiesChanged on the Device.Wireless interface.
 * Watches for ActiveAccessPoint changes that may arrive after the initial
 * Device.Wireless.GetAll callback has already run (another cold-start window:
 * NM returns ActiveAccessPoint="/" in GetAll but sets it shortly after). */
static void
_device_wifi_prop_changed(void *data, const Eldbus_Message *msg)
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

        if (!strcmp(key, "ActiveAccessPoint"))
          {
             const char *path;
             if (eldbus_message_iter_arguments_get(var, "o", &path))
               {
                  free(dev->active_ap_path);
                  if (path && strcmp(path, "/") != 0)
                    {
                       dev->active_ap_path = strdup(path);
                       DBG("Device %s ActiveAccessPoint -> %s", dev->path, path);
                    }
                  else
                    dev->active_ap_path = NULL;

                  /* Snapshot whether this device is ALREADY the adopted device
                   * before attempting adoption.  If it was already adopted the
                   * AP change is a genuine roam and must refresh the gadget.
                   * If it was not yet adopted, _try_wifi_adopt() below will
                   * call _notify_manager_update() itself — running the roam
                   * path on top of that would be a spurious double update. */
                  Eina_Bool was_adopted = (nm_manager && dev->active_conn_path &&
                                           nm_manager->active_connection_path &&
                                           !strcmp(nm_manager->active_connection_path,
                                                   dev->active_conn_path));

                  /* Re-attempt adoption if not yet done — this closes the window
                   * where NM emits AP after the initial GetAll returned "/". */
                  _try_wifi_adopt(dev);

                  /* Refresh the gadget only for a genuine roam (device was
                   * already the adopted device before this AP change). */
                  if (was_adopted && nm_manager && dev->active_ap_path)
                    {
                       eina_stringshare_replace(&nm_manager->active_ap_path,
                                                dev->active_ap_path);
                       _notify_manager_update(nm_manager);
                    }
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
        else if (!strcmp(key, "HwAddress"))
          {
             const char *addr;
             char *norm;

             if (eldbus_message_iter_arguments_get(var, "s", &addr))
               {
                  norm = _nm_bt_addr_normalize(addr);
                  free(dev->hw_address);
                  dev->hw_address = norm;
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

        /* Re-get object to get the wireless interface proxy.
         * eldbus_proxy_get() does NOT increment the object refcount — the
         * object ref is already held by dev->proxy's parent object. */
        dev->wireless_proxy = eldbus_proxy_get(obj, NM_IFACE_WIFI);

        dev->ap_added_handler =
           eldbus_proxy_signal_handler_add(dev->wireless_proxy, "AccessPointAdded",
                                           _device_ap_added, dev);
        dev->ap_removed_handler =
           eldbus_proxy_signal_handler_add(dev->wireless_proxy,
                                           "AccessPointRemoved",
                                           _device_ap_removed, dev);

        /* Watch for ActiveAccessPoint changes on the WiFi interface.
         * This is the signal that fires when NM settles its AP association
         * after the initial GetAll returned "/" (cold-start race). */
        dev->wifi_prop_changed_handler =
           eldbus_proxy_signal_handler_add(dev->wireless_proxy,
                                           "PropertiesChanged",
                                           _device_wifi_prop_changed, dev);

        dev->pending.get_aps = eldbus_proxy_call(dev->wireless_proxy,
                                                  "GetAccessPoints",
                                                  _device_get_aps_cb, dev,
                                                  -1, "");

        /* Also fetch WiFi-specific props (ActiveAccessPoint) in parallel
         * with GetAccessPoints — this avoids the ActiveConnection.GetAll
         * round-trip on the critical startup path. */
        dev->pending.get_wifi_props =
           eldbus_proxy_call(dev->proxy, "GetAll",
                             _device_wifi_props_cb, dev,
                             -1, "s", NM_IFACE_WIFI);
     }
   else if (dev->type == NM_DEVICE_TYPE_ETHERNET)
     {
        /* Attempt adoption via the factored helper — it checks state>=100 and
         * active_conn_path internally, and handles the "first wins" guard. */
        _try_ethernet_adopt(dev);
     }
   else if (dev->type == NM_DEVICE_TYPE_BLUETOOTH)
     {
        _try_bluetooth_adopt(dev);
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

   /* Persist the active AP path on the device so that _try_wifi_adopt() can
    * be called from multiple entry points (here, _device_wifi_prop_changed)
    * without re-issuing GetAll. */
   free(dev->active_ap_path);
   dev->active_ap_path = ap_path ? strdup(ap_path) : NULL;

   _try_wifi_adopt(dev);
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
   free(dev->hw_address);
   free(dev->active_conn_path);
   free(dev->active_ap_path);
   free(dev->ip4_path);

   if (dev->wireless_proxy)
     {
        if (dev->wifi_prop_changed_handler)
          {
             eldbus_signal_handler_del(dev->wifi_prop_changed_handler);
             dev->wifi_prop_changed_handler = NULL;
          }
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
        /* wireless_proxy shares the same Eldbus_Object as dev->proxy.
         * eldbus_proxy_get() does NOT increment the object's refcount, so
         * proxy_unref here must NOT be paired with an object_unref — doing so
         * would drop the object to refcount 0, triggering _eldbus_object_clear
         * which frees dev->proxy via its _on_object_free callback, causing a
         * use-after-free when the dev->proxy block below runs. */
        eldbus_proxy_unref(dev->wireless_proxy);
     }

   if (dev->proxy)
     {
        if (dev->prop_changed_handler)
          {
             eldbus_signal_handler_del(dev->prop_changed_handler);
             dev->prop_changed_handler = NULL;
          }
        /* The sole object ref belongs to this proxy (taken by eldbus_object_get
         * in _device_new and never stored separately). Unref proxy first, then
         * unref the object exactly once. */
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
             _notify_manager_update(nm);
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
   _notify_manager_update(nm);
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
   eina_stringshare_replace(&nm->active_connection_path, NULL);
   eina_stringshare_replace(&nm->active_ap_path, NULL);
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
                  eina_stringshare_replace(&nm->active_ap_path,
                                           (ap_path && strcmp(ap_path, "/"))
                                           ? ap_path : NULL);
                  _notify_manager_update(nm);
                  _notify_aps_changed(nm);
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

   /* Skip non-network types (bridge, vpn, etc.). */
   if (conn_type != NM_DEVICE_TYPE_WIFI &&
       conn_type != NM_DEVICE_TYPE_ETHERNET &&
       conn_type != NM_DEVICE_TYPE_BLUETOOTH)
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

   eina_stringshare_replace(&nm->active_ap_path, ap_path);

   /* Subscribe to property changes on the now-persistent proxy */
   nm->active_conn_signal_handler =
      eldbus_proxy_signal_handler_add(nm->active_conn_proxy, "PropertiesChanged",
                                      _active_conn_prop_changed, nm);

   if (ip4path)
     _manager_watch_ip4(nm, ip4path);

   free(probe); /* fields already transferred or NULL */

   _notify_manager_update(nm);
   _notify_aps_changed(nm);
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
                     -1, "s", NM_IFACE_ACTIVE_CONN);
}

/* -------------------------------------------------------------------------- */
/* Manager                                                                     */
/* -------------------------------------------------------------------------- */

static void _manager_free(struct NM_Manager *nm);
static struct NM_Manager *_manager_new(void);
static void _settings_conn_removed_cb(void *data, const Eldbus_Message *msg);
static void _enm_vpn_resolve_active(struct NM_Manager *nm,
                                    const char *active_path);

/* -------------------------------------------------------------------------- */
/* VPN active connection reconciliation                                        */
/* -------------------------------------------------------------------------- */

/* Per-resolve context — one per in-flight Properties.Get("Connection") call.
 * Tracked in nm->vpn_pending_resolves so _manager_free can cancel safely. */
struct _enm_resolve_ctx
{
   struct NM_Manager *nm;
   char              *active_path; /* heap copy — not stringshare */
   Eldbus_Pending    *pending;     /* our slot in nm->vpn_pending_resolves */
   Eldbus_Object     *obj;
   Eldbus_Proxy      *proxy;
};

static void
_enm_vpn_resolve_cb(void *data, const Eldbus_Message *msg,
                    Eldbus_Pending *pending EINA_UNUSED)
{
   struct _enm_resolve_ctx *ctx = data;
   Eldbus_Message_Iter *var;
   const char *err_name, *err_msg, *settings_path;
   struct NM_VPN_Connection *vc;

   /* Remove ourselves from the tracking list FIRST — before any nm access —
    * so that _manager_free's drain loop does not see a stale pointer. */
   ctx->nm->vpn_pending_resolves =
       eina_list_remove(ctx->nm->vpn_pending_resolves, ctx->pending);

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        DBG("VPN resolve Connection: %s %s", err_name ?: "", err_msg ?: "");
        goto out;
     }
   if (!eldbus_message_arguments_get(msg, "v", &var)) goto out;
   if (!eldbus_message_iter_arguments_get(var, "o", &settings_path)) goto out;

   DBG("VPN resolve: active_path=%s -> settings_path=%s",
       ctx->active_path ?: "?", settings_path ?: "?");

   vc = enm_vpn_find_by_path(ctx->nm, settings_path);
   if (vc)
     {
        DBG("VPN resolve: binding vc='%s' to active=%s",
            vc->name ?: "?", ctx->active_path);
        enm_vpn_active_bind(vc, ctx->active_path);
     }
   else
     DBG("VPN resolve: no vc matches settings path %s", settings_path);

out:
   eldbus_proxy_unref(ctx->proxy);
   eldbus_object_unref(ctx->obj);
   free(ctx->active_path);
   free(ctx);
}

static void
_enm_vpn_resolve_active(struct NM_Manager *nm, const char *active_path)
{
   struct _enm_resolve_ctx *ctx;

   if (!active_path || !strcmp(active_path, "/")) return;

   /* If a vc is already bound to this active_path, no need to resolve again */
   if (enm_vpn_find_by_active(nm, active_path)) return;

   ctx = calloc(1, sizeof(*ctx));
   if (!ctx) return;
   ctx->nm          = nm;
   ctx->active_path = strdup(active_path);
   ctx->obj         = eldbus_object_get(conn, NM_BUS_NAME, active_path);
   ctx->proxy       = eldbus_proxy_get(ctx->obj, NM_IFACE_PROPS);
   if (!ctx->proxy)
     {
        eldbus_object_unref(ctx->obj);
        free(ctx->active_path);
        free(ctx);
        return;
     }

   ctx->pending = eldbus_proxy_call(ctx->proxy, "Get",
                                    _enm_vpn_resolve_cb, ctx, -1,
                                    "ss",
                                    "org.freedesktop.NetworkManager.Connection.Active",
                                    "Connection");
   if (!ctx->pending)
     {
        /* Call failed immediately — callback will not fire; free manually */
        eldbus_proxy_unref(ctx->proxy);
        eldbus_object_unref(ctx->obj);
        free(ctx->active_path);
        free(ctx);
        return;
     }
   nm->vpn_pending_resolves =
       eina_list_append(nm->vpn_pending_resolves, ctx->pending);
}

void
_enm_vpn_reconcile_active(struct NM_Manager *nm,
                           const char * const *active_paths,
                           unsigned int n_active)
{
   struct NM_VPN_Connection *vc;
   unsigned int i;
   Eina_Bool any_unbound = EINA_FALSE;

   /* Unbind any vc whose active_path is no longer in the new set. */
   EINA_INLIST_FOREACH(nm->vpn_connections, vc)
     {
        Eina_Bool still_active = EINA_FALSE;
        if (!vc->active_path) continue;
        for (i = 0; i < n_active; i++)
          if (!strcmp(vc->active_path, active_paths[i]))
            { still_active = EINA_TRUE; break; }
        if (!still_active)
          {
             enm_vpn_active_unbind(vc);
             any_unbound = EINA_TRUE;
          }
     }

   /* Notify the UI that VPN active state changed when at least one VPN was
    * unbound.  Without this the popup and shield badge stay stale because
    * _manager_prop_changed does NOT call _notify_manager_update when other
    * connections are still active (n_active > 0 branch). */
   if (any_unbound) enm_vpn_active_changed_schedule(nm);

   /* For each active path, fire an async resolve to find and bind
    * the matching saved VPN (if any). */
   for (i = 0; i < n_active; i++)
     _enm_vpn_resolve_active(nm, active_paths[i]);
}

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
                  _notify_manager_update(nm);
               }
          }
        else if (!strcmp(key, "WirelessEnabled"))
          {
             Eina_Bool enabled;
             if (eldbus_message_iter_arguments_get(var, "b", &enabled))
               {
                  nm->wireless_enabled = enabled;
                  _notify_manager_update(nm);
               }
          }
        else if (!strcmp(key, "ActiveConnections"))
          {
             Eldbus_Message_Iter *conn_array;
             const char *aconn_path;
             /* Collect paths — iterator is single-pass; both the wifi probe
              * and the VPN reconcile need to see every path. */
             const char *active_paths[256];
             unsigned int n_active = 0;

             if (!eldbus_message_iter_arguments_get(var, "ao", &conn_array))
               continue;

             while (n_active < (sizeof(active_paths) / sizeof(active_paths[0])) &&
                    eldbus_message_iter_get_and_next(conn_array, 'o', &aconn_path))
               active_paths[n_active++] = aconn_path;

             /* If the connection we currently track is still in the new
              * active set, only the VPN (or some other non-primary) is
              * changing — leave the wifi/ethernet primary state alone so
              * the popup's IP / security subtitle doesn't flap to "wpa2"
              * for the few hundred ms it takes the re-probe to complete. */
             {
                Eina_Bool primary_still_active = EINA_FALSE;
                unsigned int i;
                if (nm->active_connection_path)
                  {
                     for (i = 0; i < n_active; i++)
                       if (!strcmp(nm->active_connection_path, active_paths[i]))
                         { primary_still_active = EINA_TRUE; break; }
                  }

                if (!primary_still_active)
                  {
                     /* Advance generation so any in-flight probes from the
                      * previous batch are discarded.  Then clear the old
                      * watchers so we don't display a stale connection
                      * while the new probes are in flight. */
                     nm->probe_generation++;
                     _manager_active_conn_watch_free(nm);
                     _manager_ip4_watch_free(nm);

                     if (n_active > 0)
                       for (i = 0; i < n_active; i++)
                         _manager_probe_active_conn(nm, active_paths[i]);
                  }
             }

             if (n_active > 0)
               {
                  /* Reconcile VPN bindings against the new active set
                   * unconditionally — it's the cheap part. */
                  _enm_vpn_reconcile_active(nm, active_paths, n_active);
               }
             else
               {
                  /* No active connections — tear down already done above */
                  eina_stringshare_replace(&nm->active_ap_path, NULL);
                  eina_stringshare_replace(&nm->active_connection_path, NULL);
                  free(nm->ip_address);
                  nm->ip_address = NULL;
                  nm->active_conn_type = NM_DEVICE_TYPE_UNKNOWN;
                  _notify_manager_update(nm);

                  /* Unbind all VPN active watchers */
                  _enm_vpn_reconcile_active(nm, NULL, 0);
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
              * running in parallel.
              *
              * Cache the active paths for VPN reconciliation: enm_vpn_enumerate
              * is dispatched after this callback and populates vpn_connections
              * asynchronously.  We store paths here so _vpn_get_settings_cb can
              * re-run reconcile once the final GetSettings reply arrives. */
             while (eldbus_message_iter_get_and_next(conn_array, 'o', &aconn_path))
               {
                  DBG("ActiveConnections (startup): cached path=%s",
                      aconn_path);
                  nm->vpn_pending_active_paths =
                      eina_list_append(nm->vpn_pending_active_paths,
                                       (void *)eina_stringshare_add(aconn_path));
               }
          }
     }

   DBG("manager_get_props done: state=%d active_ap=%s conn_type=%d",
       nm->state, nm->active_ap_path ?: "(null)", nm->active_conn_type);

   /* Always update the UI now so that disconnected/VPN-only/unassociated
    * states are shown immediately.  Device callbacks will call
    * _notify_manager_update again once they have full type and AP info. */
   _notify_manager_update(nm);

   /* Now that vpn_pending_active_paths is populated, kick off the VPN
    * enumeration explicitly.  The wifi-device callback also calls
    * enm_vpn_enumerate, but that races with this callback — we may
    * complete the GetSettings chain before the cache is populated.
    * Calling enumerate here guarantees the cache is non-empty when the
    * terminal block of _vpn_get_settings_cb runs. */
   enm_vpn_enumerate(nm);
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
   const char *conn_type = NULL;
   const char *conn_name = NULL;
   const char *conn_iface = NULL;
   char ssid_str[256];
   char *bt_addr = NULL;
   char *eth_addr = NULL;

   ssid_str[0] = '\0';

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

        while (eldbus_message_iter_get_and_next(setting_dict, 'e', &inner_entry))
          {
             if (!eldbus_message_iter_arguments_get(inner_entry, "sv",
                                                    &key, &variant))
               continue;

             if (!strcmp(setting_name, "connection"))
               {
                  if (!strcmp(key, "type"))
                    eldbus_message_iter_arguments_get(variant, "s", &conn_type);
                  else if (!strcmp(key, "id"))
                    eldbus_message_iter_arguments_get(variant, "s", &conn_name);
                  else if (!strcmp(key, "interface-name"))
                    eldbus_message_iter_arguments_get(variant, "s", &conn_iface);
               }
             else if (!strcmp(setting_name, "802-11-wireless"))
               {
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
                    }
               }
             else if (!strcmp(setting_name, "bluetooth"))
               {
                  if (!strcmp(key, "bdaddr"))
                    {
                       free(bt_addr);
                       bt_addr = _nm_bt_addr_from_iter(variant);
                    }
               }
             else if (!strcmp(setting_name, "802-3-ethernet"))
               {
                  if (!strcmp(key, "mac-address"))
                    {
                       free(eth_addr);
                       eth_addr = _nm_bt_addr_from_iter(variant);
                    }
               }
          }
     }

   if (conn_type && !strcmp(conn_type, "802-11-wireless") &&
       ssid_str[0] && ctx->nm->saved_connections)
     {
        eina_hash_del_by_key(ctx->nm->saved_connections, ssid_str);
        eina_hash_add(ctx->nm->saved_connections, ssid_str,
                      eina_stringshare_add(ctx->path));
        INF("saved_conn: added '%s' -> %s", ssid_str, ctx->path);
     }
   else if (conn_type && !strcmp(conn_type, "bluetooth"))
     {
        _nm_bluetooth_connection_add(ctx->nm, ctx->path,
                                     conn_name ?: _("Bluetooth"),
                                     bt_addr);
        INF("saved_bt_conn: added '%s' (%s) -> %s",
            conn_name ?: _("Bluetooth"), bt_addr ?: "?", ctx->path);
     }
   else if (conn_type && !strcmp(conn_type, "802-3-ethernet"))
     {
        _nm_wired_connection_add(ctx->nm, ctx->path,
                                 conn_name ?: _("Wired"),
                                 conn_iface, eth_addr);
        INF("saved_eth_conn: added '%s' (%s/%s) -> %s",
            conn_name ?: _("Wired"), conn_iface ?: "?",
            eth_addr ?: "?", ctx->path);
     }

done:
   free(bt_addr);
   free(eth_addr);
   eldbus_proxy_unref(ctx->proxy);
   eldbus_object_unref(ctx->obj);
   eina_stringshare_del(ctx->path);
   /* Refresh popup once all GetSettings replies have arrived */
   if (ctx->nm->saved_conn_pending > 0)
     ctx->nm->saved_conn_pending--;
   if (ctx->nm->saved_conn_pending == 0)
     _notify_aps_changed(ctx->nm);
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
     _notify_aps_changed(nm);
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
      _nm_bluetooth_connections_clear(nm);
      _nm_wired_connections_clear(nm);

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
 * forget button disappears without a visible race window.  The signal
 * carries the removed object path, so we surgically remove just that
 * one entry from the VPN list instead of re-enumerating everything. */
static void
_settings_conn_removed_cb(void *data, const Eldbus_Message *msg)
{
   struct NM_Manager *nm = data;
   const char *path = NULL;

   if (!nm) return;

   /* For wifi/eth the hash is keyed by SSID, not path — we still need to
    * refresh saved_connections to drop the entry from the hash. */
   enm_saved_connections_get(nm);

   if (eldbus_message_arguments_get(msg, "o", &path) && path)
     {
        INF("ConnectionRemoved signal for %s", path);
        enm_vpn_remove_by_path(nm, path);
     }
   else
     {
        /* No path in the signal — fall back to a full re-enumerate. */
        WRN("ConnectionRemoved without object path; full re-enumerate");
        enm_vpn_enumerate(nm);
     }
}

/* Called when NM adds a new connection (e.g. via AddAndActivateConnection
 * or nmcli import).  The signal carries the added object path; fetch
 * settings only for that path and append if it turns out to be VPN. */
static void
_settings_conn_added_cb(void *data, const Eldbus_Message *msg)
{
   struct NM_Manager *nm = data;
   const char *path = NULL;

   if (!nm) return;
   enm_saved_connections_get(nm);

   if (eldbus_message_arguments_get(msg, "o", &path) && path)
     {
        INF("NewConnection signal for %s", path);
        enm_vpn_settings_fetch_one(nm, path);
     }
   else
     {
        WRN("NewConnection without object path; full re-enumerate");
        enm_vpn_enumerate(nm);
     }
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

   /* Explicitly cancel all in-flight VPN D-Bus calls BEFORE freeing nm.
    * eldbus_pending_cancel() fires the callback synchronously (with an error
    * reply) while nm is still alive, so the callbacks can safely decrement
    * vpn_pending, remove their entries from vpn_pending_settings, and free
    * their ctx without ever touching freed memory.
    *
    * Order matters:
    *  1. vpn_list_pending — the ListConnections call that seeds GetSettings.
    *  2. vpn_pending_settings — each per-entry GetSettings call.
    * Each cancel synchronously fires its callback, which removes the entry
    * from the list, so we drain by repeating until the list is empty. */
   if (nm->vpn_list_pending)
     {
        /* _vpn_list_cb clears nm->vpn_list_pending as its first action. */
        eldbus_pending_cancel(nm->vpn_list_pending);
     }
   while (nm->vpn_pending_settings)
     {
        Eldbus_Pending *p = nm->vpn_pending_settings->data;
        /* _vpn_get_settings_cb removes p from the list in its out: block. */
        eldbus_pending_cancel(p);
     }

   /* Discard any startup active paths that were not yet reconciled. */
   if (nm->vpn_pending_active_paths)
     {
        const char *ap;
        EINA_LIST_FREE(nm->vpn_pending_active_paths, ap)
          eina_stringshare_del(ap);
        nm->vpn_pending_active_paths = NULL;
     }

   /* Cancel any in-flight active-path resolve calls (Properties.Get for
    * "Connection" property).  _enm_vpn_resolve_cb removes the entry from the
    * list before any nm access, so we can safely drain. */
   while (nm->vpn_pending_resolves)
     {
        Eldbus_Pending *p = nm->vpn_pending_resolves->data;
        /* _enm_vpn_resolve_cb removes p from the list as its first action. */
        eldbus_pending_cancel(p);
     }

   /* Cancel any in-flight nmcli autoconnect subprocesses.  Must be done
    * before enm_vpn_clear so we don't UAF via enm_vpn_find_by_uuid. */
   enm_vpn_autoconn_pending_cancel_all(nm);

   enm_vpn_clear(nm);

   if (nm->saved_connections)
     {
        eina_hash_free(nm->saved_connections);
        nm->saved_connections = NULL;
     }
   _nm_bluetooth_connections_clear(nm);
   _nm_wired_connections_clear(nm);

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

static void
_enm_connection_deactivate(struct NM_Manager *nm, const char *connection_path)
{
   EINA_SAFETY_ON_NULL_RETURN(nm);
   EINA_SAFETY_ON_NULL_RETURN(connection_path);

   if (!strcmp(connection_path, "/"))
     {
        WRN("Refusing to disconnect null connection path");
        return;
     }

   eldbus_proxy_call(nm->proxy, "DeactivateConnection",
                     _deactivate_cb, NULL, -1,
                     "o", connection_path);
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

   _enm_connection_deactivate(nm, nm->active_connection_path);
}

void
enm_disconnect_type(struct NM_Manager *nm, enum NM_Device_Type type)
{
   struct NM_Device *dev;
   Eina_Bool disconnected = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN(nm);

   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        if (dev->type != type) continue;
        if (dev->state < 100) continue;
        if (!dev->active_conn_path) continue;

        INF("Disconnect %s connection on device %s (%s)",
            enm_device_type_to_str(type),
            dev->interface ?: dev->path,
            dev->active_conn_path);
        _enm_connection_deactivate(nm, dev->active_conn_path);
        disconnected = EINA_TRUE;
     }

   if (disconnected) return;

   if ((nm->active_conn_type == type) &&
       nm->active_connection_path &&
       strcmp(nm->active_connection_path, "/"))
     {
        INF("Disconnect primary %s connection %s",
            enm_device_type_to_str(type),
            nm->active_connection_path);
        _enm_connection_deactivate(nm, nm->active_connection_path);
     }
}

void
enm_bluetooth_connect(struct NM_Manager *nm, struct NM_Device *dev,
                      const char *connection_path)
{
   struct connection_cb_data *cd;

   EINA_SAFETY_ON_NULL_RETURN(nm);
   EINA_SAFETY_ON_NULL_RETURN(dev);
   EINA_SAFETY_ON_NULL_RETURN(connection_path);

   cd = calloc(1, sizeof(*cd));
   EINA_SAFETY_ON_NULL_RETURN(cd);

   cd->nm = nm;
   cd->dev = dev;

   INF("ActivateConnection: bluetooth profile %s on device %s",
       connection_path, dev->path);
   eldbus_proxy_call(nm->proxy, "ActivateConnection",
                     _activate_cb, cd, NM_CONNECTION_TIMEOUT,
                     "ooo", connection_path, dev->path, "/");
}

void
enm_ethernet_connect(struct NM_Manager *nm, struct NM_Device *dev)
{
   struct connection_cb_data *cd;
   struct NM_Wired_Connection *wc;

   EINA_SAFETY_ON_NULL_RETURN(nm);
   EINA_SAFETY_ON_NULL_RETURN(dev);

   cd = calloc(1, sizeof(*cd));
   EINA_SAFETY_ON_NULL_RETURN(cd);

   cd->nm = nm;
   cd->dev = dev;
   wc = _nm_wired_connection_find_for_device(nm, dev);

   if (wc && wc->path)
     {
        INF("ActivateConnection: ethernet profile %s on device %s",
            wc->path, dev->path);
        eldbus_proxy_call(nm->proxy, "ActivateConnection",
                          _activate_cb, cd, NM_CONNECTION_TIMEOUT,
                          "ooo", wc->path, dev->path, "/");
        return;
     }

   {
      Eldbus_Message *msg_call;
      Eldbus_Message_Iter *iter, *empty_dict;

      INF("AddAndActivateConnection: no saved ethernet profile for device %s",
          dev->path);

      msg_call = eldbus_proxy_method_call_new(nm->proxy,
                                              "AddAndActivateConnection");
      iter = eldbus_message_iter_get(msg_call);
      eldbus_message_iter_arguments_append(iter, "a{sa{sv}}", &empty_dict);
      eldbus_message_iter_container_close(iter, empty_dict);
      eldbus_message_iter_basic_append(iter, 'o', dev->path);
      eldbus_message_iter_basic_append(iter, 'o', "/");
      eldbus_proxy_send(nm->proxy, msg_call, _add_activate_cb, cd,
                        NM_CONNECTION_TIMEOUT);
   }
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
_nm_missing_dialog_del(void *obj EINA_UNUSED)
{
   /* Called by EFL when the dialog is destroyed by any means (OK button,
    * window-close button, e_object_del from enter path, etc.). Reset both
    * the flag and the pointer so a future exit can show the dialog again. */
   nm_missing_dialog = NULL;
   dialog_open       = EINA_FALSE;
}

static void
_e_nm_system_name_owner_exit(Eina_Bool shutdown)
{
   if (!nm_manager) return;

   _notify_manager_inout(NULL);
   _manager_free(nm_manager);
   nm_manager = NULL;

   ecore_event_add(E_NM_EVENT_MANAGER_OUT, NULL, NULL, NULL);

   if (!shutdown && !suspended && !dialog_open)
     {
        dialog_open = EINA_TRUE;
        nm_missing_dialog =
           e_util_dialog_internal(_("NetworkManager Service Missing"),
                                  _("The NetworkManager service is not available.<br>"
                                    "Is <b>NetworkManager</b> daemon running?"));
        if (nm_missing_dialog)
          e_object_del_attach_func_set(E_OBJECT(nm_missing_dialog),
                                       _nm_missing_dialog_del);
        else
          dialog_open = EINA_FALSE; /* allocation failed — don't block future attempts */
     }
}

static void
_e_nm_system_name_owner_enter(const char *owner EINA_UNUSED)
{
   /* If our "NM missing" dialog is still on screen, dismiss it now that NM
    * is back.  The del-attach callback resets dialog_open and
    * nm_missing_dialog, so we must not touch those fields directly here. */
   if (nm_missing_dialog)
     {
        e_object_del(E_OBJECT(nm_missing_dialog));
        /* nm_missing_dialog and dialog_open are now reset by the callback */
     }
   else
     {
        dialog_open = EINA_FALSE;
     }
   nm_manager = _manager_new();
   ecore_event_add(E_NM_EVENT_MANAGER_IN, NULL, NULL, NULL);
   _notify_manager_inout(nm_manager);
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
/* Suspend / resume tracking                                                   */
/* -------------------------------------------------------------------------- */

static Eina_Bool
_e_nm_sys_suspend_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   suspended = EINA_TRUE;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_nm_sys_resume_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   suspended = EINA_FALSE;
   return ECORE_CALLBACK_PASS_ON;
}

/* ========================================================================== */
/* NetworkManager SecretAgent — D-Bus server side                             */
/* ==========================================================================
 *
 * The data layer owns the NM SecretAgent D-Bus contract (interface register,
 * method dispatch, reply construction).  The UI layer (agent.c) registers a
 * pair of callbacks via e_nm_agent_callbacks_set() and replies to requests
 * via e_nm_agent_reply_secrets() / e_nm_agent_reply_cancel().
 */

#define NM_AGENT_IFACE    "org.freedesktop.NetworkManager.SecretAgent"
#define NM_AGENT_MGR_PATH "/org/freedesktop/NetworkManager/AgentManager"
#define NM_AGENT_ID       "org.enlightenment.NetworkManager"
#define AGENT_DATA_KEY    "agent"

struct _E_NM_Agent
{
   Eldbus_Service_Interface *iface;
   Eldbus_Connection        *eldbus_conn;
   /* All outstanding GetSecrets requests, indexed implicitly by msg
    * serial.  CancelGetSecrets matches a request by (conn_path,
    * setting_name) and dispatches the UI cancel callback.  Dialogs are
    * still single-instance — the UI cb path may either stack dialogs or
    * queue requests internally — but the data layer must never silently
    * drop a request, otherwise NM is left waiting forever. */
   Eina_List                *pending;   /* E_NM_Agent_Request* */
};

struct _E_NM_Agent_Request
{
   E_NM_Agent     *agent;
   Eldbus_Message *msg;                 /* ref held while request is live */
   /* Identification used by CancelGetSecrets to find this request. */
   char           *conn_path;
   char           *setting_name;
};

static E_NM_Agent_Callbacks _agent_cbs;
static void                *_agent_cb_data;

void
e_nm_agent_callbacks_set(const E_NM_Agent_Callbacks *cbs, void *data)
{
   if (cbs) _agent_cbs = *cbs;
   else memset(&_agent_cbs, 0, sizeof(_agent_cbs));
   _agent_cb_data = data;
}

static void
_agent_request_free(E_NM_Agent_Request *req)
{
   if (!req) return;
   if (req->agent)
     req->agent->pending = eina_list_remove(req->agent->pending, req);
   if (req->msg) eldbus_message_unref(req->msg);
   free(req->conn_path);
   free(req->setting_name);
   free(req);
}

static void
_agent_dict_append_str(Eldbus_Message_Iter *array, const char *key,
                       const char *val)
{
   Eldbus_Message_Iter *dict, *variant;

   eldbus_message_iter_arguments_append(array, "{sv}", &dict);
   eldbus_message_iter_basic_append(dict, 's', key);
   variant = eldbus_message_iter_container_new(dict, 'v', "s");
   eldbus_message_iter_basic_append(variant, 's', val ?: "");
   eldbus_message_iter_container_close(dict, variant);
   eldbus_message_iter_container_close(array, dict);
}

void
e_nm_agent_reply_secrets(E_NM_Agent_Request *req, const char *psk)
{
   Eldbus_Message_Iter *iter, *outer, *inner_dict, *inner_array;
   Eldbus_Message *reply;

   if (!req) return;
   if (!req->msg) { _agent_request_free(req); return; }

   /*
    * GetSecrets reply format: a{sa{sv}}
    *   { "802-11-wireless-security": { "psk": <value> } }
    */
   reply = eldbus_message_method_return_new(req->msg);
   iter  = eldbus_message_iter_get(reply);
   eldbus_message_iter_arguments_append(iter, "a{sa{sv}}", &outer);
   eldbus_message_iter_arguments_append(outer, "{sa{sv}}", &inner_dict);
   eldbus_message_iter_basic_append(inner_dict, 's',
                                    "802-11-wireless-security");
   eldbus_message_iter_arguments_append(inner_dict, "a{sv}", &inner_array);
   _agent_dict_append_str(inner_array, "psk", psk);
   eldbus_message_iter_container_close(inner_dict, inner_array);
   eldbus_message_iter_container_close(outer, inner_dict);
   eldbus_message_iter_container_close(iter, outer);

   eldbus_connection_send(req->agent->eldbus_conn, reply, NULL, NULL, -1);

   _agent_request_free(req);
}

void
e_nm_agent_reply_cancel(E_NM_Agent_Request *req)
{
   Eldbus_Message *reply;

   if (!req) return;
   if (!req->msg) { _agent_request_free(req); return; }

   reply = eldbus_message_error_new(req->msg,
            NM_AGENT_IFACE ".UserCanceled",
            "User canceled password dialog");
   eldbus_connection_send(req->agent->eldbus_conn, reply, NULL, NULL, -1);

   _agent_request_free(req);
}

void
e_nm_agent_reply_vpn_secrets(E_NM_Agent_Request *req,
                             const char *const *fields,
                             const char *const *values,
                             unsigned int n_fields)
{
   Eldbus_Message_Iter *iter, *outer, *inner_dict, *inner_array,
                        *vpn_dict, *secrets_var, *secrets_array;
   Eldbus_Message *reply;

   if (!req) return;
   if (!req->msg) { _agent_request_free(req); return; }

   /* a{sa{sv}} → "vpn" → { "secrets": a{ss} } */
   reply = eldbus_message_method_return_new(req->msg);
   iter  = eldbus_message_iter_get(reply);
   eldbus_message_iter_arguments_append(iter, "a{sa{sv}}", &outer);
   eldbus_message_iter_arguments_append(outer, "{sa{sv}}", &inner_dict);
   eldbus_message_iter_basic_append(inner_dict, 's', "vpn");
   eldbus_message_iter_arguments_append(inner_dict, "a{sv}", &inner_array);

   eldbus_message_iter_arguments_append(inner_array, "{sv}", &vpn_dict);
   eldbus_message_iter_basic_append(vpn_dict, 's', "secrets");
   secrets_var = eldbus_message_iter_container_new(vpn_dict, 'v', "a{ss}");
   eldbus_message_iter_arguments_append(secrets_var, "a{ss}", &secrets_array);
   for (unsigned int i = 0; i < n_fields; i++)
     {
        Eldbus_Message_Iter *kv;
        eldbus_message_iter_arguments_append(secrets_array, "{ss}", &kv);
        eldbus_message_iter_basic_append(kv, 's', fields[i] ?: "");
        eldbus_message_iter_basic_append(kv, 's', values[i] ?: "");
        eldbus_message_iter_container_close(secrets_array, kv);
     }
   eldbus_message_iter_container_close(secrets_var, secrets_array);
   eldbus_message_iter_container_close(vpn_dict, secrets_var);
   eldbus_message_iter_container_close(inner_array, vpn_dict);

   eldbus_message_iter_container_close(inner_dict, inner_array);
   eldbus_message_iter_container_close(outer, inner_dict);
   eldbus_message_iter_container_close(iter, outer);

   eldbus_connection_send(req->agent->eldbus_conn, reply, NULL, NULL, -1);
   _agent_request_free(req);
}

static void
_agent_ssid_extract(Eldbus_Message_Iter *conn_props, char *ssid, size_t max)
{
   Eldbus_Message_Iter *conn_dict;

   while (eldbus_message_iter_get_and_next(conn_props, 'e', &conn_dict))
     {
        Eldbus_Message_Iter *inner, *entry, *evar, *bytes;
        const char *sect, *ekey;
        unsigned char b;
        size_t pos = 0;

        if (!eldbus_message_iter_arguments_get(conn_dict, "sa{sv}",
                                               &sect, &inner))
          continue;
        if (strcmp(sect, "802-11-wireless")) continue;

        while (eldbus_message_iter_get_and_next(inner, 'e', &entry))
          {
             if (!eldbus_message_iter_arguments_get(entry, "sv",
                                                    &ekey, &evar))
               continue;
             if (strcmp(ekey, "ssid")) continue;
             if (!eldbus_message_iter_arguments_get(evar, "ay", &bytes))
               continue;
             while (eldbus_message_iter_get_and_next(bytes, 'y', &b) &&
                    pos < max - 1)
               ssid[pos++] = (char)b;
             ssid[pos] = '\0';
             return;
          }
     }
}

/* Single-pass extraction of the VPN-relevant strings from a connection
 * settings iterator.  Eldbus iterators are forward-only, so two sequential
 * calls of a "find one key" helper on the same iter would silently skip
 * fields if the second key happens to be in a section already iterated
 * past.  This helper walks the dict exactly once and fills the out struct.
 *
 * conn_id  := connection.id  (strdup, caller-owned, may be NULL)
 * svc_type := vpn.service-type (strdup, caller-owned, may be NULL) */
struct _Agent_Vpn_Conn_Info
{
   char *conn_id;
   char *svc_type;
};

static void
_agent_vpn_conn_info_parse(Eldbus_Message_Iter *conn_props,
                           struct _Agent_Vpn_Conn_Info *out)
{
   Eldbus_Message_Iter *conn_dict;

   out->conn_id = NULL;
   out->svc_type = NULL;

   while (eldbus_message_iter_get_and_next(conn_props, 'e', &conn_dict))
     {
        Eldbus_Message_Iter *inner, *entry, *var;
        const char *sect, *ekey;
        Eina_Bool want_conn, want_vpn;

        if (!eldbus_message_iter_arguments_get(conn_dict, "sa{sv}",
                                               &sect, &inner)) continue;

        want_conn = !strcmp(sect, "connection");
        want_vpn  = !strcmp(sect, "vpn");
        if (!want_conn && !want_vpn) continue;

        while (eldbus_message_iter_get_and_next(inner, 'e', &entry))
          {
             const char *s;
             if (!eldbus_message_iter_arguments_get(entry, "sv", &ekey, &var))
               continue;
             if (want_conn && !out->conn_id && !strcmp(ekey, "id"))
               {
                  if (eldbus_message_iter_arguments_get(var, "s", &s))
                    out->conn_id = strdup(s);
               }
             else if (want_vpn && !out->svc_type &&
                      !strcmp(ekey, "service-type"))
               {
                  if (eldbus_message_iter_arguments_get(var, "s", &s))
                    out->svc_type = strdup(s);
               }
          }

        if (out->conn_id && out->svc_type) return;
     }
}

static char **
_agent_hints_extract(Eldbus_Message_Iter *hints, unsigned int *n_out)
{
   char **arr = NULL;
   unsigned int n = 0, cap = 0;
   const char *s;
   while (eldbus_message_iter_get_and_next(hints, 's', &s))
     {
        /* Always keep room for the trailing NULL sentinel. */
        if (n + 1 >= cap)
          {
             unsigned int new_cap = cap ? cap * 2 : 4;
             char **tmp = realloc(arr, sizeof(*arr) * new_cap);
             if (!tmp)
               {
                  for (unsigned int i = 0; i < n; i++) free(arr[i]);
                  free(arr);
                  *n_out = 0;
                  return NULL;
               }
             arr = tmp;
             cap = new_cap;
          }
        arr[n++] = strdup(s);
     }
   if (arr) arr[n] = NULL;
   *n_out = n;
   return arr;
}

static void
_agent_str_array_free(char **arr, unsigned int n)
{
   if (!arr) return;
   for (unsigned int i = 0; i < n; i++) free(arr[i]);
   free(arr);
}

static Eldbus_Message *
_agent_get_secrets(const Eldbus_Service_Interface *iface,
                   const Eldbus_Message *msg)
{
   E_NM_Agent *a;
   E_NM_Agent_Request *req;
   Eldbus_Message_Iter *conn_props, *hints;
   const char *conn_path, *setting_name;
   uint32_t flags;
   char ssid[64] = "";

   a = eldbus_service_object_data_get(iface, AGENT_DATA_KEY);

   if (!eldbus_message_arguments_get(msg, "a{sa{sv}}osasu",
                                     &conn_props, &conn_path,
                                     &setting_name, &hints, &flags))
     {
        WRN("GetSecrets: cannot parse arguments");
        return eldbus_message_method_return_new(msg);
     }

   DBG("GetSecrets for %s setting=%s flags=%u",
       conn_path, setting_name, flags);

   /* Only extract the SSID for wifi requests — _agent_ssid_extract walks
    * conn_props with a forward-only iterator that we cannot reset, and the
    * VPN branch needs to read its own keys from the same iterator below. */
   if (!strcmp(setting_name, "802-11-wireless-security"))
     _agent_ssid_extract(conn_props, ssid, sizeof(ssid));

   /* Allocate a new request and append it to the pending list.  Dialogs
    * remain single-instance at the UI layer (the cb path stacks or queues
    * them as it sees fit) but the data layer must hold every in-flight
    * request so that CancelGetSecrets can match by (conn_path,
    * setting_name) and so that no D-Bus reply is silently dropped. */
   req = E_NEW(E_NM_Agent_Request, 1);
   req->agent = a;
   req->msg   = eldbus_message_ref((Eldbus_Message *)msg);
   req->conn_path    = conn_path ? strdup(conn_path) : NULL;
   req->setting_name = setting_name ? strdup(setting_name) : NULL;
   a->pending = eina_list_append(a->pending, req);

   if (!strcmp(setting_name, "vpn"))
     {
        struct _Agent_Vpn_Conn_Info info;
        unsigned int n_hints = 0;
        char **hint_arr;

        /* Single forward pass over the connection dict: both connection.id
         * and vpn.service-type are collected in one walk to avoid the
         * forward-only iterator skipping the second key. */
        _agent_vpn_conn_info_parse(conn_props, &info);
        hint_arr = _agent_hints_extract(hints, &n_hints);

        if (_agent_cbs.vpn_request)
          {
             _agent_cbs.vpn_request(_agent_cb_data, req,
                                    info.conn_id ?: "VPN", info.svc_type,
                                    (const char *const *)hint_arr, n_hints);
          }
        else
          {
             WRN("No VPN SecretAgent UI callback; cancelling");
             e_nm_agent_reply_cancel(req);
          }

        free(info.conn_id);
        free(info.svc_type);
        _agent_str_array_free(hint_arr, n_hints);
     }
   else if (!strcmp(setting_name, "802-11-wireless-security"))
     {
        if (_agent_cbs.request)
          _agent_cbs.request(_agent_cb_data, req, ssid[0] ? ssid : NULL);
        else
          { WRN("No SecretAgent UI cb"); e_nm_agent_reply_cancel(req); }
     }
   else
     {
        WRN("Unhandled secret setting %s; cancelling", setting_name);
        e_nm_agent_reply_cancel(req);
     }

   return NULL;  /* reply sent asynchronously */
}

static Eldbus_Message *
_agent_cancel_get_secrets(const Eldbus_Service_Interface *iface,
                          const Eldbus_Message *msg)
{
   E_NM_Agent *a;
   const char *cancel_path = NULL, *cancel_setting = NULL;
   Eina_List *l, *ln;
   E_NM_Agent_Request *req;

   DBG("CancelGetSecrets");

   if (!eldbus_message_arguments_get(msg, "os",
                                     &cancel_path, &cancel_setting))
     {
        WRN("CancelGetSecrets: cannot parse arguments");
        return eldbus_message_method_return_new(msg);
     }

   a = eldbus_service_object_data_get(iface, AGENT_DATA_KEY);
   if (!a) return eldbus_message_method_return_new(msg);

   /* Match by (conn_path, setting_name) — there may be more than one
    * pending request, and CancelGetSecrets targets a specific one. */
   EINA_LIST_FOREACH_SAFE(a->pending, l, ln, req)
     {
        if (req->conn_path && cancel_path &&
            strcmp(req->conn_path, cancel_path) != 0) continue;
        if (req->setting_name && cancel_setting &&
            strcmp(req->setting_name, cancel_setting) != 0) continue;

        /* NM is withdrawing — UI should dismiss the dialog silently; no
         * reply should be sent, NM is no longer waiting for one. */
        if (_agent_cbs.cancel)
          _agent_cbs.cancel(_agent_cb_data, req);
        _agent_request_free(req);
        break;  /* one request per (path, setting) tuple */
     }

   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_agent_save_secrets(const Eldbus_Service_Interface *iface EINA_UNUSED,
                    const Eldbus_Message *msg)
{
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_agent_delete_secrets(const Eldbus_Service_Interface *iface EINA_UNUSED,
                      const Eldbus_Message *msg)
{
   return eldbus_message_method_return_new(msg);
}

static const Eldbus_Method _agent_methods[] = {
   {
    "GetSecrets",
    ELDBUS_ARGS({"a{sa{sv}}", "connection"}, {"o", "connection_path"},
                {"s", "setting_name"}, {"as", "hints"}, {"u", "flags"}),
    ELDBUS_ARGS({"a{sa{sv}}", "secrets"}),
    _agent_get_secrets, 0
   },
   {
    "CancelGetSecrets",
    ELDBUS_ARGS({"o", "connection_path"}, {"s", "setting_name"}),
    NULL,
    _agent_cancel_get_secrets, 0
   },
   {
    "SaveSecrets",
    ELDBUS_ARGS({"a{sa{sv}}", "connection"}, {"o", "connection_path"}),
    NULL,
    _agent_save_secrets, 0
   },
   {
    "DeleteSecrets",
    ELDBUS_ARGS({"a{sa{sv}}", "connection"}, {"o", "connection_path"}),
    NULL,
    _agent_delete_secrets, 0
   },
   { NULL, NULL, NULL, NULL, 0 }
};

static const Eldbus_Service_Interface_Desc _agent_desc = {
   NM_AGENT_IFACE, _agent_methods, NULL, NULL, NULL, NULL
};

static void
_agent_register_cb(void *data EINA_UNUSED, const Eldbus_Message *msg,
                   Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;

   if (eldbus_message_error_get(msg, &name, &text))
     WRN("SecretAgent Register failed: %s: %s", name, text);
   else
     INF("SecretAgent registered with NetworkManager");
}

static E_NM_Agent *
_e_nm_agent_new(Eldbus_Connection *eldbus_conn)
{
   Eldbus_Service_Interface *iface;
   Eldbus_Object *obj;
   Eldbus_Proxy  *proxy;
   E_NM_Agent    *a;

   a = E_NEW(E_NM_Agent, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(a, NULL);

   iface = eldbus_service_interface_register(eldbus_conn, AGENT_PATH,
                                              &_agent_desc);
   if (!iface)
     {
        ERR("Failed to register SecretAgent D-Bus interface");
        free(a);
        return NULL;
     }

   eldbus_service_object_data_set(iface, AGENT_DATA_KEY, a);
   a->iface       = iface;
   a->eldbus_conn = eldbus_conn;

   obj   = eldbus_object_get(a->eldbus_conn, NM_BUS_NAME, NM_AGENT_MGR_PATH);
   proxy = eldbus_proxy_get(obj, NM_IFACE_AGENT_MGR);
   eldbus_proxy_call(proxy, "Register", _agent_register_cb, NULL, -1,
                     "s", NM_AGENT_ID);
   eldbus_proxy_unref(proxy);
   eldbus_object_unref(obj);

   return a;
}

static void
_e_nm_agent_del(E_NM_Agent *a)
{
   if (!a) return;

   /* Tell NM to drop us from its agent list.  Fire-and-forget — NM will
    * also clean us up automatically when our bus name disappears, but
    * being explicit avoids leaving a stale entry while the process is
    * still alive (e.g. module unload + reload in the same session). */
   if (a->eldbus_conn)
     {
        Eldbus_Object *obj =
            eldbus_object_get(a->eldbus_conn, NM_BUS_NAME, NM_AGENT_MGR_PATH);
        if (obj)
          {
             Eldbus_Proxy *proxy = eldbus_proxy_get(obj, NM_IFACE_AGENT_MGR);
             if (proxy)
               {
                  eldbus_proxy_call(proxy, "Unregister",
                                    NULL, NULL, -1, "");
                  eldbus_proxy_unref(proxy);
               }
             eldbus_object_unref(obj);
          }
     }

   /* Free all pending requests — no reply will be sent, but NM stops
    * expecting one as soon as we drop the bus name / unregister. */
   while (a->pending)
     {
        E_NM_Agent_Request *req = a->pending->data;
        _agent_request_free(req);
     }
   if (a->iface) eldbus_service_object_unregister(a->iface);
   free(a);
}

/* -------------------------------------------------------------------------- */
/* Public lifecycle                                                            */
/* -------------------------------------------------------------------------- */

unsigned int
e_nm_system_init(void)
{
   init_count++;
   if (init_count > 1) return init_count;

   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   if (!conn)
     {
        init_count--;
        return 0;
     }

   E_NM_EVENT_MANAGER_IN  = ecore_event_type_new();
   E_NM_EVENT_MANAGER_OUT = ecore_event_type_new();

   eldbus_name_owner_changed_callback_add(conn, NM_BUS_NAME,
                                          _e_nm_system_name_owner_changed,
                                          NULL, EINA_TRUE);
   agent = _e_nm_agent_new(conn);

   suspend_handler = ecore_event_handler_add(E_EVENT_SYS_SUSPEND,
                                             _e_nm_sys_suspend_cb, NULL);
   resume_handler  = ecore_event_handler_add(E_EVENT_SYS_RESUME,
                                             _e_nm_sys_resume_cb, NULL);

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

   if (suspend_handler) ecore_event_handler_del(suspend_handler);
   if (resume_handler)  ecore_event_handler_del(resume_handler);
   suspend_handler = NULL;
   resume_handler  = NULL;
   suspended         = EINA_FALSE;
   dialog_open       = EINA_FALSE;
   nm_missing_dialog = NULL;

   eldbus_name_owner_changed_callback_del(conn, NM_BUS_NAME,
                                          _e_nm_system_name_owner_changed,
                                          NULL);
   _e_nm_system_name_owner_exit(EINA_TRUE);

   if (agent) _e_nm_agent_del(agent);
   if (conn) eldbus_connection_unref(conn);

   agent = NULL;
   conn  = NULL;

   E_NM_EVENT_MANAGER_OUT = 0;
   E_NM_EVENT_MANAGER_IN  = 0;

   return init_count;
}
