#if defined(__linux__)
#include "thermal.h"

typedef struct
{
   int dummy;
} Extn;

Eina_List *
temperature_get_bus_files(const char *bus)
{
   Eina_List *result;
   Eina_List *therms;
   char path[PATH_MAX];
   char busdir[PATH_MAX];
   char *name;

   result = NULL;

   snprintf(busdir, sizeof(busdir), "/sys/bus/%s/devices", bus);
   /* Look through all the devices for the given bus. */
   therms = ecore_file_ls(busdir);

   EINA_LIST_FREE(therms, name)
     {
        Eina_List *files;
        char *file;

        /* Search each device for temp*_input, these should be
         * temperature devices. */
        snprintf(path, sizeof(path), "%s/%s", busdir, name);
        files = ecore_file_ls(path);
        EINA_LIST_FREE(files, file)
          {
             if ((!strncmp("temp", file, 4)) &&
                 (!strcmp("_input", &file[strlen(file) - 6])))
               {
                  char *f;

                  snprintf(path, sizeof(path),
                           "%s/%s/%s", busdir, name, file);
                  f = strdup(path);
                  if (f) result = eina_list_append(result, f);
               }
             free(file);
          }
        free(name);
     }
   return result;
}

static void
init(Tempthread *tth)
{
   Eina_List *therms;
   char path[512];
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
        therms = ecore_file_ls("/proc/acpi/thermal_zone");
        if (therms)
          {
             char *name;

             name = eina_list_data_get(therms);
             tth->sensor_type = SENSOR_TYPE_LINUX_ACPI;
             tth->sensor_name = eina_stringshare_add(name);
             eina_list_free(therms);
          }
        else
          {
             eina_list_free(therms);
             therms = ecore_file_ls("/sys/class/thermal");
             if (therms)
               {
                  char *name;
                  Eina_List *l;

                  EINA_LIST_FOREACH(therms, l, name)
                    {
                       if (!strncmp(name, "thermal", 7))
                         {
                            tth->sensor_type = SENSOR_TYPE_LINUX_SYS;
                            tth->sensor_name = eina_stringshare_add(name);
                            eina_list_free(therms);
                            therms = NULL;
                            break;
                         }
                    }
                  if (therms) eina_list_free(therms);
               }
             if (therms)
               {
                  if (ecore_file_exists("/proc/omnibook/temperature"))
                    {
                       tth->sensor_type = SENSOR_TYPE_OMNIBOOK;
                       tth->sensor_name = eina_stringshare_add("dummy");
                    }
                  else if (ecore_file_exists("/sys/devices/temperatures/sensor1_temperature"))
                    {
                       tth->sensor_type = SENSOR_TYPE_LINUX_PBOOK;
                       tth->sensor_name = eina_stringshare_add("dummy");
                    }
                  else if (ecore_file_exists("/sys/devices/temperatures/cpu_temperature"))
                    {
                       tth->sensor_type = SENSOR_TYPE_LINUX_MACMINI;
                       tth->sensor_name = eina_stringshare_add("dummy");
                    }
                  else if (ecore_file_exists("/sys/devices/platform/coretemp.0/temp1_input"))
                    {
                       tth->sensor_type = SENSOR_TYPE_LINUX_INTELCORETEMP;
                       tth->sensor_name = eina_stringshare_add("dummy");
                    }
                  else if (ecore_file_exists("/sys/devices/platform/thinkpad_hwmon/temp1_input"))
                    {
                       tth->sensor_type = SENSOR_TYPE_LINUX_THINKPAD;
                       tth->sensor_name = eina_stringshare_add("dummy");
                    }
                  else
                    {
                       // try the i2c bus
                       therms = temperature_get_bus_files("i2c");
                       if (therms)
                         {
                            char *name;

                            if ((name = eina_list_data_get(therms)))
                              {
                                 if (ecore_file_exists(name))
                                   {
                                      int len;

                                      snprintf(path, sizeof(path),
                                               "%s", ecore_file_file_get(name));
                                      len = strlen(path);
                                      if (len > 6) path[len - 6] = '\0';
                                      tth->sensor_type = SENSOR_TYPE_LINUX_I2C;
                                      tth->sensor_path = eina_stringshare_add(name);
                                      tth->sensor_name = eina_stringshare_add(path);
                                   }
                              }
                            eina_list_free(therms);
                         }
                       if (!tth->sensor_path)
                         {
                            // try the pci bus
                            therms = temperature_get_bus_files("pci");
                            if (therms)
                              {
                                 char *name;

                                 if ((name = eina_list_data_get(therms)))
                                   {
                                      if (ecore_file_exists(name))
                                        {
                                           int len;

                                           snprintf(path, sizeof(path),
                                                    "%s", ecore_file_file_get(name));
                                           len = strlen(path);
                                           if (len > 6) path[len - 6] = '\0';
                                           tth->sensor_type = SENSOR_TYPE_LINUX_PCI;
                                           tth->sensor_path = eina_stringshare_add(name);
                                           eina_stringshare_del(tth->sensor_name);
                                           tth->sensor_name = eina_stringshare_add(path);
                                        }
                                   }
                                 eina_list_free(therms);
                              }
                         }
                    }
               }
          }
     }
   if ((tth->sensor_type) && (tth->sensor_name) && (!tth->sensor_path))
     {
        char *name;

        switch (tth->sensor_type)
          {
           case SENSOR_TYPE_NONE:
             break;
           case SENSOR_TYPE_OMNIBOOK:
             tth->sensor_path = eina_stringshare_add("/proc/omnibook/temperature");
             break;

           case SENSOR_TYPE_LINUX_MACMINI:
             tth->sensor_path = eina_stringshare_add("/sys/devices/temperatures/cpu_temperature");
             break;

           case SENSOR_TYPE_LINUX_PBOOK:
             tth->sensor_path = eina_stringshare_add("/sys/devices/temperatures/sensor1_temperature");
             break;

           case SENSOR_TYPE_LINUX_INTELCORETEMP:
             tth->sensor_path = eina_stringshare_add("/sys/devices/platform/coretemp.0/temp1_input");
             break;

           case SENSOR_TYPE_LINUX_THINKPAD:
             tth->sensor_path = eina_stringshare_add("/sys/devices/platform/thinkpad_hwmon/temp1_input");
             break;

           case SENSOR_TYPE_LINUX_I2C:
             therms = ecore_file_ls("/sys/bus/i2c/devices");

             EINA_LIST_FREE(therms, name)
               {
                  snprintf(path, sizeof(path),
                           "/sys/bus/i2c/devices/%s/%s_input",
                           name, tth->sensor_name);
                  if (ecore_file_exists(path))
                    {
                       tth->sensor_path = eina_stringshare_add(path);
                       /* We really only care about the first
                        * one for the default. */
                       break;
                    }
                  free(name);
               }
             break;

           case SENSOR_TYPE_LINUX_PCI:
             therms = ecore_file_ls("/sys/bus/pci/devices");

             EINA_LIST_FREE(therms, name)
               {
                  snprintf(path, sizeof(path),
                           "/sys/bus/pci/devices/%s/%s_input",
                           name, tth->sensor_name);
                  if (ecore_file_exists(path))
                    {
                       tth->sensor_path = eina_stringshare_add(path);
                       /* We really only care about the first
                        * one for the default. */
                       break;
                    }
                  free(name);
               }
             break;

           case SENSOR_TYPE_LINUX_ACPI:
             snprintf(path, sizeof(path),
                      "/proc/acpi/thermal_zone/%s/temperature",
                      tth->sensor_name);
             tth->sensor_path = eina_stringshare_add(path);
             break;

           case SENSOR_TYPE_LINUX_SYS:
             snprintf(path, sizeof(path),
                      "/sys/class/thermal/%s/temp", tth->sensor_name);
             tth->sensor_path = eina_stringshare_add(path);
             break;

           default:
             break;
          }
     }
}

static int
check(Tempthread *tth)
{
   FILE *f = NULL;
   int ret = 0;
   int temp = 0;
   char buf[512];
   /* TODO: Make standard parser. Seems to be two types of temperature string:
    * - Somename: <temp> C
    * - <temp>
    */
   switch (tth->sensor_type)
     {
      case SENSOR_TYPE_NONE:
        /* TODO: Slow down poller? */
        break;

      case SENSOR_TYPE_OMNIBOOK:
        f = fopen(tth->sensor_path, "r");
        if (f)
          {
             char dummy[4096];

             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             if (sscanf(buf, "%s %s %i", dummy, dummy, &temp) == 3)
               ret = 1;
             else
               goto error;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_MACMINI:
      case SENSOR_TYPE_LINUX_PBOOK:
        f = fopen(tth->sensor_path, "rb");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             if (sscanf(buf, "%i", &temp) == 1)
               ret = 1;
             else
               goto error;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_INTELCORETEMP:
      case SENSOR_TYPE_LINUX_I2C:
      case SENSOR_TYPE_LINUX_THINKPAD:
        f = fopen(tth->sensor_path, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             /* actually read the temp */
             if (sscanf(buf, "%i", &temp) == 1)
               ret = 1;
             else
               goto error;
             /* Hack for temp */
             temp = temp / 1000;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_PCI:
        f = fopen(tth->sensor_path, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             /* actually read the temp */
             if (sscanf(buf, "%i", &temp) == 1)
               ret = 1;
             else
               goto error;
             /* Hack for temp */
             temp = temp / 1000;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_ACPI:
        f = fopen(tth->sensor_path, "r");
        if (f)
          {
             char *p, *q;

             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             p = strchr(buf, ':');
             if (p)
               {
                  p++;
                  while (*p == ' ')
                    p++;
                  q = strchr(p, ' ');
                  if (q) *q = 0;
                  temp = atoi(p);
                  ret = 1;
               }
             else
               goto error;
          }
        else
          goto error;
        break;

      case SENSOR_TYPE_LINUX_SYS:
        f = fopen(tth->sensor_path, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f) == NULL) goto error;
             fclose(f);
             f = NULL;
             temp = atoi(buf);
             temp /= 1000;
             ret = 1;
          }
        else
          goto error;
        break;

      default:
        break;
     }

   if (ret) return temp;

   return -999;
error:
   if (f) fclose(f);
   tth->sensor_type = SENSOR_TYPE_NONE;
   eina_stringshare_del(tth->sensor_name);
   tth->sensor_name = NULL;
   eina_stringshare_del(tth->sensor_path);
   tth->sensor_path = NULL;
   return -999;
}

int
thermal_fallback_get(Tempthread *tth)
{
   int temp;

   init(tth);
   temp = check(tth);
   return temp;
}

#endif
