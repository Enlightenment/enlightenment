#ifndef E_NETWORKMANAGER_H
#define E_NETWORKMANAGER_H

#include "e.h"
#include <Eldbus.h>

typedef struct _E_NM_Agent         E_NM_Agent;
typedef struct _E_NM_Agent_Request E_NM_Agent_Request;

/*
 * SecretAgent UI callbacks.
 *
 * The data layer (e_networkmanager.c) owns the NM SecretAgent D-Bus
 * contract: it registers the interface, dispatches methods, and builds
 * replies.  When NM asks for a secret it invokes `request` with the
 * extracted SSID; the UI pops whatever widgets it likes and then calls
 * e_nm_agent_reply_secrets() with the collected PSK, or
 * e_nm_agent_reply_cancel() if the user declined.
 *
 * `cancel` is invoked when NM withdraws a pending GetSecrets request
 * (e.g. connection attempt aborted).  The UI should dismiss its dialog
 * WITHOUT calling any reply function — NM is no longer waiting.
 *
 * Contract for `cancel`: the callback MUST NOT store the `req` pointer
 * and MUST NOT call e_nm_agent_reply_*.  After the callback returns the
 * data layer frees the request; any subsequent access is a use-after-free.
 */
typedef void (*E_NM_Agent_Secrets_Request_Cb)(void *data,
                                               E_NM_Agent_Request *req,
                                               const char *ssid);
typedef void (*E_NM_Agent_Secrets_Cancel_Cb)(void *data,
                                              E_NM_Agent_Request *req);
/* VPN secrets request callback.
 *
 * fields is a NULL-terminated array of secret-key names (also bounded by
 * n_fields).  The array itself, the strings it points to, conn_name and
 * service_type are owned by the data layer and are valid only for the
 * duration of the callback — copy anything you need to keep before
 * returning.  The req pointer remains valid until the UI calls
 * e_nm_agent_reply_vpn_secrets() or e_nm_agent_reply_cancel(). */
typedef void (*E_NM_Agent_VPN_Secrets_Request_Cb)(void *data,
        E_NM_Agent_Request *req,
        const char *conn_name,
        const char *service_type,
        const char *message,
        const char *const *fields,
        unsigned int n_fields);

typedef struct _E_NM_Agent_Callbacks E_NM_Agent_Callbacks;
struct _E_NM_Agent_Callbacks
{
   E_NM_Agent_Secrets_Request_Cb     request;       /* wifi PSK */
   E_NM_Agent_Secrets_Cancel_Cb      cancel;
   E_NM_Agent_VPN_Secrets_Request_Cb vpn_request;
};

void e_nm_agent_callbacks_set(const E_NM_Agent_Callbacks *cbs, void *data);
void e_nm_agent_reply_secrets(E_NM_Agent_Request *req, const char *psk);
void e_nm_agent_reply_cancel(E_NM_Agent_Request *req);
void e_nm_agent_reply_vpn_secrets(E_NM_Agent_Request *req,
                                  const char *const *fields,
                                  const char *const *values,
                                  unsigned int n_fields);

/*
 * D-Bus name and interface constants shared between the main data layer
 * and the VPN sub-module.
 */
#define NM_BUS_NAME  "org.freedesktop.NetworkManager"
#define NM_IFACE_SCONN "org.freedesktop.NetworkManager.Settings.Connection"
/* org.freedesktop.NetworkManager.Connection.Active — used both for the
 * primary active connection probe in e_networkmanager.c and for binding
 * the WireGuard active-state watcher in e_networkmanager_vpn.c. */
#define NM_IFACE_ACTIVE_CONN "org.freedesktop.NetworkManager.Connection.Active"

/*
 * NM D-Bus state values, matching org.freedesktop.NetworkManager.State
 */
enum NM_State
{
   NM_STATE_UNKNOWN          = 0,
   NM_STATE_ASLEEP           = 10,
   NM_STATE_DISCONNECTED     = 20,
   NM_STATE_DISCONNECTING    = 30,
   NM_STATE_CONNECTING       = 40,
   NM_STATE_CONNECTED_LOCAL  = 50,
   NM_STATE_CONNECTED_SITE   = 60,
   NM_STATE_CONNECTED_GLOBAL = 70,
};

/*
 * NM device types, matching org.freedesktop.NetworkManager.Device.DeviceType
 */
enum NM_Device_Type
{
   NM_DEVICE_TYPE_UNKNOWN   = 0,
   NM_DEVICE_TYPE_ETHERNET  = 1,
   NM_DEVICE_TYPE_WIFI      = 2,
   NM_DEVICE_TYPE_BLUETOOTH = 5,
   NM_DEVICE_TYPE_MODEM     = 8,
};

/*
 * NM device states, matching org.freedesktop.NetworkManager.Device.State
 */
enum NM_Device_State
{
   NM_DEVICE_STATE_UNKNOWN       = 0,
   NM_DEVICE_STATE_UNMANAGED     = 10,
   NM_DEVICE_STATE_UNAVAILABLE   = 20,
   NM_DEVICE_STATE_DISCONNECTED  = 30,
   NM_DEVICE_STATE_PREPARE       = 40,
   NM_DEVICE_STATE_CONFIG        = 50,
   NM_DEVICE_STATE_NEED_AUTH     = 60,
   NM_DEVICE_STATE_IP_CONFIG     = 70,
   NM_DEVICE_STATE_IP_CHECK      = 80,
   NM_DEVICE_STATE_SECONDARIES   = 90,
   NM_DEVICE_STATE_ACTIVATED     = 100,
   NM_DEVICE_STATE_DEACTIVATING  = 110,
   NM_DEVICE_STATE_FAILED        = 120,
};

/*
 * AP security flags — these are bitmask values from NM's WpaFlags / RsnFlags
 */
enum NM_AP_Security
{
   NM_AP_SEC_NONE        = 0x00000000,
   NM_AP_SEC_PAIR_WEP40  = 0x00000001,
   NM_AP_SEC_PAIR_WEP104 = 0x00000002,
   NM_AP_SEC_PAIR_TKIP   = 0x00000004,
   NM_AP_SEC_PAIR_CCMP   = 0x00000008,
   NM_AP_SEC_GROUP_WEP40 = 0x00000010,
   NM_AP_SEC_GROUP_WEP104= 0x00000020,
   NM_AP_SEC_GROUP_TKIP  = 0x00000040,
   NM_AP_SEC_GROUP_CCMP  = 0x00000080,
   NM_AP_SEC_KEY_MGMT_PSK= 0x00000100,
   NM_AP_SEC_KEY_MGMT_802_1X = 0x00000200,
   NM_AP_SEC_KEY_MGMT_SAE    = 0x00000400,
};

enum NM_VPN_State
{
   NM_VPN_STATE_UNKNOWN      = 0,
   NM_VPN_STATE_PREPARE      = 1,
   NM_VPN_STATE_NEED_AUTH    = 2,
   NM_VPN_STATE_CONNECT      = 3,
   NM_VPN_STATE_IP_CONFIG    = 4,
   NM_VPN_STATE_ACTIVATED    = 5,
   NM_VPN_STATE_FAILED       = 6,
   NM_VPN_STATE_DISCONNECTED = 7,
};

struct NM_VPN_Connection
{
   const char *path;               /* /org/freedesktop/NetworkManager/Settings/N (stringshare) */
   EINA_INLIST;

   char  *uuid;
   char  *name;                    /* connection.id */
   char  *conn_type;               /* "vpn" or "wireguard" */
   char  *service_type;            /* plugin DBus name, NULL for wireguard */
   Eina_Bool autoconnect;

   /* Live state — populated only when an Active matches this saved connection. */
   const char *active_path;        /* stringshare; NULL when inactive */
   enum NM_VPN_State vpn_state;
   enum NM_VPN_State prev_state;   /* state before the most recent transition */
   uint32_t          fail_reason;  /* last failure reason code, 0 if none */
   char *ip_address;               /* tunnel IPv4 (heap-allocated) */

   /* Back-pointer to the owning manager — set after creation in
    * _vpn_get_settings_cb.  Never NULL once initialised. */
   struct NM_Manager     *nm;

   /* D-Bus handles tied to the active state (NULL when inactive). */
   Eldbus_Proxy          *active_proxy;
   Eldbus_Object         *active_obj;
   Eldbus_Signal_Handler *active_prop_handler;
   Eldbus_Pending        *pending_get_props;

   /* D-Bus handles for the active connection's Ip4Config object (NULL when
    * the tunnel is not up or the IP has not been assigned yet). */
   Eldbus_Proxy          *ip4_proxy;
   Eldbus_Object         *ip4_obj;
   Eldbus_Signal_Handler *ip4_prop_handler;
   Eldbus_Pending        *pending_ip4_props;
};

struct NM_Access_Point
{
   const char   *path;
   Eldbus_Proxy *proxy;
   EINA_INLIST;

   Eldbus_Signal_Handler *prop_changed_handler;
   Eldbus_Pending        *pending_get_props;

   char         *ssid;
   uint8_t       strength;
   uint32_t      wpa_flags;
   uint32_t      rsn_flags;
   uint32_t      frequency;
};

struct NM_Bluetooth_Connection
{
   const char *path;               /* /org/freedesktop/NetworkManager/Settings/N (stringshare) */
   EINA_INLIST;

   char *name;                     /* connection.id */
   char *bdaddr;                   /* normalized XX:XX:XX:XX:XX:XX, or NULL */
};

struct NM_Wired_Connection
{
   const char *path;               /* /org/freedesktop/NetworkManager/Settings/N (stringshare) */
   EINA_INLIST;

   char *name;                     /* connection.id */
   char *interface_name;           /* connection.interface-name, or NULL */
   char *hw_address;               /* normalized XX:XX:XX:XX:XX:XX, or NULL */
};

struct NM_Device
{
   const char   *path;
   Eldbus_Proxy *proxy;
   Eldbus_Proxy *wireless_proxy;
   EINA_INLIST;

   Eldbus_Signal_Handler *prop_changed_handler;
   Eldbus_Signal_Handler *wifi_prop_changed_handler; /* PropertiesChanged on wireless_proxy */
   Eldbus_Signal_Handler *ap_added_handler;
   Eldbus_Signal_Handler *ap_removed_handler;

   char              *interface;
   char              *hw_address;
   enum NM_Device_Type type;
   uint32_t            state;

   /* Transient paths read from Device.GetAll / Device.Wireless.GetAll — kept
    * up-to-date via PropertiesChanged so that re-adoption can be attempted
    * whenever any of the three conditions (state>=100, active_conn_path,
    * active_ap_path) first becomes satisfied after a cold-start miss. */
   char *active_conn_path; /* ActiveConnection object path (Device iface) */
   char *active_ap_path;   /* ActiveAccessPoint object path (Wireless iface) */
   char *ip4_path;         /* Ip4Config object path */

   Eina_Inlist  *access_points; /* NM_Access_Point inlist, WiFi only */

   struct
     {
        Eldbus_Pending *get_props;
        Eldbus_Pending *get_aps;
        Eldbus_Pending *get_wifi_props; /* Device.Wireless GetAll */
     } pending;
};

struct NM_Manager
{
   Eldbus_Proxy *proxy;       /* org.freedesktop.NetworkManager */
   Eldbus_Proxy *props_proxy; /* org.freedesktop.DBus.Properties on NM */

   Eldbus_Signal_Handler *prop_changed_handler;   /* PropertiesChanged on props_proxy */
   Eldbus_Signal_Handler *device_added_handler;   /* DeviceAdded on proxy */
   Eldbus_Signal_Handler *device_removed_handler; /* DeviceRemoved on proxy */

   Eina_Inlist  *devices;    /* NM_Device inlist */

   enum NM_State state;
   Eina_Bool     wireless_enabled;

   const char   *active_ap_path;
   const char   *active_connection_path;
   char         *ip_address;
   enum NM_Device_Type active_conn_type; /* type of the primary active connection */

   struct
     {
        Eldbus_Pending *get_props;
        Eldbus_Pending *get_devices;
        Eldbus_Pending *ip4config;
     } pending;

   /* Persistent proxy/obj for watching active connection properties.
    * Created when active connection changes, freed when it changes again
    * or on manager shutdown.  Signal-driven via PropertiesChanged. */
   Eldbus_Proxy          *active_conn_proxy;
   Eldbus_Object         *active_conn_obj;
   Eldbus_Signal_Handler *active_conn_signal_handler; /* for explicit removal */

   /* Persistent proxy/obj for watching IP4Config properties. */
   Eldbus_Proxy          *ip4_proxy;
   Eldbus_Object         *ip4_obj;
   const char            *ip4_path;
   Eldbus_Signal_Handler *ip4_prop_handler; /* for explicit removal */

   /* Saved WiFi connections: SSID (string) -> connection D-Bus path (stringshare) */
   Eina_Hash    *saved_connections;
   Eina_Inlist  *bluetooth_connections; /* NM_Bluetooth_Connection inlist */
   Eina_Inlist  *wired_connections;     /* NM_Wired_Connection inlist */
   int           saved_conn_pending;    /* outstanding GetSettings calls */
   unsigned int  saved_conn_generation; /* increment to abort in-flight GetSettings */

   Eina_Inlist *vpn_connections;        /* NM_VPN_Connection inlist */
   int           vpn_pending;           /* outstanding VPN GetSettings calls */
   unsigned int  vpn_generation;        /* increment to abort in-flight VPN callbacks */
   Eldbus_Pending *vpn_list_pending;    /* in-flight ListConnections call, or NULL */
   Eina_List     *vpn_pending_settings; /* Eldbus_Pending* per in-flight VPN GetSettings */
   Eina_List     *vpn_pending_resolves; /* Eldbus_Pending* per in-flight active-path resolve (Properties.Get "Connection") */
   Eina_List     *vpn_pending_active_paths; /* stringshare'd active paths captured at startup,
                                             * re-reconciled after enm_vpn_enumerate completes */

   /* Long-lived Settings object/proxy for ConnectionRemoved signal subscription.
    * Created in _manager_new, freed in _manager_free. */
   Eldbus_Proxy          *settings_proxy;
   Eldbus_Object         *settings_obj;
   Eldbus_Signal_Handler *conn_removed_handler;
   Eldbus_Signal_Handler *conn_added_handler;

   Eina_List     *vpn_pending_autoconn;  /* _autoconn_ctx* per in-flight nmcli connection modify */

   Eina_List     *pending_probes; /* Pending struct _Active_Conn_Probe's */
   /* Generation counter incremented each time a new batch of active-connection
    * probes is started.  Each probe captures the generation at creation time
    * and discards its result in the callback if the generation has advanced,
    * preventing stale probes from clobbering a newer watcher. */
   unsigned int  probe_generation;
};

/* Ecore Events */
extern int E_NM_EVENT_MANAGER_IN;
extern int E_NM_EVENT_MANAGER_OUT;

/* Lifecycle */
unsigned int e_nm_system_init(void);
unsigned int e_nm_system_shutdown(void);

/* Scan */
void e_nm_scan(struct NM_Manager *nm);

/* Connection actions */
void enm_ap_connect(struct NM_Manager *nm, struct NM_Device *dev,
                    struct NM_Access_Point *ap);
void enm_ap_disconnect(struct NM_Manager *nm);
void enm_disconnect_type(struct NM_Manager *nm, enum NM_Device_Type type);
void enm_bluetooth_connect(struct NM_Manager *nm,
                           struct NM_Device *dev,
                           const char *connection_path);
void enm_ethernet_connect(struct NM_Manager *nm, struct NM_Device *dev);
void enm_wireless_enabled_set(struct NM_Manager *nm, Eina_Bool enabled);

/* Find AP across all devices */
struct NM_Access_Point *enm_manager_find_ap(struct NM_Manager *nm,
                                            const char *path) EINA_ARG_NONNULL(1, 2);

/*
 * Module callbacks.
 *
 * The data layer (e_networkmanager.c) does not depend on the UI layer
 * (e_mod_main.c) directly.  Instead the module registers a set of
 * callbacks at init time; the data layer invokes them through this
 * indirection when NM state changes.
 */
typedef struct _E_NM_Mod_Callbacks E_NM_Mod_Callbacks;
struct _E_NM_Mod_Callbacks
{
   void (*aps_changed)(struct NM_Manager *nm);
   void (*manager_update)(struct NM_Manager *nm);
   void (*manager_inout)(struct NM_Manager *nm);
   void (*vpn_changed)(struct NM_Manager *nm);
   void (*vpn_active_changed)(struct NM_Manager *nm);
};

void e_nm_module_callbacks_set(const E_NM_Mod_Callbacks *cbs);

/* Utility */
const char *enm_state_to_str(enum NM_State state);
const char *enm_device_type_to_str(enum NM_Device_Type type);
const char *enm_ap_security_to_str(uint32_t wpa_flags, uint32_t rsn_flags);

/* Maps an NM connection type + vpn.service-type to a short display label.
 * conn_type:    "vpn" or "wireguard"
 * service_type: required when conn_type == "vpn", ignored for wireguard.
 *               e.g. "org.freedesktop.NetworkManager.openvpn"
 * Returns a static string; never NULL.  ("VPN" is the catch-all fallback.) */
const char *enm_vpn_type_label(const char *conn_type,
                               const char *service_type);

/* Saved connections */
void enm_saved_connections_get(struct NM_Manager *nm);
void enm_connection_delete(struct NM_Manager *nm, const char *connection_path);
struct NM_Bluetooth_Connection *enm_bluetooth_connection_find(
   struct NM_Manager *nm, const char *connection_path);
struct NM_Device *enm_bluetooth_connection_device_find(
   struct NM_Manager *nm, struct NM_Bluetooth_Connection *bc);

/* VPN data layer API: see e_networkmanager_vpn.h */

/* Log */
extern int _e_nm_log_dom;

#undef DBG
#undef INF
#undef WRN
#undef ERR

#define DBG(...) EINA_LOG_DOM_DBG(_e_nm_log_dom, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_e_nm_log_dom, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_e_nm_log_dom, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_e_nm_log_dom, __VA_ARGS__)

#endif /* E_NETWORKMANAGER_H */
