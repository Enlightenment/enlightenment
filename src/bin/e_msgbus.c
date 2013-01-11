#include "e.h"

/* local subsystem functions */
static void           _e_msgbus_request_name_cb(void *data, const EDBus_Message *msg,
                                                EDBus_Pending *pending);

static EDBus_Message *_e_msgbus_core_restart_cb(const EDBus_Service_Interface *iface,
                                                const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_core_shutdown_cb(const EDBus_Service_Interface *iface,
                                                 const EDBus_Message *msg);

static EDBus_Message *_e_msgbus_module_load_cb(const EDBus_Service_Interface *iface,
                                               const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_module_unload_cb(const EDBus_Service_Interface *iface,
                                                 const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_module_enable_cb(const EDBus_Service_Interface *iface,
                                                 const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_module_disable_cb(const EDBus_Service_Interface *iface,
                                                  const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_module_list_cb(const EDBus_Service_Interface *iface,
                                               const EDBus_Message *msg);

static EDBus_Message *_e_msgbus_profile_set_cb(const EDBus_Service_Interface *iface,
                                               const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_profile_get_cb(const EDBus_Service_Interface *iface,
                                               const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_profile_list_cb(const EDBus_Service_Interface *iface,
                                                const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_profile_add_cb(const EDBus_Service_Interface *iface,
                                               const EDBus_Message *msg);
static EDBus_Message *_e_msgbus_profile_delete_cb(const EDBus_Service_Interface *iface,
                                                  const EDBus_Message *msg);

#define E_MSGBUS_WIN_ACTION_CB_PROTO(NAME)                                                   \
  static EDBus_Message * _e_msgbus_window_##NAME##_cb(const EDBus_Service_Interface * iface, \
                                                      const EDBus_Message * msg)

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

static const EDBus_Method core_methods[] = {
   { "Restart", NULL, NULL, _e_msgbus_core_restart_cb },
   { "Shutdown", NULL, NULL, _e_msgbus_core_shutdown_cb },
   { }
};

static const EDBus_Method module_methods[] = {
   { "Load", EDBUS_ARGS({"s", "module"}), NULL, _e_msgbus_module_load_cb },
   { "Unload", EDBUS_ARGS({"s", "module"}), NULL, _e_msgbus_module_unload_cb },
   { "Enable", EDBUS_ARGS({"s", "module"}), NULL, _e_msgbus_module_enable_cb },
   { "Disable", EDBUS_ARGS({"s", "module"}), NULL, _e_msgbus_module_disable_cb },
   { "List", NULL, EDBUS_ARGS({"a(si)", "modules"}),
     _e_msgbus_module_list_cb },
   { }
};

static const EDBus_Method profile_methods[] = {
   { "Set", EDBUS_ARGS({"s", "profile"}), NULL, _e_msgbus_profile_set_cb },
   { "Get", NULL, EDBUS_ARGS({"s", "profile"}), _e_msgbus_profile_get_cb },
   { "List", NULL, EDBUS_ARGS({"as", "array_profiles"}),
     _e_msgbus_profile_list_cb },
   { "Add", EDBUS_ARGS({"s", "profile"}), NULL, _e_msgbus_profile_add_cb },
   { "Delete", EDBUS_ARGS({"s", "profile"}), NULL, _e_msgbus_profile_delete_cb },
   { }
};

static const EDBus_Method window_methods[] = {
   { "List", NULL, EDBUS_ARGS({"a(si)", "array_of_window"}),
     _e_msgbus_window_list_cb },
   { "Close", EDBUS_ARGS({"i", "window_id"}), NULL, _e_msgbus_window_close_cb },
   { "Kill", EDBUS_ARGS({"i", "window_id"}), NULL, _e_msgbus_window_kill_cb },
   { "Focus", EDBUS_ARGS({"i", "window_id"}), NULL, _e_msgbus_window_focus_cb },
   { "Iconify", EDBUS_ARGS({"i", "window_id"}), NULL,
     _e_msgbus_window_iconify_cb },
   { "Uniconify", EDBUS_ARGS({"i", "window_id"}), NULL,
     _e_msgbus_window_uniconify_cb },
   { "Maximize", EDBUS_ARGS({"i", "window_id"}), NULL,
     _e_msgbus_window_maximize_cb },
   { "Unmaximize", EDBUS_ARGS({"i", "window_id"}), NULL,
     _e_msgbus_window_unmaximize_cb },
   { }
};

#define PATH "/org/enlightenment/wm/RemoteObject"

static const EDBus_Service_Interface_Desc core_desc = {
   "org.enlightenment.wm.Core", core_methods
};

static const EDBus_Service_Interface_Desc module_desc = {
   "org.enlightenment.wm.Module", module_methods
};

static const EDBus_Service_Interface_Desc profile_desc = {
   "org.enlightenment.wm.Profile", profile_methods
};

static const EDBus_Service_Interface_Desc window_desc = {
   "org.enlightenment.wm.Window", window_methods
};

/* externally accessible functions */
EINTERN int
e_msgbus_init(void)
{
   _e_msgbus_data = E_NEW(E_Msgbus_Data, 1);

   edbus_init();

   _e_msgbus_data->conn = edbus_connection_get(EDBUS_CONNECTION_TYPE_SESSION);
   if (!_e_msgbus_data->conn)
     {
        WRN("Cannot get EDBUS_CONNECTION_TYPE_SESSION");
        return 0;
     }

   _e_msgbus_data->iface = edbus_service_interface_register(_e_msgbus_data->conn,
                                                            PATH, &core_desc);
   edbus_service_interface_register(_e_msgbus_data->conn, PATH, &module_desc);
   edbus_service_interface_register(_e_msgbus_data->conn, PATH, &profile_desc);
   edbus_service_interface_register(_e_msgbus_data->conn, PATH, &window_desc);
   edbus_name_request(_e_msgbus_data->conn, "org.enlightenment.wm.service",
                      0, _e_msgbus_request_name_cb, NULL);
   return 1;
}

EINTERN int
e_msgbus_shutdown(void)
{
   if (_e_msgbus_data->iface)
     edbus_service_object_unregister(_e_msgbus_data->iface);
   if (_e_msgbus_data->conn)
     {
        edbus_name_release(_e_msgbus_data->conn,
                           "org.enlightenment.wm.service", NULL, NULL);
        edbus_connection_unref(_e_msgbus_data->conn);
     }
   edbus_shutdown();

   E_FREE(_e_msgbus_data);
   _e_msgbus_data = NULL;
   return 1;
}

EAPI EDBus_Service_Interface *
e_msgbus_interface_attach(const EDBus_Service_Interface_Desc *desc)
{
   if (!_e_msgbus_data->iface)
     return NULL;
   return edbus_service_interface_register(_e_msgbus_data->conn, PATH, desc);
}

static void
_e_msgbus_request_name_cb(void *data __UNUSED__, const EDBus_Message *msg,
                          EDBus_Pending *pending __UNUSED__)
{
   unsigned int flag;

   if (edbus_message_error_get(msg, NULL, NULL))
     {
        ERR("Could not request bus name");
        return;
     }

   if (!edbus_message_arguments_get(msg, "u", &flag))
     {
        ERR("Could not get arguments on on_name_request");
        return;
     }

   if (!(flag & EDBUS_NAME_REQUEST_REPLY_PRIMARY_OWNER))
     ERR("Name already in use\n");
}

/* Core Handlers */
static EDBus_Message *
_e_msgbus_core_restart_cb(const EDBus_Service_Interface *iface __UNUSED__,
                          const EDBus_Message *msg)
{
   e_sys_action_do(E_SYS_RESTART, NULL);
   return edbus_message_method_return_new(msg);
}

static EDBus_Message *
_e_msgbus_core_shutdown_cb(const EDBus_Service_Interface *iface __UNUSED__,
                           const EDBus_Message *msg)
{
   e_sys_action_do(E_SYS_EXIT, NULL);
   return edbus_message_method_return_new(msg);
}

/* Modules Handlers */
static EDBus_Message *
_e_msgbus_module_load_cb(const EDBus_Service_Interface *iface __UNUSED__,
                         const EDBus_Message *msg)
{
   char *module;
   EDBus_Message *reply = edbus_message_method_return_new(msg);

   if (!edbus_message_arguments_get(msg, "s", &module))
     return reply;

   if (!e_module_find(module))
     {
        e_module_new(module);
        e_config_save_queue();
     }

   return reply;
}

static EDBus_Message *
_e_msgbus_module_unload_cb(const EDBus_Service_Interface *iface __UNUSED__,
                           const EDBus_Message *msg)
{
   char *module;
   E_Module *m;
   EDBus_Message *reply = edbus_message_method_return_new(msg);

   if (!edbus_message_arguments_get(msg, "s", &module))
     return reply;

   if ((m = e_module_find(module)))
     {
        e_module_disable(m);
        e_object_del(E_OBJECT(m));
        e_config_save_queue();
     }

   return reply;
}

static EDBus_Message *
_e_msgbus_module_enable_cb(const EDBus_Service_Interface *iface __UNUSED__,
                           const EDBus_Message *msg)
{
   char *module;
   E_Module *m;
   EDBus_Message *reply = edbus_message_method_return_new(msg);

   if (!edbus_message_arguments_get(msg, "s", &module))
     return reply;

   if ((m = e_module_find(module)))
     {
        e_module_enable(m);
        e_config_save_queue();
     }

   return reply;
}

static EDBus_Message *
_e_msgbus_module_disable_cb(const EDBus_Service_Interface *iface __UNUSED__,
                            const EDBus_Message *msg)
{
   char *module;
   E_Module *m;
   EDBus_Message *reply = edbus_message_method_return_new(msg);

   if (!edbus_message_arguments_get(msg, "s", &module))
     return reply;

   if ((m = e_module_find(module)))
     {
        e_module_disable(m);
        e_config_save_queue();
     }

   return reply;
}

static EDBus_Message *
_e_msgbus_module_list_cb(const EDBus_Service_Interface *iface __UNUSED__,
                         const EDBus_Message *msg)
{
   Eina_List *l;
   E_Module *mod;
   EDBus_Message *reply = edbus_message_method_return_new(msg);
   EDBus_Message_Iter *main_iter, *array;

   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);
   main_iter = edbus_message_iter_get(reply);
   EINA_SAFETY_ON_NULL_RETURN_VAL(main_iter, reply);

   edbus_message_iter_arguments_append(main_iter, "a(si)", &array);
   EINA_SAFETY_ON_NULL_RETURN_VAL(array, reply);

   EINA_LIST_FOREACH(e_module_list(), l, mod)
     {
        EDBus_Message_Iter *s;
        const char *name;
        int enabled;

        name = mod->name;
        enabled = mod->enabled;

        edbus_message_iter_arguments_append(array, "(si)", &s);
        if (!s) continue;
        edbus_message_iter_arguments_append(s, "si", name, enabled);
        edbus_message_iter_container_close(array, s);
     }
   edbus_message_iter_container_close(main_iter, array);

   return reply;
}

/* Profile Handlers */
static EDBus_Message *
_e_msgbus_profile_set_cb(const EDBus_Service_Interface *iface __UNUSED__,
                         const EDBus_Message *msg)
{
   char *profile;
   EDBus_Message *reply = edbus_message_method_return_new(msg);

   if (!edbus_message_arguments_get(msg, "s", &profile))
     return reply;

   e_config_save_flush();
   e_config_profile_set(profile);
   e_config_profile_save();
   e_config_save_block_set(1);
   e_sys_action_do(E_SYS_RESTART, NULL);

   return reply;
}

static EDBus_Message *
_e_msgbus_profile_get_cb(const EDBus_Service_Interface *iface __UNUSED__,
                         const EDBus_Message *msg)
{
   EDBus_Message *reply = edbus_message_method_return_new(msg);
   const char *profile;

   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);
   profile = e_config_profile_get();
   edbus_message_arguments_append(reply, "s", profile);
   return reply;
}

static EDBus_Message *
_e_msgbus_profile_list_cb(const EDBus_Service_Interface *iface __UNUSED__,
                          const EDBus_Message *msg)
{
   Eina_List *l;
   const char *name;
   EDBus_Message *reply;
   EDBus_Message_Iter *array, *main_iter;

   reply = edbus_message_method_return_new(msg);
   EINA_SAFETY_ON_NULL_RETURN_VAL(reply, NULL);

   main_iter = edbus_message_iter_get(reply);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(main_iter, reply);

   edbus_message_iter_arguments_append(main_iter, "as", &array);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(array, reply);

   EINA_LIST_FOREACH(e_config_profile_list(), l, name)
     edbus_message_iter_basic_append(array, 's', name);
   edbus_message_iter_container_close(main_iter, array);

   return reply;
}

static EDBus_Message *
_e_msgbus_profile_add_cb(const EDBus_Service_Interface *iface __UNUSED__,
                         const EDBus_Message *msg)
{
   char *profile;
   EDBus_Message *reply = edbus_message_method_return_new(msg);

   if (!edbus_message_arguments_get(msg, "s", &profile))
     return reply;
   e_config_profile_add(profile);

   return reply;
}

static EDBus_Message *
_e_msgbus_profile_delete_cb(const EDBus_Service_Interface *iface __UNUSED__,
                            const EDBus_Message *msg)
{
   char *profile;
   EDBus_Message *reply = edbus_message_method_return_new(msg);

   if (!edbus_message_arguments_get(msg, "s", &profile))
     return reply;
   if (!strcmp(e_config_profile_get(), profile))
     return edbus_message_error_new(msg,
                                    "org.enlightenment.DBus.InvalidArgument",
                                    "Can't delete active profile");
   e_config_profile_del(profile);
   return reply;
}

/* Window handlers */
static EDBus_Message *
_e_msgbus_window_list_cb(const EDBus_Service_Interface *iface __UNUSED__,
                         const EDBus_Message *msg)
{
   Eina_List *l;
   E_Border *bd;
   EDBus_Message *reply;
   EDBus_Message_Iter *main_iter, *array;

   reply = edbus_message_method_return_new(msg);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(reply, NULL);

   main_iter = edbus_message_iter_get(reply);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(main_iter, reply);

   edbus_message_iter_arguments_append(main_iter, "a(si)", &array);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(array, reply);

   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
     {
        EDBus_Message_Iter *s;

        edbus_message_iter_arguments_append(array, "(si)", &s);
        if (!s) continue;
        edbus_message_iter_arguments_append(s, "si", bd->client.icccm.name,
                                            bd->client.win);
        edbus_message_iter_container_close(array, s);
     }
   edbus_message_iter_container_close(main_iter, array);

   return reply;
}

#define E_MSGBUS_WIN_ACTION_CB_BEGIN(NAME)                                       \
  static EDBus_Message *                                                         \
  _e_msgbus_window_##NAME##_cb(const EDBus_Service_Interface * iface __UNUSED__, \
                               const EDBus_Message * msg)                        \
  {                                                                              \
     E_Border *bd;                                                               \
     int xwin;                                                                   \
                                                                                 \
     if (!edbus_message_arguments_get(msg, "i", &xwin))                          \
       return edbus_message_method_return_new(msg);                              \
     bd = e_border_find_by_client_window(xwin);                                  \
     if (bd)                                                                     \
       {
#define E_MSGBUS_WIN_ACTION_CB_END             \
  }                                            \
                                               \
  return edbus_message_method_return_new(msg); \
  }

 E_MSGBUS_WIN_ACTION_CB_BEGIN(close)
 e_border_act_close_begin(bd);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(kill)
 e_border_act_kill_begin(bd);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(focus)
 e_border_focus_set(bd, 1, 1);
 if (!bd->lock_user_stacking)
   {
      if (e_config->border_raise_on_focus)
        e_border_raise(bd);
   }
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(iconify)
 e_border_iconify(bd);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(uniconify)
 e_border_uniconify(bd);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(maximize)
 e_border_maximize(bd, e_config->maximize_policy);
 E_MSGBUS_WIN_ACTION_CB_END

  E_MSGBUS_WIN_ACTION_CB_BEGIN(unmaximize)
 e_border_unmaximize(bd, E_MAXIMIZE_BOTH);
 E_MSGBUS_WIN_ACTION_CB_END
