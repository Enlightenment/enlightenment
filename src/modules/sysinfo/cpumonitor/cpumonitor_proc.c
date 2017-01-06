#include "cpumonitor.h"

int _cpumonitor_proc_getusage(Instance *inst)
{
   long total = 0, i = 0, idle = 0, use, total_change, idle_change;
   int percent = 0;
   char buf[4096], *line, *tok;
   FILE *f;

   f = fopen("/proc/stat", "r");
   if (f)
     {
        if (fgets(buf, sizeof(buf), f) == NULL)
          {
             fclose(f);
             return 0;
          }
        fclose(f);

        line = strchr(buf, ' ')+1;
        tok = strtok(line, " ");
        while (tok)
          {
             use = atol(tok);
             total += use;
             i++;
             if (i == 4)
               idle = use;
             tok = strtok(NULL, " ");
          }
     }
   total_change = total - inst->cfg->cpumonitor.total;
   idle_change = idle - inst->cfg->cpumonitor.idle;
   if (total_change != 0)
     percent = 100 * (1 - ((float)idle_change / (float)total_change));
   if (percent > 100) percent = 100;
   else if (percent < 0) percent = 0;
   inst->cfg->cpumonitor.total = total;
   inst->cfg->cpumonitor.idle = idle;

   return percent;
}
