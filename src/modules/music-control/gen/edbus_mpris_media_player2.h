#ifndef EDBUS_MPRIS_MEDIA_PLAYER2_H
#define EDBUS_MPRIS_MEDIA_PLAYER2_H

#include <Eina.h>
#include <Ecore.h>
#include <EDBus.h>
#include "edbus_utils.h"

EDBus_Proxy *mpris_media_player2_proxy_get(EDBus_Connection *conn, const char *bus, const char *path);
void mpris_media_player2_proxy_unref(EDBus_Proxy *proxy);
void mpris_media_player2_log_domain_set(int id);
void mpris_media_player2_quit_call(EDBus_Proxy *proxy);
void mpris_media_player2_raise_call(EDBus_Proxy *proxy);
EDBus_Pending *mpris_media_player2_can_quit_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *mpris_media_player2_can_raise_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *mpris_media_player2_desktop_entry_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_String_Get_Cb cb, const void *data);
EDBus_Pending *mpris_media_player2_has_track_list_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Bool_Get_Cb cb, const void *data);
EDBus_Pending *mpris_media_player2_identity_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_String_Get_Cb cb, const void *data);
EDBus_Pending *mpris_media_player2_supported_mime_types_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Complex_Get_Cb cb, const void *data);
EDBus_Pending *mpris_media_player2_supported_uri_schemes_propget(EDBus_Proxy *proxy, EDBus_Codegen_Property_Complex_Get_Cb cb, const void *data);

#endif