#include "cpuclock.h"

int
_cpuclock_sysfs_setall(const char *control, const char *value)
{
   int num = 0;
   char filename[4096];
   FILE *f;

   while (1)
     {
        snprintf(filename, sizeof(filename), "/sys/devices/system/cpu/cpu%i/cpufreq/%s", num, control);
        f = fopen(filename, "w");

        if (!f)
          {
             return num;
          }
        fprintf(f, "%s\n", value);
        fclose(f);
        num++;
     }
   return 1;
}

int
_cpuclock_sysfs_set(const char *control, const char *value)
{
   char filename[4096];
   FILE *f;

   snprintf(filename, sizeof(filename), "/sys/devices/system/cpu/cpufreq/%s", control);
   f = fopen(filename, "w");

   if (!f)
     {
        if (_cpuclock_sysfs_setall(control, value) > 0)
          return 1;
        else
          return 0;
     }

   fprintf(f, "%s\n", value);
   fclose(f);

   return 1;
}

int
_cpuclock_sysfs_pstate(int min, int max, int turbo)
{
   FILE *f;

   f = fopen("/sys/devices/system/cpu/intel_pstate/min_perf_pct", "w");
   if (!f) return 0;
   fprintf(f, "%i\n", min);
   fclose(f);

   f = fopen("/sys/devices/system/cpu/intel_pstate/max_perf_pct", "w");
   if (!f) return 0;
   fprintf(f, "%i\n", max);
   fclose(f);

   f = fopen("/sys/devices/system/cpu/intel_pstate/no_turbo", "w");
   if (!f) return 0;
   fprintf(f, "%i\n", turbo ? 0 : 1);
   fclose(f);

   return 1;
}


