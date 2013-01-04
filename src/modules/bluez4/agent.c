#include <inttypes.h>
#include "e.h"
#include "agent.h"

#define AGENT_INTERFACE "org.bluez.Agent"
#define BLUEZ_ERROR_FAILED "org.bluez.Error.Failed"
#define GET_ERROR_MSG "edbus_message_arguments_get() error"

static EDBus_Message *
_agent_release(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   DBG("Agent Released.");
   return edbus_message_method_return_new(message);
}

static EDBus_Message *
_agent_request_pin_code(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   //FIXME: generate random number and show it in dialog
   EDBus_Message *reply = edbus_message_method_return_new(message);
   edbus_message_arguments_set(reply, "s", "123456");
   DBG("Pin Code Requested.");
   return reply;
}

static EDBus_Message *
_agent_request_passkey(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   //FIXME: generate random number and show it in dialog
   EDBus_Message *reply = edbus_message_method_return_new(message);
   edbus_message_arguments_set(reply, "s", "123456");
   DBG("Passkey Requested.");
   return reply;
}

static EDBus_Message *
_agent_display_passkey(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   //FIXME: show passkey in dialog
   const char *device;
   uint32_t passkey;
   uint16_t entered;
   if(!edbus_message_arguments_get(message, "ouq", &device, &passkey, &entered))
     return edbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   DBG("Device: %s", device);
   DBG("Passkey: %u", passkey);
   DBG("Entered: %d", entered);
   return edbus_message_method_return_new(message);
}

static EDBus_Message *
_agent_display_pin_code(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   //FIXME: show passkey in dialog
   const char *device, *pincode;
   if(!edbus_message_arguments_get(message, "os", &device, &pincode))
     return edbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   DBG("Device: %s", device);
   DBG("Passkey: %s", pincode);
   return edbus_message_method_return_new(message);
}

static EDBus_Message *
_agent_request_confirmation(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   //FIXME: Ask for confirmation in dialog
   const char *device;
   uint32_t passkey;
   if(!edbus_message_arguments_get(message, "ou", &device, &passkey))
     return edbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   DBG("Confirming request of %u for device %s", passkey, device);
   return edbus_message_method_return_new(message);
}

static EDBus_Message *
_agent_authorize(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   //FIXME: Ask for authorization in dialog
   const char *device, *uuid;
   if(!edbus_message_arguments_get(message, "os", &device, &uuid))
     return edbus_message_error_new(message, BLUEZ_ERROR_FAILED, GET_ERROR_MSG);
   DBG("Authorizing request for %s", device);
   return edbus_message_method_return_new(message);
}

static EDBus_Message *
_agent_cancel(const EDBus_Service_Interface *iface, const EDBus_Message *message)
{
   DBG("Request canceled.");
   return edbus_message_method_return_new(message);
}

static const EDBus_Method agent_methods[] = {
   { "Release", NULL, NULL, _agent_release, 0 },
   {
      "RequestPinCode", EDBUS_ARGS({"o", "device"}),
      EDBUS_ARGS({"s", "pincode"}), _agent_request_pin_code, 0
   },
   {
      "RequestPasskey", EDBUS_ARGS({"o", "device"}),
      EDBUS_ARGS({"u", "passkey"}), _agent_request_passkey, 0
   },
   {
      "DisplayPasskey",
      EDBUS_ARGS({"o", "device"},{"u", "passkey"},{"q", "entered"}),
      NULL, _agent_display_passkey, 0
   },
   {
      "DisplayPinCode", EDBUS_ARGS({"o", "device"},{"s", "pincode"}),
      NULL, _agent_display_pin_code, 0
   },
   {
      "RequestConfirmation", EDBUS_ARGS({"o", "device"},{"u", "passkey"}),
      NULL, _agent_request_confirmation, 0
   },
   {
      "Authorize", EDBUS_ARGS({"o", "device"},{"s", "uuid"}),
      NULL, _agent_authorize, 0
   },
   { "Cancel", NULL, NULL, _agent_cancel, 0 },
   { NULL, NULL, NULL, NULL, 0 }
};

static const EDBus_Service_Interface_Desc agent_iface = {
   AGENT_INTERFACE, agent_methods, NULL, NULL, NULL, NULL
};

/* Public Functions */
void ebluez4_register_agent_interfaces(EDBus_Connection *conn)
{
   edbus_service_interface_register(conn, AGENT_PATH, &agent_iface);
   edbus_service_interface_register(conn, REMOTE_AGENT_PATH, &agent_iface);
}
