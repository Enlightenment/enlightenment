#include <inttypes.h>
#include "e.h"
#include "ebluez4.h"
#include "agent.h"

#define AGENT_INTERFACE "org.bluez.Agent"
#define BLUEZ_ERROR_FAILED "org.bluez.Error.Failed"
#define GET_ERROR_MSG "edbus_message_arguments_get() error"
#define BLUEZ_ERROR_REJECTED "org.bluez.Error.Rejected"
#define REJECTED_MSG "Request was rejected"

static EDBus_Connection *bluez_conn;
static char buf[1024];

/* Local Functions */
static void
_reply(EDBus_Message *message, EDBus_Message *reply)
{
   edbus_connection_send(bluez_conn, reply, NULL, NULL, -1);
   edbus_message_unref(message);
}

static void
_pincode_ok(void *data, char *text)
{
   EDBus_Message *message = data;
   EDBus_Message *reply = edbus_message_method_return_new(message);
   edbus_message_arguments_append(reply, "s", text);
   _reply(message, reply);
}

static void
_passkey_ok(void *data, char *text)
{
   EDBus_Message *message = data;
   uint32_t passkey = (uint32_t)atoi(text);
   EDBus_Message *reply = edbus_message_method_return_new(message);
   edbus_message_arguments_append(reply, "u", passkey);
   _reply(message, reply);
}

static void
_cancel(void *data)
{
   EDBus_Message *message = data;
   EDBus_Message *reply = edbus_message_error_new(message,
                                           BLUEZ_ERROR_REJECTED, REJECTED_MSG);
   _reply(message, reply);
}

static E_Dialog *
_create_dialog(const char *title, const char *msg,
             const char *icon, const char *class)
{
   E_Container *con;
   E_Dialog *dialog;

   con = e_container_current_get(e_manager_current_get());
   dialog = e_dialog_new(con, title, class);
   e_dialog_title_set(dialog, title);
   e_dialog_icon_set(dialog, icon, 64);
   e_dialog_text_set(dialog, msg);
   return dialog;
}

static void
_display_msg(const char *title, const char *msg)
{
   E_Dialog *dialog = _create_dialog(title, msg, "view-hidden-files",
                                     "display");
   e_dialog_button_add(dialog, "OK", NULL, NULL, NULL);
   e_dialog_show(dialog);
}

static void
_reply_and_del_dialog(EDBus_Message *reply, E_Dialog *dialog)
{
   EDBus_Message *message = dialog->data;
   _reply(message, reply);
   if (!dialog) return;
   e_object_del(E_OBJECT(dialog));
}

static void
_reject(void *data, E_Dialog *dialog)
{
   const EDBus_Message *msg = dialog->data;
   EDBus_Message *reply = edbus_message_error_new(msg, BLUEZ_ERROR_REJECTED,
                                                  REJECTED_MSG);
   _reply_and_del_dialog(reply, dialog);
}

static void
_ok(void *data, E_Dialog *dialog)
{
   const EDBus_Message *msg = dialog->data;
   EDBus_Message *reply = edbus_message_method_return_new(msg);
   _reply_and_del_dialog(reply, dialog);
}

static void
_close(E_Win *win)
{
   E_Dialog *dialog = win->data;
   _reject(NULL, dialog);
}

static void
_ask(const char *title, const char *ask_msg, const char *ok_label,
                  EDBus_Message *edbus_message)
{
   E_Dialog *dialog = _create_dialog(title, ask_msg, "dialog-ask", "ask");
   dialog->data = edbus_message;
   e_win_delete_callback_set(dialog->win, _close);
   e_dialog_button_add(dialog, ok_label, NULL, _ok, NULL);
   e_dialog_button_add(dialog, "Reject", NULL, _reject, NULL);
   e_dialog_show(dialog);
}

/* Implementation of agent API */
static EDBus_Message *
_agent_release(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   DBG("Agent Released.");
   return edbus_message_method_return_new(message);
}

static EDBus_Message *
_agent_request_pin_code(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   EDBus_Message *msg = (EDBus_Message *)message;
   edbus_message_ref(msg);
   e_entry_dialog_show("Pin Code Requested", NULL,
                       "Enter the PinCode above. It should have 1-16 "
                       "characters and can be alphanumeric.", "0000",
                       "OK", "Cancel", _pincode_ok, _cancel, msg);
   return NULL;
}

static EDBus_Message *
_agent_request_passkey(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   EDBus_Message *msg = (EDBus_Message *)message;
   edbus_message_ref(msg);
   e_entry_dialog_show("Passkey Requested", NULL, "Enter the Passkey above. "
                       "It should be a numeric value between 0-999999.",
                       "0", "OK", "Cancel", _passkey_ok, _cancel, msg);
   return NULL;
}

static EDBus_Message *
_agent_display_passkey(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   const char *device;
   uint32_t passkey;
   uint16_t entered;
   Device *dev;

   if(!edbus_message_arguments_get(message, "ouq", &device, &passkey, &entered))
     return edbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, device);
   snprintf(buf, sizeof(buf), "%d keys were typed on %s. Passkey is %06d",
            entered, dev->name, passkey);
   _display_msg("Display Passkey", buf);
   return edbus_message_method_return_new(message);
}

static EDBus_Message *
_agent_display_pin_code(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   const char *device, *pincode;
   Device *dev;

   if(!edbus_message_arguments_get(message, "os", &device, &pincode))
     return edbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, device);
   snprintf(buf, sizeof(buf), "Pincode for %s is %s", dev->name, pincode);
   _display_msg("Display Pincode", buf);
   return edbus_message_method_return_new(message);
}

static EDBus_Message *
_agent_request_confirmation(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   const char *device;
   uint32_t passkey;
   Device *dev;

   if(!edbus_message_arguments_get(message, "ou", &device, &passkey))
     return edbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, device);
   snprintf(buf, sizeof(buf), "%06d is the passkey presented in %s?",
            passkey, dev->name);
   edbus_message_ref((EDBus_Message *)message);
   _ask("Confirm Request", buf, "Confirm", (EDBus_Message *)message);
   return NULL;
}

static EDBus_Message *
_agent_authorize(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   const char *device, *uuid;
   Device *dev;

   if(!edbus_message_arguments_get(message, "os", &device, &uuid))
     return edbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, device);
   snprintf(buf, sizeof(buf), "Grant permission for %s to connect?",
            dev->name);
   edbus_message_ref((EDBus_Message *)message);
   _ask("Authorize Connection", buf, "Grant", (EDBus_Message *)message);
   return NULL;
}

static EDBus_Message *
_agent_cancel(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   DBG("Request canceled.");
   return edbus_message_method_return_new(message);
}

static const EDBus_Method agent_methods[] = {
   { "Release", NULL, NULL, _agent_release },
   { "RequestPinCode", EDBUS_ARGS({"o", "device"}),
      EDBUS_ARGS({"s", "pincode"}), _agent_request_pin_code },
   { "RequestPasskey", EDBUS_ARGS({"o", "device"}),
      EDBUS_ARGS({"u", "passkey"}), _agent_request_passkey },
   { "DisplayPasskey",
      EDBUS_ARGS({"o", "device"},{"u", "passkey"},{"q", "entered"}),
      NULL, _agent_display_passkey },
   { "DisplayPinCode", EDBUS_ARGS({"o", "device"},{"s", "pincode"}),
      NULL, _agent_display_pin_code },
   { "RequestConfirmation", EDBUS_ARGS({"o", "device"},{"u", "passkey"}),
      NULL, _agent_request_confirmation },
   { "Authorize", EDBUS_ARGS({"o", "device"},{"s", "uuid"}),
      NULL, _agent_authorize },
   { "Cancel", NULL, NULL, _agent_cancel },
   { }
};

static const EDBus_Service_Interface_Desc agent_iface = {
   AGENT_INTERFACE, agent_methods
};

/* Public Functions */
void ebluez4_register_agent_interfaces(EDBus_Connection *conn)
{
   bluez_conn = conn;
   edbus_service_interface_register(conn, AGENT_PATH, &agent_iface);
   edbus_service_interface_register(conn, REMOTE_AGENT_PATH, &agent_iface);
}
