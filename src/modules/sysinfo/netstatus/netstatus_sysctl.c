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
        strncpy(ifreq.ifr_name, ifa->ifa_name, IFNAMSIZ-1);
        if (ioctl(sock, SIOCGIFDATA, &ifreq) < 0)
          return;
	 
        struct if_data * const ifi = &if_data;
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
_netstatus_sysctl_getrstatus(Instance *inst)
{
   char rin[256];
   long tot_in = 0, diffin;
   int percent = 0;
   unsigned long int incoming = 0, outgoing = 0;
#if defined(__OpenBSD__)
   _openbsd_generic_network_status(&incoming, &outgoing);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   _freebsd_generic_network_status(&incoming, &outgoing);
#endif

   tot_in = incoming;
   diffin = tot_in - inst->cfg->netstatus.in;
   if (!inst->cfg->netstatus.in)
     inst->cfg->netstatus.in = tot_in;
   else
     {
        inst->cfg->netstatus.in = tot_in;
        if (inst->cfg->netstatus.automax)
          {
             if (diffin > inst->cfg->netstatus.inmax)
               inst->cfg->netstatus.inmax = diffin;
          }
        inst->cfg->netstatus.incurrent = diffin;
        if (inst->cfg->netstatus.inmax > 0)
          percent = 100 * ((float)inst->cfg->netstatus.incurrent / (float)inst->cfg->netstatus.inmax);
        if (percent > 100) percent = 100;
        else if (percent < 0) percent = 0;
        inst->cfg->netstatus.inpercent = percent;
     }
   if (!diffin)
     {
        snprintf(rin, sizeof(rin), "%s: 0 B/s", _("Receiving"));
     }
   else
     {        
        if (diffin > 1048576)
          snprintf(rin, sizeof(rin), "%s: %.2f MB/s", _("Receiving"), ((float)diffin / 1048576));
        else if ((diffin > 1024) && (diffin < 1048576))
          snprintf(rin, sizeof(rin), "%s: %lu KB/s", _("Receiving"), (diffin / 1024));
        else
          snprintf(rin, sizeof(rin), "%s: %lu B/s", _("Receiving"), diffin);
     }
   eina_stringshare_replace(&inst->cfg->netstatus.instring, rin);
}

void
_netstatus_sysctl_gettstatus(Instance *inst)
{
   char rout[256];
   long tot_out = 0, diffout;
   int percent = 0;
   unsigned long int incoming = 0, outgoing = 0;
#if defined(__OpenBSD__)
   _openbsd_generic_network_status(&incoming, &outgoing);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   _freebsd_generic_network_status(&incoming, &outgoing);
#endif
   tot_out = outgoing;

   diffout = tot_out - inst->cfg->netstatus.out;
   if (!inst->cfg->netstatus.out)
     inst->cfg->netstatus.out = tot_out;
   else
     {
        inst->cfg->netstatus.out = tot_out;
        if (inst->cfg->netstatus.automax)
          {
             if (diffout > inst->cfg->netstatus.outmax)
               inst->cfg->netstatus.outmax = diffout;
          }
        inst->cfg->netstatus.outcurrent = diffout;
        if (inst->cfg->netstatus.outcurrent > 0)
          percent = 100 * ((float)inst->cfg->netstatus.outcurrent / (float)inst->cfg->netstatus.outmax);
        if (percent > 100) percent = 100;
        else if (percent < 0) percent = 0;
        inst->cfg->netstatus.outpercent = percent;
     }
   if (!diffout)
     {
        snprintf(rout, sizeof(rout), "%s: 0 B/s", _("Sending"));
     }
   else
     {
        if (diffout > 1048576)
          snprintf(rout, sizeof(rout), "%s: %.2f MB/s", _("Sending"), ((float)diffout / 1048576));
        else if ((diffout > 1024) && (diffout < 1048576))
          snprintf(rout, sizeof(rout), "%s: %lu KB/s", _("Sending"), (diffout / 1024));
        else
          snprintf(rout, sizeof(rout), "%s: %lu B/s", _("Sending"), diffout);
     }
   eina_stringshare_replace(&inst->cfg->netstatus.outstring, rout);
}

