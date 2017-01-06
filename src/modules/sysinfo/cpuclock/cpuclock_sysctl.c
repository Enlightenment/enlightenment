#include "cpuclock.h"
#ifdef __FreeBSD__
# include <sys/sysctl.h>
#endif

#ifdef __OpenBSD__
# include <sys/param.h>
# include <sys/resource.h>
# include <sys/sysctl.h>
#endif


#if defined __OpenBSD__
int
_cpuclock_sysctl_frequency(int new_perf)
{
   int mib[] = {CTL_HW, HW_SETPERF};
   size_t len = sizeof(new_perf);

   if (sysctl(mib, 2, NULL, 0, &new_perf, len) == -1)
     return 1;
   else
     return 0;
}
#elif defined __FreeBSD__
int
_cpuclock_sysctl_frequency(int new_perf)
{
   if (sysctlbyname("dev.cpu.0.freq", NULL, NULL, &new_perf, sizeof(new_perf)) == -1)
     return 1;
   else
     return 0;
}
#endif
