#include "thermal.h"

#if defined(__FreeBSD__) || defined(__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <errno.h>
#endif

#if defined(__OpenBSD__)
# include <sys/param.h>
# include <sys/sysctl.h>
# include <sys/sensors.h>
# include <errno.h>
# include <err.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
typedef struct
{
   int mib[CTL_MAXNAME];
#if defined(__FreeBSD__) || defined(__DragonFly__)
   unsigned int miblen;
#endif
   int dummy;
} Extn;

#if defined(__FreeBSD__) || defined(__DragonFly__)
static const char *sources[] =
  {
     "hw.acpi.thermal.tz0.temperature",
     "dev.cpu.0.temperature",
     "dev.aibs.0.temp.0",
     "dev.lm75.0.temperature",
     NULL
  };
#endif

#if defined(__OpenBSD__)
static struct sensor snsr;
static size_t slen = sizeof(snsr);
#endif

static void
init(Tempthread *tth)
{
#if defined(__OpenBSD__)
   int dev, numt;
   struct sensordev snsrdev;
   size_t sdlen = sizeof(snsrdev);
#endif

#if defined (__FreeBSD__) || defined(__DragonFly__)
   unsigned i;
   size_t len;
   int rc;
#endif

   Extn *extn;
   if (tth->initted) return;
   tth->initted = EINA_TRUE;

   extn = calloc(1, sizeof(Extn));
   tth->extn = extn;

   if ((!tth->sensor_type) ||
       ((!tth->sensor_name) ||
        (tth->sensor_name[0] == 0)))
     {
        eina_stringshare_del(tth->sensor_name);
        tth->sensor_name = NULL;
        eina_stringshare_del(tth->sensor_path);
        tth->sensor_path = NULL;

#if defined (__FreeBSD__) || defined(__DragonFly__)
        for (i = 0; sources[i]; i++)
          {
             rc = sysctlbyname(sources[i], NULL, NULL, NULL, 0);
             if (rc == 0)
               {
                  tth->sensor_type = SENSOR_TYPE_FREEBSD;
                  tth->sensor_name = eina_stringshare_add(sources[i]);
                  break;
               }
          }

#elif defined(__OpenBSD__)
        extn->mib[0] = CTL_HW;
        extn->mib[1] = HW_SENSORS;

        for (dev = 0;; dev++)
          {
             extn->mib[2] = dev;
             if (sysctl(extn->mib, 3, &snsrdev, &sdlen, NULL, 0) == -1)
               {
                  if (errno == ENOENT) /* no further sensors */
                    break;
                  else
                    continue;
               }
             if (strcmp(snsrdev.xname, "cpu0") == 0)
               {
                  tth->sensor_type = SENSOR_TYPE_OPENBSD;
                  tth->sensor_name = eina_stringshare_add("cpu0");
                  break;
               }
             else if (strcmp(snsrdev.xname, "km0") == 0)
               {
                  tth->sensor_type = SENSOR_TYPE_OPENBSD;
                  tth->sensor_name = eina_stringshare_add("km0");
                  break;
               }
          }
#endif
     }
   if ((tth->sensor_type) && (tth->sensor_name) && (!tth->sensor_path))
     {
        if (tth->sensor_type == SENSOR_TYPE_FREEBSD)
          {
#if defined(__FreeBSD__) || defined(__DragonFly__)
             len = sizeof(extn->mib) / sizeof(extn->mib[0]);
             rc = sysctlnametomib(tth->sensor_name, extn->mib, &len);
             if (rc == 0)
               {
                  extn->miblen = len;
                  tth->sensor_path = eina_stringshare_add(tth->sensor_name);
               }
#endif
          }
        else if (tth->sensor_type == SENSOR_TYPE_OPENBSD)
          {
#if defined(__OpenBSD__)
             for (numt = 0; numt < snsrdev.maxnumt[SENSOR_TEMP]; numt++)
               {
                  extn->mib[4] = numt;
                  slen = sizeof(snsr);
                  if (sysctl(extn->mib, 5, &snsr, &slen, NULL, 0) == -1)
                    continue;
                  if (slen > 0 && (snsr.flags & SENSOR_FINVALID) == 0)
                    {
                       break;
                    }
               }
#endif
          }
     }
}

static int
check(Tempthread *tth)
{
   int ret = 0;
   int temp = 0;
#if defined (__FreeBSD__) || defined(__DragonFly__)
   size_t len;
   size_t ftemp = 0;
#endif
#if defined (__FreeBSD__) || defined(__DragonFly__) || defined (__OpenBSD__)
   Extn *extn = tth->extn;
#endif

   /* TODO: Make standard parser. Seems to be two types of temperature string:
    * - Somename: <temp> C
    * - <temp>
    */
   if (tth->sensor_type == SENSOR_TYPE_FREEBSD)
     {
#if defined (__FreeBSD__) || defined(__DragonFly__)
        len = sizeof(ftemp);
        if (sysctl(extn->mib, extn->miblen, &ftemp, &len, NULL, 0) == 0)
          {
             temp = (ftemp - 2732) / 10;
             ret = 1;
          }
        else
          goto error;
#endif
     }
   else if (tth->sensor_type == SENSOR_TYPE_OPENBSD)
     {
#if defined(__OpenBSD__)
        if (sysctl(extn->mib, 5, &snsr, &slen, NULL, 0) != -1)
          {
             temp = (snsr.value - 273150000) / 1000000.0;
             ret = 1;
          }
        else
          goto error;
#endif
     }
if (ret) return temp;

   return -999;
error:
   tth->sensor_type = SENSOR_TYPE_NONE;
   eina_stringshare_del(tth->sensor_name);
   tth->sensor_name = NULL;
   eina_stringshare_del(tth->sensor_path);
   tth->sensor_path = NULL;
   return -999;
}

int
thermal_sysctl_get(Tempthread *tth)
{
   int temp;

   init(tth);
   temp = check(tth);
   return temp;
}
#endif
