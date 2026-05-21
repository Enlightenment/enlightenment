#ifndef E_NETWORKMANAGER_VPN_H
#define E_NETWORKMANAGER_VPN_H

#include "e_networkmanager.h"

/*
 * Threading contract for everything declared in this header:
 *
 * All functions are MAIN THREAD ONLY.  They drive an Eldbus connection
 * that is bound to the Enlightenment / Ecore main loop and walk Eina
 * data structures that are not protected by any lock.  Calling them
 * from a worker thread is undefined behaviour.
 */

/* Internal accessors (used by the VPN data layer to invoke UI callbacks). */
typedef void (*_mod_cb_vpn_changed_t)(struct NM_Manager *);
_mod_cb_vpn_changed_t _mod_cbs_vpn_changed_get(void);

/* Reconcile active VPN paths against known vpn_connections (e_networkmanager.c).
 * Called by _vpn_get_settings_cb when startup enumeration completes. */
void _enm_vpn_reconcile_active(struct NM_Manager *nm,
                                const char * const *active_paths,
                                unsigned int n_active);

typedef void (*_mod_cb_vpn_active_t)(struct NM_Manager *);
_mod_cb_vpn_active_t _mod_cbs_vpn_active_changed_get(void);

/* Schedule a deferred vpn_active_changed notification via Ecore_Job.
 * Safe to call from within D-Bus signal/reply callbacks — the actual
 * UI rebuild runs after the current dispatch loop iteration completes.
 * Multiple rapid calls coalesce into a single job. */
void enm_vpn_active_changed_schedule(struct NM_Manager *nm);

Eldbus_Connection *_enm_dbus_conn_get(void);

/* VPN collection management. */
void enm_vpn_enumerate(struct NM_Manager *nm);
void enm_vpn_clear(struct NM_Manager *nm);
unsigned int enm_vpn_active_count(struct NM_Manager *nm);

/* Targeted GetSettings for a single Settings.Connection path — used by the
 * ConnectionAdded signal so the whole list does not have to be re-walked
 * for every new connection.  If the path turns out to be a non-VPN, the
 * reply is ignored.  Main thread only. */
void enm_vpn_settings_fetch_one(struct NM_Manager *nm, const char *path);

/* Remove a VPN connection by path if it exists; no-op otherwise.  Fires
 * vpn_changed via the module callback exactly once if a removal occurred.
 * Main thread only. */
void enm_vpn_remove_by_path(struct NM_Manager *nm, const char *path);

/* Lifecycle. */
struct NM_VPN_Connection *enm_vpn_connection_new(const char *path);
void enm_vpn_connection_free(struct NM_VPN_Connection *vc);

/* Actions. */
void enm_vpn_activate(struct NM_Manager *nm, struct NM_VPN_Connection *vc);
void enm_vpn_deactivate(struct NM_Manager *nm, struct NM_VPN_Connection *vc);
void enm_vpn_delete(struct NM_Manager *nm, struct NM_VPN_Connection *vc);
void enm_vpn_set_autoconnect(struct NM_Manager *nm,
                             struct NM_VPN_Connection *vc, Eina_Bool on);

/* Cancel all in-flight nmcli autoconnect subprocesses.  Called from
 * _manager_free to prevent use-after-free when teardown races a subprocess. */
void enm_vpn_autoconn_pending_cancel_all(struct NM_Manager *nm);

/* Lookup helpers. */
struct NM_VPN_Connection *enm_vpn_find_by_path(struct NM_Manager *nm,
                                               const char *path);
struct NM_VPN_Connection *enm_vpn_find_by_active(struct NM_Manager *nm,
                                                 const char *active_path);
struct NM_VPN_Connection *enm_vpn_find_by_uuid(struct NM_Manager *nm,
                                               const char *uuid);

/* Internal: bind/unbind active-state watcher.  Called by e_networkmanager.c
 * when the manager learns an ActiveConnection path matches/leaves this vc. */
void enm_vpn_active_bind(struct NM_VPN_Connection *vc, const char *active_path);
void enm_vpn_active_unbind(struct NM_VPN_Connection *vc);

#endif
