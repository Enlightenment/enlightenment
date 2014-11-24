#include "eldbus_media_player2_player.h"

static int _log_main = -1;
#undef ERR
#define ERR(...) EINA_LOG_DOM_ERR(_log_main, __VA_ARGS__);
int MEDIA_PLAYER2_PLAYER_SEEKED_EVENT = 0;

void
media_player2_player_next_call(Eldbus_Proxy *proxy)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "Next");
   if (!eldbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_open_uri_call(Eldbus_Proxy *proxy, const char *arg0)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "OpenUri");
   if (!eldbus_message_arguments_append(msg, "s", arg0))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_pause_call(Eldbus_Proxy *proxy)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "Pause");
   if (!eldbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_play_call(Eldbus_Proxy *proxy)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "Play");
   if (!eldbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_play_pause_call(Eldbus_Proxy *proxy)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "PlayPause");
   if (!eldbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_previous_call(Eldbus_Proxy *proxy)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "Previous");
   if (!eldbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_seek_call(Eldbus_Proxy *proxy, int64_t arg0)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "Seek");
   if (!eldbus_message_arguments_append(msg, "x", arg0))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_set_position_call(Eldbus_Proxy *proxy, const char *arg0, int64_t arg1)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "SetPosition");
   if (!eldbus_message_arguments_append(msg, "ox", arg0, arg1))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_stop_call(Eldbus_Proxy *proxy)
{
   Eldbus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = eldbus_proxy_method_call_new(proxy, "Stop");
   if (!eldbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        eldbus_message_unref(msg);
        return;
     }
   eldbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

static void
media_player2_player_seeked_data_free(void *user_data EINA_UNUSED, void *func_data)
{
   Media_Player2_Player_Seeked_Data *s_data = func_data;
   free(s_data);
}

static void
on_media_player2_player_seeked(void *data, const Eldbus_Message *msg)
{
   Eldbus_Proxy *proxy = data;
   Media_Player2_Player_Seeked_Data *s_data = calloc(1, sizeof(Media_Player2_Player_Seeked_Data));
   s_data->proxy = proxy;
   if (!eldbus_message_arguments_get(msg, "x", &s_data->arg0))
     {
        ERR("Error: Getting arguments from message.");
        free(s_data);
        return;
     }
   ecore_event_add(MEDIA_PLAYER2_PLAYER_SEEKED_EVENT, s_data, media_player2_player_seeked_data_free, NULL);
}

static void
cb_media_player2_player_can_control(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "CanControl", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanControl", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanControl", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanControl", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_can_control_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "CanControl", cb_media_player2_player_can_control, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_go_next(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "CanGoNext", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanGoNext", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanGoNext", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanGoNext", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_can_go_next_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "CanGoNext", cb_media_player2_player_can_go_next, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_go_previous(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "CanGoPrevious", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanGoPrevious", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanGoPrevious", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanGoPrevious", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_can_go_previous_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "CanGoPrevious", cb_media_player2_player_can_go_previous, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_pause(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "CanPause", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanPause", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanPause", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanPause", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_can_pause_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "CanPause", cb_media_player2_player_can_pause, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_play(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "CanPlay", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanPlay", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanPlay", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanPlay", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_can_play_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "CanPlay", cb_media_player2_player_can_play, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_seek(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "CanSeek", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanSeek", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanSeek", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanSeek", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_can_seek_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "CanSeek", cb_media_player2_player_can_seek, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_loop_status(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "LoopStatus", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "LoopStatus", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "s", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "LoopStatus", proxy, &error_info, NULL);
        return;
     }
   cb(user_data, pending, "LoopStatus", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_loop_status_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_String_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "LoopStatus", cb_media_player2_player_loop_status, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_loop_status_set(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   const char *error, *error_msg;
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Codegen_Property_Set_Cb cb = data;
   if (eldbus_message_error_get(msg, &error, &error_msg))     {
        Eldbus_Error_Info error_info = {error, error_msg};

        cb(user_data, "LoopStatus", proxy, pending, &error_info);
        return;
     }
   cb(user_data, "LoopStatus", proxy, pending, NULL);
}

Eldbus_Pending *
media_player2_player_loop_status_propset(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Set_Cb cb, const void *data, const void *value)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(value, NULL);
   p = eldbus_proxy_property_set(proxy, "LoopStatus", "s", value, cb_media_player2_player_loop_status_set, cb);
   eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_maximum_rate(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Double_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   double v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "MaximumRate", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "MaximumRate", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "d", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "MaximumRate", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "MaximumRate", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_maximum_rate_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Double_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "MaximumRate", cb_media_player2_player_maximum_rate, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_metadata(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "Metadata", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Metadata", proxy, &error_info, NULL);
        return;
     }
   v = eldbus_message_iter_struct_like_to_eina_value(variant);
   eina_value_struct_value_get(v, "arg0", &stack_value);
   cb(user_data, pending, "Metadata", proxy, NULL, &stack_value);
   eina_value_flush(&stack_value);
   eina_value_free(v);
}

Eldbus_Pending *
media_player2_player_metadata_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Complex_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "Metadata", cb_media_player2_player_metadata, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_minimum_rate(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Double_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   double v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "MinimumRate", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "MinimumRate", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "d", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "MinimumRate", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "MinimumRate", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_minimum_rate_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Double_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "MinimumRate", cb_media_player2_player_minimum_rate, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_playback_status(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "PlaybackStatus", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "PlaybackStatus", proxy, &error_info, NULL);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "s", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "PlaybackStatus", proxy, &error_info, NULL);
        return;
     }
   cb(user_data, pending, "PlaybackStatus", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_playback_status_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_String_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "PlaybackStatus", cb_media_player2_player_playback_status, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_position(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Int64_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   int64_t v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "Position", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Position", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "x", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Position", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "Position", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_position_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Int64_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "Position", cb_media_player2_player_position, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_rate(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Double_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   double v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "Rate", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Rate", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "d", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Rate", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "Rate", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_rate_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Double_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "Rate", cb_media_player2_player_rate, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_rate_set(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   const char *error, *error_msg;
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Codegen_Property_Set_Cb cb = data;
   if (eldbus_message_error_get(msg, &error, &error_msg))     {
        Eldbus_Error_Info error_info = {error, error_msg};

        cb(user_data, "Rate", proxy, pending, &error_info);
        return;
     }
   cb(user_data, "Rate", proxy, pending, NULL);
}

Eldbus_Pending *
media_player2_player_rate_propset(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Set_Cb cb, const void *data, const void *value)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(value, NULL);
   p = eldbus_proxy_property_set(proxy, "Rate", "d", value, cb_media_player2_player_rate_set, cb);
   eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_shuffle(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
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
        cb(user_data, pending, "Shuffle", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Shuffle", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "b", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Shuffle", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "Shuffle", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_shuffle_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "Shuffle", cb_media_player2_player_shuffle, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_shuffle_set(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   const char *error, *error_msg;
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Codegen_Property_Set_Cb cb = data;
   if (eldbus_message_error_get(msg, &error, &error_msg))     {
        Eldbus_Error_Info error_info = {error, error_msg};

        cb(user_data, "Shuffle", proxy, pending, &error_info);
        return;
     }
   cb(user_data, "Shuffle", proxy, pending, NULL);
}

Eldbus_Pending *
media_player2_player_shuffle_propset(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Set_Cb cb, const void *data, const void *value)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(value, NULL);
   p = eldbus_proxy_property_set(proxy, "Shuffle", "b", value, cb_media_player2_player_shuffle_set, cb);
   eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_volume(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   Eldbus_Codegen_Property_Double_Get_Cb cb = data;
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Message_Iter *variant;
   double v;
   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        Eldbus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "Volume", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "v", &variant))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Volume", proxy, &error_info, 0);
        return;
     }
   if (!eldbus_message_iter_arguments_get(variant, "d", &v))
     {
        Eldbus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Volume", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "Volume", proxy, NULL, v);
}

Eldbus_Pending *
media_player2_player_volume_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Double_Get_Cb cb, const void *data)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = eldbus_proxy_property_get(proxy, "Volume", cb_media_player2_player_volume, cb);
   if (data)
     eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_volume_set(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   const char *error, *error_msg;
   void *user_data = eldbus_pending_data_del(pending, "__user_data");
   Eldbus_Proxy *proxy = eldbus_pending_data_del(pending, "__proxy");
   Eldbus_Codegen_Property_Set_Cb cb = data;
   if (eldbus_message_error_get(msg, &error, &error_msg))     {
        Eldbus_Error_Info error_info = {error, error_msg};

        cb(user_data, "Volume", proxy, pending, &error_info);
        return;
     }
   cb(user_data, "Volume", proxy, pending, NULL);
}

Eldbus_Pending *
media_player2_player_volume_propset(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Set_Cb cb, const void *data, const void *value)
{
   Eldbus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(value, NULL);
   p = eldbus_proxy_property_set(proxy, "Volume", "d", value, cb_media_player2_player_volume_set, cb);
   eldbus_pending_data_set(p, "__user_data", data);
   eldbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

void
media_player2_player_log_domain_set(int id)
{
   _log_main = id;
}

void
media_player2_player_proxy_unref(Eldbus_Proxy *proxy)
{
   Eldbus_Object *obj = eldbus_proxy_object_get(proxy);
   eldbus_proxy_unref(proxy);
   eldbus_object_unref(obj);
}

Eldbus_Proxy *
media_player2_player_proxy_get(Eldbus_Connection *conn, const char *bus, const char *path)
{
   Eldbus_Object *obj;
   Eldbus_Proxy *proxy;
   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(bus, NULL);
   if (!path) path = "/org/mpris/MediaPlayer2";
   obj = eldbus_object_get(conn, bus, path);
   proxy = eldbus_proxy_get(obj, "org.mpris.MediaPlayer2.Player");
   eldbus_proxy_signal_handler_add(proxy, "Seeked", on_media_player2_player_seeked, proxy);
   if (!MEDIA_PLAYER2_PLAYER_SEEKED_EVENT)
     MEDIA_PLAYER2_PLAYER_SEEKED_EVENT = ecore_event_type_new();
   return proxy;
}
