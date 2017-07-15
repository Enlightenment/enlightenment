#ifndef THERMAL_H
#define THERMAL_H

#include "../sysinfo.h"

typedef struct _Thermal_Config Thermal_Config;

struct _Thermal_Config
{
   Instance *inst;
   Evas_Object *high;
   Evas_Object *low;
};


#if defined(HAVE_EEZE)
EINTERN int thermal_udev_get(Tempthread *tth);
#endif

#if defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
EINTERN int thermal_sysctl_get(Tempthread *tth);
#endif

EINTERN Evas_Object *thermal_configure(Instance *inst);
EINTERN int thermal_fallback_get(Tempthread *tth);
EINTERN void _thermal_config_updated(Instance *inst);

#endif
