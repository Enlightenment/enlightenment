#include "config.h"

#include <Ecore.h>

#ifdef HAVE_BLUETOOTH
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#endif

double
e_sys_l2ping(const char *bluetooth_mac)
{
#ifdef HAVE_BLUETOOTH
   char send_buf[L2CAP_CMD_HDR_SIZE + 1];
   char recv_buf[L2CAP_CMD_HDR_SIZE + 1];
   char tmp[18];
   l2cap_cmd_hdr *send_cmd;
   l2cap_cmd_hdr *recv_cmd;
   struct sockaddr_l2 addr;
   socklen_t optlen;
   double start;
   int fd;

   /* Create socket */
   fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_L2CAP);
   if (fd < 0) {
      perror("Can't create socket");
      return -1;
   }

   /* Bind to local address */
   memset(&addr, 0, sizeof(addr));
   addr.l2_family = AF_BLUETOOTH;
   bacpy(&addr.l2_bdaddr, BDADDR_ANY);

   if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
     {
        perror("Can't bind socket");
	return -1;
     }

   /* Connect to remote device */
   memset(&addr, 0, sizeof(addr));
   addr.l2_family = AF_BLUETOOTH;
   str2ba(bluetooth_mac, &addr.l2_bdaddr);

   if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
     {
        perror("Can't connect");
	return -1;
     }

   start = ecore_time_get();

   /* Get local address */
   memset(&addr, 0, sizeof(addr));
   optlen = sizeof(addr);

   if (getsockname(fd, (struct sockaddr *) &addr, &optlen) < 0)
     {
        perror("Can't get local address");
	return -1;
     }

   ba2str(&addr.l2_bdaddr, tmp);

   send_cmd = (l2cap_cmd_hdr *) send_buf;
   send_cmd->ident = 200;
   send_cmd->len = htobs(1);
   send_cmd->code = L2CAP_ECHO_REQ;
   send_buf[L2CAP_CMD_HDR_SIZE] = 'A';

   if (send(fd, send_buf, L2CAP_CMD_HDR_SIZE + 1, 0) <= 0)
     {
        perror("Send failed");
	return -1;
     }

   if (recv(fd, recv_buf, L2CAP_CMD_HDR_SIZE + 1, 0) < 0)
     {
        perror("Recv failed");
	return -1;
     }

   recv_cmd = (l2cap_cmd_hdr *) recv_buf;
   recv_cmd->len = btohs(recv_cmd->len);
   if (recv_cmd->ident != 200)
     return -1; /* Wrong packet */

   close(fd);

   return ecore_time_get() - start;
#else
   (void) bluetooth_mac;
   fprintf(stderr, "e_sys_l2ping nop\n");
   return -1;
#endif
}
