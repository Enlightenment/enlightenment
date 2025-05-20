#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#ifdef HAVE_EEZE
# include <Eeze.h>
#else
# include <Eldbus.h>
#endif

#define CHECK_NONE      0
#define CHECK_ACPI      1
#define CHECK_APM       2
#define CHECK_PMU       3
#define CHECK_SYS_ACPI  4

#define UNKNOWN         0
#define NOSUBSYSTEM     1
#define SUBSYSTEM       2

#define SUSPEND         0
#define HIBERNATE       1
#define SHUTDOWN        2

#define POPUP_DEBOUNCE_CYCLES  2

typedef struct _Config_Battery Config_Battery;
struct _Config_Battery
{
  const char *udi;
  int         charge_limit;
};

typedef struct _Config Config;
struct _Config
{
   /* saved / loaded config values */
   int                  alert;          /* Alert on minutes remaining */
   int                  alert_p;        /* Alert on percentage remaining */
   int                  alert_timeout;  /* Popup dismissal timeout */
   int                  suspend_below;  /* Suspend if battery drops below this level */
   int                  suspend_method; /* Method used to suspend the machine */
   Eina_List           *battery_configs;/* Per battery config */
   /* just config state */
   E_Module            *module;
   E_Config_Dialog     *config_dialog;
   Eina_List           *instances;
   Ecore_Timer         *alert_timer;
   int                  full;
   int                  time_left;
   int                  time_full;
   int                  have_battery;
   int                  have_power;
   Eina_Bool            charging;
   int                  desktop_notifications;
#ifdef HAVE_EEZE
   Eeze_Udev_Watch     *acwatch;
   Eeze_Udev_Watch     *batwatch;
#endif
#if defined HAVE_EEZE || defined __OpenBSD__ || defined __NetBSD__
   Eina_Bool            fuzzy;
   int                  fuzzcount;
#endif
};

typedef struct _Battery Battery;
typedef struct _Ac_Adapter Ac_Adapter;
typedef struct _Battery_Hist Battery_Hist;
typedef struct _Battery_Hist_Entry Battery_Hist_Entry;

#define BATTERY_HIST_MAX 1000
struct _Battery_Hist_Entry
{
  unsigned long long timepoint; // time_t
  unsigned short     full; // 0-10000 = 0->100%
  unsigned short     power_now ; // 1unit = 0.1w
  unsigned short     ac : 1;
  unsigned short     charging : 1;
  unsigned short     pad : 14; // 14 bits to pad out rest of short
  unsigned short     pad2 ; // pad out rest of struct
}; // 16 bytes

struct _Battery_Hist
{
#define BATTERY_HISTORY_MAGIC 0xfeedee77
  unsigned int       magic; // BATTERY_HISTORY_MAGIC
#define BATTERY_HISTORY_VERSION 100
  unsigned short     version; // BATTERY_HISTORY_VERSION
  unsigned short     history_pos; // if >= BATTERY_HIST_MAX reset to 0
  int                pad;
  int                pad2;
  // no padding needed (16 byte header) as we're aligned
  Battery_Hist_Entry history[BATTERY_HIST_MAX];
};

struct _Battery
{
   const char   *udi;
#if defined HAVE_EEZE || defined __OpenBSD__ || defined __DragonFly__ || defined __FreeBSD__ || defined __NetBSD__
   Ecore_Timer  *timer;
   Eina_Bool     present E_BITFIELD;
   Eina_Bool     charging E_BITFIELD;
   Eina_Bool     is_micro_watts E_BITFIELD;
   double        last_update;
   double        percent;
   double        power_now;
   double        current_charge;
   double        design_charge;
   double        last_full_charge;
   double        design_voltage;
   double        charge_rate;
   double        time_full;
   double        time_left;
#else
   int           percent;
   int           current_charge;
   int           design_charge;
   int           last_full_charge;
   int           charge_rate;
   int           time_full;
   int           time_left;
   const char   *type;
   const char   *charge_units;
#endif
   Battery_Hist *hist;
   const char   *technology;
   const char   *model;
   const char   *vendor;
   int           charge_lim;
   int           history_power_now_max;
   Eina_Bool     got_prop E_BITFIELD;
   Eldbus_Proxy *proxy;
#if defined __FreeBSD__ || defined __DragonFly__
   int           unit;
   int           batteries;
   int           time_min;
#endif
#if defined __OpenBSD__
   int          *mib;
#endif
};

struct _Ac_Adapter
{
   const char   *udi;
   Eina_Bool     present E_BITFIELD;
   const char   *product;
   Eldbus_Proxy *proxy;
#if defined __OpenBSD__
   int          *mib;
#endif
};

Battery    *_battery_battery_find(const char *udi);
Ac_Adapter *_battery_ac_adapter_find(const char *udi);
void        _battery_device_update(void);

void        _battery_history_load(Battery *bat);
void        _battery_history_close(Battery *bat);

#ifdef HAVE_EEZE
/* in e_mod_udev.c */
int         _battery_udev_start(void);
void        _battery_udev_stop(void);
/* end e_mod_udev.c */
#elif !defined __OpenBSD__ && !defined __DragonFly__ && !defined __FreeBSD__ && !defined __NetBSD__
/* in e_mod_dbus.c */
int         _battery_upower_start(void);
void        _battery_upower_stop(void);
/* end e_mod_dbus.c */
#else
/* in e_mod_sysctl.c */
int         _battery_sysctl_start(void);
void        _battery_sysctl_stop(void);
/* end e_mod_sysctl.c */
#endif

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

E_Config_Dialog *e_int_config_battery_module(Evas_Object *parent, const char *params EINA_UNUSED);
extern Config   *battery_config;
void             _battery_config_updated(void);

#endif
