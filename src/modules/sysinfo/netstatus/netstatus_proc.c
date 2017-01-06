#include "netstatus.h"

const char *
_netstatus_proc_getrstatus(Instance *inst)
{
   long in, dummy, tot_in = 0;
   long diffin;
   char buf[4096], rin[4096], dummys[64];
   FILE *f;

   f = fopen("/proc/net/dev", "r");
   if (f)
     {
        while(fgets(buf, sizeof(buf), f) != NULL)
          {
             if (sscanf (buf, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                    "%lu %lu %lu %lu\n", dummys, &in, &dummy, &dummy,
                    &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                    &dummy, &dummy, &dummy, &dummy, &dummy, &dummy) < 17)
               continue;
             tot_in += in;
          }
        fclose(f);
     }
   diffin = tot_in - inst->cfg->netstatus.in;
   inst->cfg->netstatus.in = tot_in;
   if (!diffin) return NULL;
   else
     {
        diffin /= 0.5;
        if (diffin > 1048576)
          snprintf(rin, sizeof(rin), "Rx: %.2f MB/s", ((float)diffin / 1048576));
        else if ((diffin > 1024) && (diffin < 1048576))
          snprintf(rin, sizeof(rin), "Rx: %lu KB/s", (diffin / 1024));
        else
          snprintf(rin, sizeof(rin), "Rx: %lu B/s", diffin);
     }
   return eina_stringshare_add(rin);
}

const char *
 _netstatus_proc_gettstatus(Instance *inst)
{
   long out, dummy, tot_out = 0;
   long diffout;
   char buf[4096], rout[4096], dummys[64];
   FILE *f;

   f = fopen("/proc/net/dev", "r");
   if (f)
     {
        while(fgets(buf, sizeof(buf), f) != NULL)
          {
             if (sscanf (buf, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                    "%lu %lu %lu %lu\n", dummys, &dummy, &dummy, &dummy,
                    &dummy, &dummy, &dummy, &dummy, &dummy, &out, &dummy,
                    &dummy, &dummy, &dummy, &dummy, &dummy, &dummy) < 17)
               continue;
             tot_out += out;
          }
        fclose(f);
     }
   diffout = tot_out - inst->cfg->netstatus.out;
   inst->cfg->netstatus.out = tot_out;
   if (!diffout) return NULL;
   else
     {
        diffout /= 0.5;
        if (diffout > 1048576)
          snprintf(rout, sizeof(rout), "Tx: %.2f MB/s", ((float)diffout / 1048576));
        else if ((diffout > 1024) && (diffout < 1048576))
          snprintf(rout, sizeof(rout), "Tx: %lu KB/s", (diffout / 1024));
        else
          snprintf(rout, sizeof(rout), "Tx: %lu B/s", diffout);
     }
   return eina_stringshare_add(rout);
}

