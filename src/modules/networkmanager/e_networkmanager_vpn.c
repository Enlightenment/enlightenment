#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "e.h"
#include "e_networkmanager.h"
#include "e_networkmanager_vpn.h"
#include "e_networkmanager_import.h"

struct NM_VPN_Connection *
enm_vpn_connection_new(const char *path)
{
   struct NM_VPN_Connection *vc;
   EINA_SAFETY_ON_NULL_RETURN_VAL(path, NULL);
   vc = E_NEW(struct NM_VPN_Connection, 1);
   vc->path = eina_stringshare_add(path);
   vc->vpn_state = NM_VPN_STATE_DISCONNECTED;
   return vc;
}

void
enm_vpn_connection_free(struct NM_VPN_Connection *vc)
{
   if (!vc) return;
   enm_vpn_active_unbind(vc);
   eina_stringshare_del(vc->path);
   free(vc->uuid);
   free(vc->name);
   free(vc->conn_type);
   free(vc->service_type);
   free(vc->ip_address);
   free(vc);
}

/* -------------------------------------------------------------------------- */
/* VPN connection enumeration (Task 4)                                        */
/* -------------------------------------------------------------------------- */

/*
 * Context for the ListConnections in-flight call.
 * Carries both the manager pointer and the generation snapshot taken at
 * dispatch time, so that _vpn_list_cb can detect a stale reply without
 * touching any freed state.
 */
struct _vpn_list_ctx {
   struct NM_Manager *nm;
   unsigned int       generation;
};

struct _enm_vpn_get_settings_ctx {
   struct NM_Manager *nm;
   unsigned int       generation;
   const char        *path;    /* stringshare */
   Eldbus_Proxy      *proxy;
   Eldbus_Object     *obj;
   Eldbus_Pending    *pending; /* our own handle in nm->vpn_pending_settings */
};

static void
_vpn_get_settings_cb(void *data, const Eldbus_Message *msg,
                     Eldbus_Pending *pending EINA_UNUSED)
{
   struct _enm_vpn_get_settings_ctx *ctx = data;
   Eldbus_Message_Iter *settings, *section;
   const char *err_name, *err_msg;
   char *conn_id = NULL, *conn_type = NULL, *uuid = NULL,
        *service_type = NULL;
   Eina_Bool autoconnect = EINA_TRUE;  /* NM default */

   /* Stale reply: manager has been freed or a new enumeration started. */
   if (ctx->generation != ctx->nm->vpn_generation) goto out;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("VPN GetSettings failed for %s: %s %s",
            ctx->path, err_name ?: "", err_msg ?: "");
        goto out;
     }
   if (!eldbus_message_arguments_get(msg, "a{sa{sv}}", &settings))
     {
        ERR("VPN GetSettings: cannot parse reply for %s", ctx->path);
        goto out;
     }

   while (eldbus_message_iter_get_and_next(settings, 'e', &section))
     {
        Eldbus_Message_Iter *entries, *entry, *var;
        const char *sect_name, *key;

        if (!eldbus_message_iter_arguments_get(section, "sa{sv}",
                                               &sect_name, &entries))
          continue;

        if (!strcmp(sect_name, "connection"))
          {
             while (eldbus_message_iter_get_and_next(entries, 'e', &entry))
               {
                  if (!eldbus_message_iter_arguments_get(entry, "sv",
                                                         &key, &var)) continue;
                  if (!strcmp(key, "id"))
                    { const char *s; if (eldbus_message_iter_arguments_get(var, "s", &s)) conn_id = strdup(s); }
                  else if (!strcmp(key, "type"))
                    { const char *s; if (eldbus_message_iter_arguments_get(var, "s", &s)) conn_type = strdup(s); }
                  else if (!strcmp(key, "uuid"))
                    { const char *s; if (eldbus_message_iter_arguments_get(var, "s", &s)) uuid = strdup(s); }
                  else if (!strcmp(key, "autoconnect"))
                    { Eina_Bool b; if (eldbus_message_iter_arguments_get(var, "b", &b)) autoconnect = b; }
               }
          }
        else if (!strcmp(sect_name, "vpn"))
          {
             while (eldbus_message_iter_get_and_next(entries, 'e', &entry))
               {
                  if (!eldbus_message_iter_arguments_get(entry, "sv",
                                                         &key, &var)) continue;
                  if (!strcmp(key, "service-type"))
                    { const char *s; if (eldbus_message_iter_arguments_get(var, "s", &s)) service_type = strdup(s); }
               }
          }
     }

   if (!conn_type) goto out;
   if (strcmp(conn_type, "vpn") && strcmp(conn_type, "wireguard")) goto out;

   /* If an entry for this path already exists, prefer in-place mutation:
    * blowing it away and re-creating it loses any active binding
    * (active_path, watchers, vpn_state).  Only do a full replace when the
    * underlying connection.type changed (vpn <-> wireguard) — that affects
    * which D-Bus interface we'd subscribe to, so the binding would need
    * to be torn down and re-created anyway. */
   {
      struct NM_VPN_Connection *vc = enm_vpn_find_by_path(ctx->nm, ctx->path);
      Eina_Bool type_changed =
          (vc && conn_type && vc->conn_type &&
           strcmp(vc->conn_type, conn_type));

      if (vc && type_changed)
        {
           ctx->nm->vpn_connections =
               eina_inlist_remove(ctx->nm->vpn_connections, EINA_INLIST_GET(vc));
           enm_vpn_connection_free(vc);
           vc = NULL;
        }

      if (vc)
        {
           /* Update mutable settings in place — preserve active_path,
            * vpn_state, prev_state, and the D-Bus watcher handles. */
           free(vc->name);         vc->name         = conn_id;     conn_id     = NULL;
           free(vc->conn_type);    vc->conn_type    = conn_type;   conn_type   = NULL;
           free(vc->uuid);         vc->uuid         = uuid;        uuid        = NULL;
           free(vc->service_type); vc->service_type = service_type;service_type= NULL;
           vc->autoconnect = autoconnect;
        }
      else
        {
           vc = enm_vpn_connection_new(ctx->path);
           vc->nm = ctx->nm;
           vc->name = conn_id; conn_id = NULL;
           vc->conn_type = conn_type; conn_type = NULL;
           vc->uuid = uuid; uuid = NULL;
           vc->service_type = service_type; service_type = NULL;
           vc->autoconnect = autoconnect;
           ctx->nm->vpn_connections =
               eina_inlist_append(ctx->nm->vpn_connections, EINA_INLIST_GET(vc));
        }

      DBG("Found VPN connection: %s (%s/%s)",
          vc->name ?: "(unnamed)", vc->conn_type,
          vc->service_type ?: "-");
   }

out:
   free(conn_id); free(conn_type); free(uuid); free(service_type);
   eldbus_proxy_unref(ctx->proxy);
   eldbus_object_unref(ctx->obj);
   eina_stringshare_del(ctx->path);

   /* Remove our handle from the manager's tracking list BEFORE any other
    * nm access, so that the _manager_free cancel loop (which drains the
    * list) does not see a stale pointer and so that the manager can
    * safely detect the list is empty once all callbacks have fired. */
   if (ctx->pending)
     ctx->nm->vpn_pending_settings =
         eina_list_remove(ctx->nm->vpn_pending_settings, ctx->pending);

   /* Batch terminal notification: fire vpn_changed once when the last
    * outstanding GetSettings callback completes.  Guard the decrement —
    * after enm_vpn_clear() resets vpn_pending to 0, stale callbacks from
    * a previous (cancelled / in-flight) batch can still fire; decrementing
    * to -1 would suppress every future terminal block.  Mirror the pattern
    * used by _saved_conn_settings_cb. */
   if (ctx->nm->vpn_pending > 0)
     ctx->nm->vpn_pending--;
   if (ctx->nm->vpn_pending == 0)
     {

        DBG("VPN enumeration terminal: vpn_connections=%u, cached active paths=%u",
            eina_inlist_count(ctx->nm->vpn_connections),
            (unsigned int)eina_list_count(ctx->nm->vpn_pending_active_paths));

        /* Re-run VPN active reconcile for paths that were seen at startup
         * (cached in vpn_pending_active_paths) before vpn_connections was
         * populated.  Now that enumeration is complete, enm_vpn_find_by_path
         * can actually locate these connections. */
        if (ctx->nm->vpn_pending_active_paths)
          {
             Eina_List *l;
             const char *ap;
             unsigned int n = eina_list_count(ctx->nm->vpn_pending_active_paths);
             const char **paths = malloc(n * sizeof(const char *));
             if (paths)
               {
                  unsigned int idx = 0;
                  EINA_LIST_FOREACH(ctx->nm->vpn_pending_active_paths, l, ap)
                    paths[idx++] = ap;
                  _enm_vpn_reconcile_active(ctx->nm, paths, n);
                  free(paths);
               }
             EINA_LIST_FREE(ctx->nm->vpn_pending_active_paths, ap)
               eina_stringshare_del(ap);
             ctx->nm->vpn_pending_active_paths = NULL;
          }

        _mod_cb_vpn_changed_t cb = _mod_cbs_vpn_changed_get();
        if (cb) cb(ctx->nm);
     }

   free(ctx);
}

static void
_vpn_list_cb(void *data, const Eldbus_Message *msg,
             Eldbus_Pending *pending EINA_UNUSED)
{
   struct _vpn_list_ctx *lctx = data;
   struct NM_Manager *nm = lctx->nm;
   unsigned int gen = lctx->generation;
   Eldbus_Message_Iter *array;
   const char *err_name, *err_msg, *path;

   free(lctx);

   /* Clear our tracked handle — the call has completed (or been cancelled). */
   nm->vpn_list_pending = NULL;

   /* Stale reply: manager has been freed or a new enumeration started. */
   if (gen != nm->vpn_generation) return;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("VPN ListConnections failed: %s %s",
            err_name ?: "", err_msg ?: "");
        return;
     }
   if (!eldbus_message_arguments_get(msg, "ao", &array))
     {
        ERR("VPN ListConnections: cannot parse reply");
        return;
     }

   /* Clear current list — fresh enumeration. */
   enm_vpn_clear(nm);
   nm->vpn_pending = 0;

   while (eldbus_message_iter_get_and_next(array, 'o', &path))
     {
        struct _enm_vpn_get_settings_ctx *ctx =
            E_NEW(struct _enm_vpn_get_settings_ctx, 1);
        if (!ctx) continue;
        ctx->nm = nm;
        ctx->generation = gen;
        ctx->path = eina_stringshare_add(path);
        ctx->obj = eldbus_object_get(_enm_dbus_conn_get(),
                                     NM_BUS_NAME, path);
        if (!ctx->obj)
          {
             eina_stringshare_del(ctx->path);
             free(ctx);
             continue;
          }
        ctx->proxy = eldbus_proxy_get(ctx->obj, NM_IFACE_SCONN);
        if (!ctx->proxy)
          {
             eldbus_object_unref(ctx->obj);
             eina_stringshare_del(ctx->path);
             free(ctx);
             continue;
          }
        {
           Eldbus_Pending *p;
           nm->vpn_pending++;
           p = eldbus_proxy_call(ctx->proxy, "GetSettings",
                                 _vpn_get_settings_cb, ctx, -1, "");
           ctx->pending = p;
           nm->vpn_pending_settings =
               eina_list_append(nm->vpn_pending_settings, p);
           /* proxy and obj are freed/unref'd in _vpn_get_settings_cb */
        }
     }

   /* No GetSettings dispatched: drain startup-active-paths cache and fire
    * terminal notification immediately.  Mirrors the terminal block in
    * _vpn_get_settings_cb so the zero-VPN-connections path is not silently
    * skipped (it must still reconcile to unbind any previously-active vc). */
   if (nm->vpn_pending == 0)
     {
        if (nm->vpn_pending_active_paths)
          {
             Eina_List *l;
             const char *ap;
             unsigned int n = eina_list_count(nm->vpn_pending_active_paths);
             const char **paths = malloc(n * sizeof(const char *));
             if (paths)
               {
                  unsigned int idx = 0;
                  EINA_LIST_FOREACH(nm->vpn_pending_active_paths, l, ap)
                    paths[idx++] = ap;
                  _enm_vpn_reconcile_active(nm, paths, n);
                  free(paths);
               }
             EINA_LIST_FREE(nm->vpn_pending_active_paths, ap)
               eina_stringshare_del(ap);
             nm->vpn_pending_active_paths = NULL;
          }

        _mod_cb_vpn_changed_t cb = _mod_cbs_vpn_changed_get();
        if (cb) cb(nm);
     }
}

/* Dispatch a single GetSettings for `path`, reusing the same context type
 * as the bulk enumerator so the terminal block in _vpn_get_settings_cb
 * fires vpn_changed when this single call completes (assuming no other
 * batch is in flight).  Uses the current generation so the call is not
 * spuriously marked stale. */
void
enm_vpn_settings_fetch_one(struct NM_Manager *nm, const char *path)
{
   struct _enm_vpn_get_settings_ctx *ctx;
   Eldbus_Pending *p;

   if (!nm || !path) return;

   ctx = E_NEW(struct _enm_vpn_get_settings_ctx, 1);
   if (!ctx) return;
   ctx->nm = nm;
   ctx->generation = nm->vpn_generation;
   ctx->path = eina_stringshare_add(path);
   ctx->obj = eldbus_object_get(_enm_dbus_conn_get(), NM_BUS_NAME, path);
   if (!ctx->obj)
     {
        eina_stringshare_del(ctx->path);
        free(ctx);
        return;
     }
   ctx->proxy = eldbus_proxy_get(ctx->obj, NM_IFACE_SCONN);
   if (!ctx->proxy)
     {
        eldbus_object_unref(ctx->obj);
        eina_stringshare_del(ctx->path);
        free(ctx);
        return;
     }

   nm->vpn_pending++;
   p = eldbus_proxy_call(ctx->proxy, "GetSettings",
                         _vpn_get_settings_cb, ctx, -1, "");
   ctx->pending = p;
   nm->vpn_pending_settings = eina_list_append(nm->vpn_pending_settings, p);
}

void
enm_vpn_remove_by_path(struct NM_Manager *nm, const char *path)
{
   struct NM_VPN_Connection *vc;

   Eina_Bool was_active;

   if (!nm || !path) return;
   vc = enm_vpn_find_by_path(nm, path);
   if (!vc) return;  /* not a VPN we tracked — nothing to do */

   /* If an *active* VPN is deleted, removing it drops the active count, so the
    * status shield must be re-emitted — otherwise the gadget keeps showing the
    * secured icon for a connection that no longer exists. */
   was_active = (vc->vpn_state == NM_VPN_STATE_ACTIVATED);

   nm->vpn_connections =
       eina_inlist_remove(nm->vpn_connections, EINA_INLIST_GET(vc));
   enm_vpn_connection_free(vc);

   {
      _mod_cb_vpn_changed_t cb = _mod_cbs_vpn_changed_get();
      if (cb) cb(nm);
   }
   if (was_active) enm_vpn_active_changed_schedule(nm);
}

void
enm_vpn_enumerate(struct NM_Manager *nm)
{
   struct _vpn_list_ctx *lctx;

   if (!nm || !nm->settings_proxy) return;

   DBG("enm_vpn_enumerate called (cache has %u active paths)",
       (unsigned int)eina_list_count(nm->vpn_pending_active_paths));

   /* Bump generation so any in-flight reply from the previous call is
    * recognised as stale in its callback. */
   nm->vpn_generation++;

   lctx = malloc(sizeof(*lctx));
   if (!lctx) return;
   lctx->nm         = nm;
   lctx->generation = nm->vpn_generation;

   nm->vpn_list_pending =
       eldbus_proxy_call(nm->settings_proxy, "ListConnections",
                         _vpn_list_cb, lctx, -1, "");
}

void
enm_vpn_clear(struct NM_Manager *nm)
{
   if (!nm) return;
   while (nm->vpn_connections)
     {
        struct NM_VPN_Connection *vc = EINA_INLIST_CONTAINER_GET(
              nm->vpn_connections, struct NM_VPN_Connection);
        nm->vpn_connections = eina_inlist_remove(nm->vpn_connections,
                                                 EINA_INLIST_GET(vc));
        enm_vpn_connection_free(vc);
     }
}

struct NM_VPN_Connection *
enm_vpn_find_by_path(struct NM_Manager *nm, const char *path)
{
   struct NM_VPN_Connection *vc;
   if (!nm || !path) return NULL;
   EINA_INLIST_FOREACH(nm->vpn_connections, vc)
     if (!strcmp(vc->path, path)) return vc;
   return NULL;
}

struct NM_VPN_Connection *
enm_vpn_find_by_active(struct NM_Manager *nm, const char *active_path)
{
   struct NM_VPN_Connection *vc;
   if (!nm || !active_path) return NULL;
   EINA_INLIST_FOREACH(nm->vpn_connections, vc)
     if (vc->active_path && !strcmp(vc->active_path, active_path)) return vc;
   return NULL;
}

unsigned int
enm_vpn_active_count(struct NM_Manager *nm)
{
   struct NM_VPN_Connection *vc;
   unsigned int n = 0;
   if (!nm) return 0;
   EINA_INLIST_FOREACH(nm->vpn_connections, vc)
     if (vc->vpn_state == NM_VPN_STATE_ACTIVATED) n++;
   return n;
}

/* -------------------------------------------------------------------------- */
/* VPN actions (Task 6)                                                       */
/* -------------------------------------------------------------------------- */

void
enm_vpn_activate(struct NM_Manager *nm, struct NM_VPN_Connection *vc)
{
   if (!nm || !vc || !vc->path) return;
   /* ActivateConnection(connection, device, specific_object)
    * "/" means "let NM auto-pick the device and specific object" — standard
    * for VPN activation which is not bound to a single network device. */
   eldbus_proxy_call(nm->proxy, "ActivateConnection", NULL, NULL, -1,
                     "ooo", vc->path, "/", "/");
}

void
enm_vpn_deactivate(struct NM_Manager *nm, struct NM_VPN_Connection *vc)
{
   if (!nm || !vc || !vc->active_path) return;
   /* DeactivateConnection takes the ACTIVE connection path, not the settings path. */
   eldbus_proxy_call(nm->proxy, "DeactivateConnection", NULL, NULL, -1,
                     "o", vc->active_path);
}

/* Context for the in-flight Delete call — proxy/obj freed in the callback. */
struct _vpn_delete_ctx
{
   struct NM_Manager *nm;
   const char        *path;   /* stringshare */
   Eldbus_Proxy      *proxy;
   Eldbus_Object     *obj;
};

static void
_vpn_delete_cb(void *data, const Eldbus_Message *msg,
               Eldbus_Pending *pending EINA_UNUSED)
{
   struct _vpn_delete_ctx *ctx = data;
   const char *err_name, *err_msg;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     ERR("VPN Delete failed for %s: %s %s",
         ctx->path, err_name ?: "", err_msg ?: "");
   /* On success NM emits ConnectionRemoved, which triggers enm_vpn_enumerate
    * via _settings_conn_removed_cb — no further action needed here. */

   eldbus_proxy_unref(ctx->proxy);
   eldbus_object_unref(ctx->obj);
   eina_stringshare_del(ctx->path);
   free(ctx);
}

void
enm_vpn_delete(struct NM_Manager *nm, struct NM_VPN_Connection *vc)
{
   struct _vpn_delete_ctx *ctx;

   if (!nm || !vc || !vc->path) return;

   ctx = malloc(sizeof(*ctx));
   if (!ctx) return;

   ctx->nm    = nm;
   ctx->path  = eina_stringshare_add(vc->path);
   ctx->obj   = eldbus_object_get(_enm_dbus_conn_get(),
                                  NM_BUS_NAME, vc->path);
   if (!ctx->obj)
     {
        eina_stringshare_del(ctx->path);
        free(ctx);
        return;
     }
   ctx->proxy = eldbus_proxy_get(ctx->obj, NM_IFACE_SCONN);
   if (!ctx->proxy)
     {
        eldbus_object_unref(ctx->obj);
        eina_stringshare_del(ctx->path);
        free(ctx);
        return;
     }

   eldbus_proxy_call(ctx->proxy, "Delete",
                     _vpn_delete_cb, ctx, -1, "");
   /* ctx, proxy, and obj are freed/unref'd inside _vpn_delete_cb */
}

/* -------------------------------------------------------------------------- */
/* enm_vpn_find_by_uuid / enm_vpn_set_autoconnect                            */
/* -------------------------------------------------------------------------- */

struct NM_VPN_Connection *
enm_vpn_find_by_uuid(struct NM_Manager *nm, const char *uuid)
{
   struct NM_VPN_Connection *vc;
   if (!nm || !uuid) return NULL;
   EINA_INLIST_FOREACH(nm->vpn_connections, vc)
     if (vc->uuid && !strcmp(vc->uuid, uuid)) return vc;
   return NULL;
}

/* Context for the in-flight nmcli `connection modify ... autoconnect` flow.
 * Tracked in nm->vpn_pending_autoconn so _manager_free can cancel safely. */
struct _autoconn_ctx
{
   struct NM_Manager *nm;
   char              *uuid;       /* strdup — used to look up vc in the cb */
   Eina_Bool          target;
   Ecore_Exe         *exe;
   Ecore_Event_Handler *handler_err;
   Ecore_Event_Handler *handler_del;
   Eina_Strbuf       *stderr_buf;
};

static void
_autoconn_ctx_free(struct _autoconn_ctx *ctx)
{
   if (!ctx) return;
   if (ctx->handler_err) ecore_event_handler_del(ctx->handler_err);
   if (ctx->handler_del) ecore_event_handler_del(ctx->handler_del);
   if (ctx->stderr_buf) eina_strbuf_free(ctx->stderr_buf);
   if (ctx->exe) ecore_exe_free(ctx->exe);
   free(ctx->uuid);
   free(ctx);
}

static char *
_autoconn_shell_escape_single(const char *s)
{
   Eina_Strbuf *b;
   char *out;

   if (!s) return NULL;
   b = eina_strbuf_new();
   if (!b) return NULL;

   for (const char *p = s; *p; p++)
     {
        if (*p == '\'') eina_strbuf_append(b, "'\\''");
        else eina_strbuf_append_char(b, *p);
     }
   out = strdup(eina_strbuf_string_get(b));
   eina_strbuf_free(b);
   return out;
}

static Eina_Bool
_autoconn_stderr_cb(void *data, int type EINA_UNUSED, void *event)
{
   struct _autoconn_ctx *ctx = data;
   Ecore_Exe_Event_Data *ev = event;

   if (!ctx || !ev || (ev->exe != ctx->exe))
     return ECORE_CALLBACK_PASS_ON;

   if (ev->data && (ev->size > 0))
     eina_strbuf_append_length(ctx->stderr_buf, ev->data, ev->size);

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_autoconn_del_cb(void *data, int type EINA_UNUSED, void *event)
{
   struct _autoconn_ctx *ctx = data;
   Ecore_Exe_Event_Del *ev = event;
   const char *stderr_text;

   if (!ctx || !ev || (ev->exe != ctx->exe))
     return ECORE_CALLBACK_PASS_ON;

   ctx->nm->vpn_pending_autoconn =
       eina_list_remove(ctx->nm->vpn_pending_autoconn, ctx);

   if (ctx->handler_err)
     {
        ecore_event_handler_del(ctx->handler_err);
        ctx->handler_err = NULL;
     }
   if (ctx->handler_del)
     {
        ecore_event_handler_del(ctx->handler_del);
        ctx->handler_del = NULL;
     }
   ctx->exe = NULL; /* process already exited */

   stderr_text = ctx->stderr_buf ? eina_strbuf_string_get(ctx->stderr_buf) : "";
   if (!(ev->exited && (ev->exit_code == 0)))
     {
        ERR("nmcli autoconnect modify failed for %s: %s",
            ctx->uuid ?: "?", stderr_text ?: "");
     }
   else
     {
        struct NM_VPN_Connection *vc = enm_vpn_find_by_uuid(ctx->nm, ctx->uuid);
        if (vc) vc->autoconnect = ctx->target;
     }

   _autoconn_ctx_free(ctx);
   return ECORE_CALLBACK_DONE;
}

void
enm_vpn_set_autoconnect(struct NM_Manager *nm,
                        struct NM_VPN_Connection *vc, Eina_Bool on)
{
   struct _autoconn_ctx *ctx;
   const char *nmcli;
   char *esc_nmcli, *esc_uuid;
   char cmd[4096];
   const char *value;

   if (!nm || !vc || !vc->path || !vc->uuid) return;
   if (vc->autoconnect == on) return;

   nmcli = enm_import_nmcli_path();
   if (!nmcli)
     {
        ERR("enm_vpn_set_autoconnect: nmcli not found on PATH");
        return;
     }

   ctx = E_NEW(struct _autoconn_ctx, 1);
   if (!ctx) return;
   ctx->nm     = nm;
   ctx->target = on;
   ctx->uuid   = strdup(vc->uuid);
   if (!ctx->uuid) { free(ctx); return; }
   ctx->stderr_buf = eina_strbuf_new();
   if (!ctx->stderr_buf) { _autoconn_ctx_free(ctx); return; }

   esc_nmcli = _autoconn_shell_escape_single(nmcli);
   esc_uuid = _autoconn_shell_escape_single(vc->uuid);
   if (!esc_nmcli || !esc_uuid)
     {
        free(esc_nmcli);
        free(esc_uuid);
        _autoconn_ctx_free(ctx);
        return;
     }

   value = on ? "yes" : "no";
   snprintf(cmd, sizeof(cmd),
            "'%s' connection modify uuid '%s' connection.autoconnect %s",
            esc_nmcli, esc_uuid, value);
   free(esc_nmcli);
   free(esc_uuid);

   /* Append to tracking list BEFORE dispatching the call so that the
    * cancel-all path can drain us even if shutdown lands immediately. */
   nm->vpn_pending_autoconn = eina_list_append(nm->vpn_pending_autoconn, ctx);

   ctx->exe = ecore_exe_pipe_run(cmd,
                                 ECORE_EXE_PIPE_ERROR |
                                 ECORE_EXE_PIPE_ERROR_LINE_BUFFERED |
                                 ECORE_EXE_NOT_LEADER |
                                 ECORE_EXE_TERM_WITH_PARENT,
                                 ctx);
   if (!ctx->exe)
     {
        ERR("enm_vpn_set_autoconnect: failed to spawn nmcli");
        nm->vpn_pending_autoconn =
            eina_list_remove(nm->vpn_pending_autoconn, ctx);
        _autoconn_ctx_free(ctx);
        return;
     }

   ctx->handler_err = ecore_event_handler_add(ECORE_EXE_EVENT_ERROR,
                                              _autoconn_stderr_cb, ctx);
   ctx->handler_del = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                              _autoconn_del_cb, ctx);
}

void
enm_vpn_autoconn_pending_cancel_all(struct NM_Manager *nm)
{
   if (!nm) return;

   /* Drain in-flight nmcli autoconnect modifies.  Detach first so the
    * process-exit callback cannot re-enter the list we are draining. */
   while (nm->vpn_pending_autoconn)
     {
        struct _autoconn_ctx *ctx = nm->vpn_pending_autoconn->data;
        nm->vpn_pending_autoconn =
            eina_list_remove(nm->vpn_pending_autoconn, ctx);
        if (ctx->handler_err)
          {
             ecore_event_handler_del(ctx->handler_err);
             ctx->handler_err = NULL;
          }
        if (ctx->handler_del)
          {
             ecore_event_handler_del(ctx->handler_del);
             ctx->handler_del = NULL;
          }
        if (ctx->exe)
          {
             ecore_exe_interrupt(ctx->exe);
             ecore_exe_quit(ctx->exe);
             ecore_exe_terminate(ctx->exe);
             ecore_exe_kill(ctx->exe);
          }
        _autoconn_ctx_free(ctx);
     }
}

/* -------------------------------------------------------------------------- */
/* Active VPN state binding (Task 5)                                           */
/* -------------------------------------------------------------------------- */

#define NM_IFACE_VPN_CONN  "org.freedesktop.NetworkManager.VPN.Connection"
/* NM_IFACE_ACTIVE_CONN is defined in e_networkmanager.h */
#define NM_IFACE_DBUS_PROP "org.freedesktop.DBus.Properties"
#define NM_IFACE_IP4CONFIG "org.freedesktop.NetworkManager.IP4Config"

/* Translate NMActiveConnectionState (used by WireGuard's Connection.Active
 * "State" property) into NM_VPN_State:
 *   0 UNKNOWN       -> NM_VPN_STATE_UNKNOWN
 *   1 ACTIVATING    -> NM_VPN_STATE_CONNECT
 *   2 ACTIVATED     -> NM_VPN_STATE_ACTIVATED
 *   3 DEACTIVATING  -> NM_VPN_STATE_CONNECT  (still up, tearing down)
 *   4 DEACTIVATED   -> NM_VPN_STATE_DISCONNECTED
 */
static enum NM_VPN_State
_wg_state_to_vpn_state(uint32_t ac_state)
{
   switch (ac_state)
     {
      case 0: return NM_VPN_STATE_UNKNOWN;
      case 1: return NM_VPN_STATE_CONNECT;       /* ACTIVATING */
      case 2: return NM_VPN_STATE_ACTIVATED;     /* ACTIVATED */
      case 3: return NM_VPN_STATE_CONNECT;       /* DEACTIVATING */
      case 4: return NM_VPN_STATE_DISCONNECTED;  /* DEACTIVATED */
      default: return NM_VPN_STATE_UNKNOWN;
     }
}

/* Forward declaration */
static void _vpn_bind_ip4(struct NM_VPN_Connection *vc, const char *ip4_path);

/* -------------------------------------------------------------------------- */
/* Ip4Config tracking helpers                                                  */
/* -------------------------------------------------------------------------- */

static void
_vpn_unbind_ip4(struct NM_VPN_Connection *vc)
{
   if (!vc) return;
   if (vc->pending_ip4_props)
     {
        eldbus_pending_cancel(vc->pending_ip4_props);
        vc->pending_ip4_props = NULL;
     }
   if (vc->ip4_prop_handler)
     {
        eldbus_signal_handler_del(vc->ip4_prop_handler);
        vc->ip4_prop_handler = NULL;
     }
   if (vc->ip4_proxy)
     {
        eldbus_proxy_unref(vc->ip4_proxy);
        vc->ip4_proxy = NULL;
     }
   if (vc->ip4_obj)
     {
        eldbus_object_unref(vc->ip4_obj);
        vc->ip4_obj = NULL;
     }
}

static void
_vpn_ip4_get_all_cb(void *data, const Eldbus_Message *msg,
                    Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_VPN_Connection *vc = data;
   Eldbus_Message_Iter *props, *entry, *var;
   const char *err_name, *err_msg, *key;

   vc->pending_ip4_props = NULL;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        if (err_name &&
            strcmp(err_name, ELDBUS_ERROR_PENDING_CANCELED) != 0 &&
            strcmp(err_name, "org.freedesktop.DBus.Error.NoReply") != 0)
          WRN("Ip4Config GetAll failed for VPN '%s': %s %s",
              vc->name ?: "?", err_name ?: "", err_msg ?: "");
        return;
     }
   if (!eldbus_message_arguments_get(msg, "a{sv}", &props)) return;

   while (eldbus_message_iter_get_and_next(props, 'e', &entry))
     {
        if (!eldbus_message_iter_arguments_get(entry, "sv", &key, &var))
          continue;
        if (!strcmp(key, "AddressData"))
          {
             /* AddressData is aa{sv}: an array of dicts.
              * The variant wraps the outer array, so unwrap to get 'a'. */
             Eldbus_Message_Iter *outer_arr;
             if (!eldbus_message_iter_arguments_get(var, "aa{sv}", &outer_arr))
               continue;

             /* Iterate the outer array; each element is a{sv} */
             Eldbus_Message_Iter *inner_dict;
             while (eldbus_message_iter_get_and_next(outer_arr, 'a', &inner_dict))
               {
                  Eldbus_Message_Iter *dict_entry, *dvar;
                  const char *dkey;
                  char *found_addr = NULL;

                  while (eldbus_message_iter_get_and_next(inner_dict, 'e',
                                                          &dict_entry))
                    {
                       if (!eldbus_message_iter_arguments_get(dict_entry, "sv",
                                                              &dkey, &dvar))
                         continue;
                       if (!strcmp(dkey, "address"))
                         {
                            const char *s;
                            if (eldbus_message_iter_arguments_get(dvar, "s", &s))
                              found_addr = strdup(s);
                         }
                    }
                  if (found_addr)
                    {
                       free(vc->ip_address);
                       vc->ip_address = found_addr;
                       DBG("VPN '%s' got IP address %s", vc->name ?: "?",
                           vc->ip_address ?: "(none)");
                       break; /* first entry is sufficient */
                    }
               }
          }
     }

   enm_vpn_active_changed_schedule(vc->nm);
}

static void
_vpn_ip4_prop_changed(void *data, const Eldbus_Message *msg)
{
   struct NM_VPN_Connection *vc = data;
   Eldbus_Message_Iter *changed, *entry, *var, *invalidated;
   const char *iface, *key;

   /* The "as" invalidated array is required by the signature; passing
    * NULL crashes eldbus_message_arguments_vget with a deref of 0x0. */
   if (!eldbus_message_arguments_get(msg, "sa{sv}as",
                                     &iface, &changed, &invalidated))
     return;
   (void)invalidated;

   /* If AddressData changed, re-fetch all properties for simplicity */
   while (eldbus_message_iter_get_and_next(changed, 'e', &entry))
     {
        if (!eldbus_message_iter_arguments_get(entry, "sv", &key, &var))
          continue;
        if (!strcmp(key, "AddressData"))
          {
             /* Re-issue GetAll to re-parse */
             if (vc->pending_ip4_props)
               {
                  eldbus_pending_cancel(vc->pending_ip4_props);
                  vc->pending_ip4_props = NULL;
               }
             if (vc->ip4_proxy)
               vc->pending_ip4_props =
                   eldbus_proxy_call(vc->ip4_proxy, "GetAll",
                                     _vpn_ip4_get_all_cb, vc, -1,
                                     "s", NM_IFACE_IP4CONFIG);
             break;
          }
     }
}

static void
_vpn_bind_ip4(struct NM_VPN_Connection *vc, const char *ip4_path)
{
   Eldbus_Connection *dbus_conn;

   if (!vc || !ip4_path || !strcmp(ip4_path, "/")) return;

   /* Release any existing ip4 state */
   _vpn_unbind_ip4(vc);
   /* Also clear any previously stored address */
   free(vc->ip_address);
   vc->ip_address = NULL;

   dbus_conn = _enm_dbus_conn_get();
   vc->ip4_obj = eldbus_object_get(dbus_conn, NM_BUS_NAME, ip4_path);
   if (!vc->ip4_obj) return;
   vc->ip4_proxy = eldbus_proxy_get(vc->ip4_obj, NM_IFACE_DBUS_PROP);
   if (!vc->ip4_proxy)
     {
        eldbus_object_unref(vc->ip4_obj);
        vc->ip4_obj = NULL;
        return;
     }

   /* Subscribe PropertiesChanged */
   vc->ip4_prop_handler =
       eldbus_proxy_signal_handler_add(vc->ip4_proxy, "PropertiesChanged",
                                       _vpn_ip4_prop_changed, vc);

   /* Seed: pull current properties */
   vc->pending_ip4_props =
       eldbus_proxy_call(vc->ip4_proxy, "GetAll",
                         _vpn_ip4_get_all_cb, vc, -1,
                         "s", NM_IFACE_IP4CONFIG);

   DBG("VPN '%s' bound Ip4Config path=%s", vc->name ?: "?", ip4_path);
}

static void
_vpn_active_props_get_cb(void *data, const Eldbus_Message *msg,
                         Eldbus_Pending *pending EINA_UNUSED)
{
   struct NM_VPN_Connection *vc = data;
   Eldbus_Message_Iter *dict, *entry, *var;
   const char *err_name, *err_msg, *key;

   DBG("VPN active props cb fired for vc='%s'", vc->name ?: "?");

   vc->pending_get_props = NULL;

   if (eldbus_message_error_get(msg, &err_name, &err_msg))
     {
        ERR("VPN active props cb: error '%s' / '%s' — bailing",
            err_name ?: "(null)", err_msg ?: "(null)");
        return;
     }
   if (!eldbus_message_arguments_get(msg, "a{sv}", &dict))
     {
        ERR("VPN active props cb: failed to parse reply as a{sv} — bailing");
        return;
     }
   DBG("VPN active props cb: reply parsed, iterating dict");

   {
      /* Two-pass: first collect Ip4Config path so we can bind after state. */
      char *ip4_path_found = NULL;

      while (eldbus_message_iter_get_and_next(dict, 'e', &entry))
        {
           if (!eldbus_message_iter_arguments_get(entry, "sv", &key, &var))
             continue;
           if (!strcmp(key, "VpnState"))
             {
                uint32_t s;
                if (eldbus_message_iter_arguments_get(var, "u", &s))
                  vc->vpn_state = (enum NM_VPN_State)s;
             }
           else if (!strcmp(key, "State") &&
                    vc->conn_type && !strcmp(vc->conn_type, "wireguard"))
             {
                uint32_t s;
                if (eldbus_message_iter_arguments_get(var, "u", &s))
                  vc->vpn_state = _wg_state_to_vpn_state(s);
             }
           else if (!strcmp(key, "Reason"))
             {
                uint32_t r;
                if (eldbus_message_iter_arguments_get(var, "u", &r))
                  vc->fail_reason = r;
             }
           else if (!strcmp(key, "Ip4Config"))
             {
                const char *s;
                if (eldbus_message_iter_arguments_get(var, "o", &s) &&
                    s && strcmp(s, "/"))
                  ip4_path_found = strdup(s);
             }
        }

      if (ip4_path_found)
        {
           _vpn_bind_ip4(vc, ip4_path_found);
           free(ip4_path_found);
        }
   }

   DBG("VPN '%s' state -> %u (prev %u)",
       vc->name ?: "?", vc->vpn_state, vc->prev_state);

   /* Initial GetAll is informational, not a transition.  Sync prev_state so
    * a pre-existing FAILED state at startup does not trigger a notification
    * — that fires only on real PropertiesChanged transitions. */
   vc->prev_state = vc->vpn_state;

   enm_vpn_active_changed_schedule(vc->nm);
}

static void
_vpn_active_prop_changed(void *data, const Eldbus_Message *msg)
{
   struct NM_VPN_Connection *vc = data;
   Eldbus_Message_Iter *changed, *entry, *var, *invalidated;
   const char *iface, *key;

   if (!eldbus_message_arguments_get(msg, "sa{sv}as",
                                     &iface, &changed, &invalidated))
     return;

   {
      char *ip4_path_found = NULL;

      while (eldbus_message_iter_get_and_next(changed, 'e', &entry))
        {
           if (!eldbus_message_iter_arguments_get(entry, "sv", &key, &var))
             continue;
           if (!strcmp(key, "VpnState"))
             {
                uint32_t s;
                if (eldbus_message_iter_arguments_get(var, "u", &s))
                  {
                     /* Sync prev_state ONLY when the state field actually
                      * arrives in this signal.  Unconditional sync masks the
                      * next real transition if an unrelated property changed
                      * first (e.g. Ip4Config). */
                     vc->prev_state = vc->vpn_state;
                     vc->vpn_state = (enum NM_VPN_State)s;
                     DBG("VPN '%s' state -> %u (prev %u)",
                         vc->name ?: "?", vc->vpn_state, vc->prev_state);
                  }
             }
           else if (!strcmp(key, "State") &&
                    vc->conn_type && !strcmp(vc->conn_type, "wireguard"))
             {
                uint32_t s;
                if (eldbus_message_iter_arguments_get(var, "u", &s))
                  {
                     vc->prev_state = vc->vpn_state;
                     vc->vpn_state = _wg_state_to_vpn_state(s);
                     DBG("VPN '%s' state -> %u (prev %u)",
                         vc->name ?: "?", vc->vpn_state, vc->prev_state);
                  }
             }
           else if (!strcmp(key, "Reason"))
             {
                uint32_t r;
                if (eldbus_message_iter_arguments_get(var, "u", &r))
                  vc->fail_reason = r;
             }
           else if (!strcmp(key, "Ip4Config"))
             {
                const char *s;
                if (eldbus_message_iter_arguments_get(var, "o", &s) &&
                    s && strcmp(s, "/"))
                  ip4_path_found = strdup(s);
                else
                  {
                     /* IP4 removed (e.g. tunnel tearing down) */
                     _vpn_unbind_ip4(vc);
                     free(vc->ip_address);
                     vc->ip_address = NULL;
                  }
             }
        }

      if (ip4_path_found)
        {
           _vpn_bind_ip4(vc, ip4_path_found);
           free(ip4_path_found);
        }
   }

   enm_vpn_active_changed_schedule(vc->nm);
}

void
enm_vpn_active_bind(struct NM_VPN_Connection *vc, const char *active_path)
{
   Eldbus_Connection *dbus_conn;
   const char *iface;

   if (!vc || !active_path) return;

   /* No-op: already bound to this exact path.
    * Both sides are (or will be) stringshares — use pointer equality after
    * interning active_path so we don't spuriously unbind+rebind. */
   {
      const char *shared = eina_stringshare_add(active_path);
      Eina_Bool same = (vc->active_path == shared);
      eina_stringshare_del(shared);
      if (same) return;
   }

   /* Unbind any previous active state */
   enm_vpn_active_unbind(vc);

   vc->active_path = eina_stringshare_add(active_path);

   /* Plugin VPN → VPN.Connection interface; WireGuard → Connection.Active */
   iface = (vc->conn_type && !strcmp(vc->conn_type, "wireguard"))
           ? NM_IFACE_ACTIVE_CONN
           : NM_IFACE_VPN_CONN;

   dbus_conn = _enm_dbus_conn_get();
   vc->active_obj = eldbus_object_get(dbus_conn, NM_BUS_NAME, active_path);

   /* We use the Properties interface proxy for both signal subscription and
    * GetAll — the interface-specific proxy (iface) is not needed for Task 5.
    * Store it as active_proxy so unbind can unref it cleanly. */
   vc->active_proxy = eldbus_proxy_get(vc->active_obj, NM_IFACE_DBUS_PROP);

   /* Subscribe PropertiesChanged */
   vc->active_prop_handler =
       eldbus_proxy_signal_handler_add(vc->active_proxy, "PropertiesChanged",
                                       _vpn_active_prop_changed, vc);

   /* Seed: pull current properties for the active interface */
   vc->pending_get_props =
       eldbus_proxy_call(vc->active_proxy, "GetAll",
                         _vpn_active_props_get_cb, vc, -1,
                         "s", iface);

   DBG("VPN '%s' bound to active_path=%s (iface=%s) pending=%p",
       vc->name ?: "?", active_path, iface, vc->pending_get_props);
}

void
enm_vpn_active_unbind(struct NM_VPN_Connection *vc)
{
   if (!vc) return;

   if (vc->pending_get_props)
     {
        eldbus_pending_cancel(vc->pending_get_props);
        vc->pending_get_props = NULL;
     }
   if (vc->active_prop_handler)
     {
        eldbus_signal_handler_del(vc->active_prop_handler);
        vc->active_prop_handler = NULL;
     }
   if (vc->active_proxy)
     {
        eldbus_proxy_unref(vc->active_proxy);
        vc->active_proxy = NULL;
     }
   if (vc->active_obj)
     {
        eldbus_object_unref(vc->active_obj);
        vc->active_obj = NULL;
     }

   /* Release ip4 tracking state */
   _vpn_unbind_ip4(vc);

   eina_stringshare_replace(&vc->active_path, NULL);
   free(vc->ip_address);
   vc->ip_address = NULL;
   vc->vpn_state = NM_VPN_STATE_DISCONNECTED;
}
