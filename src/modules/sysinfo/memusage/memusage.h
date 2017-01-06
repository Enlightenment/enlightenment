#ifndef MEMUSAGE_H
#define MEMUSAGE_H

#include "../sysinfo.h"

void _memusage_config_updated(Instance *inst);
int _memusage_proc_getmemusage();
int _memusage_proc_getswapusage();

#endif
