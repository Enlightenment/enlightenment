#ifndef BATMAN_H
#define BATMAN_H

#include "../sysinfo.h"

#define CHECK_NONE      0
#define CHECK_ACPI      1
#define CHECK_APM       2
#define CHECK_PMU       3
#define CHECK_SYS_ACPI  4

#define UNKNOWN 0
#define NOSUBSYSTEM 1
#define SUBSYSTEM 2

#define SUSPEND 0
#define HIBERNATE 1
#define SHUTDOWN 2

#define POPUP_DEBOUNCE_CYCLES  2

typedef struct _Battery Battery;
typedef struct _Ac_Adapter Ac_Adapter;
typedef struct _Battery_Config Battery_Config;

struct _Battery
{
   Instance *inst;
   const char *udi;
#if defined HAVE_EEZE || defined __OpenBSD__ || defined __DragonFly__ || defined __FreeBSD__ || defined __NetBSD__
   Ecore_Poller *poll;
#endif
   Eina_Bool present:1;
   Eina_Bool charging:1;
#if defined HAVE_EEZE || defined __OpenBSD__ || defined __DragonFly__ || defined __FreeBSD__ || defined __NetBSD__
   double last_update;
   double percent;
   double current_charge;
   double design_charge;
   double last_full_charge;
   double charge_rate;
   double time_full;
   double time_left;
#else
   int percent;
   int current_charge;
   int design_charge;
   int last_full_charge;
   int charge_rate;
   int time_full;
   int time_left;
   const char *type;
   const char *charge_units;
#endif
   const char *technology;
   const char *model;
   const char *vendor;
   Eina_Bool got_prop:1;
   Eldbus_Proxy *proxy;
   int * mib;
#if defined(__FreeBSD__) || defined(__DragonFly__)
   int * mib_state;
   int * mib_units;
   int * mib_time;
   int batteries;
   int time_min;
#endif
};

struct _Ac_Adapter
{
   Instance *inst;
   const char *udi;
   Eina_Bool present:1;
   const char *product;
   Eldbus_Proxy *proxy;
   int * mib;
};

Eina_List *_batman_battery_find(const char *udi);
Eina_List *_batman_ac_adapter_find(const char *udi);
void _batman_update(Instance *inst, int full, int time_left, int time_full, Eina_Bool have_battery, Eina_Bool have_power);
void _batman_device_update(Instance *inst);
/* in batman_fallback.c */
int _batman_fallback_start(Instance *inst);
void _batman_fallback_stop(void);
/* end batman_fallback.c */
#ifdef HAVE_EEZE
/* in batman_udev.c */
int  _batman_udev_start(Instance *inst);
void _batman_udev_stop(Instance *inst);
/* end batman_udev.c */
#elif !defined __OpenBSD__ && !defined __DragonFly__ && !defined __FreeBSD__ && !defined __NetBSD__
/* in batman_upower.c */
int _batman_upower_start(Instance *inst);
void _batman_upower_stop(void);
/* end batman_upower.c */
#else
/* in batman_sysctl.c */
int _batman_sysctl_start(Instance *inst);
void _batman_sysctl_stop(void);
/* end batman_sysctl.c */
#endif

void _batman_config_updated(Instance *inst);

#endif
