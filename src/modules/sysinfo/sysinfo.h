#ifndef SYSINFO_H
#define SYSINFO_H

#include "e.h"
#ifdef HAVE_EEZE
# include <Eeze.h>
#else
# include <Eldbus.h>
#endif

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

typedef enum _E_Sysinfo_Module E_Sysinfo_Module;
enum _E_Sysinfo_Module
{
   E_SYSINFO_MODULE_NONE = 0,
   E_SYSINFO_MODULE_BATMAN ,
   E_SYSINFO_MODULE_THERMAL,
   E_SYSINFO_MODULE_CPUCLOCK,
   E_SYSINFO_MODULE_CPUMONITOR,
   E_SYSINFO_MODULE_MEMUSAGE,
   E_SYSINFO_MODULE_NETSTATUS,
   E_SYSINFO_MODULE_SYSINFO
};

typedef enum _Netstatus_Unit Netstatus_Unit;
enum _Netstatus_Unit
{
   NETSTATUS_UNIT_BYTES = 0,
   NETSTATUS_UNIT_KB,
   NETSTATUS_UNIT_MB,
   NETSTATUS_UNIT_GB
};

typedef enum _Sensor_Type
{
   SENSOR_TYPE_NONE,
#if defined __FreeBSD__ || defined __OpenBSD__ || defined __DragonflyBSD__
   SENSOR_TYPE_FREEBSD,
   SENSOR_TYPE_OPENBSD,
#else
   SENSOR_TYPE_OMNIBOOK,
   SENSOR_TYPE_LINUX_MACMINI,
   SENSOR_TYPE_LINUX_I2C,
   SENSOR_TYPE_LINUX_ACPI,
   SENSOR_TYPE_LINUX_PCI,
   SENSOR_TYPE_LINUX_PBOOK,
   SENSOR_TYPE_LINUX_INTELCORETEMP,
   SENSOR_TYPE_LINUX_THINKPAD,
   SENSOR_TYPE_LINUX_SYS
#endif
} Sensor_Type;

typedef enum _Unit
{
   CELSIUS,
   FAHRENHEIT
} Unit;

typedef struct _Tempthread Tempthread;
typedef struct _Cpu_Status       Cpu_Status;
typedef struct _CPU_Core         CPU_Core;
typedef struct _Config Config;
typedef struct _Config_Item Config_Item;
typedef struct _Instance Instance;

struct _Tempthread
{
   Instance *inst;
   int poll_interval;
   Sensor_Type sensor_type;
   const char *sensor_name;
   const char *sensor_path;
   void *extn;
#ifdef HAVE_EEZE
   Eina_List *tempdevs;
#endif
   Eina_Bool initted : 1;
};

struct _Cpu_Status
{
   Eina_List     *frequencies;
   Eina_List     *governors;
   int            cur_frequency;
#ifdef __OpenBSD__
   int            cur_percent;
#endif
   int            cur_min_frequency;
   int            cur_max_frequency;
   int            can_set_frequency;
   int            pstate_min;
   int            pstate_max;
   char          *cur_governor;
   const char    *orig_governor;
   unsigned char  active;
   unsigned char  pstate;
   unsigned char  pstate_turbo;
};

struct _CPU_Core
{
   int percent;
   long total;
   long idle;
   Evas_Object *layout;
};

struct _Config
{
   Eina_List *items;

   E_Module    *module;
   Evas_Object *config_dialog;
};

struct _Config_Item
{
   int                     id;
   int                     version;
   E_Sysinfo_Module        esm;
   struct
   {
      Evas_Object         *o_gadget;
      Evas_Object         *popup;
      Evas_Object         *configure;
      /* saved * loaded config values */
      int                  poll_interval;
      int                  alert;      /* Alert on minutes remaining */
      int                  alert_p;    /* Alert on percentage remaining */
      int                  alert_timeout;  /* Popup dismissal timeout */
      int                  suspend_below;  /* Suspend if battery drops below this level */
      int                  suspend_method; /* Method used to suspend the machine */
      int                  force_mode; /* force use of batget or hal */
      /* just config state */
      Ecore_Timer         *alert_timer;
      int                  full;
      int                  time_left;
      int                  have_battery;
      int                  have_power;
      int                  desktop_notifications;
#ifdef HAVE_EEZE
      Eeze_Udev_Watch     *acwatch;
      Eeze_Udev_Watch     *batwatch;
#endif
#if defined HAVE_EEZE || defined __OpenBSD__ || defined __NetBSD__
      Eina_Bool            fuzzy;
      int                  fuzzcount;
#endif
   } batman;
   struct
   {
      Evas_Object         *o_gadget;
      int                  poll_interval;
      int                  low, high;
      int                  sensor_type;
      int                  temp;
      const char          *sensor_name;
      Unit                 units;
#ifdef HAVE_EEZE
      Ecore_Poller        *poller;
      Tempthread          *tth;
#endif
      Ecore_Thread        *th;

      Eina_Bool            have_temp:1;
   } thermal;
   struct
   {
      Evas_Object         *o_gadget;
      int                  poll_interval;
      int                  restore_governor;
      int                  auto_powersave;
      const char          *powersave_governor;
      const char          *governor;
      int                  pstate_min;
      int                  pstate_max;
      Cpu_Status          *status;
      Ecore_Thread        *frequency_check_thread;
      Ecore_Event_Handler *handler;
   } cpuclock;
   struct
   {
      Evas_Object         *o_gadget;
      Evas_Object         *popup;
      Evas_Object         *popup_pbar;
      Evas_Object         *configure;
      int                  poll_interval;
      int                  percent;
      long                 total;
      long                 idle;

      Ecore_Thread        *usage_check_thread;
      Eina_List           *cores;
   } cpumonitor;
   struct
   {
      Evas_Object         *o_gadget;
      Evas_Object         *popup;
      Evas_Object         *configure;
      int                  poll_interval;
      int                  mem_percent;
      int                  swp_percent;
      unsigned long        mem_total;
      unsigned long        mem_used;
      unsigned long        mem_cached;
      unsigned long        mem_buffers;
      unsigned long        mem_shared;
      unsigned long        swp_total;
      unsigned long        swp_used;
      Ecore_Thread        *usage_check_thread;
   } memusage;
   struct
   {
      Evas_Object         *o_gadget;
      Evas_Object         *popup;
      Evas_Object         *popup_label;
      Evas_Object         *configure;
      Eina_Bool            automax;
      Netstatus_Unit       receive_units;
      Netstatus_Unit       send_units;
      int                  poll_interval;
      long                 in;
      long                 out;
      long                 incurrent;
      long                 outcurrent;
      long                 inmax;
      long                 outmax;
      int                  inpercent;
      int                  outpercent;
      Eina_Stringshare    *instring;
      Eina_Stringshare    *outstring;
      Ecore_Thread        *usage_check_thread;
   } netstatus;
   struct   {
      Evas_Object *o_batman;
      Evas_Object *o_thermal;
      Evas_Object *o_cpuclock;
      Evas_Object *o_cpumonitor;
      Evas_Object *o_memusage;
      Evas_Object *o_netstatus;
   } sysinfo;
};

struct _Instance
{
   Evas_Object         *o_main;
   Evas_Object         *o_table;
   Evas_Object         *popup_battery;
   Evas_Object         *warning;
   Config_Item         *cfg;
   unsigned int         notification_id;
};

EINTERN Evas_Object *config_sysinfo(E_Zone *zone, Instance *inst, E_Sysinfo_Module esm);
EINTERN Evas_Object *batman_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *thermal_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *cpuclock_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *cpumonitor_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *memusage_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *netstatus_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *sysinfo_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);

EINTERN Evas_Object *config_sysinfo(E_Zone *zone, Instance *inst, E_Sysinfo_Module esm);
EINTERN Evas_Object *sysinfo_batman_create(Evas_Object *parent, Instance *inst);
EINTERN Evas_Object *sysinfo_thermal_create(Evas_Object *parent, Instance *inst);
EINTERN Evas_Object *sysinfo_cpuclock_create(Evas_Object *parent, Instance *inst);
EINTERN Evas_Object *sysinfo_cpumonitor_create(Evas_Object *parent, Instance *inst);
EINTERN Evas_Object *sysinfo_memusage_create(Evas_Object *parent, Instance *inst);
EINTERN Evas_Object *sysinfo_netstatus_create(Evas_Object *parent, Instance *inst);

EINTERN void sysinfo_batman_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED);
EINTERN void sysinfo_thermal_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED);
EINTERN void sysinfo_cpuclock_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED);
EINTERN void sysinfo_cpumonitor_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED);
EINTERN void sysinfo_memusage_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED);
EINTERN void sysinfo_netstatus_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED);

extern Config *sysinfo_config;
extern Eina_List *sysinfo_instances;
extern E_Module *module;

#endif
