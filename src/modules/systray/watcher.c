#include <Eina.h>
#include <Eldbus.h>
#include <Ecore.h>

#define WATCHER_BUS "org.kde.StatusNotifierWatcher"
#define PATH "/StatusNotifierWatcher"
#define IFACE "org.kde.StatusNotifierWatcher"
#define PROTOCOL_VERSION 1

#define ERROR_HOST_ALREADY_REGISTERED "org.kde.StatusNotifierWatcher.Host.AlreadyRegistered"
#define ERROR_ITEM_ALREADY_REGISTERED "org.kde.StatusNotifierWatcher.Item.AlreadyRegistered"

static Eldbus_Connection *conn = NULL;
static Eldbus_Service_Interface *iface = NULL;
static Eina_List *items;
static Eina_List *hosts;
static Eina_Bool started;
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
   const char *svc, *service = data;

   if (strcmp(new_id, ""))
     return;

   svc = strchr(service, '/') + 1;

   printf("Item gone!\n");

   eldbus_service_signal_emit(iface, ITEM_UNREGISTERED, svc);
   items = eina_list_remove(items, service);
   eldbus_name_owner_changed_callback_del(conn, bus, item_name_monitor_cb, service);
   eina_stringshare_del(service);
}

static Eldbus_Message *
register_item_cb(const Eldbus_Service_Interface *s_iface, const Eldbus_Message *msg)
{
   const char *service, *svc;
   char buf[1024];
   Eina_Bool stupid;

   printf("Item registered\n");

   if (!eldbus_message_arguments_get(msg, "s", &service))
     return NULL;
   svc = service;
   /* if stupid, this app does not conform to http://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/
    * and is expecting to have its send id watched as it is not providing a real bus name here */
   stupid = !!strncmp(svc, "org.", 4);

   snprintf(buf, sizeof(buf), "%s/%s", stupid ? eldbus_message_sender_get(msg) : svc, stupid ? svc : "StatusNotifierItem");
   service = eina_stringshare_add(buf);
   if (eina_list_data_find(items, service))
     {
        eina_stringshare_del(service);
        return eldbus_message_error_new(msg, ERROR_ITEM_ALREADY_REGISTERED, "");
     }

   items = eina_list_append(items, service);
   eldbus_service_signal_emit(s_iface, ITEM_REGISTERED, svc);
   eldbus_name_owner_changed_callback_add(conn, stupid ? eldbus_message_sender_get(msg) : svc,
                                         item_name_monitor_cb, service,
                                         EINA_FALSE);

   return eldbus_message_method_return_new(msg);
}

static void
host_name_monitor_cb(void *data EINA_UNUSED, const char *bus, const char *old_id EINA_UNUSED, const char *new_id)
{
   if (strcmp(new_id, ""))
     return;

   printf("Host gone!\n");

   eldbus_service_signal_emit(iface, HOST_UNREGISTERED);
   eldbus_name_owner_changed_callback_del(conn, bus, host_name_monitor_cb, NULL);
}

static Eldbus_Message *
register_host_cb(const Eldbus_Service_Interface *s_iface, const Eldbus_Message *msg)
{
   const char *tmpservice = NULL, *service;

   printf("Host registed!\n");

   if (!eldbus_message_arguments_get(msg, "s", &tmpservice))
     return NULL;

   service = eina_stringshare_add(tmpservice);
   if (eina_list_data_find(hosts, service))
     {
        eina_stringshare_del(service);
        return eldbus_message_error_new(msg, ERROR_HOST_ALREADY_REGISTERED, "");
     }

   hosts = eina_list_append(hosts, service);
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
          {
             eldbus_message_iter_arguments_append(array, "s", service);
             printf("Add %s\n", service);
          }
        eldbus_message_iter_container_close(iter, array);
     }
   else if (!strcmp(propname, "IsStatusNotifierHostRegistered"))
     eldbus_message_iter_arguments_append(iter, "b", EINA_TRUE);
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
systray_notifier_dbus_watcher_start(void)
{
   started = EINA_TRUE;
   iface = eldbus_service_interface_register(conn, PATH, &iface_desc);
}

void
systray_notifier_dbus_watcher_stop(void)
{
   const char *txt;

   if (!started) return;

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
}

static void
name_request_cb(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   const char *error, *error_msg;
   unsigned flag;
   Eldbus_Object *obj;

   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        return;
     }

   if (!eldbus_message_arguments_get(msg, "u", &flag))
     {
        return;
     }

   if (flag == ELDBUS_NAME_REQUEST_REPLY_PRIMARY_OWNER)
     {
        systray_notifier_dbus_watcher_start();
        return;
     }
   else
     {
        //there is allready a daemon
        printf("There is allready a daemon");
        ecore_main_loop_quit();
        return;
     }
}

int main(int argc, char **argv)
{
   eina_init();
   ecore_init();
   eldbus_init();

   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);

   eldbus_name_request(conn, WATCHER_BUS, ELDBUS_NAME_REQUEST_FLAG_REPLACE_EXISTING,
                          name_request_cb, NULL);

   ecore_main_loop_begin();

   systray_notifier_dbus_watcher_stop();

   eldbus_connection_unref(conn);

   eldbus_shutdown();
   ecore_shutdown();
   eina_shutdown();
   return 1;
}