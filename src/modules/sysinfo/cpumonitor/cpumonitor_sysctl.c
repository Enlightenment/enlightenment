#include "cpumonitor.h"

#if defined(__FreeBSD__) || defined (__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
# define CPU_STATES 5
#endif

#if defined(__OpenBSD__)
# include <sys/sysctl.h>
# define CPU_STATES 6
#endif

 int
_cpumonitor_sysctl_getcores(void)
{
   int cores = 0;
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
   size_t len;
   int mib[2] = { CTL_HW, HW_NCPU };

   len = sizeof(cores);
   if (sysctl(mib, 2, &cores, &len, NULL, 0) < 0) return 0; 
#endif
   return cores;
}


void
_cpumonitor_sysctl_getusage(Instance *inst)
{
   CPU_Core *core;
   size_t size;
   unsigned long percent_all = 0, total_all = 0, idle_all = 0;
   int i, j;
#if defined(__FreeBSD__) || defined(__DragonFly__)
   int ncpu = _cpumonitor_sysctl_getcores();
   if (!ncpu) return;
   size =  sizeof(long) * (CPU_STATES * ncpu);
   long cpu_times[ncpu][CPU_STATES];

   if (sysctlbyname("kern.cp_times", cpu_times, &size, NULL, 0) < 0)
     return;
 
   for (i = 0; i < ncpu; i++)
     {
        long *cpu = cpu_times[i];
        double total = 0;

        for (j = 0; j < CPU_STATES; j++) 
           total += cpu[j];

        double used = total - (cpu[4]); //  + cpu[2] + cpu[1] + cpu[0]);
        double ratio = (float) total / 100.0;

        double percent = ((double)used / (double)ratio);
        if (percent > 100) 
           percent = 100;
        else if (percent < 0) 
           percent = 0;

        core = eina_list_nth(inst->cfg->cpumonitor.cores, i);
        core->percent = (int) percent;
        core->total = total;
        core->idle = cpu[4];

        percent_all += (int) percent;
        total_all += total;
        idle_all += core->idle;
     }
   inst->cfg->cpumonitor.total = total_all / ncpu;
   inst->cfg->cpumonitor.idle = idle_all / ncpu;
   inst->cfg->cpumonitor.percent = (int) (percent_all / ncpu);
#elif defined(__OpenBSD__)
   int ncpu = _cpumonitor_sysctl_getcores();
   if (!ncpu) return;
   if (ncpu == 1)
     {
        long cpu_times[CPU_STATES];
        int cpu_time_mib[] = { CTL_KERN, KERN_CPTIME };
        size = CPU_STATES * sizeof(long);
        if (sysctl(cpu_time_mib, 2, &cpu_times, &size, NULL, 0) < 0)
          return;

        long total = 0;
        for (j = 0; j < CPU_STATES; j++) 
          total += cpu_times[j];
        
        //user", "nice", "system", "interrupt", "idle", NULL
        long used = total - cpu_times[0] + cpu_times[1] + cpu_times[2] + cpu_times[3];
        double ratio = (double) total / 100.0;
        double percent =  100.0 - ((double) used / (double) ratio);
        if (percent > 100)
          percent = 100;
        else if (percent < 0)
          percent = 0;
         
        inst->cfg->cpumonitor.total = total;
        inst->cfg->cpumonitor.idle = cpu_times[3];
        inst->cfg->cpumonitor.percent = (int) percent; 
     }
   else if (ncpu > 1)
     {
        for (i = 0; i < ncpu; i++)        
          {
             long cpu_times[CPU_STATES];
             size = CPU_STATES * sizeof(long);
             int cpu_time_mib[] = { CTL_KERN, KERN_CPTIME2, 0 };
             cpu_time_mib[2] = i;
             if (sysctl(cpu_time_mib, 3, &cpu_times, &size, NULL, 0) < 0)
               return;

             long total = 0;
             for (j = 0; j < CPU_STATES - 1; j++)
               total += cpu_times[j];
	     
             long used = total - (cpu_times[4]); 
             double ratio = (float) total / 100.0;
             int percent = used / ratio;
             if (percent > 100) 
               percent = 100;
             else if (percent < 0) 
               percent = 0;
             core = eina_list_nth(inst->cfg->cpumonitor.cores, i);
             core->percent = (int) percent;
             core->total = total;
             core->idle = cpu_times[4];

             percent_all += (int) percent;
             total_all += total;
             idle_all += core->idle;
          }
   
        inst->cfg->cpumonitor.total = total_all / ncpu;
        inst->cfg->cpumonitor.idle = idle_all / ncpu;
        inst->cfg->cpumonitor.percent = (percent_all / ncpu) + (percent_all % ncpu);
     }
#endif
}

