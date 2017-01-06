#ifndef NETSTATUS_H
#define NETSTATUS_H

#include "../sysinfo.h"

void _netstatus_config_updated(Instance *inst);
const char *_netstatus_proc_getrstatus(Instance *inst);
const char *_netstatus_proc_gettstatus(Instance *inst);

#endif
