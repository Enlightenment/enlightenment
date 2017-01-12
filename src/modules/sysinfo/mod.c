#include "sysinfo.h"

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;
Eina_List *sysinfo_instances = NULL;
E_Module *module = NULL;
Config *sysinfo_config = NULL;

EINTERN void
sysinfo_init(void)
{
   conf_item_edd = E_CONFIG_DD_NEW("Sysinfo_Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, esm, INT);
   E_CONFIG_VAL(D, T, batman.poll_interval, INT);
   E_CONFIG_VAL(D, T, batman.alert, INT);
   E_CONFIG_VAL(D, T, batman.alert_p, INT);
   E_CONFIG_VAL(D, T, batman.alert_timeout, INT);
   E_CONFIG_VAL(D, T, batman.suspend_below, INT);
   E_CONFIG_VAL(D, T, batman.force_mode, INT);
   #if defined HAVE_EEZE || defined __OpenBSD__ || defined __NetBSD__
   E_CONFIG_VAL(D, T, batman.fuzzy, INT);
#endif
   E_CONFIG_VAL(D, T, batman.desktop_notifications, INT);
   E_CONFIG_VAL(D, T, thermal.poll_interval, INT);
   E_CONFIG_VAL(D, T, thermal.low, INT);
   E_CONFIG_VAL(D, T, thermal.high, INT);
   E_CONFIG_VAL(D, T, thermal.sensor_type, INT);
   E_CONFIG_VAL(D, T, thermal.sensor_name, STR);
   E_CONFIG_VAL(D, T, thermal.units, INT);
   E_CONFIG_VAL(D, T, cpuclock.poll_interval, INT);
   E_CONFIG_VAL(D, T, cpuclock.restore_governor, INT);
   E_CONFIG_VAL(D, T, cpuclock.auto_powersave, INT);
   E_CONFIG_VAL(D, T, cpuclock.powersave_governor, STR);
   E_CONFIG_VAL(D, T, cpuclock.governor, STR);
   E_CONFIG_VAL(D, T, cpuclock.pstate_min, INT);
   E_CONFIG_VAL(D, T, cpuclock.pstate_max, INT);
   E_CONFIG_VAL(D, T, cpumonitor.poll_interval, INT);
   E_CONFIG_VAL(D, T, memusage.poll_interval, INT);
   E_CONFIG_VAL(D, T, netstatus.poll_interval, INT);

   conf_edd = E_CONFIG_DD_NEW("Sysinfo_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   sysinfo_config = e_config_domain_load("module.sysinfo", conf_edd);

   if (!sysinfo_config)
     {
        Config_Item *ci;

        sysinfo_config = E_NEW(Config, 1);
        ci = E_NEW(Config_Item, 1);
        ci->id = 0;
        ci->esm = E_SYSINFO_MODULE_NONE;

        ci->batman.poll_interval = 512;
        ci->batman.alert = 30;
        ci->batman.alert_p = 10;
        ci->batman.alert_timeout = 0;
        ci->batman.suspend_below = 0;
        ci->batman.force_mode = 0;
        ci->batman.full = -2;
        ci->batman.time_left = -2;
        ci->batman.time_full = -2;
        ci->batman.have_battery = -2;
        ci->batman.have_power = -2;
#if defined HAVE_EEZE || defined __OpenBSD__ || defined __NetBSD__
        ci->batman.fuzzy = 0;
#endif
        ci->batman.desktop_notifications = 0;
        ci->thermal.poll_interval = 128;
        ci->thermal.low = 30;
        ci->thermal.high = 80;
        ci->thermal.sensor_type = SENSOR_TYPE_NONE;
        ci->thermal.sensor_name = NULL;
        ci->thermal.temp = -900;
        ci->thermal.units = CELSIUS;
        ci->cpuclock.poll_interval = 32;
        ci->cpuclock.restore_governor = 0;
        ci->cpuclock.auto_powersave = 1;
        ci->cpuclock.powersave_governor = NULL;
        ci->cpuclock.governor = NULL;
        ci->cpuclock.pstate_min = 1;
        ci->cpuclock.pstate_max = 101;
        ci->cpumonitor.poll_interval = 32;
        ci->cpumonitor.total = 0;
        ci->cpumonitor.idle = 0;
        ci->cpumonitor.percent = 0;
        ci->cpumonitor.popup = NULL;
        ci->cpumonitor.configure = NULL;
        ci->memusage.poll_interval = 32;
        ci->memusage.real = 0;
        ci->memusage.swap = 0;
        ci->memusage.popup = NULL;
        ci->memusage.configure = NULL;
        ci->netstatus.poll_interval = 32;
        ci->netstatus.in = 0;
        ci->netstatus.out = 0;
        ci->netstatus.instring = NULL;
        ci->netstatus.outstring = NULL;
        ci->netstatus.popup = NULL;
        ci->netstatus.configure = NULL;

        E_CONFIG_LIMIT(ci->batman.poll_interval, 4, 4096);
        E_CONFIG_LIMIT(ci->batman.alert, 0, 60);
        E_CONFIG_LIMIT(ci->batman.alert_p, 0, 100);
        E_CONFIG_LIMIT(ci->batman.alert_timeout, 0, 300);
        E_CONFIG_LIMIT(ci->batman.suspend_below, 0, 50);
        E_CONFIG_LIMIT(ci->batman.force_mode, 0, 2);
        E_CONFIG_LIMIT(ci->batman.desktop_notifications, 0, 1);
        E_CONFIG_LIMIT(ci->thermal.poll_interval, 1, 1024);
        E_CONFIG_LIMIT(ci->thermal.low, 0, 100);
        E_CONFIG_LIMIT(ci->thermal.high, 0, 220);
        E_CONFIG_LIMIT(ci->thermal.units, CELSIUS, FAHRENHEIT);
        E_CONFIG_LIMIT(ci->cpuclock.poll_interval, 1, 1024);
        E_CONFIG_LIMIT(ci->cpumonitor.poll_interval, 1, 1024);
        E_CONFIG_LIMIT(ci->memusage.poll_interval, 1, 1024);
        E_CONFIG_LIMIT(ci->netstatus.poll_interval, 1, 1024);

        sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);
     }
   e_gadget_type_add("Batman", batman_create, NULL);
   e_gadget_type_add("Thermal", thermal_create, NULL);
   e_gadget_type_add("CpuClock", cpuclock_create, NULL);
   e_gadget_type_add("CpuMonitor", cpumonitor_create, NULL);
   e_gadget_type_add("MemUsage", memusage_create, NULL);
   e_gadget_type_add("NetStatus", netstatus_create, NULL);
   e_gadget_type_add("SysInfo", sysinfo_create, NULL);
}

EINTERN void
sysinfo_shutdown(void)
{
   if (sysinfo_config)
     {
        Config_Item *ci;
        EINA_LIST_FREE(sysinfo_config->items, ci)
          {
             free(ci);
          }
        E_FREE(sysinfo_config);
     }
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);

   e_gadget_type_del("Batman");
   e_gadget_type_del("Thermal");
   e_gadget_type_del("CpuClock");
   e_gadget_type_del("CpuMonitor");
   e_gadget_type_del("MemUsage");
   e_gadget_type_del("NetStatus");
   e_gadget_type_del("SysInfo");
}

E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Sysinfo"
};

E_API void *
e_modapi_init(E_Module *m)
{
   sysinfo_init();

   module = m;
   sysinfo_config->module = m;
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   sysinfo_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.sysinfo", conf_edd, sysinfo_config);
   return 1;
}

