#include "eldbus_mpris_media_player2.h"

static int _log_main = -1;
#undef ERR
#define ERR(...) EINA_LOG_DOM_ERR(_log_main, __VA_ARGS__);

void
mpris_media_player2_quit_call(Eldbus_Proxy *proxy)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "Quit");
   if (!eldbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
mpris_media_player2_raise_call(Eldbus_Proxy *proxy)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "Raise");
   if (!eldbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

static void
cb_mpris_media_player2_can_quit(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Bool_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   Eina_Bool v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "CanQuit", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanQuit", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanQuit", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanQuit", proxy, NULL, v);
}

Eldbus_Pending *
mpris_media_player2_can_quit_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "CanQuit", cb_mpris_media_player2_can_quit, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_can_raise(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Bool_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   Eina_Bool v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "CanRaise", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanRaise", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanRaise", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanRaise", proxy, NULL, v);
}

Eldbus_Pending *
mpris_media_player2_can_raise_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "CanRaise", cb_mpris_media_player2_can_raise, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_desktop_entry(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_String_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   const char *v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "DesktopEntry", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "DesktopEntry", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "s", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "DesktopEntry", proxy, &error_info, NULL);
        return;
     }
   cb(user_data, pending, "DesktopEntry", proxy, NULL, v);
}

Eldbus_Pending *
mpris_media_player2_desktop_entry_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_String_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "DesktopEntry", cb_mpris_media_player2_desktop_entry, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_has_track_list(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Bool_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   Eina_Bool v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "HasTrackList", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "HasTrackList", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "HasTrackList", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "HasTrackList", proxy, NULL, v);
}

Eldbus_Pending *
mpris_media_player2_has_track_list_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "HasTrackList", cb_mpris_media_player2_has_track_list, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_identity(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_String_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   const char *v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "Identity", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Identity", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "s", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Identity", proxy, &error_info, NULL);
        return;
     }
   cb(user_data, pending, "Identity", proxy, NULL, v);
}

Eldbus_Pending *
mpris_media_player2_identity_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_String_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "Identity", cb_mpris_media_player2_identity, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_supported_mime_types(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Complex_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   Eina_Value *v, stack_value;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "SupportedMimeTypes", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "SupportedMimeTypes", proxy, &error_info, NULL);
        return;
     }
   v = eldbus_message_iter_struct_like_to_eina_value(variant);
   eina_value_struct_value_get(v, "arg0", &stack_value);
   cb(user_data, pending, "SupportedMimeTypes", proxy, NULL, &stack_value);
   eina_value_flush(&stack_value);
   eina_value_free(v);
}

Eldbus_Pending *
mpris_media_player2_supported_mime_types_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Complex_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "SupportedMimeTypes", cb_mpris_media_player2_supported_mime_types, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_mpris_media_player2_supported_uri_schemes(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Complex_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   Eina_Value *v, stack_value;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "SupportedUriSchemes", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "SupportedUriSchemes", proxy, &error_info, NULL);
        return;
     }
   v = eldbus_message_iter_struct_like_to_eina_value(variant);
   eina_value_struct_value_get(v, "arg0", &stack_value);
   cb(user_data, pending, "SupportedUriSchemes", proxy, NULL, &stack_value);
   eina_value_flush(&stack_value);
   eina_value_free(v);
}

Eldbus_Pending *
mpris_media_player2_supported_uri_schemes_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Complex_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "SupportedUriSchemes", cb_mpris_media_player2_supported_uri_schemes, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

void
mpris_media_player2_log_domain_set(int id)
{
   _log_main = id;
}

void
mpris_media_player2_proxy_unref(Eldbus_Proxy *proxy)
{
   Eldbus_Object *obj = eldbus_proxy_object_get(proxy);
   eldbus_proxy_unref(proxy);
   eldbus_object_unref(obj);
}

Eldbus_Proxy *
mpris_media_player2_proxy_get(Eldbus_Connection *conn, const char *bus, const char *path)
{
   Eldbus_Object *obj;
   Eldbus_Proxy *proxy;
   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(bus, NULL);
   if (!path) path = "/org/mpris/MediaPlayer2";
   obj = eldbus_object_get(conn, bus, path);
   proxy = eldbus_proxy_get(obj, "org.mpris.MediaPlayer2");
   return proxy;
}
