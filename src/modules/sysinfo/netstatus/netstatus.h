#ifndef NETSTATUS_H
#define NETSTATUS_H

#include "../sysinfo.h"

void _netstatus_config_updated(Instance *inst);
void _netstatus_proc_getrstatus(Instance *inst);
void _netstatus_proc_gettstatus(Instance *inst);
Evas_Object *netstatus_configure(Instance *inst);
#endif
