#ifndef IMFOS_DEVICES_H_
#define IMFOS_DEVICES_H_

#include <e.h>

typedef struct _Imfos_Device
{
   const char *syspath;
   const char *name;
   const char *dev_name;
   int type;
   Ecore_Thread *thread;
   int capacities;
   int poll_interval;
   int powersave_min_state;
   int priority;
   int timeout;
   double time_start;
   double last_catch;
   int catch_count;
   int catch_time_average;
   Eina_Bool init_time_fixed;
   Eina_Bool catched;
   Eina_Bool async;
   Eina_Bool auto_screen_on;
   Eina_Bool auto_screen_off;
   Eina_Bool auto_logout;
   Eina_Bool auto_login;
   Eina_Bool timeout_enabled;
   Eina_Bool found;
   Eina_Bool enabled;
   Eina_Bool dev_name_locked;
   struct
     {
        struct
          {
             Evas_Object *cam;
             Eina_Bool flip;
             int init_time;
             int init_time_average;
          } v4l;
        struct
          {
             const char *ssid;
          } wifi;
        struct
          {
             const char *id;
          } bluetooth;
     } param;
} Imfos_Device;

void imfos_devices_init(void);
void imfos_devices_shutdown(void);
Eina_Bool imfos_devices_run(Imfos_Device *dev);
Eina_Bool imfos_devices_cancel(Imfos_Device *dev);
Eina_Bool imfos_devices_timeout(Imfos_Device *dev);


#endif /* IMFOS_DEVICES_H_ */
