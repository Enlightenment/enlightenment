#include "e_system.h"
#if defined __OpenBSD__
// no local funcs
#elif defined __FreeBSD__
// no local funcs
#else

typedef struct
{
  char *syspath;
} Dev;

static Eina_List *devices = NULL;
static Eeze_Udev_Watch *watch = NULL;

static Dev *
dev_find(const char *syspath)
{
  Eina_List *l;
  Dev *d;

  EINA_LIST_FOREACH(devices, l, d)
    {
      if (!strcmp(syspath, d->syspath)) return d;
    }
  return NULL;
}

static Eina_Bool
dev_have(const char *file)
{
  if (dev_find(file)) return EINA_TRUE;
  return EINA_FALSE;
}

static void
dev_del(Dev *d)
{
  if (!d) return;
  devices = eina_list_remove(devices, d);
  free(d->syspath);
  free(d);
}

static Dev *
dev_add(const char *syspath)
{
  Dev *d = NULL;

  fprintf(stderr, "BAT: ADD BAT [%s]\n", syspath);
  d = calloc(1, sizeof(Dev));
  if (!d) return NULL;
  d->syspath = strdup(syspath);
  if (!d->syspath)
    {
      free(d);
      return NULL;
    }
  devices = eina_list_append(devices, d);
  return d;
}

static void
_cb_eeze(const char *syspath, Eeze_Udev_Event ev,
         void *data EINA_UNUSED, Eeze_Udev_Watch *w EINA_UNUSED)
{
  if ((ev & EEZE_UDEV_EVENT_ADD) ||
      (ev & EEZE_UDEV_EVENT_ONLINE))
    {
      if (!dev_have(syspath)) dev_add(syspath);
    }
  else if ((ev & EEZE_UDEV_EVENT_REMOVE) ||
           (ev & EEZE_UDEV_EVENT_OFFLINE))
    {
      dev_del(dev_find(syspath));
    }
  // else - some update but we don't care here

}

static void
_cb_battery_lim_set(void *data EINA_UNUSED, const char *params EINA_UNUSED)
{ // reply = "dev1 -|p dev2 -|p ..."
  char dev[1024] = "", buf2[32];
  int val = 0;
  Dev *d;
  int fd;

  if (sscanf(params, "%1023s %i", dev, &val) != 2) return;
  if (val < 1) val = 1;
  else if (val > 100) val = 100;
  d = dev_find(dev);
  fprintf(stderr, "BAT: device = %p for [%s]\n", d, dev);
  if (!d) return;
  snprintf(dev, sizeof(dev), "%s/charge_control_end_threshold", d->syspath);
  snprintf(buf2, sizeof(buf2), "%i", val);
  fd = open(dev, O_WRONLY);
  fprintf(stderr, "BAT: device [%s] fd = %i -> [%s]\n", dev, fd, buf2);
  if (fd >= 0)
    {
      if (write(fd, buf2, strlen(buf2)) <= 0)
        ERR("Write failed of [%s] to [%s]\n", buf2, dev);
      close(fd);
    }
}
#endif

void
e_system_battery_init(void)
{
#if defined __OpenBSD__
// no local funcs
#elif defined __FreeBSD__
// no local funcs
#else
  Eina_List *devs;
  const char *dev;

  devs = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_BAT, NULL);
  fprintf(stderr, "BAT: devices = %p\n", devs);
  EINA_LIST_FREE(devs, dev)
    {
      if (!dev_have(dev)) dev_add(dev);
    }

  watch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_BAT, EEZE_UDEV_EVENT_NONE,
                              _cb_eeze, NULL);
#endif
  e_system_inout_command_register("battery-lim-set", _cb_battery_lim_set, NULL);
}

void
e_system_battery_shutdown(void)
{
   // only shutdown things we really have to - no need to free mem etc.
}
