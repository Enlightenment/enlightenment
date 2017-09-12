#include "netstatus.h"
#if defined(__FreeBSD__) || defined(__DragonFly__)
# include <sys/socket.h>
# include <sys/sysctl.h>
# include <net/if.h>
# include <net/if_mib.h>
#endif

#if defined(__OpenBSD__)
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/sysctl.h>
# include <sys/sockio.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <net/if_types.h>
# include <ifaddrs.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)

static int
get_ifmib_general(int row, struct ifmibdata *data)
{
   int mib[6];
   size_t len;

   mib[0] = CTL_NET;
   mib[1] = PF_LINK;
   mib[2] = NETLINK_GENERIC;
   mib[3] = IFMIB_IFDATA;
   mib[4] = row;
   mib[5] = IFDATA_GENERAL;

   len = sizeof(*data);

   return sysctl(mib, 6, data, &len, NULL, 0);
}

static void
_freebsd_generic_network_status(unsigned long int *in, unsigned long int *out)
{
   struct ifmibdata *ifmd;
   size_t len;
   int i, count;

   len = sizeof(count);
   if (sysctlbyname("net.link.generic.system.ifcount", &count, &len, NULL, 0) < 0)
     return;

   ifmd = malloc(sizeof(struct ifmibdata));
   if (!ifmd)
     return;

   for (i = 1; i <= count; i++)
     {
        get_ifmib_general(i, ifmd);
        *in += ifmd->ifmd_data.ifi_ibytes;
        *out += ifmd->ifmd_data.ifi_obytes;
     }

   free(ifmd);
}

#endif

#if defined(__OpenBSD__)
static void
_openbsd_generic_network_status(unsigned long int *in, unsigned long int *out)
{
   struct ifaddrs *interfaces, *ifa;

   if (getifaddrs(&interfaces) < 0)
     return;

   int sock = socket(AF_INET, SOCK_STREAM, 0);
   if (sock < 0)
     return;

   for (ifa = interfaces; ifa; ifa = ifa->ifa_next)
     {
        struct ifreq ifreq;
        struct if_data if_data;

        ifreq.ifr_data = (void *)&if_data;
        strncpy(ifreq.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFDATA, &ifreq) < 0)
          return;

        struct if_data *const ifi = &if_data;
        if (ifi->ifi_type == IFT_ETHER || ifi->ifi_type == IFT_IEEE80211)
          {
             if (ifi->ifi_ibytes)
               *in += ifi->ifi_ibytes;
             else
               *in += 0;

             if (ifi->ifi_obytes)
               *out += ifi->ifi_obytes;
             else
               *out += 0;
          }
     }
   close(sock);
}

#endif

void
_netstatus_sysctl_getstatus(Eina_Bool automax,
                            time_t *last_checked,
                            unsigned long *prev_in,
                            unsigned long *prev_incurrent,
                            unsigned long *prev_inmax,
                            int *prev_inpercent,
                            unsigned long *prev_out,
                            unsigned long *prev_outcurrent,
                            unsigned long *prev_outmax,
                            int *prev_outpercent)
{
   unsigned long tot_out = 0, tot_in = 0, diffin, diffout;
   int percent = 0;
   unsigned long int incoming = 0, outgoing = 0;
   time_t current = time(NULL);
   time_t diff = 0;
#if defined(__OpenBSD__)
   _openbsd_generic_network_status(&incoming, &outgoing);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   _freebsd_generic_network_status(&incoming, &outgoing);
#endif
   if (!*last_checked)
     *last_checked = current;
   if ((current - *last_checked) < 1)
     return;
   else
     diff = current - *last_checked;

   tot_in = incoming;
   tot_out = outgoing;

   diffin = tot_in - *prev_in;
   if (diff > 1)
     diffin /= diff;
   if (!*prev_in)
     *prev_in = tot_in;
   else
     {
        *prev_in = tot_in;
        if (automax)
          {
             if (diffin > *prev_inmax)
               *prev_inmax = diffin;
          }
        *prev_incurrent = diffin;
        if (*prev_inmax > 0)
          percent = 100 * ((float)*prev_incurrent / (float)*prev_inmax);
        if (percent > 100) percent = 100;
        else if (percent < 0)
          percent = 0;
        *prev_inpercent = percent;
     }
   
   percent = 0;

   diffout = tot_out - *prev_out;
   if (diff > 1)
     diffout /= diff;
   if (!*prev_out)
     *prev_out = tot_out;
   else
     {
        *prev_out = tot_out;
        if (automax)
          {
             if (diffout > *prev_outmax)
               *prev_outmax = diffout;
          }
        *prev_outcurrent = diffout;
        if (*prev_outcurrent > 0)
          percent = 100 * ((float)*prev_outcurrent / (float)*prev_outmax);
        if (percent > 100) percent = 100;
        else if (percent < 0)
          percent = 0;
        *prev_outpercent = percent;
     }
   *last_checked = current;
}

