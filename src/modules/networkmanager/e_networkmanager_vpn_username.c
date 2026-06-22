#ifdef ENM_VPN_USERNAME_TEST
/* Standalone unit-test build: no EFL, no config.h. */
typedef unsigned char Eina_Bool;
# define EINA_TRUE  1
# define EINA_FALSE 0
# include <string.h>
#else
# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif
# include <string.h>
# include <stdlib.h>
# include "e.h"
# include "e_networkmanager.h"
# include "e_networkmanager_import.h"
# include "e_networkmanager_vpn_username.h"
#endif

Eina_Bool
enm_vpn_username_needed(const char *svc_short, const char *conn_type,
                        const char *current_username)
{
   if (current_username && current_username[0]) return EINA_FALSE;
   if (!svc_short) return EINA_FALSE;

   if (!strcmp(svc_short, "openvpn"))
     {
        if (!conn_type) return EINA_FALSE;
        return (!strcmp(conn_type, "password") ||
                !strcmp(conn_type, "password-tls"))
               ? EINA_TRUE : EINA_FALSE;
     }

   if (!strcmp(svc_short, "pptp")        || !strcmp(svc_short, "l2tp") ||
       !strcmp(svc_short, "fortisslvpn") || !strcmp(svc_short, "vpnc") ||
       !strcmp(svc_short, "openswan")    || !strcmp(svc_short, "libreswan") ||
       !strcmp(svc_short, "strongswan"))
     return EINA_TRUE;

   return EINA_FALSE;
}

#ifndef ENM_VPN_USERNAME_TEST

/* ---- shared shell-escape (same scheme as e_networkmanager_import.c) ------- */
static char *
_username_shell_escape(const char *s)
{
   Eina_Strbuf *b;
   char *out;

   if (!s) return strdup("");
   b = eina_strbuf_new();
   if (!b) return NULL;
   for (const char *p = s; *p; p++)
     {
        if (*p == '\'') eina_strbuf_append(b, "'\\''");
        else            eina_strbuf_append_char(b, *p);
     }
   out = strdup(eina_strbuf_string_get(b));
   eina_strbuf_free(b);
   return out;
}

typedef struct _Username_Set_Ctx
{
   Enm_Username_Done_Cb  cb;
   void                 *data;
   Ecore_Exe            *exe;
   Ecore_Event_Handler  *handler_del;
} Username_Set_Ctx;

static Eina_Bool
_username_set_on_del(void *data, int type EINA_UNUSED, void *event)
{
   Username_Set_Ctx *ctx = data;
   Ecore_Exe_Event_Del *ev = event;

   if (!ev || !ev->exe || ev->exe != ctx->exe)
     return ECORE_CALLBACK_PASS_ON;

   if (ctx->handler_del) ecore_event_handler_del(ctx->handler_del);
   if (ctx->cb)
     ctx->cb(ctx->data, (ev->exited && ev->exit_code == 0));
   free(ctx);
   return ECORE_CALLBACK_DONE;
}

void
enm_vpn_username_set(const char *conn_name, const char *username,
                     Enm_Username_Done_Cb cb, void *data)
{
   const char *nmcli;
   char *esc_nmcli, *esc_name, *esc_user;
   char cmd[4096];
   Username_Set_Ctx *ctx;

   if (!conn_name) { if (cb) cb(data, EINA_FALSE); return; }

   nmcli = enm_import_nmcli_path();
   if (!nmcli) { if (cb) cb(data, EINA_FALSE); return; }

   esc_nmcli = _username_shell_escape(nmcli);
   esc_name  = _username_shell_escape(conn_name);
   esc_user  = _username_shell_escape(username);
   if (!esc_nmcli || !esc_name || !esc_user)
     {
        free(esc_nmcli); free(esc_name); free(esc_user);
        if (cb) cb(data, EINA_FALSE);
        return;
     }

   /* '+vpn.data' updates the single username key; plain 'vpn.data' would
    * REPLACE the whole dictionary, wiping connection-type/ca/cert/etc. */
   snprintf(cmd, sizeof(cmd),
            "'%s' connection modify '%s' +vpn.data 'username=%s'",
            esc_nmcli, esc_name, esc_user);
   free(esc_nmcli); free(esc_name); free(esc_user);

   ctx = calloc(1, sizeof(*ctx));
   if (!ctx) { if (cb) cb(data, EINA_FALSE); return; }
   ctx->cb = cb;
   ctx->data = data;
   ctx->exe = ecore_exe_pipe_run(cmd,
                                 ECORE_EXE_NOT_LEADER |
                                 ECORE_EXE_TERM_WITH_PARENT, ctx);
   if (!ctx->exe) { free(ctx); if (cb) cb(data, EINA_FALSE); return; }
   ctx->handler_del = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                              _username_set_on_del, ctx);
}

/* ---- shared username dialog ----------------------------------------------- */

typedef struct _Username_Dialog
{
   E_Dialog                *dialog;
   Evas_Object             *entry;
   Enm_Username_Entered_Cb  cb;
   void                    *data;
   Eina_Bool                replied;
} Username_Dialog;

static void
_username_dialog_finish(Username_Dialog *ud, const char *value)
{
   if (!ud->replied)
     {
        ud->replied = EINA_TRUE;
        if (ud->cb) ud->cb(ud->data, value);
     }
   e_object_del(E_OBJECT(ud->dialog));
}

static void
_username_dialog_ok_cb(void *data, E_Dialog *dialog EINA_UNUSED)
{
   Username_Dialog *ud = data;
   char *user = elm_entry_markup_to_utf8(elm_entry_entry_get(ud->entry));
   _username_dialog_finish(ud, user ?: "");
   free(user);
}

static void
_username_dialog_cancel_cb(void *data, E_Dialog *dialog EINA_UNUSED)
{
   _username_dialog_finish(data, NULL);
}

static void
_username_entry_activated_cb(void *data, Evas_Object *o EINA_UNUSED,
                             void *ev EINA_UNUSED)
{
   _username_dialog_ok_cb(data, NULL);
}

static void
_username_dialog_key_down_cb(void *data, Evas *e EINA_UNUSED,
                             Evas_Object *o EINA_UNUSED, void *event)
{
   Evas_Event_Key_Down *ev = event;
   if (!strcmp(ev->key, "Escape")) _username_dialog_finish(data, NULL);
}

static void
_username_dialog_del_cb(void *data)
{
   E_Dialog *dialog = data;
   Username_Dialog *ud = e_object_data_get(E_OBJECT(dialog));
   if (!ud) return;
   if (!ud->replied)
     {
        ud->replied = EINA_TRUE;
        if (ud->cb) ud->cb(ud->data, NULL);   /* WM close == cancel */
     }
   free(ud);
}

void
enm_vpn_username_dialog(const char *conn_name, const char *type_label,
                        const char *initial,
                        Enm_Username_Entered_Cb cb, void *data)
{
   Username_Dialog *ud;
   E_Dialog *dialog;
   Evas_Object *frame, *table, *label, *entry, *spacer;
   Evas_Coord minw = 280 * e_scale;
   char header[160];

   dialog = e_dialog_new(NULL, "E", "nm_vpn_username");
   if (!dialog) { if (cb) cb(data, NULL); return; }

   ud = E_NEW(Username_Dialog, 1);
   if (!ud) { e_object_del(E_OBJECT(dialog)); if (cb) cb(data, NULL); return; }
   ud->dialog = dialog;
   ud->cb = cb;
   ud->data = data;

   e_dialog_resizable_set(dialog, 0);
   e_dialog_title_set(dialog, _("VPN Username Required"));
   e_dialog_border_icon_set(dialog, "dialog-password");
   e_dialog_button_add(dialog, _("OK"), NULL, _username_dialog_ok_cb, ud);
   e_dialog_button_add(dialog, _("Cancel"), NULL, _username_dialog_cancel_cb, ud);

   snprintf(header, sizeof(header), "%s — %s",
            conn_name ?: "VPN", type_label ?: _("VPN"));
   frame = elm_frame_add(dialog->win);
   elm_object_text_set(frame, header);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);

   table = elm_table_add(frame);
   elm_table_padding_set(table, 8 * e_scale, 4 * e_scale);
   elm_object_content_set(frame, table);
   evas_object_show(table);

   /* Transparent spacer forces the entry column to a usable width (~25 chars);
    * a scrollable single-line entry otherwise collapses to one character in an
    * elm_table cell. */
   spacer = evas_object_rectangle_add(evas_object_evas_get(table));
   evas_object_color_set(spacer, 0, 0, 0, 0);
   evas_object_size_hint_min_set(spacer, minw, 1);
   evas_object_size_hint_weight_set(spacer, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(spacer, EVAS_HINT_FILL, 0.0);
   elm_table_pack(table, spacer, 1, 0, 1, 1);
   evas_object_show(spacer);

   label = elm_label_add(table);
   elm_object_text_set(label, _("Username"));
   evas_object_size_hint_align_set(label, 1.0, 0.5);
   elm_table_pack(table, label, 0, 1, 1, 1);
   evas_object_show(label);

   entry = elm_entry_add(table);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   if (initial && initial[0]) elm_object_text_set(entry, initial);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_min_set(entry, minw, 0);
   evas_object_smart_callback_add(entry, "activated",
                                  _username_entry_activated_cb, ud);
   elm_table_pack(table, entry, 1, 1, 1, 1);
   evas_object_show(entry);
   ud->entry = entry;

   evas_object_show(frame);
   e_dialog_content_set(dialog, frame, minw, 0);
   e_dialog_show(dialog);

   evas_object_event_callback_add(dialog->bg_object, EVAS_CALLBACK_KEY_DOWN,
                                  _username_dialog_key_down_cb, ud);
   e_object_del_attach_func_set(E_OBJECT(dialog), _username_dialog_del_cb);
   e_object_data_set(E_OBJECT(dialog), ud);
   elm_object_focus_set(entry, EINA_TRUE);
   elm_win_center(dialog->win, 1, 1);
}

/* ---- post-import orchestrator --------------------------------------------- */

/* Parse a `key = value, key = value` list (nmcli -g vpn.data output) for one
 * key.  Returns a malloc'd value or NULL.  Tolerant of surrounding spaces. */
static char *
_nmcli_kv_find(const char *blob, const char *key)
{
   size_t klen = strlen(key);
   const char *p = blob;

   while (p && *p)
     {
        while (*p == ' ' || *p == ',') p++;
        if (!strncmp(p, key, klen))
          {
             const char *q = p + klen;
             while (*q == ' ') q++;
             if (*q == '=')
               {
                  const char *v;
                  const char *end;
                  q++;
                  while (*q == ' ') q++;
                  v = q;
                  end = strchr(v, ',');
                  if (!end) end = v + strlen(v);
                  while (end > v && (end[-1] == ' ')) end--;
                  return strndup(v, (size_t)(end - v));
               }
          }
        p = strchr(p, ',');
        if (p) p++;
     }
   return NULL;
}

typedef struct _Username_Probe_Ctx
{
   char                *conn_uuid;  /* unique id for nmcli ops */
   char                *svc;        /* service short name, from import type */
   Ecore_Exe           *exe;
   Eina_Strbuf         *out;
   Ecore_Event_Handler *h_data;
   Ecore_Event_Handler *h_del;
} Username_Probe_Ctx;

static Eina_Bool
_probe_on_data(void *data, int type EINA_UNUSED, void *event)
{
   Username_Probe_Ctx *ctx = data;
   Ecore_Exe_Event_Data *ev = event;
   if (!ev || ev->exe != ctx->exe) return ECORE_CALLBACK_PASS_ON;
   if (ev->data && ev->size > 0)
     eina_strbuf_append_length(ctx->out, ev->data, ev->size);
   return ECORE_CALLBACK_DONE;
}

static void
_probe_username_entered(void *data, const char *username)
{
   char *conn_name = data;
   if (username) enm_vpn_username_set(conn_name, username, NULL, NULL);
   free(conn_name);
}

static Eina_Bool
_probe_on_del(void *data, int type EINA_UNUSED, void *event)
{
   Username_Probe_Ctx *ctx = data;
   Ecore_Exe_Event_Del *ev = event;
   char *conn_type = NULL, *username = NULL;
   const char *blob;
   Eina_Bool needed;

   if (!ev || ev->exe != ctx->exe) return ECORE_CALLBACK_PASS_ON;

   if (ctx->h_data) ecore_event_handler_del(ctx->h_data);
   if (ctx->h_del)  ecore_event_handler_del(ctx->h_del);

   /* Single line: the vpn.data list (queried alone to avoid a multi-line
    * ecore_exe data/exit race).  The service short name comes from the import
    * type, so we never need vpn.service-type here. */
   blob = eina_strbuf_string_get(ctx->out);
   conn_type = _nmcli_kv_find(blob, "connection-type");
   username  = _nmcli_kv_find(blob, "username");

   needed = enm_vpn_username_needed(ctx->svc, conn_type, username);

   if (needed)
     {
        char *uuid_copy = strdup(ctx->conn_uuid);
        if (uuid_copy)
          enm_vpn_username_dialog(NULL,        /* generic "VPN" header */
                                  ctx->svc,     /* type label */
                                  username,
                                  _probe_username_entered,
                                  uuid_copy);
     }

   free(conn_type); free(username);
   free(ctx->conn_uuid); free(ctx->svc);
   eina_strbuf_free(ctx->out);
   free(ctx);
   return ECORE_CALLBACK_DONE;
}

void
enm_vpn_username_maybe_prompt(const char *conn_uuid, const char *svc_short)
{
   const char *nmcli;
   char *esc_nmcli, *esc_uuid, cmd[4096];
   Username_Probe_Ctx *ctx;

   if (!conn_uuid) return;
   nmcli = enm_import_nmcli_path();
   if (!nmcli) return;

   esc_nmcli = _username_shell_escape(nmcli);
   esc_uuid  = _username_shell_escape(conn_uuid);
   if (!esc_nmcli || !esc_uuid) { free(esc_nmcli); free(esc_uuid); return; }

   /* Query ONLY vpn.data, by UUID (unique).  vpn.data prints as a single
    * comma-joined line, which avoids the multi-line ecore_exe data/exit race
    * that drops trailing lines.  The service short name comes from the import
    * type, so vpn.service-type is not needed. */
   snprintf(cmd, sizeof(cmd),
            "LC_ALL=C '%s' -g vpn.data connection show '%s'",
            esc_nmcli, esc_uuid);
   free(esc_nmcli); free(esc_uuid);

   ctx = calloc(1, sizeof(*ctx));
   if (!ctx) return;
   ctx->conn_uuid = strdup(conn_uuid);
   ctx->svc       = svc_short ? strdup(svc_short) : NULL;
   ctx->out = eina_strbuf_new();
   if (!ctx->conn_uuid || !ctx->out)
     { free(ctx->conn_uuid); free(ctx->svc); if (ctx->out) eina_strbuf_free(ctx->out); free(ctx); return; }

   ctx->exe = ecore_exe_pipe_run(cmd,
                                 ECORE_EXE_PIPE_READ |
                                 ECORE_EXE_PIPE_READ_LINE_BUFFERED |
                                 ECORE_EXE_NOT_LEADER |
                                 ECORE_EXE_TERM_WITH_PARENT, ctx);
   if (!ctx->exe)
     { free(ctx->conn_uuid); free(ctx->svc); eina_strbuf_free(ctx->out); free(ctx); return; }
   ctx->h_data = ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _probe_on_data, ctx);
   ctx->h_del  = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,  _probe_on_del,  ctx);
}

#endif
