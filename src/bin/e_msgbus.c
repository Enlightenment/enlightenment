#include "e.h"

///////////////////////////////////////////////////////////////////////////
#define E_BUS   "org.enlightenment.wm.service"
#define E_IFACE "org.enlightenment.wm.Core"
#define E_PATH  "/org/enlightenment/wm/RemoteObject"
static void            _e_msgbus_core_request_name_cb(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending);
static Eldbus_Message *_e_msgbus_core_version_cb(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_core_restart_cb(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_core_shutdown_cb(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg);

static const Eldbus_Method _e_core_methods[] =
{
   { "Version", NULL, ELDBUS_ARGS({"s", "version"}), _e_msgbus_core_version_cb, 0 },
   { "Restart", NULL, NULL, _e_msgbus_core_restart_cb, 0 },
   { "Shutdown", NULL, NULL, _e_msgbus_core_shutdown_cb, 0 },
   { NULL, NULL, NULL, NULL, 0}
};

static const Eldbus_Service_Interface_Desc _e_core_desc = {
   E_IFACE, _e_core_methods, NULL, NULL, NULL, NULL
};
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
#define SCREENSAVER_BUS   "org.freedesktop.ScreenSaver"
#define SCREENSAVER_IFACE "org.freedesktop.ScreenSaver"
#define SCREENSAVER_PATH  "/org/freedesktop/ScreenSaver"
#define SCREENSAVER_PATH2 "/ScreenSaver"
static void            _e_msgbus_screensaver_request_name_cb(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending);
static void            _e_msgbus_screensaver_inhibit_free(E_Msgbus_Data_Screensaver_Inhibit *inhibit);
static void            _e_msgbus_screensaver_owner_change_cb(void *data EINA_UNUSED, const char *bus, const char *old_id, const char *new_id);
static Eldbus_Message *_e_msgbus_screensaver_inhibit_cb(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_screensaver_uninhibit_cb(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_screensaver_getactive_cb(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg);

static const Eldbus_Method _screensaver_core_methods[] =
{
   { "Inhibit", ELDBUS_ARGS({"s", "application_name"}, {"s", "reason_for_inhibit"}), ELDBUS_ARGS({"u", "cookie"}), _e_msgbus_screensaver_inhibit_cb, 0 },
   { "UnInhibit", ELDBUS_ARGS({"u", "cookie"}), NULL, _e_msgbus_screensaver_uninhibit_cb, 0 },
   { "GetActive", NULL, ELDBUS_ARGS({"b", "active"}), _e_msgbus_screensaver_getactive_cb, 0 },
   { NULL, NULL, NULL, NULL, 0}
};

static const Eldbus_Service_Interface_Desc _screensaver_core_desc = {
   SCREENSAVER_IFACE, _screensaver_core_methods, NULL, NULL, NULL, NULL
};
///////////////////////////////////////////////////////////////////////////

E_API E_Msgbus_Data *e_msgbus_data = NULL;

/* externally accessible functions */
EINTERN int
e_msgbus_init(void)
{
   e_msgbus_data = E_NEW(E_Msgbus_Data, 1);

   eldbus_init();

   e_msgbus_data->conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);
   if (!e_msgbus_data->conn)
     {
        WRN("Cannot get ELDBUS_CONNECTION_TYPE_SESSION");
        return 0;
     }
   e_msgbus_data->e_iface = eldbus_service_interface_register
     (e_msgbus_data->conn, E_PATH, &_e_core_desc);
   eldbus_name_request(e_msgbus_data->conn, E_BUS, 0,
                       _e_msgbus_core_request_name_cb, NULL);

   e_msgbus_data->screensaver_iface = eldbus_service_interface_register
     (e_msgbus_data->conn, SCREENSAVER_PATH, &_screensaver_core_desc);
   e_msgbus_data->screensaver_iface2 = eldbus_service_interface_register
     (e_msgbus_data->conn, SCREENSAVER_PATH2, &_screensaver_core_desc);
   eldbus_name_request(e_msgbus_data->conn, SCREENSAVER_BUS, 0,
                       _e_msgbus_screensaver_request_name_cb, NULL);
   return 1;
}

EINTERN int
e_msgbus_shutdown(void)
{
   E_Msgbus_Data_Screensaver_Inhibit *inhibit;

   if (e_msgbus_data->screensaver_iface2)
     eldbus_service_object_unregister(e_msgbus_data->screensaver_iface2);
   if (e_msgbus_data->screensaver_iface)
     eldbus_service_object_unregister(e_msgbus_data->screensaver_iface);
   if (e_msgbus_data->e_iface)
     eldbus_service_object_unregister(e_msgbus_data->e_iface);
   if (e_msgbus_data->conn)
     {
        eldbus_name_release(e_msgbus_data->conn, E_BUS, NULL, NULL);
        eldbus_connection_unref(e_msgbus_data->conn);
     }
   eldbus_shutdown();

   EINA_LIST_FREE(e_msgbus_data->screensaver_inhibits, inhibit)
     _e_msgbus_screensaver_inhibit_free(inhibit);

   E_FREE(e_msgbus_data);
   return 1;
}

E_API Eldbus_Service_Interface *
e_msgbus_interface_attach(const Eldbus_Service_Interface_Desc *desc)
{
   if (!e_msgbus_data->e_iface) return NULL;
   return eldbus_service_interface_register(e_msgbus_data->conn, E_PATH, desc);
}

///////////////////////////////////////////////////////////////////////////
static void
_e_msgbus_core_request_name_cb(void *data EINA_UNUSED,
                               const Eldbus_Message *msg,
                               Eldbus_Pending *pending EINA_UNUSED)
{
   unsigned int flag;

   if (eldbus_message_error_get(msg, NULL, NULL))
     {
        ERR("Could not request bus name");
        return;
     }
   if (!eldbus_message_arguments_get(msg, "u", &flag))
     {
        ERR("Could not get arguments on on_name_request");
        return;
     }
   if (!(flag & ELDBUS_NAME_REQUEST_REPLY_PRIMARY_OWNER))
     WRN("Enlightenment core name already in use\n");
}

static Eldbus_Message *
_e_msgbus_core_version_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                          const Eldbus_Message *msg)
{
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);
   eldbus_message_arguments_append(reply, "s", VERSION);
   return reply;
}

static Eldbus_Message *
_e_msgbus_core_restart_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                          const Eldbus_Message *msg)
{
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
     ERR("DBus restart API disabled for security reasons");
   else
     e_sys_action_do(E_SYS_RESTART, NULL);
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_e_msgbus_core_shutdown_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                           const Eldbus_Message *msg)
{
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
     ERR("DBus shutdown API disabled for security reasons");
   else
     e_sys_action_do(E_SYS_EXIT, NULL);
   return eldbus_message_method_return_new(msg);
}
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
static void
_e_msgbus_screensaver_request_name_cb(void *data EINA_UNUSED,
                                      const Eldbus_Message *msg,
                                      Eldbus_Pending *pending EINA_UNUSED)
{
   unsigned int flag;

   if (eldbus_message_error_get(msg, NULL, NULL))
     {
        ERR("Could not request bus name");
        return;
     }
   if (!eldbus_message_arguments_get(msg, "u", &flag))
     {
        ERR("Could not get arguments on on_name_request");
        return;
     }
   if (!(flag & ELDBUS_NAME_REQUEST_REPLY_PRIMARY_OWNER))
     WRN("Screensaver name already in use\n");
}

static void
_e_msgbus_screensaver_inhibit_free(E_Msgbus_Data_Screensaver_Inhibit *inhibit)
{
   printf("INH: inhibit remove %i [%s] [%s] [%s]\n", inhibit->cookie, inhibit->application, inhibit->reason, inhibit->sender);
   if (inhibit->application) eina_stringshare_del(inhibit->application);
   if (inhibit->reason) eina_stringshare_del(inhibit->reason);
   if (inhibit->sender)
     {
        eldbus_name_owner_changed_callback_del(e_msgbus_data->conn,
                                               inhibit->sender,
                                               _e_msgbus_screensaver_owner_change_cb,
                                               NULL);
        eina_stringshare_del(inhibit->sender);
     }
   free(inhibit);
}

static void
_e_msgbus_screensaver_owner_change_cb(void *data EINA_UNUSED, const char *bus EINA_UNUSED, const char *old_id, const char *new_id)
{
   Eina_List *l, *ll;
   E_Msgbus_Data_Screensaver_Inhibit *inhibit;
   Eina_Bool removed = EINA_FALSE;

   printf("INH: owner change... [%s] [%s] [%s]\n", bus, old_id, new_id);
   if ((new_id) && (!new_id[0]))
     {
        EINA_LIST_FOREACH_SAFE(e_msgbus_data->screensaver_inhibits, l, ll, inhibit)
          {
             if ((inhibit->sender) && (!strcmp(inhibit->sender, old_id)))
               {
                  _e_msgbus_screensaver_inhibit_free(inhibit);
                  e_msgbus_data->screensaver_inhibits =
                    eina_list_remove_list
                     (e_msgbus_data->screensaver_inhibits, l);
                  removed = EINA_TRUE;
               }
          }
        if ((removed) && (!e_msgbus_data->screensaver_inhibits))
          {
             // stop inhibiting SS
             e_screensaver_update();
          }
     }
}

static Eldbus_Message *
_e_msgbus_screensaver_inhibit_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                                 const Eldbus_Message *msg)
{
   Eldbus_Message *reply = NULL;
   static unsigned int cookie = 1;
   const char *application_name =  NULL, *reason_for_inhibit = NULL;
   E_Msgbus_Data_Screensaver_Inhibit *inhibit;

   if (!eldbus_message_arguments_get(msg, "ss",
                                     &application_name, &reason_for_inhibit))
     {
        ERR("Can't get application_name and reason_for_inhibit");
        goto err;
     }
   inhibit = E_NEW(E_Msgbus_Data_Screensaver_Inhibit, 1);
   if (inhibit)
     {
        cookie++;
        inhibit->application = eina_stringshare_add(application_name);
        inhibit->reason = eina_stringshare_add(reason_for_inhibit);
        inhibit->sender = eina_stringshare_add(eldbus_message_sender_get(msg));
        if (inhibit->sender)
          eldbus_name_owner_changed_callback_add(e_msgbus_data->conn,
                                                 inhibit->sender,
                                                 _e_msgbus_screensaver_owner_change_cb,
                                                 NULL, EINA_FALSE);
        inhibit->cookie = cookie;
        printf("INH: inhibit [%s] [%s] [%s] -> %i\n", inhibit->application, inhibit->reason, inhibit->sender, inhibit->cookie);
        e_msgbus_data->screensaver_inhibits =
          eina_list_append(e_msgbus_data->screensaver_inhibits, inhibit);
        reply = eldbus_message_method_return_new(msg);
        if (reply)
          eldbus_message_arguments_append(reply, "u", inhibit->cookie);
     }
   if ((e_msgbus_data->screensaver_inhibits) &&
       (eina_list_count(e_msgbus_data->screensaver_inhibits) == 1))
     {
        // start inhibiting SS
        e_screensaver_deactivate();
        e_screensaver_update();
     }
err:
   return reply;
}

static Eldbus_Message *
_e_msgbus_screensaver_uninhibit_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                                   const Eldbus_Message *msg)
{
   unsigned int cookie = 0;

   if (!eldbus_message_arguments_get(msg, "u", &cookie))
     return NULL;
   e_msgbus_screensaver_inhibit_remove(cookie);
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_e_msgbus_screensaver_getactive_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                                   const Eldbus_Message *msg)
{
   Eldbus_Message *reply = NULL;

   reply = eldbus_message_method_return_new(msg);
   printf("INH: getactive = %i\n", e_screensaver_on_get());
   if (reply)
     eldbus_message_arguments_append(reply, "b", e_screensaver_on_get());
   return reply;
}

E_API void
e_msgbus_screensaver_inhibit_remove(unsigned int cookie)
{
   Eina_Bool removed = EINA_FALSE;
   Eina_List *l;
   E_Msgbus_Data_Screensaver_Inhibit *inhibit;

   EINA_LIST_FOREACH(e_msgbus_data->screensaver_inhibits, l, inhibit)
     {
        if (inhibit->cookie == cookie)
          {
             _e_msgbus_screensaver_inhibit_free(inhibit);
             e_msgbus_data->screensaver_inhibits =
               eina_list_remove_list
                 (e_msgbus_data->screensaver_inhibits, l);
             removed = EINA_TRUE;
             break;
          }
     }
   if ((removed) && (!e_msgbus_data->screensaver_inhibits))
     {
        // stop inhibiting SS
        e_screensaver_update();
     }
}

///////////////////////////////////////////////////////////////////////////
