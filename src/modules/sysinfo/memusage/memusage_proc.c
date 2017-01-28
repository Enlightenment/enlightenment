#include "memusage.h"


unsigned long _line_parse(const char *line)
{
   char *p, *tok;

   p = strchr(line, ':') + 1;
   while(isspace(*p)) p++;
   tok = strtok(p, " ");
   return atol(tok);
}

void _memusage_proc_getusage(unsigned long *mem_total,
                             unsigned long *mem_active,
                             unsigned long *mem_cached,
                             unsigned long *mem_buffers,
                             unsigned long *swp_total,
                             unsigned long *swp_active)
{
   char line[256];
   int found = 0;
   long tmp_swp_total = -1;
   long tmp_swp_free = -1;
   FILE *f;

   f = fopen("/proc/meminfo", "r");
   if (!f) return;

   while(fgets(line, sizeof(line), f) != NULL)
     {
        if (!strncmp("MemTotal:", line, 9))
          {
             *mem_total = _line_parse(line);
             found++;
          }
        else if (!strncmp("Active:", line, 7))
          {
             *mem_active = _line_parse(line);
             found++;
          }
        else if (!strncmp("Cached:", line, 7))
          {
             *mem_cached = _line_parse(line);
             found++;
          }
        else if (!strncmp("Buffers:", line, 8))
          {
             *mem_buffers = _line_parse(line);
             found++;
          }
        else if (!strncmp("SwapTotal:", line, 10))
          {
             tmp_swp_total = _line_parse(line);
             found++;
          }
        else if (!strncmp("SwapFree:", line, 9))
          {
             tmp_swp_free = _line_parse(line);
             found++;
          }

         if (found >= 6)
           break;
     }
   fclose(f);

   if ((tmp_swp_total != -1) && (tmp_swp_free != -1))
     {
        *swp_total = tmp_swp_total;
        *swp_active = tmp_swp_total - tmp_swp_free;
     }
}
