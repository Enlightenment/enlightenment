#include "e_mod_main.h"
#include "imfos_devices.h"
#include "imfos_v4l.h"
#include <Eeze.h>

static Imfos_Device *_imfos_devices_add(const char *syspath, int type);
static void _imfos_devices_del(Imfos_Device *dev);
static void _imfos_devices_eeze_events(const char *syspath, Eeze_Udev_Event ev, void *data, Eeze_Udev_Watch *watch);

static Eina_List *_delayed_jobs = NULL;
static Eeze_Udev_Watch *_eeze_watcher = NULL;


static Imfos_Device *
_imfos_devices_new(const char *syspath)
{
   Imfos_Device *dev;

   dev = calloc(1, sizeof(Imfos_Device));
   dev->syspath = eina_stringshare_ref(syspath);
   dev->name = eeze_udev_syspath_get_sysattr(syspath, "name");
   dev->dev_name = eeze_udev_syspath_get_property(syspath, "DEVNAME");
   printf("New device %s(%s)\n", dev->name, dev->dev_name);
   return dev;
}


static void
_imfos_devices_del(Imfos_Device *dev)
{
   eina_stringshare_del(dev->syspath);
   eina_stringshare_del(dev->name);
   eina_stringshare_del(dev->dev_name);
   free(dev);
}

static Eina_Bool
_imfos_devices_check_capabilities(const char *syspath, const char *dev_name)
{
        /* TODO check capacities */
   return EINA_TRUE;
}

static Imfos_Device *
_imfos_devices_add(const char *syspath, int type)
{
   Eina_List *l;
   Imfos_Device *dev;
   const char *name;
   const char *dev_name;

   name = eeze_udev_syspath_get_sysattr(syspath, "name");
   dev_name = eeze_udev_syspath_get_property(syspath, "DEVNAME");
   printf("New device %s(%s) know(%d)\n", name, dev_name, eina_list_count(imfos_config->devices));

   EINA_LIST_FOREACH(imfos_config->devices, l, dev)
     {
        if (dev->name == name)
          {
             printf("Found by name %s\n");
             break;
          }
     }
   if (dev)
     {
        printf("Updating device %s\n", name);
        dev->syspath = eina_stringshare_ref(syspath);
        dev->dev_name = dev_name;
        dev->type = type;
        eina_stringshare_del(name);
     }
   else
     {
        if (_imfos_devices_check_capabilities(syspath, dev_name))
          {
             printf("First time\n");
             dev = calloc(1, sizeof(Imfos_Device));
             dev->syspath = eina_stringshare_ref(syspath);
             dev->name = name;
             dev->type = type;
             dev->dev_name = dev_name;
             imfos_config->devices =
                eina_list_append(imfos_config->devices, dev);
          }
        else
          {
             eina_stringshare_del(name);
             eina_stringshare_del(dev_name);
          }
     }
   return dev;
}

static void
_imfos_device_remove(Imfos_Device *dev)
{
   eina_stringshare_del(dev->syspath);
   dev->syspath = NULL;
   if (!dev->dev_name_locked)
     {
        eina_stringshare_del(dev->dev_name);
        dev->dev_name = NULL;
     }
   if (dev->thread) ecore_thread_cancel(dev->thread);
}

static void
_imfos_devices_eeze_events(const char *syspath, Eeze_Udev_Event ev,
                           void *data EINA_UNUSED,
                           Eeze_Udev_Watch *watch EINA_UNUSED)
{
   Imfos_Device *dev;
   Eina_List *l;

   if (ev == EEZE_UDEV_EVENT_ADD)
     {
        dev = _imfos_devices_add(syspath, EEZE_UDEV_TYPE_V4L);
     }
   else if (ev == EEZE_UDEV_EVENT_REMOVE)
     {
        EINA_LIST_FOREACH(imfos_config->devices, l, dev)
          {
             if (dev->syspath == syspath)
               {
                 _imfos_device_remove(dev);
                 break;
               }
          }
     }
}


void
imfos_devices_init(void)
{
   Eina_List *devices;
   const char *syspath;

   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_V4L, NULL);
   EINA_LIST_FREE(devices, syspath)
     {
        _imfos_devices_add(syspath, EEZE_UDEV_TYPE_V4L);
        eina_stringshare_del(syspath);
     }
     /*
   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_KEYBOARD, NULL);
   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_MOUSE, NULL);
   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_NET, NULL);
   */
   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_BLUETOOTH, NULL);
   EINA_LIST_FREE(devices, syspath)
     {
        _imfos_devices_add(syspath, EEZE_UDEV_TYPE_BLUETOOTH);
        eina_stringshare_del(syspath);
     }
   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_DRIVE_REMOVABLE, NULL);
   EINA_LIST_FREE(devices, syspath)
     {
        _imfos_devices_add(syspath, EEZE_UDEV_TYPE_DRIVE_REMOVABLE);
        eina_stringshare_del(syspath);
     }

   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_V4L, NULL);
   _eeze_watcher = eeze_udev_watch_add(EEZE_UDEV_TYPE_V4L,
                                       (EEZE_UDEV_EVENT_ADD | EEZE_UDEV_EVENT_REMOVE),
                                       _imfos_devices_eeze_events, NULL);
}

void
imfos_devices_shutdown(void)
{
   Imfos_Device *dev;
   eeze_udev_watch_del(_eeze_watcher);
}


Eina_Bool
imfos_devices_timeout(Imfos_Device *dev)
{
   double loop_time;

   if (dev->timeout > 0)
     {
        loop_time = ecore_time_get() - dev->time_start;
        if (loop_time > dev->timeout)
          {
             printf("timeout\n");
             return EINA_TRUE;
          }
     }

   return ecore_thread_check(dev->thread);
}

static void
_imfos_devices_thread_run(void *data, Ecore_Thread *thread)
{
   Imfos_Device *dev;

   dev = data;

   dev->catched = EINA_FALSE;
   dev->time_start = ecore_time_get();
   imfos_v4l_run(dev);

}

static void
_imfos_device_run(void)
{
   Eina_List *l;
   Imfos_Device *dev;

   EINA_LIST_FOREACH(_delayed_jobs, l, dev)
     {
        if (!dev->thread)
          {
             imfos_devices_run(dev);
             _delayed_jobs = eina_list_remove_list(_delayed_jobs, l);
             break;
          }
     }
}


static void
_imfos_devices_thread_clean(void *data, Ecore_Thread *thread)
{
   Imfos_Device *dev;
   double catch_time;

   dev = data;

   printf("Imfos thread End\n");
   if (dev->catched)
     {
        catch_time = dev->time_start - ecore_time_get();
        dev->catch_time_average =
           ((dev->catch_time_average * dev->catch_count) + catch_time)
           / (dev->catch_count + 1);
        ++dev->catch_count;
     }

   imfos_v4l_clean(dev);
   dev->thread = NULL;

   _imfos_device_run();
}



static void
_imfos_devices_thread_cancel(void *data, Ecore_Thread *thread)
{
   printf("Imfos thread cancel\n");
}

Eina_Bool
imfos_devices_run(Imfos_Device *dev)
{
   Ecore_Thread *thread;


   if (dev->thread)
     {
        ecore_thread_cancel(dev->thread);
        if (!dev->async)
          {
             _delayed_jobs = eina_list_append(_delayed_jobs, dev);
             return EINA_TRUE;
          }
     }
   dev->thread = ecore_thread_run(_imfos_devices_thread_run,
                                  _imfos_devices_thread_clean,
                                  _imfos_devices_thread_clean,
                                  dev);

   return EINA_FALSE;
}

Eina_Bool
imfos_devices_cancel(Imfos_Device *dev)
{
   if (dev->thread)
     {
        ecore_thread_cancel(dev->thread);
        return EINA_TRUE;
     }
   return EINA_FALSE;
}
