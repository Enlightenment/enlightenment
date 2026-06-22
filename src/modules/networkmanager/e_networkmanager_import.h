#ifndef E_NETWORKMANAGER_IMPORT_H
#define E_NETWORKMANAGER_IMPORT_H

#include "e.h"

/*
 * Threading contract for everything declared in this header:
 *
 * All functions are MAIN THREAD ONLY.  enm_import_run spawns a subprocess
 * via Ecore_Exe and dispatches the completion callback on the main loop;
 * the caller must not invoke any of these helpers from a worker thread.
 *
 * enm_import_run has NO cancellation API.  Once dispatched, the caller
 * MUST keep `data` alive until done_cb fires — there is no way to detach
 * an in-flight import.
 */

/* Returns the absolute path to nmcli, or NULL if it isn't on PATH.
 * Cached after the first call. */
const char *enm_import_nmcli_path(void);

/* Auto-detect a VPN type from a file extension.
 * Returns one of: "wireguard", "openvpn", "vpnc", "openconnect", NULL. */
const char *enm_import_detect_type(const char *file_path);

/* Callback fired once when nmcli import completes.
 *
 * stderr_text points into an internal buffer that is freed immediately
 * after the callback returns; copy it if you need to retain.
 *
 * conn_name is the imported connection's name parsed from nmcli stdout
 * (NULL on failure or when it cannot be parsed); it too is freed right
 * after the callback returns.
 *
 * data must remain valid until done_cb fires.  enm_import_run does not
 * return a cancellation handle, so the caller is responsible for
 * ensuring it does not deallocate data while a call is in flight.
 */
typedef void (*Enm_Import_Done_Cb)(void *data, Eina_Bool ok,
                                   const char *stderr_text,
                                   const char *conn_name);

/* Run nmcli connection import asynchronously.  done_cb is invoked exactly
 * once. */
void enm_import_run(const char *type, const char *file_path,
                    Enm_Import_Done_Cb done_cb, void *data);

#endif
