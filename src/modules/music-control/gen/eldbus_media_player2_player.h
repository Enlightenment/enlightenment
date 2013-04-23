#ifndef ELDBUS_MEDIA_PLAYER2_PLAYER_H
#define ELDBUS_MEDIA_PLAYER2_PLAYER_H

#include <Eina.h>
#include <Ecore.h>
#include <Eldbus.h>
#include "eldbus_utils.h"

Eldbus_Proxy *media_player2_player_proxy_get(Eldbus_Connection *conn, const char *bus, const char *path);
void media_player2_player_proxy_unref(Eldbus_Proxy *proxy);
void media_player2_player_log_domain_set(int id);
void media_player2_player_next_call(Eldbus_Proxy *proxy);
void media_player2_player_open_uri_call(Eldbus_Proxy *proxy, const char *arg0);
void media_player2_player_pause_call(Eldbus_Proxy *proxy);
void media_player2_player_play_call(Eldbus_Proxy *proxy);
void media_player2_player_play_pause_call(Eldbus_Proxy *proxy);
void media_player2_player_previous_call(Eldbus_Proxy *proxy);
void media_player2_player_seek_call(Eldbus_Proxy *proxy, int64_t arg0);
void media_player2_player_set_position_call(Eldbus_Proxy *proxy, const char *arg0, int64_t arg1);
void media_player2_player_stop_call(Eldbus_Proxy *proxy);
extern int MEDIA_PLAYER2_PLAYER_SEEKED_EVENT;
typedef struct _Media_Player2_Player_Seeked_Data
{
   Eldbus_Proxy *proxy;
   int64_t arg0;
} Media_Player2_Player_Seeked_Data;
Eldbus_Pending *media_player2_player_can_control_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_can_go_next_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_can_go_previous_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_can_pause_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_can_play_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_can_seek_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_loop_status_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_String_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_loop_status_propset(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Set_Cb cb, const void *data, const void *value);
Eldbus_Pending *media_player2_player_maximum_rate_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Double_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_metadata_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Complex_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_minimum_rate_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Double_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_playback_status_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_String_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_position_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Int64_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_rate_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Double_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_rate_propset(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Set_Cb cb, const void *data, const void *value);
Eldbus_Pending *media_player2_player_shuffle_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_shuffle_propset(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Set_Cb cb, const void *data, const void *value);
Eldbus_Pending *media_player2_player_volume_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Double_Get_Cb cb, const void *data);
Eldbus_Pending *media_player2_player_volume_propset(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Set_Cb cb, const void *data, const void *value);

#endif