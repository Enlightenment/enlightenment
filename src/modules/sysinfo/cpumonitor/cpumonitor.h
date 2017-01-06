#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include "../sysinfo.h"

void _cpuclock_config_updated(Instance *inst);
int _cpumonitor_proc_getusage(Instance *inst);

#endif
