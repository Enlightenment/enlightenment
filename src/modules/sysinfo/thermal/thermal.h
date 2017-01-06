#ifndef THERMAL_H
#define THERMAL_H

#include "../sysinfo.h"

#ifdef HAVE_EEZE
int thermal_udev_get(Tempthread *tth);
#endif

#if defined __OpenBSD__ || defined __DragonFly__ || defined __FreeBSD__
int thermal_sysctl_get(Tempthread *tth);
#endif

int thermal_fallback_get(Tempthread *tth);
void _thermal_config_updated(Instance *inst);

#endif
