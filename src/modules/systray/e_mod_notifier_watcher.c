#include "e_mod_notifier_host_private.h"

#define PATH "/StatusNotifierWatcher"
#define IFACE "org.kde.StatusNotifierWatcher"
#define PROTOCOL_VERSION 1

#define ERROR_HOST_ALREADY_REGISTERED "org.kde.StatusNotifierWatcher.Host.AlreadyRegistered"
#define ERROR_ITEM_ALREADY_REGISTERED "org.kde.StatusNotifierWatcher.Item.AlreadyRegistered"

static Eldbus_Connection *conn = NULL;
static Eldbus_Service_Interface *iface = NULL;
static Eina_List *items;
static const char *host_service = NULL;
static E_Notifier_Watcher_Item_Registered_Cb registered_cb;
static E_Notifier_Watcher_Item_Unregistered_Cb unregistered_cb;
static void *user_data;

enum
{
   ITEM_REGISTERED = 0,
   ITEM_UNREGISTERED,
   HOST_REGISTERED,
   HOST_UNREGISTERED
};

static void
item_name_monitor_cb(void *data, const char *bus, const char *old_id EINA_UNUSED, const char *new_id)
{
   const char *service = data;

   if (strcmp(new_id, ""))
     return;

   eldbus_service_signal_emit(iface, ITEM_UNREGISTERED, service);
   items = eina_list_remove(items, service);
   if (unregistered_cb)
     unregistered_cb(user_data, service);
   eina_stringshare_del(service);
   eldbus_name_owner_changed_callback_del(conn, bus, item_name_monitor_cb, service);
}

static Eldbus_Message *
register_item_cb(const Eldbus_Service_Interface *s_iface, const Eldbus_Message *msg)
{
   const char *service;
   char buf[1024];

   if (!eldbus_message_arguments_get(msg, "s", &service))
     return NULL;

   snprintf(buf, sizeof(buf), "%s%s", eldbus_message_sender_get(msg), service);
   service = eina_stringshare_add(buf);
   if (eina_list_data_find(items, service))
     {
        eina_stringshare_del(service);
        return eldbus_message_error_new(msg, ERROR_ITEM_ALREADY_REGISTERED, "");
     }

   items = eina_list_append(items, service);
   eldbus_service_signal_emit(s_iface, ITEM_REGISTERED, service);
   eldbus_name_owner_changed_callback_add(conn, eldbus_message_sender_get(msg),
                                         item_name_monitor_cb, service,
                                         EINA_FALSE);

   if (registered_cb)
     registered_cb(user_data, service);
   return eldbus_message_method_return_new(msg);
}

static void
host_name_monitor_cb(void *data EINA_UNUSED, const char *bus, const char *old_id EINA_UNUSED, const char *new_id)
{
   if (strcmp(new_id, ""))
     return;

   eldbus_service_signal_emit(iface, HOST_UNREGISTERED);
   eina_stringshare_del(host_service);
   host_service = NULL;
   eldbus_name_owner_changed_callback_del(conn, bus, host_name_monitor_cb, NULL);
}

static Eldbus_Message *
register_host_cb(const Eldbus_Service_Interface *s_iface, const Eldbus_Message *msg)
{
   if (host_service)
     return eldbus_message_error_new(msg, ERROR_HOST_ALREADY_REGISTERED, "");

   if (!eldbus_message_arguments_get(msg, "s", &host_service))
     return NULL;

   host_service = eina_stringshare_add(host_service);
   eldbus_service_signal_emit(s_iface, HOST_REGISTERED);
   eldbus_name_owner_changed_callback_add(conn, eldbus_message_sender_get(msg),
                                         host_name_monitor_cb, NULL, EINA_FALSE);
   return eldbus_message_method_return_new(msg);
}

static Eina_Bool
properties_get(const Eldbus_Service_Interface *s_iface EINA_UNUSED, const char *propname, Eldbus_Message_Iter *iter, const Eldbus_Message *request_msg EINA_UNUSED, Eldbus_Message **error EINA_UNUSED)
{
   if (!strcmp(propname, "ProtocolVersion"))
     eldbus_message_iter_basic_append(iter, 'i', PROTOCOL_VERSION);
   else if (!strcmp(propname, "RegisteredStatusNotifierItems"))
     {
        Eldbus_Message_Iter *array;
        Eina_List *l;
        const char *service;

        eldbus_message_iter_arguments_append(iter, "as", &array);
        EINA_LIST_FOREACH(items, l, service)
          eldbus_message_iter_arguments_append(array, "s", service);
        eldbus_message_iter_container_close(iter, array);
     }
   else if (!strcmp(propname, "IsStatusNotifierHostRegistered"))
     eldbus_message_iter_arguments_append(iter, "b", host_service ? EINA_TRUE : EINA_FALSE);
   return EINA_TRUE;
}

static const Eldbus_Property properties[] =
{
   { "RegisteredStatusNotifierItems", "as", NULL, NULL, 0 },
   { "IsStatusNotifierHostRegistered", "b", NULL, NULL, 0 },
   { "ProtocolVersion", "i", NULL, NULL, 0 },
   { NULL, NULL, NULL, NULL, 0 }
};

static const Eldbus_Signal signals[] = {
   { "StatusNotifierItemRegistered", ELDBUS_ARGS({"s", "service"}), 0 },
   { "StatusNotifierItemUnregistered", ELDBUS_ARGS({"s", "service"}), 0 },
   { "StatusNotifierHostRegistered", NULL, 0 },
   { "StatusNotifierHostUnregistered", NULL, 0 },
   { NULL, NULL, 0 }
};

static const Eldbus_Method methods[] =
{
   { "RegisterStatusNotifierItem", ELDBUS_ARGS({"s", "service"}), NULL,
      register_item_cb, 0 },
   { "RegisterStatusNotifierHost", ELDBUS_ARGS({"s", "service"}), NULL,
      register_host_cb, 0 },
   { NULL, NULL, NULL, NULL, 0 }
};

static const Eldbus_Service_Interface_Desc iface_desc = {
   IFACE, methods, signals, properties, properties_get, NULL
};

void
systray_notifier_dbus_watcher_start(Eldbus_Connection *connection, E_Notifier_Watcher_Item_Registered_Cb registered, E_Notifier_Watcher_Item_Unregistered_Cb unregistered, const void *data)
{
   EINA_SAFETY_ON_TRUE_RETURN(!!conn);
   conn = connection;
   iface = eldbus_service_interface_register(conn, PATH, &iface_desc);
   registered_cb = registered;
   unregistered_cb = unregistered;
   user_data = (void *)data;
   host_service = eina_stringshare_add("internal");
}

void
systray_notifier_dbus_watcher_stop(void)
{
   const char *txt;

   eldbus_service_interface_unregister(iface);
   EINA_LIST_FREE(items, txt)
     {
        char *bus;
        int i;

        for (i = 0; txt[i] != '/'; i++);
        i++;
        bus = malloc(sizeof(char) * i);
        snprintf(bus, i, "%s", txt);
        eldbus_name_owner_changed_callback_del(conn, bus, item_name_monitor_cb, txt);
        free(bus);
        eina_stringshare_del(txt);
     }
   if (host_service)
     eina_stringshare_del(host_service);
   conn = NULL;
}
