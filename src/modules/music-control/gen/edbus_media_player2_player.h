#ifndef EDBUS_MEDIA_PLAYER2_PLAYER_H
#define EDBUS_MEDIA_PLAYER2_PLAYER_H

#include <Eina.h>
#include <Ecore.h>
#include <EDBus.h>
#include "edbus_utils.h"

EDBus_Proxy *media_player2_player_proxy_get(EDBus_Connection *conn, const char *bus, const char *path);
void media_player2_player_proxy_unref(EDBus_Proxy *proxy);
void media_player2_player_log_domain_set(int id);
void media_player2_player_next_call(EDBus_Proxy *proxy);
void media_player2_player_open_uri_call(EDBus_Proxy *proxy, const char *arg0);
void media_player2_player_pause_call(EDBus_Proxy *proxy);
void media_player2_player_play_call(EDBus_Proxy *proxy);
void media_player2_player_play_pause_call(EDBus_Proxy *proxy);
void media_player2_player_previous_call(EDBus_Proxy *proxy);
void media_player2_player_seek_call(EDBus_Proxy *proxy, int64_t arg0);
void media_player2_player_set_position_call(EDBus_Proxy *proxy, const char *arg0, int64_t arg1);
void media_player2_player_stop_call(EDBus_Proxy *proxy);
extern int MEDIA_PLAYER2_PLAYER_SEEKED_EVENT;
typedef struct _Media_Player2_Player_Seeked_Data
{
   EDBus_Proxy *proxy;
   int64_t arg0;
} Media_Player2_Player_Seeked_Data;
EDBus_Pending *media_player2_player_can_control_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_can_go_next_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_can_go_previous_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_can_pause_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_can_play_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_can_seek_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_loop_status_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_String_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_loop_status_propset(EDBus_Proxy *proxy, EDBus_Codegen_Property_Set_Cb cb, const void *data, const void *value);
EDBus_Pending *media_player2_player_maximum_rate_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Double_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_metadata_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Complex_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_minimum_rate_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Double_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_playback_status_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_String_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_position_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Int64_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_rate_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Double_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_rate_propset(EDBus_Proxy *proxy, EDBus_Codegen_Property_Set_Cb cb, const void *data, const void *value);
EDBus_Pending *media_player2_player_shuffle_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_shuffle_propset(EDBus_Proxy *proxy, EDBus_Codegen_Property_Set_Cb cb, const void *data, const void *value);
EDBus_Pending *media_player2_player_volume_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Double_Get_Cb cb, const void *data);
EDBus_Pending *media_player2_player_volume_propset(EDBus_Proxy *proxy, EDBus_Codegen_Property_Set_Cb cb, const void *data, const void *value);

#endif