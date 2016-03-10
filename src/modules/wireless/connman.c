#include "wireless.h"

#define CONNMAN_BUS_NAME "net.connman"
#define CONNMAN_MANAGER_IFACE CONNMAN_BUS_NAME ".Manager"
#define CONNMAN_SERVICE_IFACE CONNMAN_BUS_NAME ".Service"
#define CONNMAN_TECHNOLOGY_IFACE CONNMAN_BUS_NAME ".Technology"
#define CONNMAN_TECHNOLOGY_PATH_ETHERNET "/net/connman/technology/ethernet"
#define CONNMAN_TECHNOLOGY_PATH_WIFI "/net/connman/technology/wifi"
#define CONNMAN_TECHNOLOGY_PATH_BT "/net/connman/technology/bluetooth"
#define CONNMAN_TECHNOLOGY_PATH_CELLULAR "/net/connman/technology/cellular"
#define CONNMAN_AGENT_IFACE "net.connman.Agent"
#define CONNMAN_AGENT_PATH "/org/enlightenment/wireless/agent"

#define CONNMAN_SERVICE_TYPE_ITER(i) \
  for ((i) = 0; (i) < CONNMAN_SERVICE_TYPE_LAST; (i)++)

#define MILLI_PER_SEC 1000
#define CONNMAN_CONNECTION_TIMEOUT 60 * MILLI_PER_SEC

#undef DBG
#undef INF
#undef WRN
#undef ERR

#define DBG(...) EINA_LOG_DOM_DBG(_connman_log_dom, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_connman_log_dom, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_connman_log_dom, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_connman_log_dom, __VA_ARGS__)

typedef enum
{
   CONNMAN_STATE_NONE = -1, /* All unknown states */
   CONNMAN_STATE_OFFLINE,
   CONNMAN_STATE_IDLE,
   CONNMAN_STATE_ASSOCIATION,
   CONNMAN_STATE_CONFIGURATION,
   CONNMAN_STATE_READY,
   CONNMAN_STATE_ONLINE,
   CONNMAN_STATE_DISCONNECT,
   CONNMAN_STATE_FAILURE,
} Connman_State;

typedef enum
{
   CONNMAN_SERVICE_TYPE_NONE = -1, /* All non-supported types */
   CONNMAN_SERVICE_TYPE_ETHERNET = 0,
   CONNMAN_SERVICE_TYPE_WIFI = 1,
   CONNMAN_SERVICE_TYPE_BLUETOOTH = 2,
   CONNMAN_SERVICE_TYPE_CELLULAR = 3,
   CONNMAN_SERVICE_TYPE_LAST = 4,
} Connman_Service_Type;

typedef struct Connman_Technology
{
   Connman_Service_Type type;
   Eldbus_Proxy *proxy;
   Eina_Stringshare *tethering_ssid;
   Eina_Stringshare *tethering_passwd;
   Eina_Bool powered : 1;
   Eina_Bool connected : 1;
   Eina_Bool tethering : 1;
} Connman_Technology;

typedef struct
{
   EINA_INLIST;
   Eldbus_Proxy *proxy;

   /* Private */
   struct
   {
      Eldbus_Pending *connect;
      Eldbus_Pending *disconnect;
      Eldbus_Pending *remov;
      void *data;
   } pending;
   Eldbus_Signal_Handler *handler;

   /* Properties */
   Eina_Stringshare *path;
   Eina_Stringshare *name;
   Wireless_Network_Security security;
   Connman_State state;
   Connman_Service_Type type;
   uint8_t strength;

   /* Connection */
   unsigned int method;
   Eina_Stringshare *address;
   Eina_Stringshare *gateway;
   union
   {
      struct
      {
         Eina_Stringshare *netmask;
      } v4;
      struct
      {
         Eina_Stringshare *prefixlength;
         Wireless_Network_IPv6_Privacy privacy;
      } v6;
   } ip;

   Eina_Array *domain_servers;
   Eina_Array *name_servers;
   Eina_Array *time_servers;

   /* Proxy */
   unsigned int proxy_type;
   Eina_Stringshare *proxy_url;
   Eina_Array *proxy_servers;
   Eina_Array *proxy_excludes;
   Eina_Bool ipv6 : 1;
   Eina_Bool favorite : 1;
} Connman_Service;

typedef enum
{
   CONNMAN_FIELD_STATE_MANDATORY,
   CONNMAN_FIELD_STATE_OPTIONAL,
   CONNMAN_FIELD_STATE_ALTERNATE,
   CONNMAN_FIELD_STATE_INFO,
} Connman_Field_State;

typedef struct Connman_Field
{
   const char *name;

   Connman_Field_State requirement;
   const char *type;
   const char *value;
} Connman_Field;

static int _connman_log_dom = -1;

static Eldbus_Proxy *proxy_manager;

static Eldbus_Pending *pending_gettechnologies;
static Eldbus_Pending *pending_getservices;
static Eldbus_Pending *pending_getproperties_manager;

static Eina_List *signal_handlers;

static Eina_Inlist *connman_services_list[CONNMAN_SERVICE_TYPE_LAST];
static Eina_Hash *connman_services[CONNMAN_SERVICE_TYPE_LAST];
static Eldbus_Service_Interface *agent_iface;

static Connman_Service *connman_current_service[CONNMAN_SERVICE_TYPE_LAST];
static Wireless_Connection *connman_current_connection[CONNMAN_SERVICE_TYPE_LAST];

static Connman_Technology connman_technology[CONNMAN_SERVICE_TYPE_LAST];

/* connman -> wireless */
static Eina_Hash *connman_services_map[CONNMAN_SERVICE_TYPE_LAST];

static inline Eina_Bool
_connman_service_is_connected(const Connman_Service *cs)
{
   return (cs->state >= CONNMAN_STATE_ASSOCIATION) && (cs->state <= CONNMAN_STATE_ONLINE);
}

static void
_eldbus_proxy_del(Eldbus_Proxy *proxy)
{
   Eldbus_Object *obj;

   obj = eldbus_proxy_object_get(proxy);
   eldbus_proxy_unref(proxy);
   eldbus_object_unref(obj);
}

static void
_connman_service_connect_cb(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   Connman_Service *cs = data;
   const char *error;

   /* FIXME */
   cs->pending.connect = NULL;
   eldbus_message_error_get(msg, NULL, &error);
}

static Eina_Bool 
_connman_service_connect(Wireless_Network *wn)
{
   Connman_Service *cs;

   cs = eina_hash_find(connman_services[wn->type], wn->path);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cs, EINA_FALSE);
   if (!cs->pending.connect)
     cs->pending.connect = eldbus_proxy_call(cs->proxy, "Connect",
                                             _connman_service_connect_cb, cs,
                                             CONNMAN_CONNECTION_TIMEOUT, "");
   return !!cs->pending.connect;
}

static void
_connman_update_technologies(void)
{
   Eina_Bool avail[CONNMAN_SERVICE_TYPE_LAST];
   int i;

   CONNMAN_SERVICE_TYPE_ITER(i)
     avail[i] = connman_technology[i].type > -1;
   wireless_service_type_available_set(avail);
}

static void
_connman_update_enabled_technologies(void)
{
   Eina_Bool enabled[CONNMAN_SERVICE_TYPE_LAST];
   int i;

   CONNMAN_SERVICE_TYPE_ITER(i)
     enabled[i] = connman_technology[i].powered;
   wireless_service_type_enabled_set(enabled);
}

static Wireless_Network_State
_connman_wifi_state_convert(Connman_State state)
{
   Wireless_Network_State wifi_state;
   switch (state)
     {
      case CONNMAN_STATE_ASSOCIATION:
      case CONNMAN_STATE_CONFIGURATION:
        wifi_state = WIRELESS_NETWORK_STATE_CONFIGURING;
        break;
      case CONNMAN_STATE_READY:
        wifi_state = WIRELESS_NETWORK_STATE_CONNECTED;
        break;
      case CONNMAN_STATE_ONLINE:
        wifi_state = WIRELESS_NETWORK_STATE_ONLINE;
        break;
      case CONNMAN_STATE_FAILURE:
        wifi_state = WIRELESS_NETWORK_STATE_FAILURE;
        break;
      case CONNMAN_STATE_NONE:
      case CONNMAN_STATE_OFFLINE:
      case CONNMAN_STATE_IDLE:
      case CONNMAN_STATE_DISCONNECT:
      default:
        wifi_state = WIRELESS_NETWORK_STATE_NONE;
     }
   return wifi_state;
}

static Wireless_Network *
_connman_service_convert(Connman_Service *cs)
{
   Wireless_Network *wn;

   wn = E_NEW(Wireless_Network, 1);
   memcpy(wn, &cs->path, offsetof(Wireless_Network, connect_cb));
   wn->state = _connman_wifi_state_convert(cs->state);
   wn->connect_cb = _connman_service_connect;
   return wn;
}

static void
_connman_update_current_network(Connman_Service *cs, Connman_Service_Type type)
{
   if (connman_current_service[type] != cs)
     {
        E_FREE(connman_current_connection[type]);
        if (cs)
          connman_current_connection[type] = E_NEW(Wireless_Connection, 1);
     }
   connman_current_service[type] = cs;
   if (cs)
     {
        connman_current_connection[type]->wn = eina_hash_find(connman_services_map[type], &cs);
        memcpy(&connman_current_connection[type]->method,
          &cs->method, sizeof(Wireless_Connection) - sizeof(void*));
     }
   else
     connman_current_connection[type] = NULL;
   wireless_wifi_current_networks_set(connman_current_connection);
}

static void
_connman_update_networks(Connman_Service_Type type)
{
   Eina_Array *arr;
   Connman_Service *cs;
   Wireless_Network *wn;
   Eina_Hash *map;
   Connman_Service *services[CONNMAN_SERVICE_TYPE_LAST] = {NULL};

   map = connman_services_map[type];
   connman_services_map[type] = eina_hash_pointer_new(free);
   arr = eina_array_new(eina_hash_population(connman_services[type]));
   EINA_INLIST_FOREACH(connman_services_list[type], cs)
     {
        wn = _connman_service_convert(cs);
        eina_hash_add(connman_services_map[type], &cs, wn);
        eina_array_push(arr, wn);
        if (connman_current_service[type] && _connman_service_is_connected(cs))
          services[type] = cs;
     }
   memcpy(&connman_current_service, services, CONNMAN_SERVICE_TYPE_LAST * sizeof(void*));
   arr = wireless_networks_set(arr);
   _connman_update_current_network(connman_current_service[type], type);
   eina_hash_free(map);
   eina_array_free(arr);
}

static void
_connman_update_airplane_mode(Eina_Bool offline)
{
   wireless_airplane_mode_set(offline);
}

static Connman_State
str_to_state(const char *s)
{
   if (!strcmp(s, "offline"))
     return CONNMAN_STATE_OFFLINE;
   if (!strcmp(s, "idle"))
     return CONNMAN_STATE_IDLE;
   if (!strcmp(s, "association"))
     return CONNMAN_STATE_ASSOCIATION;
   if (!strcmp(s, "configuration"))
     return CONNMAN_STATE_CONFIGURATION;
   if (!strcmp(s, "ready"))
     return CONNMAN_STATE_READY;
   if (!strcmp(s, "online"))
     return CONNMAN_STATE_ONLINE;
   if (!strcmp(s, "disconnect"))
     return CONNMAN_STATE_DISCONNECT;
   if (!strcmp(s, "failure"))
     return CONNMAN_STATE_FAILURE;

   ERR("Unknown state %s", s);
   return CONNMAN_STATE_NONE;
}

static Connman_Service_Type
str_to_type(const char *s)
{
   if (!strcmp(s, "ethernet"))
     return CONNMAN_SERVICE_TYPE_ETHERNET;
   if (!strcmp(s, "wifi"))
     return CONNMAN_SERVICE_TYPE_WIFI;
   if (!strcmp(s, "bluetooth"))
     return CONNMAN_SERVICE_TYPE_BLUETOOTH;
   if (!strcmp(s, "cellular"))
     return CONNMAN_SERVICE_TYPE_CELLULAR;

   DBG("Unknown type %s", s);
   return CONNMAN_SERVICE_TYPE_NONE;
}

static Wireless_Network_Security
str_to_security(const char *s)
{
   if (!strcmp(s, "none")) return WIRELESS_NETWORK_SECURITY_NONE;
   if (!strcmp(s, "wep")) return WIRELESS_NETWORK_SECURITY_WEP;
   if (!strcmp(s, "psk")) return WIRELESS_NETWORK_SECURITY_PSK;
   if (!strcmp(s, "ieee8021x")) return WIRELESS_NETWORK_SECURITY_IEEE8021X;
   if (!strcmp(s, "wps")) return WIRELESS_NETWORK_SECURITY_WPS;
   CRI("UNKNOWN TYPE %s", s);
   return WIRELESS_NETWORK_SECURITY_NONE;
}

static void
_connman_service_free(Connman_Service *cs)
{
   if (!cs) return;

   if (cs->pending.connect)
     {
        eldbus_pending_cancel(cs->pending.connect);
        free(cs->pending.data);
     }
   else if (cs->pending.disconnect)
     {
        eldbus_pending_cancel(cs->pending.disconnect);
        free(cs->pending.data);
     }
   else if (cs->pending.remov)
     {
        eldbus_pending_cancel(cs->pending.remov);
        free(cs->pending.data);
     }
   eina_stringshare_del(cs->address);
   eina_stringshare_del(cs->gateway);
   if (cs->ipv6)
     eina_stringshare_del(cs->ip.v6.prefixlength);
   else
     eina_stringshare_del(cs->ip.v4.netmask);

   eina_stringshare_del(cs->proxy_url);

   array_clear(cs->domain_servers);
   array_clear(cs->name_servers);
   array_clear(cs->time_servers);

   array_clear(cs->proxy_servers);
   array_clear(cs->proxy_excludes);

   eina_stringshare_del(cs->name);
   eina_stringshare_del(cs->path);
   eldbus_signal_handler_del(cs->handler);
   DBG("service free %p || proxy %p", cs, cs->proxy);
   _eldbus_proxy_del(cs->proxy);
   connman_services_list[cs->type] = eina_inlist_remove(connman_services_list[cs->type], EINA_INLIST_GET(cs));

   free(cs);
}

static void
_connman_service_parse_stringarray(Eldbus_Message_Iter *value, Eina_Array **arr)
{
   Eldbus_Message_Iter *itr_array;
   const char *s;

   EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value, "as",
                                                         &itr_array));
   if (*arr)
     {
        while (eina_array_count(*arr))
          eina_stringshare_del(eina_array_pop(*arr));
     }
   else
     *arr = eina_array_new(1);
   while (eldbus_message_iter_get_and_next(itr_array, 's', &s))
     eina_array_push(*arr, eina_stringshare_add(s));
}

static void
_connman_service_parse_prop_changed(Connman_Service *cs, const char *prop_name, Eldbus_Message_Iter *value)
{
   DBG("service %p %s prop %s", cs, cs->path, prop_name);

   if (!strcmp(prop_name, "State"))
     {
        const char *state;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "s",
                                                                     &state));
        cs->state = str_to_state(state);
        DBG("New state: %s %d", state, cs->state);
     }
   if (!strcmp(prop_name, "Favorite"))
     {
        Eina_Bool state;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "b",
                                                                     &state));
        cs->favorite = !!state;
        DBG("New favorite state: %d", cs->favorite);
     }
   else if (!strcmp(prop_name, "Name"))
     {
        const char *name;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "s",
                                                                     &name));
        eina_stringshare_replace(&cs->name, name);
        DBG("New name: %s", cs->name);
     }
   else if (!strcmp(prop_name, "Type"))
     {
        const char *type;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "s",
                                                                     &type));
        cs->type = str_to_type(type);
        DBG("New type: %s %d", type, cs->type);
     }
   else if (!strcmp(prop_name, "Strength"))
     {
        uint8_t strength;
        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value,
                                                                     "y",
                                                                     &strength));
        cs->strength = strength;
        DBG("New strength: %d", strength);
     }
   else if (!strcmp(prop_name, "Security"))
     {
        const char *s;
        Eldbus_Message_Iter *itr_array;

        DBG("Old security: %u", cs->security);
        cs->security = WIRELESS_NETWORK_SECURITY_NONE;

        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value, "as",
                                                                        &itr_array));
        while (eldbus_message_iter_get_and_next(itr_array, 's', &s))
          cs->security |= str_to_security(s);
        DBG("New security %u", cs->security);
     }
   else if (!strcmp(prop_name, "IPv4"))
     {
        Eldbus_Message_Iter *array, *dict;

        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value, "a{sv}", &array));
        while (eldbus_message_iter_get_and_next(array, 'e', &dict))
          {
             Eldbus_Message_Iter *var;
             const char *name, *val;

             EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(dict, "sv", &name, &var));
             if (!strcmp(name, "Method"))
               {
                  cs->method = WIRELESS_NETWORK_IPV4_METHOD_OFF;
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  if (!strcmp(val, "off"))
                    cs->method = WIRELESS_NETWORK_IPV4_METHOD_OFF;
                  else if (!strcmp(val, "dhcp"))
                    cs->method = WIRELESS_NETWORK_IPV4_METHOD_DHCP;
                  else if (!strcmp(val, "manual"))
                    cs->method = WIRELESS_NETWORK_IPV4_METHOD_MANUAL;
                  else if (!strcmp(val, "fixed"))
                    cs->method = WIRELESS_NETWORK_IPV4_METHOD_FIXED;
               }
             else if (!strcmp(name, "Address"))
               {
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  eina_stringshare_replace(&cs->address, val);
               }
             else if (!strcmp(name, "Netmask"))
               {
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  eina_stringshare_replace(&cs->ip.v4.netmask, val);
               }
             else if (!strcmp(name, "Gateway"))
               {
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  eina_stringshare_replace(&cs->gateway, val);
               }
          }
     }
   else if (!strcmp(prop_name, "IPv6"))
     {
        Eldbus_Message_Iter *array, *dict;

        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value, "a{sv}", &array));
        while (eldbus_message_iter_get_and_next(array, 'e', &dict))
          {
             Eldbus_Message_Iter *var;
             const char *name, *val;

             EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(dict, "sv", &name, &var));
             if (!strcmp(name, "Method"))
               {
                  cs->method = WIRELESS_NETWORK_IPV6_METHOD_OFF;
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  if (!strcmp(val, "auto"))
                    cs->method = WIRELESS_NETWORK_IPV6_METHOD_AUTO;
                  else if (!strcmp(val, "manual"))
                    cs->method = WIRELESS_NETWORK_IPV6_METHOD_MANUAL;
                  else if (!strcmp(val, "6to4"))
                    cs->method = WIRELESS_NETWORK_IPV6_METHOD_6TO4;
                  else if (!strcmp(val, "fixed"))
                    cs->method = WIRELESS_NETWORK_IPV6_METHOD_FIXED;
               }
             else if (!strcmp(name, "Privacy"))
               {
                  cs->ip.v6.privacy = WIRELESS_NETWORK_IPV6_PRIVACY_DISABLED;
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  if (!strcmp(val, "disabled"))
                    cs->ip.v6.privacy = WIRELESS_NETWORK_IPV6_PRIVACY_DISABLED;
                  else if (!strcmp(val, "enabled"))
                    cs->ip.v6.privacy = WIRELESS_NETWORK_IPV6_PRIVACY_ENABLED;
                  else if (!strcmp(val, "preferred"))
                    cs->ip.v6.privacy = WIRELESS_NETWORK_IPV6_PRIVACY_PREFERRED;
               }
             else if (!strcmp(name, "Address"))
               {
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  eina_stringshare_replace(&cs->address, val);
               }
             else if (!strcmp(name, "PrefixLength"))
               {
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  eina_stringshare_replace(&cs->ip.v6.prefixlength, val);
               }
             else if (!strcmp(name, "Gateway"))
               {
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  eina_stringshare_replace(&cs->gateway, val);
               }
          }
     }
   else if (!strcmp(prop_name, "Proxy"))
     {
        Eldbus_Message_Iter *array, *dict;

        EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(value, "a{sv}", &array));
        while (eldbus_message_iter_get_and_next(array, 'e', &dict))
          {
             Eldbus_Message_Iter *var;
             const char *name, *val;

             EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(dict, "sv", &name, &var));
             if (!strcmp(name, "Method"))
               {
                  cs->proxy_type = WIRELESS_PROXY_TYPE_DIRECT;
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  if (!strcmp(val, "manual"))
                    cs->proxy_type = WIRELESS_PROXY_TYPE_MANUAL;
                  else if (!strcmp(val, "auto"))
                    cs->proxy_type = WIRELESS_PROXY_TYPE_AUTO;
               }
             else if (!strcmp(name, "URL"))
               {
                  EINA_SAFETY_ON_FALSE_RETURN(eldbus_message_iter_arguments_get(var, "s", &val));
                  eina_stringshare_replace(&cs->proxy_url, val);
               }
             else if (!strcmp(name, "Servers"))
               _connman_service_parse_stringarray(value, &cs->proxy_servers);
             else if (!strcmp(name, "Excludes"))
               _connman_service_parse_stringarray(value, &cs->proxy_excludes);
          }
     }
   else if (!strcmp(prop_name, "Nameservers"))
     _connman_service_parse_stringarray(value, &cs->name_servers);
   else if (!strcmp(prop_name, "Timeservers"))
     _connman_service_parse_stringarray(value, &cs->time_servers);
   else if (!strcmp(prop_name, "Domains"))
     _connman_service_parse_stringarray(value, &cs->domain_servers);
}

static void
_connman_service_prop_dict_changed(Connman_Service *cs, Eldbus_Message_Iter *props)
{
   Eldbus_Message_Iter *dict;

   while (eldbus_message_iter_get_and_next(props, 'e', &dict))
     {
        char *name;
        Eldbus_Message_Iter *var;

        if (eldbus_message_iter_arguments_get(dict, "sv", &name, &var))
          _connman_service_parse_prop_changed(cs, name, var);
     }
   if (_connman_service_is_connected(cs))
     _connman_update_current_network(cs, cs->type);
}

static void
_connman_service_property(void *data, const Eldbus_Message *msg)
{
   Connman_Service *cs = data;
   Eldbus_Message_Iter *var;
   const char *name;

   if (eldbus_message_arguments_get(msg, "sv", &name, &var))
     _connman_service_parse_prop_changed(cs, name, var);
   if (_connman_service_is_connected(cs))
     _connman_update_current_network(cs, cs->type);
}

static Connman_Service *
_connman_service_new(const char *path, Eldbus_Message_Iter *props)
{
   Connman_Service *cs;
   Eldbus_Object *obj;

   cs = E_NEW(Connman_Service, 1);
   cs->path = eina_stringshare_add(path);

   obj = eldbus_object_get(dbus_conn, CONNMAN_BUS_NAME, path);
   cs->proxy = eldbus_proxy_get(obj, CONNMAN_SERVICE_IFACE);
   cs->handler = eldbus_proxy_signal_handler_add(cs->proxy, "PropertyChanged",
                                  _connman_service_property, cs);

   _connman_service_prop_dict_changed(cs, props);
   connman_services_list[cs->type] = eina_inlist_append(connman_services_list[cs->type], EINA_INLIST_GET(cs));
   eina_hash_add(connman_services[cs->type], cs->path, cs);
   DBG("Added service: %p %s || proxy %p", cs, path, cs->proxy);
   return cs;
}

static void
_connman_manager_agent_register(void *data EINA_UNUSED, const Eldbus_Message *msg EINA_UNUSED, Eldbus_Pending *pending EINA_UNUSED)
{
   /* FIXME: should this do something? */
}

static Eina_Bool
_connman_technology_parse_prop_changed(Connman_Technology *ct, const char *name, Eldbus_Message_Iter *value)
{
   Eina_Bool val;
   const char *str;
   Eina_Bool ret = EINA_FALSE;

   if (!strcmp(name, "Powered"))
     {
        eldbus_message_iter_arguments_get(value, "b", &val);
        val = !!val;
        if (val != ct->powered) ret = EINA_TRUE;
        ct->powered = !!val;
     }
   else if (!strcmp(name, "Connected"))
     {
        eldbus_message_iter_arguments_get(value, "b", &val);
        ct->connected = !!val;
     }
   else if (!strcmp(name, "Tethering"))
     {
        eldbus_message_iter_arguments_get(value, "b", &val);
        ct->tethering = !!val;
     }
   else if (!strcmp(name, "TetheringIdentifier"))
     {
        eldbus_message_iter_arguments_get(value, "b", &str);
        ct->tethering_ssid = eina_stringshare_add(str);
     }
   else if (!strcmp(name, "TetheringPassphrase"))
     {
        eldbus_message_iter_arguments_get(value, "b", &str);
        ct->tethering_passwd = eina_stringshare_add(str);
     }
   return ret;
}

static void
_connman_technology_event_property(void *data, const Eldbus_Message *msg)
{
   Eldbus_Message_Iter *var;
   const char *name;
   Connman_Technology *ct = NULL;
   int i;

   CONNMAN_SERVICE_TYPE_ITER(i)
     if (data == connman_technology[i].proxy)
       {
          ct = &connman_technology[i];
          break;
       }
   if (!ct) return;

   if (!eldbus_message_arguments_get(msg, "sv", &name, &var))
     ERR("Could not parse message %p", msg);
   else if (_connman_technology_parse_prop_changed(ct, name, var))
     _connman_update_enabled_technologies();
}

static Eina_Bool
_connman_manager_parse_prop_changed(const char *name, Eldbus_Message_Iter *value)
{
   if (!strcmp(name, "State"))
     {
        const char *state;

        if (!eldbus_message_iter_arguments_get(value, "s", &state))
          return EINA_FALSE;
        DBG("New state: %s", state);
        //_connman_update_state(str_to_state(state));
     }
   else if (!strcmp(name, "OfflineMode"))
     {
        Eina_Bool offline;
        if (!eldbus_message_iter_arguments_get(value, "b", &offline))
          return EINA_FALSE;
        _connman_update_airplane_mode(offline);
     }
   else
     return EINA_FALSE;

   return EINA_TRUE;
}

static void
_connman_manager_getproperties(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *array, *dict;
   const char *name, *text;

   pending_getproperties_manager = NULL;
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not get properties. %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        ERR("Error getting arguments.");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        const char *key;
        Eldbus_Message_Iter *var;

        if (eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          _connman_manager_parse_prop_changed(key, var);
     }
}

static void
_connman_manager_getservices(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *array, *s;
   const char *name, *text;
   int i;
   Eina_Bool update[CONNMAN_SERVICE_TYPE_LAST] = {0};

   pending_getservices = NULL;
   CONNMAN_SERVICE_TYPE_ITER(i)
     eina_hash_free_buckets(connman_services[i]);
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not get services. %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a(oa{sv})", &array))
     {
        ERR("Error getting array");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'r', &s))
     {
        const char *path;
        Eldbus_Message_Iter *inner_array;
        Connman_Service *cs;

        if (!eldbus_message_iter_arguments_get(s, "oa{sv}", &path, &inner_array))
          continue;

        cs = _connman_service_new(path, inner_array);
        update[cs->type] = 1;
     }
   CONNMAN_SERVICE_TYPE_ITER(i)
     if (update[i]) _connman_update_networks(i);
}

static void
_connman_manager_gettechnologies(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *array, *s;
   const char *name, *text;

   pending_gettechnologies = NULL;
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not get technologies. %s: %s", name, text);
        return;
     }

   if (!eldbus_message_arguments_get(msg, "a(oa{sv})", &array))
     {
        ERR("Error getting array");
        return;
     }

   while (eldbus_message_iter_get_and_next(array, 'r', &s))
     {
        const char *path;
        Eldbus_Message_Iter *inner_array, *dict;
        Connman_Technology *ct = NULL;
        Eldbus_Object *obj;
        int i;
        const char *paths[] =
        {
           CONNMAN_TECHNOLOGY_PATH_ETHERNET,
           CONNMAN_TECHNOLOGY_PATH_WIFI,
           CONNMAN_TECHNOLOGY_PATH_BT,
           CONNMAN_TECHNOLOGY_PATH_CELLULAR,
        };

        if (!eldbus_message_iter_arguments_get(s, "oa{sv}", &path, &inner_array))
          continue;
        CONNMAN_SERVICE_TYPE_ITER(i)
          {
             if (strcmp(path, paths[i])) continue;
             ct = &connman_technology[i];
             ct->type = i;

             obj = eldbus_object_get(dbus_conn, CONNMAN_BUS_NAME, paths[i]);
             ct->proxy = eldbus_proxy_get(obj, CONNMAN_TECHNOLOGY_IFACE);
             signal_handlers = eina_list_append(signal_handlers,
               eldbus_proxy_signal_handler_add(ct->proxy, "PropertyChanged",
                                             _connman_technology_event_property, ct->proxy));
          }
        if (!ct)
          {
             ERR("No handler for technology: %s", path);
             continue;
          }
        while (eldbus_message_iter_get_and_next(inner_array, 'e', &dict))
          {
             Eldbus_Message_Iter *var;

             if (eldbus_message_iter_arguments_get(dict, "sv", &name, &var))
               _connman_technology_parse_prop_changed(ct, name, var);
          }
     }
   /* scan not supported on bluetooth */
   if (connman_technology[CONNMAN_SERVICE_TYPE_BLUETOOTH].proxy)
     pending_getservices = eldbus_proxy_call(proxy_manager, "GetServices", _connman_manager_getservices,
       NULL, -1, "");
   else if (connman_technology[CONNMAN_SERVICE_TYPE_WIFI].proxy)
     eldbus_proxy_call(connman_technology[CONNMAN_SERVICE_TYPE_WIFI].proxy, "Scan", NULL, NULL, -1, "");
   _connman_update_technologies();
   _connman_update_enabled_technologies();
}

static void
_connman_manager_event_services(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message_Iter *changed, *removed, *s;
   const char *path;
   int i;
   Eina_Bool update[CONNMAN_SERVICE_TYPE_LAST] = {0};

   if (pending_getservices) return;

   if (!eldbus_message_arguments_get(msg, "a(oa{sv})ao", &changed, &removed))
     {
        ERR("Error getting arguments");
        return;
     }

   while (eldbus_message_iter_get_and_next(removed, 'o', &path))
     {
        CONNMAN_SERVICE_TYPE_ITER(i)
          {
             if (!eina_hash_del_by_key(connman_services[i], path)) continue;
             DBG("Removed service: %s", path);
             update[i] = 1;
             break;
          }
     }

   while (eldbus_message_iter_get_and_next(changed, 'r', &s))
     {
        Connman_Service *cs;
        Eldbus_Message_Iter *array;
        Eina_Bool found = EINA_FALSE;

        if (!eldbus_message_iter_arguments_get(s, "oa{sv}", &path, &array))
          continue;

        CONNMAN_SERVICE_TYPE_ITER(i)
          {
             cs = eina_hash_find(connman_services[i], path);
             if (!cs) continue;
             _connman_service_prop_dict_changed(cs, array);
             found = update[cs->type] = 1;
             DBG("Changed service: %p %s", cs, path);
             break;
          }
        if (!found)
          {
             cs = _connman_service_new(path, array);
             update[cs->type] = 1;
          }
     }
   CONNMAN_SERVICE_TYPE_ITER(i)
     if (update[i]) _connman_update_networks(i);
}

static void
_connman_manager_event_property(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message_Iter *var;
   const char *name;

   if (pending_getproperties_manager) return;
   if (!eldbus_message_arguments_get(msg, "sv", &name, &var))
     {
        ERR("Could not parse message %p", msg);
        return;
     }

   _connman_manager_parse_prop_changed(name, var);
}

static Eldbus_Message *
_connman_agent_release(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   DBG("Agent released");
   wireless_authenticate_cancel();
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_connman_agent_report_error(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg EINA_UNUSED)
{
#warning FIXME
   return NULL;
}

static Eldbus_Message *
_connman_agent_request_browser(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *path, *url;
   int i;
   Connman_Service *cs;
   Wireless_Network *wn;

   if (!eldbus_message_arguments_get(msg, "ss", &path, &url))
     {
        ERR("Could not parse message %p", msg);
        return NULL;
     }

   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        cs = eina_hash_find(connman_services[i], path);
        if (cs) break;
     }
   if (!cs) return NULL;
   wn = eina_hash_find(connman_services_map[i], &cs);
   EINA_SAFETY_ON_NULL_RETURN_VAL(wn, NULL);
   wireless_authenticate_external(wn, url);
   return NULL;
}

static Eina_Bool
_connman_field_parse_value(Connman_Field *field, const char *key, Eldbus_Message_Iter *value, const char *signature)
{
   if (!strcmp(key, "Type"))
     {
        EINA_SAFETY_ON_FALSE_RETURN_VAL(signature[0] == 's', EINA_FALSE);
        eldbus_message_iter_basic_get(value, &field->type);
        return EINA_TRUE;
     }

   if (!strcmp(key, "Requirement"))
     {
        const char *req;
        const char *types[] =
        {
           [CONNMAN_FIELD_STATE_MANDATORY] = "mandatory",
           [CONNMAN_FIELD_STATE_OPTIONAL] = "optional",
           [CONNMAN_FIELD_STATE_ALTERNATE] = "alternate",
           [CONNMAN_FIELD_STATE_INFO] = "informational",
        };
        int i;
        EINA_SAFETY_ON_FALSE_RETURN_VAL(signature[0] == 's', EINA_FALSE);
        eldbus_message_iter_basic_get(value, &req);
        for (i = 0; i <= CONNMAN_FIELD_STATE_INFO; i++)
          if (!strcmp(req, types[i]))
            {
               field->requirement = i;
               break;
            }
        return EINA_TRUE;
     }

   if (!strcmp(key, "Alternates"))
     {
        EINA_SAFETY_ON_FALSE_RETURN_VAL(signature[0] == 'a', EINA_FALSE);
        /* ignore alternates */
        return EINA_TRUE;
     }

   if (!strcmp(key, "Value"))
     {
        EINA_SAFETY_ON_FALSE_RETURN_VAL(signature[0] == 's', EINA_FALSE);
        eldbus_message_iter_basic_get(value, &field->value);
        return EINA_TRUE;
     }

   DBG("Ignored unknown argument: %s", key);
   return EINA_FALSE;
}

static Eina_Bool
_connman_field_parse(Connman_Field *field, Eldbus_Message_Iter *value, const char *signature EINA_UNUSED)
{
   Eldbus_Message_Iter *array, *dict;

   eldbus_message_iter_arguments_get(value, "a{sv}", &array);
   EINA_SAFETY_ON_NULL_RETURN_VAL(array, EINA_FALSE);

   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        const char *key;
        char *sig2;

        if (!eldbus_message_iter_arguments_get(dict, "sv", &key, &var))
          return EINA_FALSE;
        sig2 = eldbus_message_iter_signature_get(var);
        if (!sig2)
          return EINA_FALSE;

        if (!_connman_field_parse_value(field, key, var, sig2))
          {
             free(sig2);
             return EINA_FALSE;
          }
        free(sig2);
     }

   return EINA_TRUE;
}

static void
_connman_dbus_dict_append_array(Eldbus_Message_Iter *array, const char *key, Eina_Array *val)
{
   Eldbus_Message_Iter *dict, *variant, *array2;
   Eina_Stringshare *str;
   unsigned int i;
   Eina_Array_Iterator it;

   eldbus_message_iter_arguments_append(array, "{sv}", &dict);
   eldbus_message_iter_basic_append(dict, 's', key);
   variant = eldbus_message_iter_container_new(dict, 'v', "as");
   array2 = eldbus_message_iter_container_new(variant, 'a', "s");
   EINA_ARRAY_ITER_NEXT(val, i, str, it)
     eldbus_message_iter_basic_append(array2, 's', str);
   eldbus_message_iter_container_close(variant, array2);
   eldbus_message_iter_container_close(dict, variant);
   eldbus_message_iter_container_close(array, dict);
}

static void
_connman_dbus_dict_append_string(Eldbus_Message_Iter *array, const char *key, const char *val)
{
   Eldbus_Message_Iter *dict, *variant;

   eldbus_message_iter_arguments_append(array, "{sv}", &dict);
   eldbus_message_iter_basic_append(dict, 's', key);
   variant = eldbus_message_iter_container_new(dict, 'v', "s");
   eldbus_message_iter_basic_append(variant, 's', val ?: "");
   eldbus_message_iter_container_close(dict, variant);
   eldbus_message_iter_container_close(array, dict);
}

static void
_connman_dbus_dict_append_bool(Eldbus_Message_Iter *array, const char *key, Eina_Bool val)
{
   Eldbus_Message_Iter *dict, *variant;

   eldbus_message_iter_arguments_append(array, "{sv}", &dict);
   eldbus_message_iter_basic_append(dict, 's', key);
   variant = eldbus_message_iter_container_new(dict, 'v', "b");
   eldbus_message_iter_basic_append(variant, 'b', !!val);
   eldbus_message_iter_container_close(dict, variant);
   eldbus_message_iter_container_close(array, dict);
}

static void
_connman_agent_auth_send(void *data, const Eina_Array *fields)
{
   Eldbus_Message *reply;
   Eldbus_Message_Iter *iter, *array;
   const char *f, *fprev;
   unsigned int i;
   Eina_Array_Iterator it;

   if (!fields)
     {
        reply = eldbus_message_error_new(data,
                                       "net.connman.Agent.Error.Canceled",
                                       "User canceled dialog");
        eldbus_connection_send(dbus_conn, reply, NULL, NULL, -1);
        return;
     }
   reply = eldbus_message_method_return_new(data);
   iter = eldbus_message_iter_get(reply);
   eldbus_message_iter_arguments_append(iter, "a{sv}", &array);

   EINA_ARRAY_ITER_NEXT(fields, i, f, it)
     {
        if (i % 2)
          _connman_dbus_dict_append_string(array, fprev, f);
        else
          fprev = f;
     }
   eldbus_message_iter_container_close(iter, array);

   eldbus_connection_send(dbus_conn, reply, NULL, NULL, -1);
}

static Eldbus_Message *
_connman_agent_request_input(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message_Iter *array, *dict;
   const char *path;
   Eina_Array *arr = NULL;

   if (!eldbus_message_arguments_get(msg, "oa{sv}", &path, &array))
     return eldbus_message_method_return_new(msg);

   /* FIXME: WISPr - net.connman.Agent.Error.LaunchBrowser */
   while (eldbus_message_iter_get_and_next(array, 'e', &dict))
     {
        Eldbus_Message_Iter *var;
        char *signature;
        Connman_Field field = { NULL };

        if (!eldbus_message_iter_arguments_get(dict, "sv", &field.name, &var))
          goto err;
        signature = eldbus_message_iter_signature_get(var);
        if (!signature) goto err;

        if (!_connman_field_parse(&field, var, signature))
          {
             free(signature);
             goto err;
          }
        free(signature);

        DBG("AGENT Got field:\n"
            "\tName: %s\n"
            "\tType: %s\n"
            "\tRequirement: %d\n"
            "\tAlternates: (omit array)\n"
            "\tValue: %s",
            field.name, field.type, field.requirement, field.value);

        if (field.requirement != CONNMAN_FIELD_STATE_MANDATORY) continue;
        if (!arr) arr = eina_array_new(1);
        eina_array_push(arr, eina_stringshare_add(field.name));
     }
   wireless_authenticate(arr, _connman_agent_auth_send, eldbus_message_ref((Eldbus_Message *)msg));
   array_clear(arr);
   return NULL;

err:
   eina_array_free(arr);
   WRN("Failed to parse msg");
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_connman_agent_cancel(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   DBG("Agent canceled");
   wireless_authenticate_cancel();

   return reply;
}

static const Eldbus_Method methods[] = {
   { "Release", NULL, NULL, _connman_agent_release, 0 },
   {
    "ReportError", ELDBUS_ARGS({"o", "service"}, {"s", "error"}), NULL,
    _connman_agent_report_error, 0
   },
   //{
    //"ReportPeerError", ELDBUS_ARGS({"o", "peer"}, {"s", "error"}), NULL,
    //_connman_agent_report_peer_error, 0
   //},
   {
    "RequestBrowser", ELDBUS_ARGS({"o", "service"}, {"s", "url"}), NULL,
     _connman_agent_request_browser, 0
   },
   {
    "RequestInput", ELDBUS_ARGS({"o", "service"}, {"a{sv}", "fields"}),
    ELDBUS_ARGS({"a{sv}", ""}), _connman_agent_request_input, 0
   },
   //{
    //"RequestPeerAuthorization", ELDBUS_ARGS({"o", "peer"}, {"a{sv}", "fields"}),
    //ELDBUS_ARGS({"a{sv}", ""}), _connman_agent_request_peer_auth, 0
   //},
   { "Cancel", NULL, NULL, _connman_agent_cancel, 0 },
   { NULL, NULL, NULL, NULL, 0 }
};

static const Eldbus_Service_Interface_Desc desc = {
   CONNMAN_AGENT_IFACE, methods, NULL, NULL, NULL, NULL
};

static void
_connman_start(void)
{
   Eldbus_Object *obj;
   int i;

   CONNMAN_SERVICE_TYPE_ITER(i)
     connman_services[i] = eina_hash_string_superfast_new((Eina_Free_Cb)_connman_service_free);

   obj = eldbus_object_get(dbus_conn, CONNMAN_BUS_NAME, "/");
   proxy_manager = eldbus_proxy_get(obj, CONNMAN_MANAGER_IFACE);

   signal_handlers = eina_list_append(signal_handlers,
     eldbus_proxy_signal_handler_add(proxy_manager, "PropertyChanged",
                                   _connman_manager_event_property, NULL));
   signal_handlers = eina_list_append(signal_handlers,
     eldbus_proxy_signal_handler_add(proxy_manager, "ServicesChanged",
                                   _connman_manager_event_services, NULL));

   pending_gettechnologies = eldbus_proxy_call(proxy_manager, "GetTechnologies", _connman_manager_gettechnologies,
     NULL, -1, "");
   pending_getproperties_manager = eldbus_proxy_call(proxy_manager, "GetProperties", _connman_manager_getproperties,
     NULL, -1, "");

   agent_iface = eldbus_service_interface_register(dbus_conn, CONNMAN_AGENT_PATH, &desc);
   eldbus_proxy_call(proxy_manager, "RegisterAgent",
                       _connman_manager_agent_register, NULL, -1, "o", CONNMAN_AGENT_PATH);
}

static void
_connman_end(void)
{
   int i;

   if (!proxy_manager) return;
   eldbus_proxy_call(proxy_manager, "UnregisterAgent", NULL, NULL, -1, "o", CONNMAN_AGENT_PATH);

   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        E_FREE_FUNC(connman_services[i], eina_hash_free);
        if (!connman_technology[i].proxy) continue;
        E_FREE_FUNC(connman_technology[i].proxy, _eldbus_proxy_del);
     }
   E_FREE_FUNC(pending_getservices, eldbus_pending_cancel);
   E_FREE_FUNC(pending_getproperties_manager, eldbus_pending_cancel);
   signal_handlers = eina_list_free(signal_handlers);

   E_FREE_FUNC(proxy_manager, _eldbus_proxy_del);
   E_FREE_FUNC(agent_iface, eldbus_service_object_unregister);
}

static void
_connman_name_owner_changed(void *data EINA_UNUSED, const char *bus EINA_UNUSED, const char *from EINA_UNUSED, const char *to)
{
   if (to[0])
     _connman_start();
   else
     _connman_end();
}

EINTERN void
connman_init(void)
{
   int i;

   if (_connman_log_dom > -1) return;
   CONNMAN_SERVICE_TYPE_ITER(i)
     connman_technology[i].type = -1;
   eldbus_name_owner_changed_callback_add(dbus_conn, CONNMAN_BUS_NAME,
                                         _connman_name_owner_changed,
                                         NULL, EINA_TRUE);
   _connman_log_dom = eina_log_domain_register("wireless.connman", EINA_COLOR_ORANGE);
}

EINTERN void
connman_shutdown(void)
{
   int i;
   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        E_FREE_FUNC(connman_services_map[i], eina_hash_free);
        E_FREE(connman_current_connection[i]);
        connman_current_service[i] = NULL;
     }
   _connman_end();
   eldbus_name_owner_changed_callback_del(dbus_conn, CONNMAN_BUS_NAME, _connman_name_owner_changed, NULL);
   eina_log_domain_unregister(_connman_log_dom);
   _connman_log_dom = -1;
}

EINTERN void
connman_technology_enabled_set(Wireless_Service_Type type, Eina_Bool state)
{
   Eldbus_Message_Iter *main_iter, *var;
   Eldbus_Message *msg;

   EINA_SAFETY_ON_NULL_RETURN(connman_technology[type].proxy);
   msg = eldbus_proxy_method_call_new(connman_technology[type].proxy, "SetProperty");
   main_iter = eldbus_message_iter_get(msg);
   eldbus_message_iter_basic_append(main_iter, 's', "Powered");
   var = eldbus_message_iter_container_new(main_iter, 'v', "b");
   eldbus_message_iter_basic_append(var, 'b', state);
   eldbus_message_iter_container_close(main_iter, var);

   eldbus_proxy_send(connman_technology[type].proxy, msg, NULL, NULL, -1);
}

static void
_connman_service_edit_timeservers_cb(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;
   /* FIXME */
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not set properties. %s: %s", name, text);
     }
}

static void
_connman_service_edit_nameservers_cb(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;
   /* FIXME */
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not set properties. %s: %s", name, text);
     }
}

static void
_connman_service_edit_domains_cb(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;
   /* FIXME */
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not set properties. %s: %s", name, text);
     }
}

static void
_connman_service_edit_proxy_cb(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;
   /* FIXME */
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not set properties. %s: %s", name, text);
     }
}

static void
_connman_service_edit_cb(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;
   /* FIXME */
   if (eldbus_message_error_get(msg, &name, &text))
     {
        ERR("Could not set properties. %s: %s", name, text);
     }
}

static void
_connman_service_remove_cb(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;

   if (eldbus_message_error_get(msg, &name, &text))
     ERR("Could not remove service. %s: %s", name, text);
}

EINTERN void
connman_service_edit(const char *path, Wireless_Connection *wc)
{
   int i;
   Eldbus_Message *msg;
   Eldbus_Message_Iter *iter, *variant, *array;
   Connman_Service *cs = NULL;
   const char *prop[] =
   {
      "IPv4.Configuration",
      "IPv6.Configuration",
   };
   const char *method[] =
   {
      "off",
      "manual",
      "dhcp",
      "fixed",
   };

   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        cs = eina_hash_find(connman_services[i], path);
        if (cs) break;
     }
   EINA_SAFETY_ON_NULL_RETURN(cs);

   msg = eldbus_proxy_method_call_new(cs->proxy, "SetProperty");
   iter = eldbus_message_iter_get(msg);
   eldbus_message_iter_basic_append(iter, 's', prop[cs->ipv6]);
   variant = eldbus_message_iter_container_new(iter, 'v', "a{sv}");
   eldbus_message_iter_arguments_append(variant, "a{sv}", &array);
   _connman_dbus_dict_append_string(array, "Method", method[wc->method]);
   _connman_dbus_dict_append_string(array, "Address", wc->address);
   _connman_dbus_dict_append_string(array, "Gateway", wc->gateway);
   if (wc->ipv6)
     {
        const char *privacy[] =
        {
           [WIRELESS_NETWORK_IPV6_PRIVACY_DISABLED] = "disabled",
           [WIRELESS_NETWORK_IPV6_PRIVACY_ENABLED] = "enabled",
           [WIRELESS_NETWORK_IPV6_PRIVACY_PREFERRED] = "preferred",
        };
        _connman_dbus_dict_append_string(array, "PrefixLength", wc->ip.v6.prefixlength);
        if (wc->method == WIRELESS_NETWORK_IPV6_METHOD_AUTO)
          _connman_dbus_dict_append_string(array, "Privacy", privacy[wc->ip.v6.privacy]);
     }
   else
     _connman_dbus_dict_append_string(array, "Netmask", wc->ip.v4.netmask);
   eldbus_message_iter_container_close(variant, array);
   eldbus_message_iter_container_close(iter, variant);

   eldbus_proxy_send(cs->proxy, msg, _connman_service_edit_cb, NULL, -1);
}

EINTERN void
connman_service_edit_proxy(const char *path, Wireless_Connection *wc)
{
   int i;
   Eldbus_Message *msg;
   Eldbus_Message_Iter *iter, *variant, *array;
   Connman_Service *cs = NULL;
   const char *method[] =
   {
      "direct",
      "manual",
      "auto",
   };

   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        cs = eina_hash_find(connman_services[i], path);
        if (cs) break;
     }
   EINA_SAFETY_ON_NULL_RETURN(cs);

   msg = eldbus_proxy_method_call_new(cs->proxy, "SetProperty");
   iter = eldbus_message_iter_get(msg);
   eldbus_message_iter_basic_append(iter, 's', "Proxy");
   variant = eldbus_message_iter_container_new(iter, 'v', "a{sv}");
   eldbus_message_iter_arguments_append(variant, "a{sv}", &array);
   _connman_dbus_dict_append_string(array, "Method", method[wc->proxy_type]);
   switch (wc->proxy_type)
     {
      case WIRELESS_PROXY_TYPE_DIRECT: break;
      case WIRELESS_PROXY_TYPE_MANUAL:
        _connman_dbus_dict_append_array(array, "Servers", wc->proxy_servers);
        _connman_dbus_dict_append_array(array, "Excludes", wc->proxy_excludes);
        break;
      case WIRELESS_PROXY_TYPE_AUTO:
        _connman_dbus_dict_append_string(array, "Address", wc->proxy_url);
        break;
     }
   eldbus_message_iter_container_close(variant, array);
   eldbus_message_iter_container_close(iter, variant);

   eldbus_proxy_send(cs->proxy, msg, _connman_service_edit_proxy_cb, NULL, -1);
}

EINTERN void
connman_service_edit_domains(const char *path, Wireless_Connection *wc)
{
   int i;
   Eldbus_Message *msg;
   Eldbus_Message_Iter *iter;
   Connman_Service *cs = NULL;

   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        cs = eina_hash_find(connman_services[i], path);
        if (cs) break;
     }
   EINA_SAFETY_ON_NULL_RETURN(cs);

   msg = eldbus_proxy_method_call_new(cs->proxy, "SetProperty");
   iter = eldbus_message_iter_get(msg);
   _connman_dbus_dict_append_array(iter, "Domains.Configuration", wc->domain_servers);

   eldbus_proxy_send(cs->proxy, msg, _connman_service_edit_domains_cb, NULL, -1);
}

EINTERN void
connman_service_edit_nameservers(const char *path, Wireless_Connection *wc)
{
   int i;
   Eldbus_Message *msg;
   Eldbus_Message_Iter *iter;
   Connman_Service *cs = NULL;

   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        cs = eina_hash_find(connman_services[i], path);
        if (cs) break;
     }
   EINA_SAFETY_ON_NULL_RETURN(cs);

   msg = eldbus_proxy_method_call_new(cs->proxy, "SetProperty");
   iter = eldbus_message_iter_get(msg);
   _connman_dbus_dict_append_array(iter, "Nameservers.Configuration", wc->name_servers);

   eldbus_proxy_send(cs->proxy, msg, _connman_service_edit_nameservers_cb, NULL, -1);
}

EINTERN void
connman_service_edit_timeservers(const char *path, Wireless_Connection *wc)
{
   int i;
   Eldbus_Message *msg;
   Eldbus_Message_Iter *iter;
   Connman_Service *cs = NULL;

   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        cs = eina_hash_find(connman_services[i], path);
        if (cs) break;
     }
   EINA_SAFETY_ON_NULL_RETURN(cs);

   msg = eldbus_proxy_method_call_new(cs->proxy, "SetProperty");
   iter = eldbus_message_iter_get(msg);
   _connman_dbus_dict_append_array(iter, "Timeservers.Configuration", wc->time_servers);

   eldbus_proxy_send(cs->proxy, msg, _connman_service_edit_timeservers_cb, NULL, -1);
}

EINTERN void
connman_service_remove(const char *path)
{
   int i;
   Eldbus_Message *msg;
   Connman_Service *cs = NULL;

   CONNMAN_SERVICE_TYPE_ITER(i)
     {
        cs = eina_hash_find(connman_services[i], path);
        if (cs) break;
     }
   EINA_SAFETY_ON_NULL_RETURN(cs);

   msg = eldbus_proxy_method_call_new(cs->proxy, "Remove");
   eldbus_proxy_send(cs->proxy, msg, _connman_service_remove_cb, NULL, -1);
}

EINTERN void
connman_airplane_mode_set(Eina_Bool set)
{
   Eldbus_Message *msg;
   Eldbus_Message_Iter *iter;

   msg = eldbus_proxy_method_call_new(proxy_manager, "SetProperty");
   iter = eldbus_message_iter_get(msg);
   _connman_dbus_dict_append_bool(iter, "OfflineMode", set);
   eldbus_proxy_send(proxy_manager, msg, NULL, NULL, -1);
}
