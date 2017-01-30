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
                             unsigned long *mem_used,
                             unsigned long *mem_cached,
                             unsigned long *mem_buffers,
                             unsigned long *mem_shared,
                             unsigned long *swp_total,
                             unsigned long *swp_used)
{
   char line[256];
   int found = 0;
   unsigned long tmp_swp_total = 0;
   unsigned long tmp_swp_free = 0;
   unsigned long tmp_mem_free = 0;
   unsigned long tmp_mem_cached = 0;
   unsigned long tmp_mem_slab = 0;
   FILE *f;

   *mem_total = 0;
   *mem_used = 0;
   *mem_cached = 0;
   *mem_buffers = 0;
   *mem_shared = 0;
   *swp_total = 0;
   *swp_used = 0;

   f = fopen("/proc/meminfo", "r");
   if (!f) return;

   while(fgets(line, sizeof(line), f) != NULL)
     {
        if (!strncmp("MemTotal:", line, 9))
          {
             *mem_total = _line_parse(line);
             found++;
          }
        else if (!strncmp("MemFree:", line, 8))
          {
             tmp_mem_free = _line_parse(line);
             found++;
          }
        else if (!strncmp("Cached:", line, 7))
          {
             tmp_mem_cached = _line_parse(line);
             found++;
          }
        else if (!strncmp("Slab:", line, 5))
          {
             tmp_mem_slab = _line_parse(line);
             found++;
          }
        else if (!strncmp("Buffers:", line, 8))
          {
             *mem_buffers = _line_parse(line);
             found++;
          }
        else if (!strncmp("Shmem:", line, 6))
          {
             *mem_shared = _line_parse(line);
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

        if (found >= 8)
          break;
     }
   fclose(f);

   *mem_cached = tmp_mem_cached + tmp_mem_slab;
   *mem_used = *mem_total - tmp_mem_free - *mem_cached - *mem_buffers;

   *swp_total = tmp_swp_total;
   *swp_used = tmp_swp_total - tmp_swp_free;
}
