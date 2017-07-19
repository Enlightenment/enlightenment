#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include "../sysinfo.h"

EINTERN void _cpumonitor_config_updated(Instance *inst);
EINTERN int _cpumonitor_proc_getcores(void);
EINTERN void _cpumonitor_proc_getusage(unsigned long *prev_total, unsigned long *prev_idle, int *prev_precent, Eina_List *cores);
EINTERN int _cpumonitor_sysctl_getcores(void);
EINTERN void _cpumonitor_sysctl_getusage(unsigned long *prev_total, unsigned long *prev_idle, int *prev_precent, Eina_List *cores);
EINTERN Evas_Object *cpumonitor_configure(Instance *inst);
#endif
