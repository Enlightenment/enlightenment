#include "cpumonitor.h"

#if defined(__FreeBSD__) || defined (__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
# define CPU_STATES 5
#endif

#if defined(__OpenBSD__)
# include <sys/sched.h>
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
_cpumonitor_sysctl_getusage(unsigned long *prev_total, unsigned long *prev_idle, int *prev_percent, Eina_List *cores)
{
   CPU_Core *core;
   size_t size;
   unsigned long percent_all = 0, total_all = 0, idle_all = 0;
   int i, j;
#if defined(__FreeBSD__) || defined(__DragonFly__)
   int ncpu = _cpumonitor_sysctl_getcores();
   if (!ncpu) return;
   size = sizeof(unsigned long) * (CPU_STATES * ncpu);
   unsigned long cpu_times[ncpu][CPU_STATES];

   if (sysctlbyname("kern.cp_times", cpu_times, &size, NULL, 0) < 0)
     return;

   for (i = 0; i < ncpu; i++)
     {
        unsigned long *cpu = cpu_times[i];
        double total = 0;

        for (j = 0; j < CPU_STATES; j++)
          total += cpu[j];

        core = eina_list_nth(cores, i);

        int diff_total = total - core->total;
        int diff_idle = cpu[4] - core->idle;

        if (diff_total == 0) diff_total = 1;

        double ratio = diff_total / 100.0;
        unsigned long used = diff_total - diff_idle;

        int percent = used / ratio;
        if (percent > 100)
          percent = 100;
        else if (percent < 0)
          percent = 0;

        core->percent = percent;
        core->total = total;
        core->idle = cpu[4];

        percent_all += (int)percent;
        total_all += total;
        idle_all += core->idle;
     }
   *prev_total = total_all / ncpu;
   *prev_idle = idle_all / ncpu;
   *prev_percent = (int)(percent_all / ncpu);
#elif defined(__OpenBSD__)
   int ncpu = _cpumonitor_sysctl_getcores();
   struct cpustats cpu_times[CPU_STATES];
   memset(&cpu_times, 0, CPU_STATES * sizeof(struct cpustats));
   if (!ncpu) return;
   for (i = 0; i < ncpu; i++)
     {
        unsigned long total =  0, idle;
        int diff_total, diff_idle;
        int cpu_time_mib[] = { CTL_KERN, KERN_CPUSTATS, 0 };

        size = sizeof(struct cpustats);
        cpu_time_mib[2] = i;
        if (sysctl(cpu_time_mib, 3, &cpu_times[i], &size, NULL, 0) < 0)
          return;

        for (j = 0; j < CPU_STATES; j++)
          total += cpu_times[i].cs_time[j];

        idle = cpu_times[i].cs_time[CP_IDLE];
        core = eina_list_nth(cores, i);
        diff_total = total - core->total;
        diff_idle = idle - core->idle;

        core->total = total;
        core->idle = idle;

        if (diff_total == 0) diff_total = 1;

        double ratio = diff_total / 100;
        unsigned long used = diff_total - diff_idle;
        int percent = used / ratio;

        core->percent = percent;

        percent_all += (int)percent;
        total_all += total;
        idle_all += core->idle;

        *prev_total = (total_all / ncpu);
        *prev_idle = (idle_all / ncpu);
        *prev_percent = (percent_all / ncpu) + (percent_all % ncpu);
     }
#endif
}

