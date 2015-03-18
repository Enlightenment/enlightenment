#include "e.h"

/* local subsystem functions */
static void           _e_msgbus_request_name_cb(void *data, const Eldbus_Message *msg,
                                                Eldbus_Pending *pending);

static Eldbus_Message *_e_msgbus_core_version_cb(const Eldbus_Service_Interface *iface,
                                                const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_core_restart_cb(const Eldbus_Service_Interface *iface,
                                                const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_core_shutdown_cb(const Eldbus_Service_Interface *iface,
                                                 const Eldbus_Message *msg);

static Eldbus_Message *_e_msgbus_module_load_cb(const Eldbus_Service_Interface *iface,
                                               const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_module_unload_cb(const Eldbus_Service_Interface *iface,
                                                 const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_module_enable_cb(const Eldbus_Service_Interface *iface,
                                                 const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_module_disable_cb(const Eldbus_Service_Interface *iface,
                                                  const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_module_list_cb(const Eldbus_Service_Interface *iface,
                                               const Eldbus_Message *msg);

static Eldbus_Message *_e_msgbus_profile_set_cb(const Eldbus_Service_Interface *iface,
                                               const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_profile_get_cb(const Eldbus_Service_Interface *iface,
                                               const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_profile_list_cb(const Eldbus_Service_Interface *iface,
                                                const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_profile_add_cb(const Eldbus_Service_Interface *iface,
                                               const Eldbus_Message *msg);
static Eldbus_Message *_e_msgbus_profile_delete_cb(const Eldbus_Service_Interface *iface,
                                                  const Eldbus_Message *msg);

#define E_MSGBUS_WIN_ACTION_CB_PROTO(NAME)                                                   \
  static Eldbus_Message * _e_msgbus_window_##NAME##_cb(const Eldbus_Service_Interface * iface, \
                                                      const Eldbus_Message * msg)

E_MSGBUS_WIN_ACTION_CB_PROTO(list);
E_MSGBUS_WIN_ACTION_CB_PROTO(close);
E_MSGBUS_WIN_ACTION_CB_PROTO(kill);
E_MSGBUS_WIN_ACTION_CB_PROTO(focus);
E_MSGBUS_WIN_ACTION_CB_PROTO(iconify);
E_MSGBUS_WIN_ACTION_CB_PROTO(uniconify);
E_MSGBUS_WIN_ACTION_CB_PROTO(maximize);
E_MSGBUS_WIN_ACTION_CB_PROTO(unmaximize);

/* local subsystem globals */
static E_Msgbus_Data *_e_msgbus_data = NULL;

static const Eldbus_Method core_methods[] = {
   { "Version", NULL, ELDBUS_ARGS({"s", "version"}), _e_msgbus_core_version_cb, 0 },
   { "Restart", NULL, NULL, _e_msgbus_core_restart_cb, 0 },
   { "Shutdown", NULL, NULL, _e_msgbus_core_shutdown_cb, 0 },
   { NULL, NULL, NULL, NULL, 0}
};

static const Eldbus_Method module_methods[] = {
   { "Load", ELDBUS_ARGS({"s", "module"}), NULL, _e_msgbus_module_load_cb, 0 },
   { "Unload", ELDBUS_ARGS({"s", "module"}), NULL, _e_msgbus_module_unload_cb, 0 },
   { "Enable", ELDBUS_ARGS({"s", "module"}), NULL, _e_msgbus_module_enable_cb, 0 },
   { "Disable", ELDBUS_ARGS({"s", "module"}), NULL, _e_msgbus_module_disable_cb, 0 },
   { "List", NULL, ELDBUS_ARGS({"a(si)", "modules"}),
     _e_msgbus_module_list_cb, 0 },
   { NULL, NULL, NULL, NULL, 0}
};

static const Eldbus_Method profile_methods[] = {
   { "Set", ELDBUS_ARGS({"s", "profile"}), NULL, _e_msgbus_profile_set_cb, 0 },
   { "Get", NULL, ELDBUS_ARGS({"s", "profile"}), _e_msgbus_profile_get_cb, 0 },
   { "List", NULL, ELDBUS_ARGS({"as", "array_profiles"}),
     _e_msgbus_profile_list_cb, 0 },
   { "Add", ELDBUS_ARGS({"s", "profile"}), NULL, _e_msgbus_profile_add_cb, 0 },
   { "Delete", ELDBUS_ARGS({"s", "profile"}), NULL, _e_msgbus_profile_delete_cb, 0 },
   { NULL, NULL, NULL, NULL, 0}
};

static const Eldbus_Method window_methods[] = {
   { "List", NULL, ELDBUS_ARGS({"a(si)", "array_of_window"}),
     _e_msgbus_window_list_cb, 0 },
   { "Close", ELDBUS_ARGS({"i", "window_id"}), NULL, _e_msgbus_window_close_cb, 0 },
   { "Kill", ELDBUS_ARGS({"i", "window_id"}), NULL, _e_msgbus_window_kill_cb, 0 },
   { "Focus", ELDBUS_ARGS({"i", "window_id"}), NULL, _e_msgbus_window_focus_cb, 0 },
   { "Iconify", ELDBUS_ARGS({"i", "window_id"}), NULL,
     _e_msgbus_window_iconify_cb, 0 },
   { "Uniconify", ELDBUS_ARGS({"i", "window_id"}), NULL,
     _e_msgbus_window_uniconify_cb, 0 },
   { "Maximize", ELDBUS_ARGS({"i", "window_id"}), NULL,
     _e_msgbus_window_maximize_cb, 0 },
   { "Unmaximize", ELDBUS_ARGS({"i", "window_id"}), NULL,
     _e_msgbus_window_unmaximize_cb, 0 },
   { NULL, NULL, NULL, NULL, 0}
};

#define PATH "/org/enlightenment/wm/RemoteObject"

static const Eldbus_Service_Interface_Desc core_desc = {
   "org.enlightenment.wm.Core", core_methods, NULL, NULL, NULL, NULL
};

static const Eldbus_Service_Interface_Desc module_desc = {
   "org.enlightenment.wm.Module", module_methods, NULL, NULL, NULL, NULL
};

static const Eldbus_Service_Interface_Desc profile_desc = {
   "org.enlightenment.wm.Profile", profile_methods, NULL, NULL, NULL, NULL
};

static const Eldbus_Service_Interface_Desc window_desc = {
   "org.enlightenment.wm.Window", window_methods, NULL, NULL, NULL, NULL
};

/* externally accessible functions */
EINTERN int
e_msgbus_init(void)
{
   _e_msgbus_data = E_NEW(E_Msgbus_Data, 1);

   eldbus_init();

   _e_msgbus_data->conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);
   if (!_e_msgbus_data->conn)
     {
        WRN("Cannot get ELDBUS_CONNECTION_TYPE_SESSION");
        return 0;
     }

   _e_msgbus_data->iface = eldbus_service_interface_register(_e_msgbus_data->conn,
                                                            PATH, &core_desc);
   eldbus_service_interface_register(_e_msgbus_data->conn, PATH, &module_desc);
   eldbus_service_interface_register(_e_msgbus_data->conn, PATH, &profile_desc);
   eldbus_service_interface_register(_e_msgbus_data->conn, PATH, &window_desc);
   eldbus_name_request(_e_msgbus_data->conn, "org.enlightenment.wm.service",
                      0, _e_msgbus_request_name_cb, NULL);
   return 1;
}

EINTERN int
e_msgbus_shutdown(void)
{
   if (_e_msgbus_data->iface)
     eldbus_service_object_unregister(_e_msgbus_data->iface);
   if (_e_msgbus_data->conn)
     {
        eldbus_name_release(_e_msgbus_data->conn,
                           "org.enlightenment.wm.service", NULL, NULL);
        eldbus_connection_unref(_e_msgbus_data->conn);
     }
   eldbus_shutdown();

   E_FREE(_e_msgbus_data);
   _e_msgbus_data = NULL;
   return 1;
}

EAPI Eldbus_Service_Interface *
e_msgbus_interface_attach(const Eldbus_Service_Interface_Desc *desc)
{
   if (!_e_msgbus_data->iface)
     return NULL;
   return eldbus_service_interface_register(_e_msgbus_data->conn, PATH, desc);
}

static void
_e_msgbus_request_name_cb(void *data EINA_UNUSED, const Eldbus_Message *msg,
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
     WRN("Name already in use\n");
}

/* Core Handlers */
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
   e_sys_action_do(E_SYS_RESTART, NULL);
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_e_msgbus_core_shutdown_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                           const Eldbus_Message *msg)
{
   e_sys_action_do(E_SYS_EXIT, NULL);
   return eldbus_message_method_return_new(msg);
}

/* Modules Handlers */
static Eldbus_Message *
_e_msgbus_module_load_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                         const Eldbus_Message *msg)
{
   char *module;
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   if (!eldbus_message_arguments_get(msg, "s", &module))
     return reply;

   if (!e_module_find(module))
     {
        e_module_new(module);
        e_config_save_queue();
     }

   return reply;
}

static Eldbus_Message *
_e_msgbus_module_unload_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                           const Eldbus_Message *msg)
{
   char *module;
   E_Module *m;
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   if (!eldbus_message_arguments_get(msg, "s", &module))
     return reply;

   if ((m = e_module_find(module)))
     {
        e_module_disable(m);
        e_object_del(E_OBJECT(m));
        e_config_save_queue();
     }

   return reply;
}

static Eldbus_Message *
_e_msgbus_module_enable_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                           const Eldbus_Message *msg)
{
   char *module;
   E_Module *m;
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   if (!eldbus_message_arguments_get(msg, "s", &module))
     return reply;

   if ((m = e_module_find(module)))
     {
        e_module_enable(m);
        e_config_save_queue();
     }

   return reply;
}

static Eldbus_Message *
_e_msgbus_module_disable_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                            const Eldbus_Message *msg)
{
   char *module;
   E_Module *m;
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   if (!eldbus_message_arguments_get(msg, "s", &module))
     return reply;

   if ((m = e_module_find(module)))
     {
        e_module_disable(m);
        e_config_save_queue();
     }

   return reply;
}

static Eldbus_Message *
_e_msgbus_module_list_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                         const Eldbus_Message *msg)
{
   Eina_List *l;
   E_Module *mod;
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);
   Eldbus_Message_Iter *main_iter, *array;

   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);
   main_iter = eldbus_message_iter_get(reply);
   EINA_SAFETY_ON_NULL_RETURN_VAL(main_iter, reply);

   eldbus_message_iter_arguments_append(main_iter, "a(si)", &array);
   EINA_SAFETY_ON_NULL_RETURN_VAL(array, reply);

   EINA_LIST_FOREACH(e_module_list(), l, mod)
     {
        Eldbus_Message_Iter *s;
        const char *name;
        int enabled;

        name = mod->name;
        enabled = mod->enabled;

        eldbus_message_iter_arguments_append(array, "(si)", &s);
        if (!s) continue;
        eldbus_message_iter_arguments_append(s, "si", name, enabled);
        eldbus_message_iter_container_close(array, s);
     }
   eldbus_message_iter_container_close(main_iter, array);

   return reply;
}

/* Profile Handlers */
static Eldbus_Message *
_e_msgbus_profile_set_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                         const Eldbus_Message *msg)
{
   char *profile;
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   if (!eldbus_message_arguments_get(msg, "s", &profile))
     return reply;

   e_config_save_flush();
   e_config_profile_set(profile);
   e_config_profile_save();
   e_config_save_block_set(1);
   e_sys_action_do(E_SYS_RESTART, NULL);

   return reply;
}

static Eldbus_Message *
_e_msgbus_profile_get_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                         const Eldbus_Message *msg)
{
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);
   const char *profile;

   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);
   profile = e_config_profile_get();
   eldbus_message_arguments_append(reply, "s", profile);
   return reply;
}

static Eldbus_Message *
_e_msgbus_profile_list_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                          const Eldbus_Message *msg)
{
   Eina_List *l;
   char *name;
   Eldbus_Message *reply;
   Eldbus_Message_Iter *array, *main_iter;

   reply = eldbus_message_method_return_new(msg);
   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);

   main_iter = eldbus_message_iter_get(reply);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(main_iter, reply);

   eldbus_message_iter_arguments_append(main_iter, "as", &array);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(array, reply);

   l = e_config_profile_list();
   EINA_LIST_FREE(l, name)
     {
        eldbus_message_iter_basic_append(array, 's', name);
        free(name);
     }
   eldbus_message_iter_container_close(main_iter, array);

   return reply;
}

static Eldbus_Message *
_e_msgbus_profile_add_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                         const Eldbus_Message *msg)
{
   char *profile;
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);

   if (!eldbus_message_arguments_get(msg, "s", &profile))
     return reply;
   e_config_profile_add(profile);

   return reply;
}

static Eldbus_Message *
_e_msgbus_profile_delete_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                            const Eldbus_Message *msg)
{
   char *profile;

   if (!eldbus_message_arguments_get(msg, "s", &profile))
     return eldbus_message_method_return_new(msg);
   if (!strcmp(e_config_profile_get(), profile))
     return eldbus_message_error_new(msg,
                                    "org.enlightenment.DBus.InvalidArgument",
                                    "Can't delete active profile");
   e_config_profile_del(profile);
   return eldbus_message_method_return_new(msg);
}

/* Window handlers */
static Eldbus_Message *
_e_msgbus_window_list_cb(const Eldbus_Service_Interface *iface EINA_UNUSED,
                         const Eldbus_Message *msg)
{
   const Eina_List *l;
   E_Client *ec;
   Eldbus_Message *reply;
   Eldbus_Message_Iter *main_iter, *array;

   reply = eldbus_message_method_return_new(msg);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(reply, NULL);

   main_iter = eldbus_message_iter_get(reply);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(main_iter, reply);

   eldbus_message_iter_arguments_append(main_iter, "a(si)", &array);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(array, reply);

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        Eldbus_Message_Iter *s;

        if (e_client_util_ignored_get(ec)) continue;

        eldbus_message_iter_arguments_append(array, "(si)", &s);
        if (!s) continue;
        eldbus_message_iter_arguments_append(s, "si", ec->icccm.name,
                                            e_client_util_win_get(ec));
        eldbus_message_iter_container_close(array, s);
     }
   eldbus_message_iter_container_close(main_iter, array);

   return reply;
}

#define E_MSGBUS_WIN_ACTION_CB_BEGIN(NAME)                                       \
  static Eldbus_Message *                                                         \
  _e_msgbus_window_##NAME##_cb(const Eldbus_Service_Interface * iface EINA_UNUSED, \
                               const Eldbus_Message * msg)                        \
  {                                                                              \
     E_Client *ec;                                                               \
     int xwin;                                                                   \
                                                                                 \
     if (!eldbus_message_arguments_get(msg, "i", &xwin))                          \
       return eldbus_message_method_return_new(msg);                              \
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, xwin);                                  \
     if (ec)                                                                     \
       {
#define E_MSGBUS_WIN_ACTION_CB_END             \
  }                                            \
                                               \
  return eldbus_message_method_return_new(msg); \
  }

 E_MSGBUS_WIN_ACTION_CB_BEGIN(close)
 e_client_act_close_begin(ec);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(kill)
 e_client_act_kill_begin(ec);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(focus)
 e_client_activate(ec, 1);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(iconify)
 e_client_iconify(ec);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(uniconify)
 e_client_uniconify(ec);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(maximize)
 e_client_maximize(ec, e_config->maximize_policy);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(unmaximize)
 e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
 E_MSGBUS_WIN_ACTION_CB_END
