#include "memusage.h"

int _memusage_proc_getswapusage(void)
{
   long swap_total = 0, swap_free = 0, swap_used = 0;
   int percent = 0;
   char buf[4096], *line, *tok;
   FILE *f;

   f = fopen("/proc/meminfo", "r");
   if (f)
     {
        while(fgets(buf, sizeof(buf), f) != NULL)
          {
             if (!strncmp("SwapTotal:", buf, 10))
               {
                  line = strchr(buf, ':')+1;
                  while(isspace(*line)) line++;
                  tok = strtok(line, " ");
                  swap_total = atol(tok);
               }
             else if (!strncmp("SwapFree:", buf, 9))
               {
                  line = strchr(buf, ':')+1;
                  while(isspace(*line)) line++;
                  tok = strtok(line, " ");
                  swap_free = atol(tok);
               }
             if (swap_total && swap_free)
               break;
          }
        fclose(f);

        swap_used = swap_total - swap_free;
        percent = 100 * ((float)swap_used / (float)swap_total);
     }
   if (percent > 100) percent = 100;
   else if (percent < 0) percent = 0;

   return percent;
}

int _memusage_proc_getmemusage(void)
{
   long mem_total = 0, mem_free = 0, mem_used = 0;
   int percent = 0;
   char buf[4096], *line, *tok;
   FILE *f;

   f = fopen("/proc/meminfo", "r");
   if (f)
     {
        while(fgets(buf, sizeof(buf), f) != NULL)
          {
             if (!strncmp("MemTotal:", buf, 9))
               {
                  line = strchr(buf, ':')+1;
                  while(isspace(*line)) line++;
                  tok = strtok(line, " ");
                  mem_total = atol(tok);
               }
             else if (!strncmp("MemFree:", buf, 8))
               {
                  line = strchr(buf, ':')+1;
                  while(isspace(*line)) line++;
                  tok = strtok(line, " ");
                  mem_free = atol(tok);
               }
             if (mem_total && mem_free)
               break;
          }
        fclose(f);

        mem_used = mem_total - mem_free;
        percent = 100 * ((float)mem_used / (float)mem_total);
     }
   if (percent > 100) percent = 100;
   else if (percent < 0) percent = 0;

   return percent;
}

