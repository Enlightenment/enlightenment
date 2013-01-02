#include "edbus_mpris_media_player2.h"

static int _log_main = -1;
#undef ERR
#define ERR(...) EINA_LOG_DOM_ERR(_log_main, __VA_ARGS__);

void
mpris_media_player2_quit_call(EDBus_Proxy *proxy)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "Quit");
   if (!edbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
mpris_media_player2_raise_call(EDBus_Proxy *proxy)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "Raise");
   if (!edbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

static void
cb_mpris_media_player2_can_quit(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Bool_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   Eina_Bool v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "CanQuit", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanQuit", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanQuit", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanQuit", proxy, NULL, v);
}

EDBus_Pending *
mpris_media_player2_can_quit_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "CanQuit", cb_mpris_media_player2_can_quit, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_can_raise(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Bool_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   Eina_Bool v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "CanRaise", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanRaise", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanRaise", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanRaise", proxy, NULL, v);
}

EDBus_Pending *
mpris_media_player2_can_raise_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "CanRaise", cb_mpris_media_player2_can_raise, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_desktop_entry(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_String_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   const char *v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "DesktopEntry", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "DesktopEntry", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "s", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "DesktopEntry", proxy, &error_info, NULL);
        return;
     }
   cb(user_data, pending, "DesktopEntry", proxy, NULL, v);
}

EDBus_Pending *
mpris_media_player2_desktop_entry_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_String_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "DesktopEntry", cb_mpris_media_player2_desktop_entry, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_has_track_list(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Bool_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   Eina_Bool v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "HasTrackList", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "HasTrackList", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "HasTrackList", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "HasTrackList", proxy, NULL, v);
}

EDBus_Pending *
mpris_media_player2_has_track_list_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "HasTrackList", cb_mpris_media_player2_has_track_list, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_identity(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_String_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   const char *v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "Identity", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Identity", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "s", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Identity", proxy, &error_info, NULL);
        return;
     }
   cb(user_data, pending, "Identity", proxy, NULL, v);
}

EDBus_Pending *
mpris_media_player2_identity_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_String_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "Identity", cb_mpris_media_player2_identity, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_supported_mime_types(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Complex_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   Eina_Value *v, stack_value;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "SupportedMimeTypes", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "SupportedMimeTypes", proxy, &error_info, NULL);
        return;
     }
   v = edbus_message_iter_struct_like_to_eina_value(variant);
   eina_value_struct_value_get(v, "arg0", &stack_value);
   cb(user_data, pending, "SupportedMimeTypes", proxy, NULL, &stack_value);
   eina_value_flush(&stack_value);
   eina_value_free(v);
}

EDBus_Pending *
mpris_media_player2_supported_mime_types_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Complex_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "SupportedMimeTypes", cb_mpris_media_player2_supported_mime_types, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_supported_uri_schemes(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Complex_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   Eina_Value *v, stack_value;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "SupportedUriSchemes", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "SupportedUriSchemes", proxy, &error_info, NULL);
        return;
     }
   v = edbus_message_iter_struct_like_to_eina_value(variant);
   eina_value_struct_value_get(v, "arg0", &stack_value);
   cb(user_data, pending, "SupportedUriSchemes", proxy, NULL, &stack_value);
   eina_value_flush(&stack_value);
   eina_value_free(v);
}

EDBus_Pending *
mpris_media_player2_supported_uri_schemes_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Complex_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "SupportedUriSchemes", cb_mpris_media_player2_supported_uri_schemes, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

void
mpris_media_player2_log_domain_set(int id)
{
   _log_main = id;
}

void
mpris_media_player2_proxy_unref(EDBus_Proxy *proxy)
{
   EDBus_Object *obj = edbus_proxy_object_get(proxy);
   edbus_proxy_unref(proxy);
   edbus_object_unref(obj);
}

EDBus_Proxy *
mpris_media_player2_proxy_get(EDBus_Connection *conn, const char *bus, const char *path)
{
   EDBus_Object *obj;
   EDBus_Proxy *proxy;
   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(bus, NULL);
   if (!path) path = "/org/mpris/MediaPlayer2";
   obj = edbus_object_get(conn, bus, path);
   proxy = edbus_proxy_get(obj, "org.mpris.MediaPlayer2");
   return proxy;
}
