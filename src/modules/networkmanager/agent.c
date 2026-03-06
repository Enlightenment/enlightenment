#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "e.h"
#include "e_networkmanager.h"
#include "e_mod_main.h"

#define NM_AGENT_IFACE    "org.freedesktop.NetworkManager.SecretAgent"
#define NM_AGENT_MGR_IFACE "org.freedesktop.NetworkManager.AgentManager"
#define NM_AGENT_MGR_PATH  "/org/freedesktop/NetworkManager/AgentManager"
#define NM_AGENT_ID       "org.enlightenment.NetworkManager"
#define AGENT_KEY         "agent"

/* Internal input struct for one dialog field */
typedef struct _E_NM_Agent_Input E_NM_Agent_Input;

struct _E_NM_Agent_Input
{
   char *key;
   char *value;
   int   show_password;
};

struct _E_NM_Agent
{
   E_Dialog                 *dialog;
   Eldbus_Service_Interface *iface;
   Eldbus_Message           *msg;
   Eldbus_Connection        *conn;
   Eina_Bool                 canceled E_BITFIELD;
};

/* -------------------------------------------------------------------------- */
/* D-Bus reply helpers                                                         */
/* -------------------------------------------------------------------------- */

static void
_dict_append_basic(Eldbus_Message_Iter *array, const char *key, void *val)
{
   Eldbus_Message_Iter *dict, *variant;

   eldbus_message_iter_arguments_append(array, "{sv}", &dict);
   eldbus_message_iter_basic_append(dict, 's', key);
   variant = eldbus_message_iter_container_new(dict, 'v', "s");
   eldbus_message_iter_basic_append(variant, 's', val ?: "");
   eldbus_message_iter_container_close(dict, variant);
   eldbus_message_iter_container_close(array, dict);
}

/* -------------------------------------------------------------------------- */
/* Dialog callbacks                                                            */
/* -------------------------------------------------------------------------- */

static void
_dialog_ok_cb(void *data, E_Dialog *dialog)
{
   E_NM_Agent *agent = data;
   E_NM_Agent_Input *input;
   Evas_Object *toolbook, *list;
   Eldbus_Message_Iter *iter, *outer_array, *inner_dict, *inner_array;
   Eina_List *input_list, *l;
   Eldbus_Message *reply;

   toolbook = agent->dialog->content_object;

   list = evas_object_data_get(toolbook, "psk");
   if (!list)
     {
        list = evas_object_data_get(toolbook, "password");
        if (!list)
          {
             ERR("Couldn't get user input.");
             e_object_del(E_OBJECT(dialog));
             return;
          }
     }

   agent->canceled = EINA_FALSE;
   input_list = evas_object_data_get(list, "input_list");

   /*
    * GetSecrets reply format:
    *   a{sa{sv}}
    *   { "802-11-wireless-security": { "psk": <value> } }
    */
   reply = eldbus_message_method_return_new(agent->msg);
   iter  = eldbus_message_iter_get(reply);
   eldbus_message_iter_arguments_append(iter, "a{sa{sv}}", &outer_array);

   eldbus_message_iter_arguments_append(outer_array, "{sa{sv}}", &inner_dict);
   eldbus_message_iter_basic_append(inner_dict, 's',
                                    "802-11-wireless-security");
   eldbus_message_iter_arguments_append(inner_dict, "a{sv}", &inner_array);

   EINA_LIST_FOREACH(input_list, l, input)
     _dict_append_basic(inner_array, input->key, input->value);

   eldbus_message_iter_container_close(inner_dict, inner_array);
   eldbus_message_iter_container_close(outer_array, inner_dict);
   eldbus_message_iter_container_close(iter, outer_array);

   eldbus_connection_send(agent->conn, reply, NULL, NULL, -1);

   e_object_del(E_OBJECT(dialog));
}

static void
_dialog_cancel_cb(void *data, E_Dialog *dialog)
{
   E_NM_Agent *agent = data;
   agent->canceled = EINA_TRUE;
   e_object_del(E_OBJECT(dialog));
}

static void
_dialog_key_down_cb(void *data, Evas *e EINA_UNUSED,
                    Evas_Object *o EINA_UNUSED, void *event)
{
   Evas_Event_Key_Down *ev = event;
   E_NM_Agent *agent = data;

   if (!strcmp(ev->key, "Return"))
     _dialog_ok_cb(agent, agent->dialog);
   else if (!strcmp(ev->key, "Escape"))
     _dialog_cancel_cb(agent, agent->dialog);
}

static void
_dialog_send_cancel(E_NM_Agent *agent)
{
   Eldbus_Message *reply;

   reply = eldbus_message_error_new(agent->msg,
                                    "org.freedesktop.NetworkManager."
                                    "SecretAgent.UserCanceled",
                                    "User canceled password dialog");
   eldbus_connection_send(agent->conn, reply, NULL, NULL, -1);
}

static void
_dialog_del_cb(void *data)
{
   E_Dialog *dialog = data;
   E_NM_Agent *agent = e_object_data_get(E_OBJECT(dialog));

   if (agent->canceled)
     _dialog_send_cancel(agent);

   eldbus_message_unref(agent->msg);
   agent->msg    = NULL;
   agent->dialog = NULL;
}

static void
_page_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
          Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_NM_Agent_Input *input;
   Eina_List *input_list;

   input_list = evas_object_data_get(obj, "input_list");
   EINA_LIST_FREE(input_list, input)
     {
        free(input->key);
        free(input);
     }
}

static void
_show_password_cb(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *entry = data;
   int hidden;

   hidden = !e_widget_check_checked_get(obj);
   e_widget_entry_password_set(entry, hidden);
}

/* -------------------------------------------------------------------------- */
/* Dialog construction                                                         */
/* -------------------------------------------------------------------------- */

static void
_dialog_psk_add(E_NM_Agent *agent, const char *ssid)
{
   Evas_Object *toolbook, *list, *framelist, *entry, *check;
   E_NM_Agent_Input *input;
   Eina_List *input_list;
   char header[128];
   Evas *evas;

   evas     = evas_object_evas_get(agent->dialog->win);
   toolbook = agent->dialog->content_object;

   input       = E_NEW(E_NM_Agent_Input, 1);
   input->key  = strdup("psk");
   entry = e_widget_entry_add(agent->dialog->win, &(input->value),
                              NULL, NULL, NULL);
   evas_object_show(entry);
   e_widget_entry_password_set(entry, 1);

   list = evas_object_data_get(toolbook, "psk");
   if (!list)
     {
        list = e_widget_list_add(evas, 0, 0);
        e_widget_toolbook_page_append(toolbook, NULL,
                                      _("WiFi Password"),
                                      list, 1, 1, 1, 1, 0.5, 0.0);
        evas_object_data_set(toolbook, "psk", list);
        e_widget_toolbook_page_show(toolbook, 0);
        evas_object_event_callback_add(list, EVAS_CALLBACK_DEL,
                                       _page_del, NULL);
        e_widget_focus_set(entry, 1);
     }

   input_list = evas_object_data_get(list, "input_list");
   input_list = eina_list_append(input_list, input);
   evas_object_data_set(list, "input_list", input_list);

   snprintf(header, sizeof(header),
            _("Password required for \"%s\":"), ssid ?: "network");

   framelist = e_widget_framelist_add(evas, header, 0);
   evas_object_show(framelist);
   e_widget_list_object_append(list, framelist, 1, 1, 0.5);
   e_widget_framelist_object_append(framelist, entry);

   check = e_widget_check_add(evas, _("Show password"),
                               &(input->show_password));
   evas_object_show(check);
   e_widget_framelist_object_append(framelist, check);
   evas_object_smart_callback_add(check, "changed",
                                  _show_password_cb, entry);

   e_util_win_auto_resize_fill(agent->dialog->win);
}

static E_Dialog *
_dialog_new(E_NM_Agent *agent, const char *ssid)
{
   Evas_Object *toolbook;
   E_Dialog    *dialog;
   int          mw, mh;

   dialog = e_dialog_new(NULL, "E", "nm_secret_agent");
   if (!dialog) return NULL;

   e_dialog_resizable_set(dialog, 1);
   e_dialog_title_set(dialog, _("WiFi Password Required"));
   e_dialog_border_icon_set(dialog, "dialog-password");

   e_dialog_button_add(dialog, _("Connect"), NULL, _dialog_ok_cb, agent);
   e_dialog_button_add(dialog, _("Cancel"),  NULL, _dialog_cancel_cb, agent);
   agent->canceled = EINA_TRUE; /* closing window acts as cancel */

   toolbook = e_widget_toolbook_add(
                evas_object_evas_get(dialog->win),
                48 * e_scale, 48 * e_scale);
   evas_object_show(toolbook);

   e_widget_size_min_get(toolbook, &mw, &mh);
   if (mw < 280) mw = 280;
   if (mh < 140) mh = 140;
   e_dialog_content_set(dialog, toolbook, mw, mh);
   e_dialog_show(dialog);

   evas_object_event_callback_add(dialog->bg_object, EVAS_CALLBACK_KEY_DOWN,
                                  _dialog_key_down_cb, agent);
   e_object_del_attach_func_set(E_OBJECT(dialog), _dialog_del_cb);
   e_object_data_set(E_OBJECT(dialog), agent);
   e_dialog_button_focus_num(dialog, 0);
   elm_win_center(dialog->win, 1, 1);

   _dialog_psk_add(agent, ssid);

   return dialog;
}

/* -------------------------------------------------------------------------- */
/* SecretAgent D-Bus method handlers                                           */
/* -------------------------------------------------------------------------- */

static Eldbus_Message *
_agent_get_secrets(const Eldbus_Service_Interface *iface,
                   const Eldbus_Message *msg)
{
   E_NM_Agent *agent;
   Eldbus_Message_Iter *conn_props, *conn_dict, *hints;
   const char *conn_path, *setting_name;
   uint32_t flags;
   char ssid[64] = "";

   agent = eldbus_service_object_data_get(iface, AGENT_KEY);

   /*
    * GetSecrets(a{sa{sv}} connection, o connection_path,
    *            s setting_name, as hints, u flags)
    */
   if (!eldbus_message_arguments_get(msg, "a{sa{sv}}osasu",
                                     &conn_props, &conn_path,
                                     &setting_name, &hints, &flags))
     {
        WRN("GetSecrets: cannot parse arguments");
        return eldbus_message_method_return_new(msg);
     }

   DBG("GetSecrets for %s setting=%s flags=%u", conn_path, setting_name, flags);

   /* Try to extract SSID from connection properties */
   while (eldbus_message_iter_get_and_next(conn_props, 'e', &conn_dict))
     {
        Eldbus_Message_Iter *inner;
        const char *sect;

        if (!eldbus_message_iter_arguments_get(conn_dict, "sa{sv}", &sect,
                                               &inner))
          continue;

        if (!strcmp(sect, "802-11-wireless"))
          {
             Eldbus_Message_Iter *entry, *evar;
             const char *ekey;

             while (eldbus_message_iter_get_and_next(inner, 'e', &entry))
               {
                  if (!eldbus_message_iter_arguments_get(entry, "sv", &ekey,
                                                         &evar))
                    continue;
                  if (!strcmp(ekey, "ssid"))
                    {
                       Eldbus_Message_Iter *bytes;
                       unsigned char b;
                       size_t pos = 0;

                       if (eldbus_message_iter_arguments_get(evar, "ay",
                                                             &bytes))
                         while (eldbus_message_iter_get_and_next(bytes, 'y',
                                                                 &b) &&
                                pos < sizeof(ssid) - 1)
                           ssid[pos++] = (char)b;
                       ssid[pos] = '\0';
                    }
               }
          }
     }

   /* Discard any previous pending request */
   if (agent->msg) eldbus_message_unref(agent->msg);
   agent->msg = eldbus_message_ref((Eldbus_Message *)msg);

   if (agent->dialog)
     e_object_del(E_OBJECT(agent->dialog));

   agent->dialog = _dialog_new(agent, ssid[0] ? ssid : NULL);
   if (!agent->dialog)
     {
        eldbus_message_unref(agent->msg);
        agent->msg = NULL;
        return eldbus_message_method_return_new(msg);
     }

   /* Return NULL — reply will be sent asynchronously from _dialog_ok_cb */
   return NULL;
}

static Eldbus_Message *
_agent_cancel_get_secrets(const Eldbus_Service_Interface *iface,
                          const Eldbus_Message *msg)
{
   E_NM_Agent *agent;

   DBG("CancelGetSecrets");

   agent = eldbus_service_object_data_get(iface, AGENT_KEY);
   if (agent && agent->dialog)
     {
        agent->canceled = EINA_FALSE; /* don't send error reply on del */
        e_object_del(E_OBJECT(agent->dialog));
     }

   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_agent_save_secrets(const Eldbus_Service_Interface *iface EINA_UNUSED,
                    const Eldbus_Message *msg)
{
   /* no-op */
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
_agent_delete_secrets(const Eldbus_Service_Interface *iface EINA_UNUSED,
                      const Eldbus_Message *msg)
{
   /* no-op */
   return eldbus_message_method_return_new(msg);
}

/* -------------------------------------------------------------------------- */
/* Interface descriptor                                                        */
/* -------------------------------------------------------------------------- */

static const Eldbus_Method _agent_methods[] = {
   {
    "GetSecrets",
    ELDBUS_ARGS({"a{sa{sv}}", "connection"}, {"o", "connection_path"},
                {"s", "setting_name"}, {"as", "hints"}, {"u", "flags"}),
    ELDBUS_ARGS({"a{sa{sv}}", "secrets"}),
    _agent_get_secrets, 0
   },
   {
    "CancelGetSecrets",
    ELDBUS_ARGS({"o", "connection_path"}, {"s", "setting_name"}),
    NULL,
    _agent_cancel_get_secrets, 0
   },
   {
    "SaveSecrets",
    ELDBUS_ARGS({"a{sa{sv}}", "connection"}, {"o", "connection_path"}),
    NULL,
    _agent_save_secrets, 0
   },
   {
    "DeleteSecrets",
    ELDBUS_ARGS({"a{sa{sv}}", "connection"}, {"o", "connection_path"}),
    NULL,
    _agent_delete_secrets, 0
   },
   { NULL, NULL, NULL, NULL, 0 }
};

static const Eldbus_Service_Interface_Desc _agent_desc = {
   NM_AGENT_IFACE, _agent_methods, NULL, NULL, NULL, NULL
};

/* -------------------------------------------------------------------------- */
/* AgentManager registration                                                   */
/* -------------------------------------------------------------------------- */

static void
_agent_register_cb(void *data EINA_UNUSED, const Eldbus_Message *msg,
                   Eldbus_Pending *pending EINA_UNUSED)
{
   const char *name, *text;

   if (eldbus_message_error_get(msg, &name, &text))
     WRN("SecretAgent Register failed: %s: %s", name, text);
   else
     INF("SecretAgent registered with NetworkManager");
}

static void
_agent_register(E_NM_Agent *agent)
{
   Eldbus_Object *obj;
   Eldbus_Proxy  *proxy;

   obj   = eldbus_object_get(agent->conn,
                              "org.freedesktop.NetworkManager",
                              NM_AGENT_MGR_PATH);
   proxy = eldbus_proxy_get(obj, NM_AGENT_MGR_IFACE);

   eldbus_proxy_call(proxy, "Register", _agent_register_cb, NULL, -1,
                     "s", NM_AGENT_ID);

   /* Unref immediately; the pending call keeps them alive */
   eldbus_proxy_unref(proxy);
   eldbus_object_unref(obj);
}

/* -------------------------------------------------------------------------- */
/* Public lifecycle                                                            */
/* -------------------------------------------------------------------------- */

E_NM_Agent *
enm_agent_new(Eldbus_Connection *eldbus_conn)
{
   Eldbus_Service_Interface *iface;
   E_NM_Agent *agent;

   agent = E_NEW(E_NM_Agent, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(agent, NULL);

   iface = eldbus_service_interface_register(eldbus_conn, AGENT_PATH,
                                              &_agent_desc);
   if (!iface)
     {
        ERR("Failed to register SecretAgent D-Bus interface");
        free(agent);
        return NULL;
     }

   eldbus_service_object_data_set(iface, AGENT_KEY, agent);

   agent->iface = iface;
   agent->conn  = eldbus_conn;

   _agent_register(agent);

   return agent;
}

void
enm_agent_del(E_NM_Agent *agent)
{
   EINA_SAFETY_ON_NULL_RETURN(agent);

   if (agent->msg)
     {
        eldbus_message_unref(agent->msg);
        agent->msg = NULL;
     }

   if (agent->dialog)
     {
        agent->canceled = EINA_FALSE; /* suppress cancel reply on del */
        e_object_del(E_OBJECT(agent->dialog));
        agent->dialog = NULL;
     }

   eldbus_service_object_unregister(agent->iface);
   agent->iface = NULL;
   free(agent);
}
