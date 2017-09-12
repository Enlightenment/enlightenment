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

EINTERN void _netstatus_config_updated(Instance *inst);
EINTERN void _netstatus_proc_getstatus(Eina_Bool automax, time_t *last_checked,
    unsigned long *prev_in, unsigned long *prev_incurrent, unsigned long *prev_inmax,
    int *prev_inpercent, unsigned long *prev_out, unsigned long *prev_outcurrent,
    unsigned long *prev_outmax, int *prev_outpercent);
EINTERN void _netstatus_sysctl_getstatus(Eina_Bool automax, time_t *last_checked, 
    unsigned long *prev_in, unsigned long *prev_incurrent, unsigned long *prev_inmax,
    int *prev_inpercent, unsigned long *prev_out, unsigned long *prev_outcurrent,
    unsigned long *prev_outmax, int *prev_outpercent);
EINTERN Evas_Object *netstatus_configure(Instance *inst);
#endif
