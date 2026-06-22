#ifndef E_NETWORKMANAGER_VPN_USERNAME_H
#define E_NETWORKMANAGER_VPN_USERNAME_H

#include <Eina.h>

/*
 * VPN username capture.
 *
 * NetworkManager stores a VPN username as a connection *property*
 * (vpn.data["username"]), not as a secret, and the SecretAgent D-Bus API
 * (GetSecrets) only ever deals with secrets.  NM therefore never prompts for
 * a missing username: importing an .ovpn with a bare `auth-user-pass` yields a
 * connection with password-flags set but no username, which is unusable.
 *
 * This long-standing NetworkManager behaviour is reported in:
 *   - https://bugzilla.redhat.com/show_bug.cgi?id=1535517
 *   - https://bugzilla.redhat.com/show_bug.cgi?id=1548873
 *   - https://bbs.archlinux.org/viewtopic.php?id=286378
 *   - https://bbs.archlinux.org/viewtopic.php?id=225395
 *   - https://github.com/pop-os/cosmic-settings/issues/1820
 *
 * All functions here are MAIN THREAD ONLY.
 */

/* True when the VPN type uses username auth and current_username is empty.
 * svc_short is the short service name ("openvpn", "pptp", ...); conn_type is
 * vpn.data["connection-type"] (may be NULL). */
Eina_Bool enm_vpn_username_needed(const char *svc_short,
                                  const char *conn_type,
                                  const char *current_username);

/* Called with the entered username (NULL on cancel). */
typedef void (*Enm_Username_Entered_Cb)(void *data, const char *username);

/* Called when the nmcli modify completes. */
typedef void (*Enm_Username_Done_Cb)(void *data, Eina_Bool ok);

/* Persist username to vpn.data["username"] via `nmcli connection modify`. */
void enm_vpn_username_set(const char *conn_name, const char *username,
                          Enm_Username_Done_Cb cb, void *data);

/* Single-field username dialog.  Calls cb(username) on OK, cb(NULL) on cancel. */
void enm_vpn_username_dialog(const char *conn_name, const char *type_label,
                             const char *initial,
                             Enm_Username_Entered_Cb cb, void *data);

/* Post-import orchestrator: query the connection (by UUID), and if it needs a
 * username, prompt for one and persist it.  svc_short is the VPN service short
 * name ("openvpn", "vpnc", ...) from the import type.  Fire-and-forget. */
void enm_vpn_username_maybe_prompt(const char *conn_uuid, const char *svc_short);

#endif
