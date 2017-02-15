#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include "../sysinfo.h"

void _cpumonitor_config_updated(Instance *inst);
int _cpumonitor_proc_getcores(void);
void _cpumonitor_proc_getusage(Instance *inst);
int _cpumonitor_sysctl_getcores(void);
void _cpumonitor_sysctl_getusage(Instance *inst);
Evas_Object *cpumonitor_configure(Instance *inst);
#endif
