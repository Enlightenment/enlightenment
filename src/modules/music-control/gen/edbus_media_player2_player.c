#include "edbus_media_player2_player.h"

static int _log_main = -1;
#undef ERR
#define ERR(...) EINA_LOG_DOM_ERR(_log_main, __VA_ARGS__);
int MEDIA_PLAYER2_PLAYER_SEEKED_EVENT;

void
media_player2_player_next_call(EDBus_Proxy *proxy)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "Next");
   if (!edbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_open_uri_call(EDBus_Proxy *proxy, const char *arg0)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "OpenUri");
   if (!edbus_message_arguments_append(msg, "s", arg0))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_pause_call(EDBus_Proxy *proxy)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "Pause");
   if (!edbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_play_call(EDBus_Proxy *proxy)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "Play");
   if (!edbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_play_pause_call(EDBus_Proxy *proxy)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "PlayPause");
   if (!edbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_previous_call(EDBus_Proxy *proxy)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "Previous");
   if (!edbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_seek_call(EDBus_Proxy *proxy, int64_t arg0)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "Seek");
   if (!edbus_message_arguments_append(msg, "x", arg0))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_set_position_call(EDBus_Proxy *proxy, const char *arg0, int64_t arg1)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "SetPosition");
   if (!edbus_message_arguments_append(msg, "ox", arg0, arg1))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

void
media_player2_player_stop_call(EDBus_Proxy *proxy)
{
   EDBus_Message *msg;
   EINA_SAFETY_ON_NULL_RETURN(proxy);
   msg = edbus_proxy_method_call_new(proxy, "Stop");
   if (!edbus_message_arguments_append(msg, ""))
     {
        ERR("Error: Filling message.");
        return;
     }
   edbus_proxy_send(proxy, msg, NULL, NULL, -1);
}

static void
media_player2_player_seeked_data_free(void *user_data, void *func_data)
{
   Media_Player2_Player_Seeked_Data *s_data = user_data;
   free(s_data);
}

static void
on_media_player2_player_seeked(void *data, const EDBus_Message *msg)
{
   EDBus_Proxy *proxy = data;
   Media_Player2_Player_Seeked_Data *s_data = calloc(1, sizeof(Media_Player2_Player_Seeked_Data));
   s_data->proxy = proxy;
   if (!edbus_message_arguments_get(msg, "x", &s_data->arg0))
     {
        ERR("Error: Getting arguments from message.");
        return;
     }
   ecore_event_add(MEDIA_PLAYER2_PLAYER_SEEKED_EVENT, s_data, media_player2_player_seeked_data_free, NULL);
}

static void
cb_media_player2_player_can_control(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "CanControl", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanControl", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanControl", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanControl", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_can_control_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "CanControl", cb_media_player2_player_can_control, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_go_next(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "CanGoNext", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanGoNext", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanGoNext", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanGoNext", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_can_go_next_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "CanGoNext", cb_media_player2_player_can_go_next, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_go_previous(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "CanGoPrevious", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanGoPrevious", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanGoPrevious", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanGoPrevious", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_can_go_previous_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "CanGoPrevious", cb_media_player2_player_can_go_previous, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_pause(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "CanPause", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanPause", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanPause", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanPause", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_can_pause_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "CanPause", cb_media_player2_player_can_pause, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_play(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "CanPlay", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanPlay", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanPlay", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanPlay", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_can_play_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "CanPlay", cb_media_player2_player_can_play, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_can_seek(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "CanSeek", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanSeek", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "CanSeek", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "CanSeek", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_can_seek_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "CanSeek", cb_media_player2_player_can_seek, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_loop_status(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "LoopStatus", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "LoopStatus", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "s", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "LoopStatus", proxy, &error_info, NULL);
        return;
     }
   cb(user_data, pending, "LoopStatus", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_loop_status_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_String_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "LoopStatus", cb_media_player2_player_loop_status, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_loop_status_set(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   const char *error, *error_msg;
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Codegen_Property_Set_Cb cb = data;
   if (edbus_message_error_get(msg, &error, &error_msg))     {
        EDBus_Error_Info error_info = {error, error_msg};

        cb(user_data, "LoopStatus", proxy, pending, &error_info);
        return;
     }
   cb(user_data, "LoopStatus", proxy, pending, NULL);
}

EDBus_Pending *
media_player2_player_loop_status_propset(EDBus_Proxy *proxy, EDBus_Codegen_Property_Set_Cb cb, const void *data, const void *value)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(value, NULL);
   p = edbus_proxy_property_set(proxy, "LoopStatus", "s", value, cb_media_player2_player_loop_status_set, data);
   edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_maximum_rate(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Double_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   double v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "MaximumRate", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "MaximumRate", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "d", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "MaximumRate", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "MaximumRate", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_maximum_rate_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Double_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "MaximumRate", cb_media_player2_player_maximum_rate, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_metadata(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "Metadata", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Metadata", proxy, &error_info, NULL);
        return;
     }
   v = edbus_message_iter_struct_like_to_eina_value(variant);
   eina_value_struct_value_get(v, "arg0", &stack_value);
   cb(user_data, pending, "Metadata", proxy, NULL, &stack_value);
   eina_value_flush(&stack_value);
   eina_value_free(v);
}

EDBus_Pending *
media_player2_player_metadata_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Complex_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "Metadata", cb_media_player2_player_metadata, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_minimum_rate(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Double_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   double v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "MinimumRate", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "MinimumRate", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "d", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "MinimumRate", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "MinimumRate", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_minimum_rate_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Double_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "MinimumRate", cb_media_player2_player_minimum_rate, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_playback_status(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "PlaybackStatus", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "PlaybackStatus", proxy, &error_info, NULL);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "s", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "PlaybackStatus", proxy, &error_info, NULL);
        return;
     }
   cb(user_data, pending, "PlaybackStatus", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_playback_status_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_String_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "PlaybackStatus", cb_media_player2_player_playback_status, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_position(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Int64_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   int64_t v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "Position", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Position", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "x", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Position", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "Position", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_position_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Int64_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "Position", cb_media_player2_player_position, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_rate(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Double_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   double v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "Rate", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Rate", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "d", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Rate", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "Rate", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_rate_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Double_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "Rate", cb_media_player2_player_rate, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_rate_set(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   const char *error, *error_msg;
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Codegen_Property_Set_Cb cb = data;
   if (edbus_message_error_get(msg, &error, &error_msg))     {
        EDBus_Error_Info error_info = {error, error_msg};

        cb(user_data, "Rate", proxy, pending, &error_info);
        return;
     }
   cb(user_data, "Rate", proxy, pending, NULL);
}

EDBus_Pending *
media_player2_player_rate_propset(EDBus_Proxy *proxy, EDBus_Codegen_Property_Set_Cb cb, const void *data, const void *value)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(value, NULL);
   p = edbus_proxy_property_set(proxy, "Rate", "d", value, cb_media_player2_player_rate_set, data);
   edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_shuffle(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
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
        cb(user_data, pending, "Shuffle", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Shuffle", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "b", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Shuffle", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "Shuffle", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_shuffle_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "Shuffle", cb_media_player2_player_shuffle, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_shuffle_set(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   const char *error, *error_msg;
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Codegen_Property_Set_Cb cb = data;
   if (edbus_message_error_get(msg, &error, &error_msg))     {
        EDBus_Error_Info error_info = {error, error_msg};

        cb(user_data, "Shuffle", proxy, pending, &error_info);
        return;
     }
   cb(user_data, "Shuffle", proxy, pending, NULL);
}

EDBus_Pending *
media_player2_player_shuffle_propset(EDBus_Proxy *proxy, EDBus_Codegen_Property_Set_Cb cb, const void *data, const void *value)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(value, NULL);
   p = edbus_proxy_property_set(proxy, "Shuffle", "b", value, cb_media_player2_player_shuffle_set, data);
   edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_volume(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   const char *error, *error_msg;
   EDBus_Codegen_Property_Double_Get_Cb cb = data;
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Message_Iter *variant;
   double v;
   if (edbus_message_error_get(msg, &error, &error_msg))
     {
        EDBus_Error_Info error_info = {error, error_msg};
        cb(user_data, pending, "Volume", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_arguments_get(msg, "v", &variant))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Volume", proxy, &error_info, 0);
        return;
     }
   if (!edbus_message_iter_arguments_get(variant, "d", &v))
     {
        EDBus_Error_Info error_info = {"", ""};
        cb(user_data, pending, "Volume", proxy, &error_info, 0);
        return;
     }
   cb(user_data, pending, "Volume", proxy, NULL, v);
}

EDBus_Pending *
media_player2_player_volume_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Double_Get_Cb cb, const void *data)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   p = edbus_proxy_property_get(proxy, "Volume", cb_media_player2_player_volume, cb);
   if (data)
     edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

static void
cb_media_player2_player_volume_set(void *data, const EDBus_Message *msg, EDBus_Pending *pending)
{
   const char *error, *error_msg;
   void *user_data = edbus_pending_data_del(pending, "__user_data");
   EDBus_Proxy *proxy = edbus_pending_data_del(pending, "__proxy");
   EDBus_Codegen_Property_Set_Cb cb = data;
   if (edbus_message_error_get(msg, &error, &error_msg))     {
        EDBus_Error_Info error_info = {error, error_msg};

        cb(user_data, "Volume", proxy, pending, &error_info);
        return;
     }
   cb(user_data, "Volume", proxy, pending, NULL);
}

EDBus_Pending *
media_player2_player_volume_propset(EDBus_Proxy *proxy, EDBus_Codegen_Property_Set_Cb cb, const void *data, const void *value)
{
   EDBus_Pending *p;
   EINA_SAFETY_ON_NULL_RETURN_VAL(proxy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(value, NULL);
   p = edbus_proxy_property_set(proxy, "Volume", "d", value, cb_media_player2_player_volume_set, data);
   edbus_pending_data_set(p, "__user_data", data);
   edbus_pending_data_set(p, "__proxy", proxy);
   return p;
}

void
media_player2_player_log_domain_set(int id)
{
   _log_main = id;
}

void
media_player2_player_proxy_unref(EDBus_Proxy *proxy)
{
   EDBus_Object *obj = edbus_proxy_object_get(proxy);
   edbus_proxy_unref(proxy);
   edbus_object_unref(obj);
}

EDBus_Proxy *
media_player2_player_proxy_get(EDBus_Connection *conn, const char *bus, const char *path)
{
   EDBus_Object *obj;
   EDBus_Proxy *proxy;
   EINA_SAFETY_ON_NULL_RETURN_VAL(conn, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(bus, NULL);
   if (!path) path = "/org/mpris/MediaPlayer2";
   obj = edbus_object_get(conn, bus, path);
   proxy = edbus_proxy_get(obj, "org.mpris.MediaPlayer2.Player");
   edbus_proxy_signal_handler_add(proxy, "Seeked", on_media_player2_player_seeked, proxy);
   if (!MEDIA_PLAYER2_PLAYER_SEEKED_EVENT)
     MEDIA_PLAYER2_PLAYER_SEEKED_EVENT = ecore_event_type_new();
   return proxy;
}
