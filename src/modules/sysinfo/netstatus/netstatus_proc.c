#include "netstatus.h"

void
_netstatus_proc_getstatus(Eina_Bool automax,
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
   unsigned long in, out, dummy, tot_in = 0, tot_out = 0;
   unsigned long diffin, diffout;
   int percent = 0;
   char buf[4096], dummys[64];
   FILE *f;
   time_t current = time(NULL);
   time_t diff = 0;

   if (!*last_checked)
     *last_checked = current;
   if ((current - *last_checked) < 1)
     return;
   else
     diff = current - *last_checked;

   f = fopen("/proc/net/dev", "r");
   if (f)
     {
        while (fgets(buf, sizeof(buf), f) != NULL)
          {
             if (sscanf(buf, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                             "%lu %lu %lu %lu\n", dummys, &in, &dummy, &dummy,
                        &dummy, &dummy, &dummy, &dummy, &dummy, &out, &dummy,
                        &dummy, &dummy, &dummy, &dummy, &dummy, &dummy) < 17)
               continue;
             tot_in += in;
             tot_out += out;
          }
        fclose(f);
     }

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

