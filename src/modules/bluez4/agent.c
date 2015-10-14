#include <inttypes.h>
#include "e.h"
#include "ebluez4.h"
#include "agent.h"

#define AGENT_INTERFACE "org.bluez.Agent"
#define BLUEZ_ERROR_FAILED "org.bluez.Error.Failed"
#define GET_ERROR_MSG "eldbus_message_arguments_get() error"
#define BLUEZ_ERROR_REJECTED "org.bluez.Error.Rejected"
#define REJECTED_MSG "Request was rejected"

static Eldbus_Connection *bluez_conn;
static char buf[1024];

/* Local Functions */
static void
_reply(Eldbus_Message *message, Eldbus_Message *reply)
{
   eldbus_connection_send(bluez_conn, reply, NULL, NULL, -1);
   eldbus_message_unref(message);
}

static void
_pincode_ok(void *data, char *text)
{
   Eldbus_Message *message = data;
   Eldbus_Message *reply = eldbus_message_method_return_new(message);
   eldbus_message_arguments_append(reply, "s", text);
   _reply(message, reply);
}

static void
_passkey_ok(void *data, char *text)
{
   Eldbus_Message *message = data;
   uint32_t passkey = (uint32_t)atoi(text);
   Eldbus_Message *reply = eldbus_message_method_return_new(message);
   eldbus_message_arguments_append(reply, "u", passkey);
   _reply(message, reply);
}

static void
_cancel(void *data)
{
   Eldbus_Message *message = data;
   Eldbus_Message *reply = eldbus_message_error_new(message,
                                           BLUEZ_ERROR_REJECTED, REJECTED_MSG);
   _reply(message, reply);
}

static E_Dialog *
_create_dialog(const char *title, const char *msg,
             const char *icon, const char *class)
{
   E_Dialog *dialog;

   dialog = e_dialog_new(NULL, title, class);
   e_dialog_title_set(dialog, _(title));
   e_dialog_icon_set(dialog, icon, 64);
   e_dialog_text_set(dialog, msg);
   return dialog;
}

static void
_display_msg(const char *title, const char *msg)
{
   E_Dialog *dialog = _create_dialog(title, msg, "view-hidden-files",
                                     "display");
   e_dialog_button_add(dialog, _("OK"), NULL, NULL, NULL);
   e_dialog_show(dialog);
}

static void
_reply_and_del_dialog(Eldbus_Message *reply, E_Dialog *dialog)
{
   Eldbus_Message *message;

   if (!dialog) return;
   if (!(message = dialog->data)) return;
   _reply(message, reply);
   e_object_del(E_OBJECT(dialog));
}

static void
_reject(void *data EINA_UNUSED, E_Dialog *dialog)
{
   const Eldbus_Message *msg = dialog->data;
   Eldbus_Message *reply = eldbus_message_error_new(msg, BLUEZ_ERROR_REJECTED,
                                                  REJECTED_MSG);
   _reply_and_del_dialog(reply, dialog);
}

static void
_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Dialog *dialog = data;
   _reject(NULL, dialog);
}

static void
_ok(void *data EINA_UNUSED, E_Dialog *dialog)
{
   const Eldbus_Message *msg = dialog->data;
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);
   evas_object_event_callback_del_full(dialog->win, EVAS_CALLBACK_DEL, _close, dialog);
   _reply_and_del_dialog(reply, dialog);
}

static void
_ask(const char *title, const char *ask_msg, const char *ok_label,
                  Eldbus_Message *eldbus_message)
{
   E_Dialog *dialog = _create_dialog(title, ask_msg, "dialog-ask", "ask");
   dialog->data = eldbus_message;
   evas_object_event_callback_add(dialog->win, EVAS_CALLBACK_DEL, _close, dialog);
   e_dialog_button_add(dialog, ok_label, NULL, _ok, NULL);
   e_dialog_button_add(dialog, _("Reject"), NULL, _reject, NULL);
   e_dialog_show(dialog);
}

/* Implementation of agent API */
static Eldbus_Message *
_agent_release(const Eldbus_Service_Interface *iface EINA_UNUSED,
               const Eldbus_Message *message)
{
   DBG("Agent Released.");
   return eldbus_message_method_return_new(message);
}

static Eldbus_Message *
_agent_request_pin_code(const Eldbus_Service_Interface *iface EINA_UNUSED,
                        const Eldbus_Message *message)
{
   Eldbus_Message *msg = (Eldbus_Message *)message;
   eldbus_message_ref(msg);
   e_entry_dialog_show(_("Pin Code Requested"), NULL,
                       _("Enter the PinCode above. It should have 1-16 "
                       "characters and can be alphanumeric."), "0000",
                       _("OK"), _("Cancel"), _pincode_ok, _cancel, msg);
   return NULL;
}

static Eldbus_Message *
_agent_request_passkey(const Eldbus_Service_Interface *iface EINA_UNUSED,
                       const Eldbus_Message *message)
{
   Eldbus_Message *msg = (Eldbus_Message *)message;
   eldbus_message_ref(msg);
   e_entry_dialog_show(_("Passkey Requested"), NULL,
                       _("Enter the Passkey above. "
                       "It should be a numeric value between 0-999999."),
                       "0", _("OK"), _("Cancel"), _passkey_ok, _cancel, msg);
   return NULL;
}

static Eldbus_Message *
_agent_display_passkey(const Eldbus_Service_Interface *iface EINA_UNUSED,
                       const Eldbus_Message *message)
{
   const char *device;
   uint32_t passkey;
   uint16_t entered;
   Device *dev;

   if(!eldbus_message_arguments_get(message, "ouq", &device, &passkey, &entered))
     return eldbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, device);
   snprintf(buf, sizeof(buf), _("%d keys were typed on %s. Passkey is %06d"),
            entered, dev->name, passkey);
   _display_msg(N_("Display Passkey"), buf);
   return eldbus_message_method_return_new(message);
}

static Eldbus_Message *
_agent_display_pin_code(const Eldbus_Service_Interface *iface EINA_UNUSED,
                        const Eldbus_Message *message)
{
   const char *device, *pincode;
   Device *dev;

   if(!eldbus_message_arguments_get(message, "os", &device, &pincode))
     return eldbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, device);
   snprintf(buf, sizeof(buf), _("Pincode for %s is %s"), dev->name, pincode);
   _display_msg(N_("Display Pincode"), buf);
   return eldbus_message_method_return_new(message);
}

static Eldbus_Message *
_agent_request_confirmation(const Eldbus_Service_Interface *iface EINA_UNUSED,
                            const Eldbus_Message *message)
{
   const char *device;
   uint32_t passkey;
   Device *dev;

   if(!eldbus_message_arguments_get(message, "ou", &device, &passkey))
     return eldbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, device);
   snprintf(buf, sizeof(buf), _("%06d is the passkey presented in %s?"),
            passkey, dev->name);
   eldbus_message_ref((Eldbus_Message *)message);
   _ask(N_("Confirm Request"), buf, _("Confirm"), (Eldbus_Message *)message);
   return NULL;
}

static Eldbus_Message *
_agent_authorize(const Eldbus_Service_Interface *iface EINA_UNUSED,
                 const Eldbus_Message *message)
{
   const char *device, *uuid;
   Device *dev;

   if(!eldbus_message_arguments_get(message, "os", &device, &uuid))
     return eldbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   dev = eina_list_search_unsorted(ctxt->devices, ebluez4_dev_path_cmp, device);
   snprintf(buf, sizeof(buf), _("Grant permission for %s to connect?"),
            dev->name);
   eldbus_message_ref((Eldbus_Message *)message);
   _ask(N_("Authorize Connection"), buf, _("Grant"), (Eldbus_Message *)message);
   return NULL;
}

static Eldbus_Message *
_agent_cancel(const Eldbus_Service_Interface *iface EINA_UNUSED,
              const Eldbus_Message *message)
{
   DBG("Request canceled.");
   return eldbus_message_method_return_new(message);
}

static const Eldbus_Method agent_methods[] = {
   { "Release", NULL, NULL, _agent_release, 0 },
   { "RequestPinCode", ELDBUS_ARGS({"o", "device"}),
      ELDBUS_ARGS({"s", "pincode"}), _agent_request_pin_code, 0 },
   { "RequestPasskey", ELDBUS_ARGS({"o", "device"}),
      ELDBUS_ARGS({"u", "passkey"}), _agent_request_passkey, 0 },
   { "DisplayPasskey",
      ELDBUS_ARGS({"o", "device"},{"u", "passkey"},{"q", "entered"}),
      NULL, _agent_display_passkey, 0 },
   { "DisplayPinCode", ELDBUS_ARGS({"o", "device"},{"s", "pincode"}),
      NULL, _agent_display_pin_code, 0 },
   { "RequestConfirmation", ELDBUS_ARGS({"o", "device"},{"u", "passkey"}),
      NULL, _agent_request_confirmation, 0 },
   { "Authorize", ELDBUS_ARGS({"o", "device"},{"s", "uuid"}),
      NULL, _agent_authorize, 0 },
   { "Cancel", NULL, NULL, _agent_cancel, 0 },
   { NULL, NULL, NULL, NULL, 0 }
};

static const Eldbus_Service_Interface_Desc agent_iface = {
   AGENT_INTERFACE, agent_methods, NULL, NULL, NULL, NULL
};

/* Public Functions */
void ebluez4_register_agent_interfaces(Eldbus_Connection *conn)
{
   bluez_conn = conn;
   eldbus_service_interface_register(conn, AGENT_PATH, &agent_iface);
   eldbus_service_interface_register(conn, REMOTE_AGENT_PATH, &agent_iface);
}
