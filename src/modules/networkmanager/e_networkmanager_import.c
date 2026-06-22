#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "e.h"
#include "e_networkmanager_import.h"

/* -------------------------------------------------------------------------- */
/* nmcli path detection                                                       */
/* -------------------------------------------------------------------------- */

static const char *_nmcli_path   = NULL;  /* eina_stringshare'd */
static Eina_Bool   _nmcli_probed = EINA_FALSE;

const char *
enm_import_nmcli_path(void)
{
   if (_nmcli_probed) return _nmcli_path;
   _nmcli_probed = EINA_TRUE;

   const char *path_env = getenv("PATH");
   if (!path_env) return NULL;

   char *path_copy = strdup(path_env);
   if (!path_copy) return NULL;

   char *saveptr = NULL;
   char *dir = strtok_r(path_copy, ":", &saveptr);
   while (dir)
     {
        char candidate[PATH_MAX];
        snprintf(candidate, sizeof(candidate), "%s/nmcli", dir);
        if (access(candidate, X_OK) == 0)
          {
             _nmcli_path = eina_stringshare_add(candidate);
             break;
          }
        dir = strtok_r(NULL, ":", &saveptr);
     }

   free(path_copy);
   return _nmcli_path;
}

/* -------------------------------------------------------------------------- */
/* File extension → VPN type detection                                        */
/* -------------------------------------------------------------------------- */

const char *
enm_import_detect_type(const char *file_path)
{
   if (!file_path) return NULL;

   const char *ext = strrchr(file_path, '.');
   if (!ext) return NULL;
   ext++; /* skip the '.' */

   if      (strcasecmp(ext, "conf") == 0) return "wireguard";
   else if (strcasecmp(ext, "ovpn") == 0) return "openvpn";
   else if (strcasecmp(ext, "pcf")  == 0) return "vpnc";
   else if (strcasecmp(ext, "xml")  == 0) return "openconnect";

   return NULL;
}

/* -------------------------------------------------------------------------- */
/* Async nmcli import                                                         */
/* -------------------------------------------------------------------------- */

typedef struct _Import_Ctx
{
   Enm_Import_Done_Cb   done_cb;
   void                *data;
   Ecore_Exe           *exe;
   Eina_Strbuf         *stderr_buf;
   Eina_Strbuf         *stdout_buf;
   Ecore_Event_Handler *handler_err;
   Ecore_Event_Handler *handler_out;
   Ecore_Event_Handler *handler_del;
} Import_Ctx;

static void
_import_ctx_free(Import_Ctx *ctx)
{
   if (!ctx) return;
   if (ctx->handler_err) ecore_event_handler_del(ctx->handler_err);
   if (ctx->handler_out) ecore_event_handler_del(ctx->handler_out);
   if (ctx->handler_del) ecore_event_handler_del(ctx->handler_del);
   if (ctx->stderr_buf)  eina_strbuf_free(ctx->stderr_buf);
   if (ctx->stdout_buf)  eina_strbuf_free(ctx->stdout_buf);
   /* exe is already gone by the time _del fires; don't del it here */
   free(ctx);
}

static char *
_shell_escape_single(const char *s)
{
   /* Replace each ' with '\''. Returns a heap string the caller must free. */
   if (!s) return NULL;
   Eina_Strbuf *b = eina_strbuf_new();
   if (!b) return NULL;
   for (const char *p = s; *p; p++)
     {
        if (*p == '\'') eina_strbuf_append(b, "'\\''");
        else            eina_strbuf_append_char(b, *p);
     }
   char *out = strdup(eina_strbuf_string_get(b));
   eina_strbuf_free(b);
   return out;
}

static Eina_Bool
_on_stderr(void *data, int type EINA_UNUSED, void *event)
{
   Import_Ctx *ctx = data;
   Ecore_Exe_Event_Data *ev = event;

   if (!ev || !ev->exe || ev->exe != ctx->exe)
     return ECORE_CALLBACK_PASS_ON;

   if (ev->data && ev->size > 0)
     eina_strbuf_append_length(ctx->stderr_buf, ev->data, ev->size);

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_on_stdout(void *data, int type EINA_UNUSED, void *event)
{
   Import_Ctx *ctx = data;
   Ecore_Exe_Event_Data *ev = event;

   if (!ev || !ev->exe || ev->exe != ctx->exe)
     return ECORE_CALLBACK_PASS_ON;
   if (ev->data && ev->size > 0)
     eina_strbuf_append_length(ctx->stdout_buf, ev->data, ev->size);
   return ECORE_CALLBACK_DONE;
}

/* Parse `Connection 'NAME' (UUID) successfully added.` -> malloc'd UUID.
 * The UUID (not the name) is returned because connection names are not
 * unique, which makes later `nmcli ... <name>` operations ambiguous. */
static char *
_import_parse_conn_uuid(const char *out)
{
   const char *a, *b;
   if (!out) return NULL;
   a = strchr(out, '(');
   if (!a) return NULL;
   a++;
   b = strchr(a, ')');
   if (!b) return NULL;
   return strndup(a, (size_t)(b - a));
}

static Eina_Bool
_on_del(void *data, int type EINA_UNUSED, void *event)
{
   Import_Ctx *ctx = data;
   Ecore_Exe_Event_Del *ev = event;

   if (!ev || !ev->exe || ev->exe != ctx->exe)
     return ECORE_CALLBACK_PASS_ON;

   /* Prevent double-free: clear handlers before freeing ctx */
   ecore_event_handler_del(ctx->handler_err);
   ctx->handler_err = NULL;
   if (ctx->handler_out)
     { ecore_event_handler_del(ctx->handler_out); ctx->handler_out = NULL; }
   ecore_event_handler_del(ctx->handler_del);
   ctx->handler_del = NULL;

   Eina_Bool ok = (ev->exited && ev->exit_code == 0);
   const char *stderr_text = eina_strbuf_string_get(ctx->stderr_buf);
   char *conn_uuid = ok ? _import_parse_conn_uuid(
                              eina_strbuf_string_get(ctx->stdout_buf)) : NULL;

   ctx->done_cb(ctx->data, ok, stderr_text, conn_uuid);
   free(conn_uuid);

   _import_ctx_free(ctx);
   return ECORE_CALLBACK_DONE;
}

void
enm_import_run(const char *type, const char *file_path,
               Enm_Import_Done_Cb done_cb, void *data)
{
   EINA_SAFETY_ON_NULL_RETURN(type);
   EINA_SAFETY_ON_NULL_RETURN(file_path);
   EINA_SAFETY_ON_NULL_RETURN(done_cb);

   const char *nmcli = enm_import_nmcli_path();
   if (!nmcli)
     {
        done_cb(data, EINA_FALSE, "nmcli not found on PATH", NULL);
        return;
     }

   char *esc_nmcli    = _shell_escape_single(nmcli);
   char *esc_type     = _shell_escape_single(type);
   char *esc_filepath = _shell_escape_single(file_path);

   if (!esc_nmcli || !esc_type || !esc_filepath)
     {
        free(esc_nmcli);
        free(esc_type);
        free(esc_filepath);
        done_cb(data, EINA_FALSE, "out of memory", NULL);
        return;
     }

   char cmd[4096];
   /* LC_ALL=C so the "Connection 'NAME' ... added" banner we parse for the
    * connection name is not localized. */
   snprintf(cmd, sizeof(cmd),
            "LC_ALL=C '%s' connection import type '%s' file '%s'",
            esc_nmcli, esc_type, esc_filepath);

   free(esc_nmcli);
   free(esc_type);
   free(esc_filepath);

   Import_Ctx *ctx = calloc(1, sizeof(Import_Ctx));
   if (!ctx)
     {
        done_cb(data, EINA_FALSE, "out of memory", NULL);
        return;
     }

   ctx->done_cb    = done_cb;
   ctx->data       = data;
   ctx->stderr_buf = eina_strbuf_new();
   if (!ctx->stderr_buf)
     {
        done_cb(data, EINA_FALSE, "out of memory", NULL);
        free(ctx);
        return;
     }
   ctx->stdout_buf = eina_strbuf_new();
   if (!ctx->stdout_buf)
     {
        done_cb(data, EINA_FALSE, "out of memory", NULL);
        eina_strbuf_free(ctx->stderr_buf);
        free(ctx);
        return;
     }

   ctx->exe = ecore_exe_pipe_run(cmd,
                                 ECORE_EXE_PIPE_READ |
                                 ECORE_EXE_PIPE_READ_LINE_BUFFERED |
                                 ECORE_EXE_PIPE_ERROR |
                                 ECORE_EXE_PIPE_ERROR_LINE_BUFFERED |
                                 ECORE_EXE_NOT_LEADER |
                                 ECORE_EXE_TERM_WITH_PARENT,
                                 ctx);
   if (!ctx->exe)
     {
        done_cb(data, EINA_FALSE, "failed to spawn nmcli", NULL);
        _import_ctx_free(ctx);
        return;
     }

   ctx->handler_out = ecore_event_handler_add(ECORE_EXE_EVENT_DATA,
                                              _on_stdout, ctx);
   ctx->handler_err = ecore_event_handler_add(ECORE_EXE_EVENT_ERROR,
                                               _on_stderr, ctx);
   ctx->handler_del = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                               _on_del, ctx);
}
