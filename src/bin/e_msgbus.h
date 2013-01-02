#ifdef E_TYPEDEFS

typedef struct _E_Msgbus_Data E_Msgbus_Data;

#else
#ifndef E_MSGBUS_H
#define E_MSGBUS_H

/* This is the dbus subsystem, but edbus namespace is taken by edbus */

struct _E_Msgbus_Data
{
   EDBus_Connection *conn;
   EDBus_Service_Interface *iface;
};

EINTERN int e_msgbus_init(void);
EINTERN int e_msgbus_shutdown(void);
EAPI EDBus_Service_Interface *e_msgbus_interface_attach(const EDBus_Service_Interface_Desc *desc);

#endif
#endif
