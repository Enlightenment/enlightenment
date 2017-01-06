#ifndef CPUCLOCK_H
#define CPUCLOCK_H

#include "../sysinfo.h"

void _cpuclock_config_updated(Instance *inst);
int _cpuclock_sysfs_setall(const char *control, const char *value);
int _cpuclock_sysfs_set(const char *control, const char *value);
int _cpuclock_sysfs_pstate(int min, int max, int turbo);
#if defined __OpenBSD__ || defined __FreeBSD__
int _cpuclock_sysctl_frequency(int new_perf)
#endif

#endif
