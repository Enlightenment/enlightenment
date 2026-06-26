/* Standalone unit test for enm_vpn_username_needed.
 * Build: gcc -DENM_VPN_USERNAME_TEST test_vpn_username.c -o /tmp/t && /tmp/t */
#include <assert.h>
#include <stdio.h>
#include "l.h"

#define ENM_VPN_USERNAME_TEST 1
#include "e_networkmanager_vpn_username.c"

int main(void)
{
   /* openvpn: only password / password-tls need a username */
   assert(enm_vpn_username_needed("openvpn", "password", NULL));
   assert(enm_vpn_username_needed("openvpn", "password-tls", ""));
   assert(!enm_vpn_username_needed("openvpn", "tls", NULL));
   assert(!enm_vpn_username_needed("openvpn", NULL, NULL));

   /* already has a username -> never needed */
   assert(!enm_vpn_username_needed("openvpn", "password", "alice"));

   /* xauth / password-based types always need it when empty */
   assert(enm_vpn_username_needed("pptp", NULL, NULL));
   assert(enm_vpn_username_needed("l2tp", NULL, ""));
   assert(enm_vpn_username_needed("vpnc", NULL, NULL));
   assert(enm_vpn_username_needed("libreswan", NULL, NULL));
   assert(!enm_vpn_username_needed("libreswan", NULL, "bob"));

   /* unknown / NULL type -> not needed */
   assert(!enm_vpn_username_needed("wireguard", NULL, NULL));
   assert(!enm_vpn_username_needed(NULL, NULL, NULL));

   L("all enm_vpn_username_needed assertions passed");
   return 0;
}
