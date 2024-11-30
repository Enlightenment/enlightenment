#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "e.h"
#include "cpf.h"

typedef struct _Config       Config;

#define CPUFREQ_CONFIG_VERSION 2

struct _Config
{
   // saved * loaded config values
   int                  config_version;
   double               check_interval;
   int                  power_lo;
   int                  power_hi;
   // just state
   E_Module            *module;
   Eina_List           *instances;
   Ecore_Event_Handler *handler;
   E_Config_Dialog     *config_dialog;
};

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

E_Config_Dialog *e_int_config_cpufreq_module(Evas_Object *parent, const char *params);
void _cpufreq_poll_interval_update(void);
void _cpufreq_set_governor(const char *governor);
void _cpufreq_set_pstate(int min, int max);

extern Config *cpufreq_config;

/**
 * @addtogroup Optional_Gadgets
 * @{
 *
 * @defgroup Module_CPUFreq CPU Frequency Monitor
 *
 * Monitors CPU frequency and may do actions given some thresholds.
 *
 * @see Module_Temperature
 * @see Module_Battery
 * @}
 */
#endif
