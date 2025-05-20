#include "e.h"
#include "e_mod_main.h"

#ifdef HAVE_EEZE

static void _battery_udev_event_battery(const char *syspath, Eeze_Udev_Event event, void *data, Eeze_Udev_Watch *watch);
static void _battery_udev_event_ac(const char *syspath, Eeze_Udev_Event event, void *data, Eeze_Udev_Watch *watch);
static void _battery_udev_battery_add(const char *syspath);
static void _battery_udev_ac_add(const char *syspath);
static void _battery_udev_battery_del(const char *syspath);
static void _battery_udev_ac_del(const char *syspath);
static Eina_Bool _battery_udev_battery_update_poll(void *data);
static void _battery_udev_battery_update(const char *syspath, Battery *bat);
static void _battery_udev_ac_update(const char *syspath, Ac_Adapter *ac);

extern Eina_List *device_batteries;
extern Eina_List *device_ac_adapters;
extern double init_time;

int
_battery_udev_start(void)
{
   Eina_List *devices;
   const char *dev;

   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_BAT, NULL);
   EINA_LIST_FREE(devices, dev)
     _battery_udev_battery_add(dev);

   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_AC, NULL);
   EINA_LIST_FREE(devices, dev)
     _battery_udev_ac_add(dev);

   if (!battery_config->batwatch)
     battery_config->batwatch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_BAT, EEZE_UDEV_EVENT_NONE, _battery_udev_event_battery, NULL);
   if (!battery_config->acwatch)
     battery_config->acwatch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_AC, EEZE_UDEV_EVENT_NONE, _battery_udev_event_ac, NULL);

   init_time = ecore_time_get();
   return 1;
}

void
_battery_udev_stop(void)
{
   Ac_Adapter *ac;
   Battery *bat;

   if (battery_config->batwatch)
     eeze_udev_watch_del(battery_config->batwatch);
   if (battery_config->acwatch)
     eeze_udev_watch_del(battery_config->acwatch);

   EINA_LIST_FREE(device_ac_adapters, ac)
     {
        free(ac);
     }
   EINA_LIST_FREE(device_batteries, bat)
     {
        eina_stringshare_del(bat->udi);
        eina_stringshare_del(bat->technology);
        eina_stringshare_del(bat->model);
        eina_stringshare_del(bat->vendor);
        ecore_timer_del(bat->timer);
        _battery_history_close(bat);
        free(bat);
     }
}

static void
_battery_udev_event_battery(const char *syspath, Eeze_Udev_Event event, void *data, Eeze_Udev_Watch *watch EINA_UNUSED)
{
   if ((event & EEZE_UDEV_EVENT_ADD) ||
       (event & EEZE_UDEV_EVENT_ONLINE))
     _battery_udev_battery_add(syspath);
   else if ((event & EEZE_UDEV_EVENT_REMOVE) ||
            (event & EEZE_UDEV_EVENT_OFFLINE))
     _battery_udev_battery_del(syspath);
   else /* must be change */
     _battery_udev_battery_update(syspath, data);
}

static void
_battery_udev_event_ac(const char *syspath, Eeze_Udev_Event event, void *data, Eeze_Udev_Watch *watch EINA_UNUSED)
{
   if ((event & EEZE_UDEV_EVENT_ADD) ||
       (event & EEZE_UDEV_EVENT_ONLINE))
     _battery_udev_ac_add(syspath);
   else if ((event & EEZE_UDEV_EVENT_REMOVE) ||
            (event & EEZE_UDEV_EVENT_OFFLINE))
     _battery_udev_ac_del(syspath);
   else /* must be change */
     _battery_udev_ac_update(syspath, data);
}

static void
_battery_udev_battery_add(const char *syspath)
{
   Battery *bat;
   const char *type, *test;
   double full_design = 0.0;
   double voltage_min_design = 0.0;
   double full = 0.0;
   Config_Battery *cb;
   Eina_List *l;

   if ((bat = _battery_battery_find(syspath)))
     {
        eina_stringshare_del(syspath);
        _battery_udev_battery_update(NULL, bat);
        return;
     }
   type = eeze_udev_syspath_get_property(syspath, "POWER_SUPPLY_TYPE");
   if (type)
     {
        if ((!strcmp(type, "USB")) || (!strcmp(type, "Mains")))
          {
             _battery_udev_ac_add(syspath);
             eina_stringshare_del(type);
             return;
          }
        if (!!strcmp(type, "Battery"))
          {
             eina_stringshare_del(type);
             return;
          }
        eina_stringshare_del(type);
     }
   // filter out dummy batteries with no design and no full charge level
   test = eeze_udev_syspath_get_property(syspath, "POWER_SUPPLY_ENERGY_FULL_DESIGN");
   if (!test)
     test = eeze_udev_syspath_get_property(syspath, "POWER_SUPPLY_CHARGE_FULL_DESIGN");
   if (test)
    {
      full_design = strtod(test, NULL);
      eina_stringshare_del(test);
    }

   test = eeze_udev_syspath_get_property(syspath, "POWER_SUPPLY_ENERGY_FULL");
   if (!test)
    {
      test = eeze_udev_syspath_get_property(syspath, "POWER_SUPPLY_VOLTAGE_MIN_DESIGN");
      if (test)
        voltage_min_design = strtod(test, NULL);
      test = eeze_udev_syspath_get_property(syspath, "POWER_SUPPLY_CHARGE_FULL");
    }
   if (test)
    {
      full = strtod(test, NULL);
      eina_stringshare_del(test);
    }

   if ((eina_dbl_exact(full_design, 0)) &&
       (eina_dbl_exact(full, 0)))
    { // ignore this battery - no full and no full design
      return;
    }

   if (!(bat = E_NEW(Battery, 1)))
     {
        eina_stringshare_del(syspath);
        return;
     }
   bat->design_voltage = voltage_min_design;
   bat->last_update = ecore_time_get();
   bat->udi = eina_stringshare_add(syspath);
   bat->timer = ecore_timer_add(10.0, _battery_udev_battery_update_poll, bat);

   test = eeze_udev_syspath_get_sysattr(syspath, "charge_control_end_threshold");
   if (!test) bat->charge_lim = -1;
   else
    {
      bat->charge_lim = atoi(test);
      // find a matching battery config entry and retore from there
      EINA_LIST_FOREACH(battery_config->battery_configs, l, cb)
        {
          if ((cb->udi) && (bat->udi) && (!strcmp(cb->udi, bat->udi)))
            {
              if (cb->charge_limit < 10) cb->charge_limit = 10;
              else if (cb->charge_limit > 100) cb->charge_limit = 100;
              bat->charge_lim = cb->charge_limit;
              e_system_send("battery-lim-set", "%s %i\n", bat->udi, bat->charge_lim);
            }
        }
    }

   device_batteries = eina_list_append(device_batteries, bat);
   _battery_udev_battery_update(syspath, bat);
}

static void
_battery_udev_ac_add(const char *syspath)
{
   Ac_Adapter *ac;

   if ((ac = _battery_ac_adapter_find(syspath)))
     {
        eina_stringshare_del(syspath);
        _battery_udev_ac_update(NULL, ac);
        return;
     }

   if (!(ac = E_NEW(Ac_Adapter, 1)))
     {
        eina_stringshare_del(syspath);
        return;
     }
   ac->udi = eina_stringshare_add(syspath);
   device_ac_adapters = eina_list_append(device_ac_adapters, ac);
   _battery_udev_ac_update(syspath, ac);
}

static void
_battery_udev_battery_del(const char *syspath)
{
   Battery *bat;

   if (!(bat = _battery_battery_find(syspath)))
     {
        eina_stringshare_del(syspath);
        _battery_device_update();
        return;
     }

   device_batteries = eina_list_remove(device_batteries, bat);
   eina_stringshare_del(bat->udi);
   eina_stringshare_del(bat->technology);
   eina_stringshare_del(bat->model);
   eina_stringshare_del(bat->vendor);
   ecore_timer_del(bat->timer);
   free(bat);
}

static void
_battery_udev_ac_del(const char *syspath)
{
   Ac_Adapter *ac;

   if (!(ac = _battery_ac_adapter_find(syspath)))
     {
        eina_stringshare_del(syspath);
        _battery_device_update();
        return;
     }

   device_ac_adapters = eina_list_remove(device_ac_adapters, ac);
   eina_stringshare_del(ac->udi);
   free(ac);
}

static Eina_Bool
_battery_udev_battery_update_poll(void *data)
{
   _battery_udev_battery_update(NULL, data);

   return EINA_TRUE;
}

# define GET_NUM(TYPE, VALUE, PROP) \
  do                                                                                        \
    {                                                                                       \
      test = eeze_udev_syspath_get_property(TYPE->udi, #PROP);                              \
      if (test)                                                                             \
        {                                                                                   \
           TYPE->VALUE = strtod(test, NULL);                                                \
           eina_stringshare_del(test);                                                      \
        }                                                                                   \
    }                                                                                       \
  while (0)

# define GET_STR(TYPE, VALUE, PROP) TYPE->VALUE = eeze_udev_syspath_get_property(TYPE->udi, #PROP)

static void
_battery_udev_battery_update(const char *syspath, Battery *bat)
{
   int pcharging;
   const char *test;
   double t, charge, voltage_now;

   if (!bat)
     {
        if (!(bat = _battery_battery_find(syspath)))
          {
             _battery_udev_battery_add(syspath);
             return;
          }
     }
   GET_NUM(bat, present, POWER_SUPPLY_PRESENT);
   if (!bat->got_prop) /* only need to get these once */
     {
        GET_STR(bat, technology, POWER_SUPPLY_TECHNOLOGY);
        GET_STR(bat, model, POWER_SUPPLY_MODEL_NAME);
        GET_STR(bat, vendor, POWER_SUPPLY_MANUFACTURER);
        GET_NUM(bat, design_charge, POWER_SUPPLY_ENERGY_FULL_DESIGN);
        if (eina_dbl_exact(bat->design_charge, 0))
         {
           GET_NUM(bat, design_charge, POWER_SUPPLY_CHARGE_FULL_DESIGN);
           if (bat->design_voltage > 0.0)
             {
               bat->design_charge = bat->design_charge * bat->design_voltage / 1000000.0;
               bat->is_micro_watts = EINA_TRUE;
             }
         }
     }
   bat->power_now = 0;
   voltage_now = bat->design_voltage;
   test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_VOLTAGE_NOW");
   if (test)
     {
        voltage_now = (double)strtod(test, NULL);
        eina_stringshare_del(test);
     }
   GET_NUM(bat, power_now, POWER_SUPPLY_POWER_NOW);
   if (eina_dbl_exact(bat->power_now, 0))
     {
       test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_CURRENT_NOW");
       if (test)
         {
           double current_now = strtod(test, NULL);

           eina_stringshare_del(test);
           if (voltage_now > 0.0)
             {
               bat->power_now = current_now * voltage_now / 1000000.0;
             }
         }
     }
   GET_NUM(bat, last_full_charge, POWER_SUPPLY_ENERGY_FULL);
   if (eina_dbl_exact(bat->last_full_charge, 0))
     {
       GET_NUM(bat, last_full_charge, POWER_SUPPLY_CHARGE_FULL);
       if (bat->design_voltage > 0.0)
         {
           bat->last_full_charge = bat->last_full_charge * bat->design_voltage / 1000000.0;
           bat->is_micro_watts = EINA_TRUE;
         }
     }
   else
     bat->is_micro_watts = EINA_TRUE;
   pcharging = bat->charging;
   test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_STATUS");
   if (test)
     {
        if (!strcmp(test, "Charging"))
          bat->charging = 1;
        else if ((!strcmp(test, "Unknown")) && (bat->charge_rate > 0))
          bat->charging = 1;
        else
          bat->charging = 0;
        eina_stringshare_del(test);
     }
   else
     bat->charging = 0;

   test = eeze_udev_syspath_get_sysattr(bat->udi, "charge_control_end_threshold");
   if (!test) bat->charge_lim = -1;
   else bat->charge_lim = atoi(test);

   test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_ENERGY_NOW");
   if (!test)
     test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_CHARGE_NOW");
   if (!test)
     {
        if (eina_dbl_exact(bat->last_full_charge, 0))
          {
             bat->last_full_charge = 10000;
             bat->design_charge = 10000;
          }
        test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_CAPACITY");
     }
   if (test)
     {
        double charge_rate = 0;
        double last_charge_rate;
        double td;

        last_charge_rate = bat->charge_rate;
        charge = strtod(test, NULL);
        if (bat->design_voltage > 0.0)
           charge = charge * bat->design_voltage / 1000000.0;
        eina_stringshare_del(test);
        t = ecore_time_get();
        td = t - bat->last_update;
        if (td <= 0.0) td = 0.001;
        if ((bat->is_micro_watts) && (!eina_dbl_exact(bat->power_now, 0)))
          {
            if (!bat->charging) charge_rate = -bat->power_now / 3600.0;
            else                charge_rate = bat->power_now / 3600.0;
          }
        else if ((bat->got_prop) &&
            (!eina_dbl_exact(charge, bat->current_charge)) &&
            (!eina_dbl_exact(bat->current_charge, 0)))
          charge_rate =
            ((charge - bat->current_charge) / td);
        if ((!eina_dbl_exact(charge_rate, 0)) ||
            eina_dbl_exact(bat->last_update, 0) ||
            eina_dbl_exact(bat->current_charge, 0))
          {
             bat->last_update = t;
             bat->current_charge = charge;
             bat->charge_rate = charge_rate;
          }
        bat->percent = (10000.0 * bat->current_charge) / bat->last_full_charge;
        if (bat->got_prop)
          {
             if ((!((bat->is_micro_watts) && (!eina_dbl_exact(bat->power_now, 0)))) &&
                 ((pcharging == bat->charging)))
               charge_rate = (charge_rate + last_charge_rate) / 2.0;
             if ((!bat->charging) && (!eina_dbl_exact(charge_rate, 0)))
               {
                  if (battery_config->fuzzy && (battery_config->fuzzcount <= 10) && (bat->time_left > 0))                    bat->time_left = (((0 - bat->current_charge) / charge_rate) + bat->time_left) / 2;
                  else
                    bat->time_left = (0 - bat->current_charge) / charge_rate;
                  bat->time_full = -1;
               }
             else if (!eina_dbl_exact(charge_rate, 0))
               {
                  if (battery_config->fuzzy && (++battery_config->fuzzcount <= 10) && (bat->time_full > 0))
                    bat->time_full = (((bat->last_full_charge - bat->current_charge) / charge_rate) + bat->time_full) / 2;
                  else
                    bat->time_full = (bat->last_full_charge - bat->current_charge) / charge_rate;
                  bat->time_left = -1;
               }
            if (pcharging == bat->charging) bat->charge_rate = charge_rate;
          }
        else
          {
             bat->time_full = -1;
             bat->time_left = -1;
          }
     }
   if (battery_config->fuzzcount > 10) battery_config->fuzzcount = 0;
   if (bat->got_prop)
     _battery_device_update();
   bat->got_prop = 1;
}

static void
_battery_udev_ac_update(const char *syspath, Ac_Adapter *ac)
{
   const char *test;

   if (!ac)
     {
        if (!(ac = _battery_ac_adapter_find(syspath)))
          {
             _battery_udev_ac_add(syspath);
             return;
          }
     }

   GET_NUM(ac, present, POWER_SUPPLY_ONLINE);
   /* yes, it's really that simple. */

   _battery_device_update();
}
#endif
