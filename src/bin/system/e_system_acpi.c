#include "e_system.h"
#if defined __OpenBSD__
// no local funcs
#elif defined __FreeBSD__
// no local funcs
#else
#  include <linux/input.h>
#  include <fcntl.h>

typedef struct
{
  Ecore_Fd_Handler *handler;
  char *name;
  char *file;
  int fd;
} Dev;

typedef struct
{
  unsigned short type;
  unsigned short code;
  int            value;
  const char    *str;
} Input_Dev_Map;

static Eina_List *devices = NULL;
static Eeze_Udev_Watch *watch[16] = { NULL };

static const Input_Dev_Map devmap[] = {
  { EV_KEY, KEY_POWER,           1, "button/power PBTN 00000080 00000000"         },
  { EV_KEY, KEY_SLEEP,           1, "button/sleep SBTN 00000080 00000000"         },
  { EV_SW,  SW_LID,              1, "button/lid LID close"                        },
  { EV_SW,  SW_LID,              0, "button/lid LID open"                         },
  { EV_SW,  SW_TABLET_MODE,      0, "video/tabletmode TBLT 0000008A 00000000"     },
  { EV_SW,  SW_TABLET_MODE,      1, "video/tabletmode TBLT 0000008A 00000001"     },
  { EV_KEY, KEY_ZOOM,            1, "button/zoom ZOOM 00000080 00000000"          },
  { EV_KEY, KEY_BRIGHTNESSDOWN,  1, "video/brightnessdown BRTDN 00000087 00000000"},
  { EV_KEY, KEY_BRIGHTNESSUP,    1, "video/brightnessup BRTUP 00000086 00000000"  },
  { EV_KEY, KEY_SWITCHVIDEOMODE, 1, "video/switchmode VMOD 00000080 00000000"     },
  { EV_KEY, KEY_VOLUMEDOWN,     1, "button/volumedown VOLDN 00000080 00000000"   },
  { EV_KEY, KEY_VOLUMEUP,       1, "button/volumeup VOLUP 00000080 00000000"     },
  { EV_KEY, KEY_MUTE,           1, "button/mute MUTE 00000080 00000000"          },
  { EV_KEY, KEY_NEXTSONG,       1, "cd/next CDNEXT 00000080 00000000"            },
  { EV_KEY, KEY_PREVIOUSSONG,   1, "cd/prev CDPREV 00000080 00000000"            },
  { EV_KEY, KEY_PLAYPAUSE,      1, "cd/play CDPLAY 00000080 00000000"            },
  { EV_KEY, KEY_STOPCD,         1, "cd/stop CDSTOP 00000080 00000000"            },
  { EV_KEY, KEY_BATTERY,        1, "button/battery BAT 00000080 00000000"        },
  { EV_KEY, KEY_SCREENLOCK,     1, "button/screenlock SCRNLCK 00000080 00000000" },
  { EV_KEY, KEY_WLAN,           1, "button/wlan WLAN 00000080 00000000"          }
};

static Eina_Bool
is_good(int fd)
{
#  define LONGBITS       (sizeof(long) * 8)
#  define NBITS(x)       ((((x) - 1) / LONGBITS) + 1)
#  define OFF(x)         ((x) % LONGBITS)
#  define LONG(x)        ((x) / LONGBITS)
#  define HASBIT(b, arr) ((arr[LONG(b)] >> OFF(b)) & 1)
  unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
  unsigned int i, type;

  // get type of event supported
  memset(bit, 0, sizeof(bit));
  if (ioctl(fd, EVIOCGBIT(0, sizeof(bit[0])), bit[0]) < 0) return -1;
  for (type = 0; type < EV_CNT; type++)
    {
      if (!HASBIT(type, bit[0])) continue; // skip
      for (i = 0; i < (sizeof(devmap) / sizeof(devmap[0])); i++)
        {
          if (devmap[i].type != type) continue; // skip
          // get every code supported
          memset(bit, 0, sizeof(bit));
          ioctl(fd, EVIOCGBIT(type, sizeof(bit[type])), bit[type]);
          if (HASBIT(devmap[i].code, bit[type])) return EINA_TRUE;
        }
    }
  return EINA_FALSE;
}

static Dev *
dev_find(const char *file)
{
  Eina_List *l;
  Dev *d;

  EINA_LIST_FOREACH(devices, l, d)
    {
      if (!strcmp(file, d->file)) return d;
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
  if (d->fd >= 0) close(d->fd);
  if (d->handler) ecore_main_fd_handler_del(d->handler);
  free(d->name);
  free(d->file);
  free(d);
}

static Eina_Bool
cb_event_fd_active(void *data, Ecore_Fd_Handler *fd_handler)
{
  Dev *d = data;
  struct input_event ev;
  ssize_t size;
  unsigned int i;
  Eina_Bool lost;

  if (ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_READ))
    {
      lost = EINA_FALSE;
      size = read(d->fd, &ev, sizeof(struct input_event));
      if (size == sizeof(struct input_event))
        {
          for (i = 0; i < (sizeof(devmap) / sizeof(devmap[0])); i++)
            {
              if ((ev.type == devmap[i].type) &&
                  (ev.code == devmap[i].code) &&
                  (ev.value == devmap[i].value))
                {
                  e_system_inout_command_send("acpi-event", "%s",
                                              devmap[i].str);
                  break;
                }
            }
        }
      else
        {
          lost = ((errno == EIO) ||
                  (errno == EBADF) ||
                  (errno == EPIPE) ||
                  (errno == EINVAL) ||
                  (errno == ENOSPC) ||
                  (errno == ENODEV) ||
                  (errno == EISDIR) ||
                  (size > 0)
                 );
        }
      if (lost) dev_del(d);
    }
   return ECORE_CALLBACK_RENEW;
}

static Dev *
dev_add(const char *file)
{
  int fd;
  Dev *d = NULL;
  char name[256];

  if (ecore_file_is_dir(file)) return NULL;
  fd = open(file, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) return NULL;
  if (!is_good(fd)) goto err;
  d = calloc(1, sizeof(Dev));
  if (!d) goto err;
  d->fd = fd;
  if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) d->name = strdup(name);
  d->file = strdup(file);
  if (!d->file) goto err;
  d->handler = ecore_main_fd_handler_add(d->fd, ECORE_FD_READ,
                                         cb_event_fd_active, d, NULL, NULL);
  if (!d->handler) goto err;
  return d;
err:
  close(fd);
  if (d)
    {
      if (d->handler) ecore_main_fd_handler_del(d->handler);
      free(d->name);
      free(d->file);
      free(d);
    }
  return NULL;
}

static void
_cb_eeze(const char *str, Eeze_Udev_Event ev EINA_UNUSED,
         void *data EINA_UNUSED, Eeze_Udev_Watch *w EINA_UNUSED)
{
  const char *file;
  char buf[128];

  if (!str) return;
  file = ecore_file_file_get(str);
  if (!file) return;
  if (!!strncmp(file, "event", 5)) return;
  if (strlen(file) > 100) return; // just stupidly too big
  snprintf(buf, sizeof(buf), "/dev/input/%s", file);
  if (!dev_have(buf)) dev_add(buf);
}

static void
dev_open(void)
{
  Eina_Iterator *it;
  Eina_File_Direct_Info *info;
  int i = 0;

#  define WATCH(t) \
    watch[i++] = eeze_udev_watch_add(t, EEZE_UDEV_EVENT_ADD, _cb_eeze, NULL)
  WATCH(EEZE_UDEV_TYPE_KEYBOARD);
  WATCH(EEZE_UDEV_TYPE_MOUSE);
  WATCH(EEZE_UDEV_TYPE_TOUCHPAD);
  WATCH(EEZE_UDEV_TYPE_POWER_AC);
  WATCH(EEZE_UDEV_TYPE_BLUETOOTH);
  WATCH(EEZE_UDEV_TYPE_JOYSTICK);
  WATCH(EEZE_UDEV_TYPE_BACKLIGHT);
  WATCH(EEZE_UDEV_TYPE_LEDS);
  WATCH(EEZE_UDEV_TYPE_GPIO);
  it  = eina_file_direct_ls("/dev/input");
  EINA_ITERATOR_FOREACH(it, info) dev_add(info->path);
  eina_iterator_free(it);
}
#endif

void
e_system_acpi_init(void)
{
#  if defined __OpenBSD__
// no local funcs
#  elif defined __FreeBSD__
// no local funcs
#  else
  dev_open();
#endif
}

void
e_system_acpi_shutdown(void)
{
   // only shutdown things we really have to - no need to free mem etc.
}
