#ifndef ELDBUS_MPRIS_MEDIA_PLAYER2_H
#define ELDBUS_MPRIS_MEDIA_PLAYER2_H

#include <Eina.h>
#include <Ecore.h>
#include <Eldbus.h>
#include "eldbus_utils.h"

Eldbus_Proxy *mpris_media_player2_proxy_get(Eldbus_Connection *conn, const char *bus, const char *path);
void mpris_media_player2_proxy_unref(Eldbus_Proxy *proxy);
void mpris_media_player2_log_domain_set(int id);
void mpris_media_player2_quit_call(Eldbus_Proxy *proxy);
void mpris_media_player2_raise_call(Eldbus_Proxy *proxy);
Eldbus_Pending *mpris_media_player2_can_quit_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *mpris_media_player2_can_raise_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *mpris_media_player2_desktop_entry_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_String_Get_Cb cb, const void *data);
Eldbus_Pending *mpris_media_player2_has_track_list_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Bool_Get_Cb cb, const void *data);
Eldbus_Pending *mpris_media_player2_identity_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_String_Get_Cb cb, const void *data);
Eldbus_Pending *mpris_media_player2_supported_mime_types_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Complex_Get_Cb cb, const void *data);
Eldbus_Pending *mpris_media_player2_supported_uri_schemes_propget(Eldbus_Proxy *proxy, Eldbus_Codegen_Property_Complex_Get_Cb cb, const void *data);

#endif