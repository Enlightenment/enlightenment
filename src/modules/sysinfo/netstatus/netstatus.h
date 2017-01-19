#ifndef NETSTATUS_H
#define NETSTATUS_H

#include "../sysinfo.h"

typedef struct _Netstatus_Config Netstatus_Config;
struct _Netstatus_Config
{
   Instance *inst;
   Evas_Object *transfer_check;
   Evas_Object *receive_max;
   Evas_Object *receive_units;
   Evas_Object *send_max;
   Evas_Object *send_units;
   int receive_unit_adjust;
   int send_unit_adjust;
};

void _netstatus_config_updated(Instance *inst);
void _netstatus_proc_getrstatus(Instance *inst);
void _netstatus_proc_gettstatus(Instance *inst);
Evas_Object *netstatus_configure(Instance *inst);
#endif
