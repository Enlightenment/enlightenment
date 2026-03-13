#ifndef E_NETWORKMANAGER_H
#define E_NETWORKMANAGER_H

#include "e.h"
#include <Eldbus.h>

typedef struct _E_NM_Agent E_NM_Agent;

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

struct NM_Access_Point
{
   const char   *path;
   Eldbus_Proxy *proxy;
   EINA_INLIST;

   char         *ssid;
   uint8_t       strength;
   uint32_t      wpa_flags;
   uint32_t      rsn_flags;
   uint32_t      frequency;
};

struct NM_Device
{
   const char   *path;
   Eldbus_Proxy *proxy;
   Eldbus_Proxy *wireless_proxy;
   EINA_INLIST;

   char              *interface;
   enum NM_Device_Type type;
   uint32_t            state;

   /* Transient paths read from Device.GetAll — used during startup probe
    * to avoid an extra ActiveConnection.GetAll round-trip. */
   char *active_conn_path; /* ActiveConnection object path */
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
        Eldbus_Pending *active_conn;
        Eldbus_Pending *ip4config;
     } pending;

   /* Persistent proxy/obj for watching active connection properties.
    * Created when active connection changes, freed when it changes again
    * or on manager shutdown.  Signal-driven via PropertiesChanged. */
   Eldbus_Proxy          *active_conn_proxy;
   Eldbus_Object         *active_conn_obj;
   Eldbus_Signal_Handler *active_conn_signal_handler; /* for explicit removal */

   /* Persistent proxy/obj for watching IP4Config properties. */
   Eldbus_Proxy  *ip4_proxy;
   Eldbus_Object *ip4_obj;
   const char    *ip4_path;

   /* Saved WiFi connections: SSID (string) -> connection D-Bus path (stringshare) */
   Eina_Hash    *saved_connections;
   int           saved_conn_pending; /* outstanding GetSettings calls */

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
unsigned int e_nm_system_init(Eldbus_Connection *eldbus_conn) EINA_ARG_NONNULL(1);
unsigned int e_nm_system_shutdown(void);

/* Scan */
void e_nm_scan(struct NM_Manager *nm);

/* Connection actions */
void enm_ap_connect(struct NM_Manager *nm, struct NM_Device *dev,
                    struct NM_Access_Point *ap);
void enm_ap_disconnect(struct NM_Manager *nm);
void enm_wireless_enabled_set(struct NM_Manager *nm, Eina_Bool enabled);

/* Find AP across all devices */
struct NM_Access_Point *enm_manager_find_ap(struct NM_Manager *nm,
                                            const char *path) EINA_ARG_NONNULL(1, 2);

/* UI callbacks (implemented in e_mod_main.c) */
void enm_mod_aps_changed(struct NM_Manager *nm);
void enm_mod_manager_update(struct NM_Manager *nm);
void enm_mod_manager_inout(struct NM_Manager *nm);

/* Utility */
const char *enm_state_to_str(enum NM_State state);
const char *enm_device_type_to_str(enum NM_Device_Type type);
const char *enm_ap_security_to_str(uint32_t wpa_flags, uint32_t rsn_flags);

/* Saved connections */
void enm_saved_connections_get(struct NM_Manager *nm);
void enm_connection_delete(struct NM_Manager *nm, const char *connection_path);

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
