#ifndef E_WIRELESS_H
# define E_WIRELESS_H

#include "e.h"

typedef enum
{
   WIRELESS_SERVICE_TYPE_NONE = -1,
   WIRELESS_SERVICE_TYPE_ETHERNET,
   WIRELESS_SERVICE_TYPE_WIFI,
   WIRELESS_SERVICE_TYPE_BLUETOOTH,
   WIRELESS_SERVICE_TYPE_CELLULAR,
   WIRELESS_SERVICE_TYPE_LAST,
} Wireless_Service_Type;

typedef enum
{
   WIRELESS_NETWORK_STATE_NONE,
   WIRELESS_NETWORK_STATE_CONFIGURING,
   WIRELESS_NETWORK_STATE_CONNECTED,
   WIRELESS_NETWORK_STATE_ONLINE,
   WIRELESS_NETWORK_STATE_FAILURE,
} Wireless_Network_State;

typedef enum
{
   WIRELESS_NETWORK_SECURITY_NONE = 0,
   WIRELESS_NETWORK_SECURITY_WEP = (1 << 0),
   WIRELESS_NETWORK_SECURITY_PSK = (1 << 1),
   WIRELESS_NETWORK_SECURITY_IEEE8021X = (1 << 2),
   WIRELESS_NETWORK_SECURITY_WPS = (1 << 3),
} Wireless_Network_Security;

typedef enum
{
   WIRELESS_NETWORK_IPV4_METHOD_OFF,
   WIRELESS_NETWORK_IPV4_METHOD_MANUAL,
   WIRELESS_NETWORK_IPV4_METHOD_DHCP,
   WIRELESS_NETWORK_IPV4_METHOD_FIXED,
} Wireless_Network_IPv4_Method;

typedef enum
{
   WIRELESS_NETWORK_IPV6_METHOD_OFF,
   WIRELESS_NETWORK_IPV6_METHOD_MANUAL,
   WIRELESS_NETWORK_IPV6_METHOD_AUTO,
   WIRELESS_NETWORK_IPV6_METHOD_6TO4,
   WIRELESS_NETWORK_IPV6_METHOD_FIXED,
} Wireless_Network_IPv6_Method;

typedef enum
{
   WIRELESS_NETWORK_IPV6_PRIVACY_DISABLED,
   WIRELESS_NETWORK_IPV6_PRIVACY_ENABLED,
   WIRELESS_NETWORK_IPV6_PRIVACY_PREFERRED,
} Wireless_Network_IPv6_Privacy;

typedef enum
{
   WIRELESS_PROXY_TYPE_DIRECT,
   WIRELESS_PROXY_TYPE_MANUAL,
   WIRELESS_PROXY_TYPE_AUTO,
} Wireless_Proxy_Type;

typedef struct Wireless_Network Wireless_Network;

typedef Eina_Bool (*Wireless_Network_Connect_Cb)(Wireless_Network *);

struct Wireless_Network
{
   Eina_Stringshare *path;//dbus path
   Eina_Stringshare *name;
   Wireless_Network_Security security;
   Wireless_Network_State state;
   Wireless_Service_Type type;
   uint8_t strength;

   Wireless_Network_Connect_Cb connect_cb;
};

typedef struct Wireless_Connection
{
   Wireless_Network *wn;
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

   Wireless_Proxy_Type proxy_type;
   Eina_Stringshare *proxy_url;
   Eina_Array *proxy_servers;
   Eina_Array *proxy_excludes;
   Eina_Bool ipv6 : 1;
   Eina_Bool favorite : 1;
} Wireless_Connection;

typedef void (*Wireless_Auth_Cb)(void *data, const Eina_Array *fields);

extern Eldbus_Connection *dbus_conn;

EINTERN void wireless_service_type_available_set(Eina_Bool *avail);
EINTERN void wireless_service_type_enabled_set(Eina_Bool *enabled);
EINTERN void wireless_wifi_current_networks_set(Wireless_Connection **current);
EINTERN Eina_Array *wireless_networks_set(Eina_Array *networks);
EINTERN void wireless_airplane_mode_set(Eina_Bool enabled);
EINTERN void wireless_authenticate(const Eina_Array *fields, Wireless_Auth_Cb cb, void *data);
EINTERN void wireless_authenticate_cancel(void);
EINTERN void wireless_authenticate_external(Wireless_Network *wn, const char *url);

static inline void
array_clear(Eina_Array *arr)
{
   if (arr)
     while (eina_array_count(arr))
       eina_stringshare_del(eina_array_pop(arr));
   eina_array_free(arr);
}

#endif
