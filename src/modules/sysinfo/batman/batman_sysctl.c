#include "batman.h"

#include <sys/types.h>
#include <sys/sysctl.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <sys/param.h>
# include <sys/sensors.h>
#endif

extern Eina_List *batman_device_batteries;
extern Eina_List *batman_device_ac_adapters;

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
static Eina_Bool _batman_sysctl_battery_update_poll(void *data);
static int       _batman_sysctl_battery_update(Instance *inst);
#endif

int
_batman_sysctl_start(Instance *inst)
{
#if defined(__OpenBSD__) || defined(__NetBSD__)
   int mib[] = {CTL_HW, HW_SENSORS, 0, 0, 0};
   int devn, i = 0;
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);
   char name[256];
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   size_t len;
#endif
#if defined(__OpenBSD__) || defined(__NetBSD__)
   for (devn = 0;; devn++) 
     {
        mib[2] = devn;
        if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENXIO)
               continue;
             if (errno == ENOENT)
               break;
          }

        snprintf(name, sizeof(name), "acpibat%d", i);
        if (!strcmp(name, snsrdev.xname))
          {
             Battery *bat = E_NEW(Battery, 1);
             if (!bat)
               return 0;
             bat->inst = inst;
             bat->udi = eina_stringshare_add(name),
             bat->mib = malloc(sizeof(int) * 5);
             if (!bat->mib) return 0;
             bat->mib[0] = mib[0];
             bat->mib[1] = mib[1];
             bat->mib[2] = mib[2];
             bat->technology = eina_stringshare_add("Unknown");
             bat->model = eina_stringshare_add("Unknown");
             bat->last_update = ecore_time_get();
             bat->vendor = eina_stringshare_add("Unknown");
             bat->poll = ecore_poller_add(ECORE_POLLER_CORE,
                                          inst->cfg->batman.poll_interval,
                                          _batman_sysctl_battery_update_poll, inst);
             batman_device_batteries = eina_list_append(batman_device_batteries, bat);
             ++i;
         }

       if (!strcmp("acpiac0", snsrdev.xname))
         {
            Ac_Adapter *ac = E_NEW(Ac_Adapter, 1);
            if (!ac) return 0;
            ac->inst = inst;
            ac->udi = eina_stringshare_add("acpiac0");
            ac->mib = malloc(sizeof(int) * 5);
            if (!ac->mib) return 0;
            ac->mib[0] = mib[0];
            ac->mib[1] = mib[1];
            ac->mib[2] = mib[2];
            batman_device_ac_adapters = eina_list_append(batman_device_ac_adapters, ac);
         }
     }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   if ((sysctlbyname("hw.acpi.battery.life", NULL, &len, NULL, 0)) != -1)
     {
        Battery *bat = E_NEW(Battery, 1);
        if (!bat)
          return 0;
        bat->inst = inst;
        bat->mib = malloc(sizeof(int) * 4);
        if (!bat->mib) return 0;
        sysctlnametomib("hw.acpi.battery.life", bat->mib, &len); 

        bat->mib_state = malloc(sizeof(int) * 4);
        if (!bat->mib_state) return 0;
        sysctlnametomib("hw.acpi.battery.state", bat->mib_state, &len);

        bat->mib_time  = malloc(sizeof(int) * 4);
        if (!bat->mib_time) return 0;
        sysctlnametomib("hw.acpi.battery.time", bat->mib_time, &len);

        bat->mib_units = malloc(sizeof(int) * 4);
        if(!bat->mib_units) return 0;
        sysctlnametomib("hw.acpi.battery.units", bat->mib_units, &len);

        bat->last_update = ecore_time_get();
        bat->udi = eina_stringshare_add("hw.acpi.battery");
        bat->technology = eina_stringshare_add("Unknown");
        bat->model = eina_stringshare_add("Unknown");
        bat->vendor = eina_stringshare_add("Unknown");

        bat->poll = ecore_poller_add(ECORE_POLLER_CORE,
                       inst->cfg->batman.poll_interval,
                       _batman_sysctl_battery_update_poll, inst);

        batman_device_batteries = eina_list_append(batman_device_batteries, bat);
     }

   if ((sysctlbyname("hw.acpi.acline", NULL, &len, NULL, 0)) != -1)
     {
        Ac_Adapter *ac = E_NEW(Ac_Adapter, 1);
        if (!ac)
         return 0;
        ac->inst = inst;
        ac->mib = malloc(sizeof(int) * 3);
        if (!ac->mib) return 0;
        len = sizeof(ac->mib);
        sysctlnametomib("hw.acpi.acline", ac->mib, &len);
 
        ac->udi = eina_stringshare_add("hw.acpi.acline");

        batman_device_ac_adapters = eina_list_append(batman_device_ac_adapters, ac);
     }
#endif
   _batman_sysctl_battery_update(inst);

   return 1;
}

void
_batman_sysctl_stop(void)
{
   Battery *bat;
   Ac_Adapter *ac;

   EINA_LIST_FREE(batman_device_ac_adapters, ac) 
     {
        E_FREE_FUNC(ac->udi, eina_stringshare_del);
        E_FREE_FUNC(ac->mib, free);
        E_FREE_FUNC(ac, free);
     }
   
   EINA_LIST_FREE(batman_device_batteries, bat)
     {
        E_FREE_FUNC(bat->udi, eina_stringshare_del);
        E_FREE_FUNC(bat->technology, eina_stringshare_del);
        E_FREE_FUNC(bat->model, eina_stringshare_del);
        E_FREE_FUNC(bat->vendor, eina_stringshare_del);
        E_FREE_FUNC(bat->poll, ecore_poller_del);
#if defined(__FreeBSD__) || defined(__DragonFly__)
        E_FREE_FUNC(bat->mib_state, free);
        E_FREE_FUNC(bat->mib_time, free);
        E_FREE_FUNC(bat->mib_units, free);
#endif
        E_FREE_FUNC(bat->mib, free);
        E_FREE_FUNC(bat, free);
     }
}

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
static Eina_Bool
_batman_sysctl_battery_update_poll(void *data)
{
   Instance *inst = data;

   _batman_sysctl_battery_update(inst);

   return EINA_TRUE;
}
#endif

static int
_batman_sysctl_battery_update(Instance *inst)
{
   Battery *bat;
   Ac_Adapter *ac;
   Eina_List *l;
#if defined(__OpenBSD__) || defined(__NetBSD__)
   double _time, charge;
   struct sensor s;
   size_t slen = sizeof(struct sensor);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   double _time;
   int value;
   size_t len;
#endif
   EINA_LIST_FOREACH(batman_device_batteries, l, bat)
     {
        /* update the poller interval */
        ecore_poller_poller_interval_set(bat->poll,
                                        inst->cfg->batman.poll_interval);
#if defined(__OpenBSD__) || defined(__NetBSD__)
          /* last full capacity */
        bat->mib[3] = 7;
        bat->mib[4] = 0;
        if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) != -1)
          {
            bat->last_full_charge = (double)s.value;
          }
    
        /* remaining capacity */
        bat->mib[3] = 7;
        bat->mib[4] = 3;
        if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) != -1)
          {
             charge = (double)s.value;
          }
  
        /* This is a workaround because there's an ACPI bug */ 
        if ((EINA_FLT_EQ(charge, 0.0)) || (EINA_FLT_EQ(bat->last_full_charge, 0.0)))
          {
            /* last full capacity */
            bat->mib[3] = 8;
            bat->mib[4] = 0;
            if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) != -1)
              {
                 bat->last_full_charge = (double)s.value;
                 if (bat->last_full_charge < 0) return EINA_TRUE;
              }

            /* remaining capacity */
            bat->mib[3] = 8;
            bat->mib[4] = 3;
            if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) != -1)
              {
                 charge = (double)s.value;
              }
          }
       
         bat->got_prop = 1;
 
         _time = ecore_time_get();
         if ((bat->got_prop) && (!EINA_FLT_EQ(charge, bat->current_charge)))
           bat->charge_rate = ((charge - bat->current_charge) / (_time - bat->last_update));
         bat->last_update = _time;
         bat->current_charge = charge;
         bat->percent = 100 * (bat->current_charge / bat->last_full_charge);
         if (bat->current_charge >= bat->last_full_charge)
           bat->percent = 100;

         if (bat->got_prop)
           {
              if (bat->charge_rate > 0)
                {
                   if (inst->cfg->batman.fuzzy && (++inst->cfg->batman.fuzzcount <= 10) && (bat->time_full > 0))
                     bat->time_full = (((bat->last_full_charge - bat->current_charge) / bat->charge_rate) + bat->time_full) / 2;
                   else
                     bat->time_full = (bat->last_full_charge - bat->current_charge) / bat->charge_rate;
                   bat->time_left = -1;
                }
              else
                {
                   if (inst->cfg->batman.fuzzy && (inst->cfg->batman.fuzzcount <= 10) && (bat->time_left > 0))
                     bat->time_left = (((0 - bat->current_charge) / bat->charge_rate) + bat->time_left) / 2;
                   else
                     bat->time_left = (0 - bat->current_charge) / bat->charge_rate;
                   bat->time_full = -1;
                }
           }
         else
           {
              bat->time_full = -1;
              bat->time_left = -1;
           }
   
       /* battery state 1: discharge, 2: charge */
         bat->mib[3] = 10;
         bat->mib[4] = 0;
         if (sysctl(bat->mib, 5, &s, &slen, NULL, 0) == -1)
           {
              if (s.value == 2)
                bat->charging = 1;
              else
                bat->charging = 0;
           }
          _batman_device_update(bat->inst);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
         len = sizeof(value);
         if ((sysctl(bat->mib, 4, &value, &len, NULL, 0)) == -1)
           {
             return EINA_FALSE;
           }
    
         bat->percent = value;

         _time = ecore_time_get();
         bat->last_update = _time;

         len = sizeof(value);
         if ((sysctl(bat->mib_state, 4, &value, &len, NULL, 0)) == -1)
           {
             return EINA_FALSE;
           }

         bat->charging = !value;
         bat->got_prop = 1;

         bat->time_full = -1;
         bat->time_left = -1;

         len = sizeof(bat->time_min);
         if ((sysctl(bat->mib_time, 4, &bat->time_min, &len, NULL, 0)) == -1)
           {
              bat->time_min = -1;
           }
   
         len = sizeof(bat->batteries);
         if ((sysctl(bat->mib_units, 4, &bat->batteries, &len, NULL, 0)) == -1)
           {
              bat->batteries = 1;
           }

         if (bat->time_min >= 0) bat->time_left = bat->time_min * 60;
         if (bat->batteries == 1) bat->time_left = -1;
         _batman_device_update(inst);
#endif 
     }


   EINA_LIST_FOREACH(batman_device_ac_adapters, l, ac) 
     {
#if defined(__OpenBSD__) || defined(__NetBSD__)
        /* AC State */
        ac->mib[3] = 9;
        ac->mib[4] = 0;
        if (sysctl(ac->mib, 5, &s, &slen, NULL, 0) != -1)
          {
             if (s.value)
               ac->present = 1;
             else
               ac->present = 0;
          }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
        len = sizeof(value);
        if ((sysctl(ac->mib, 3, &value, &len, NULL, 0)) != -1)
          {
             ac->present = value;
          }
#endif
     } 

   return EINA_TRUE;
}



