#include "batman.h"

static void _batman_udev_event_battery(const char *syspath, Eeze_Udev_Event event, void *data, Eeze_Udev_Watch *watch);
static void _batman_udev_event_ac(const char *syspath, Eeze_Udev_Event event, void *data, Eeze_Udev_Watch *watch);
static void _batman_udev_battery_add(const char *syspath, Instance *inst);
static void _batman_udev_ac_add(const char *syspath, Instance *inst);
static void _batman_udev_battery_del(const char *syspath, Instance *inst);
static void _batman_udev_ac_del(const char *syspath, Instance *inst);
static Eina_Bool _batman_udev_battery_update_poll(void *data);
static void _batman_udev_battery_update(const char *syspath, Battery *bat, Instance *inst);
static void _batman_udev_ac_update(const char *syspath, Ac_Adapter *ac, Instance *inst);

extern Eina_List *batman_device_batteries;
extern Eina_List *batman_device_ac_adapters;
extern double batman_init_time;

int
_batman_udev_start(Instance *inst)
{
   Eina_List *devices;
   const char *dev;

   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_BAT, NULL);
   EINA_LIST_FREE(devices, dev)
     _batman_udev_battery_add(dev, inst);

   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_AC, NULL);
   EINA_LIST_FREE(devices, dev)
     _batman_udev_ac_add(dev, inst);

   if (!inst->cfg->batman.batwatch)
     inst->cfg->batman.batwatch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_BAT, EEZE_UDEV_EVENT_NONE, _batman_udev_event_battery, inst);
   if (!inst->cfg->batman.acwatch)
     inst->cfg->batman.acwatch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_AC, EEZE_UDEV_EVENT_NONE, _batman_udev_event_ac, inst);

   batman_init_time = ecore_time_get();
   return 1;
}

void
_batman_udev_stop(Instance *inst)
{
   Ac_Adapter *ac;
   Battery *bat;

   if (inst->cfg->batman.batwatch)
     E_FREE_FUNC(inst->cfg->batman.batwatch, eeze_udev_watch_del);
   if (inst->cfg->batman.acwatch)
     E_FREE_FUNC(inst->cfg->batman.acwatch, eeze_udev_watch_del);

   EINA_LIST_FREE(batman_device_ac_adapters, ac)
     {
        E_FREE_FUNC(ac, free);
     }
   EINA_LIST_FREE(batman_device_batteries, bat)
     {
	eina_stringshare_del(bat->udi);
	eina_stringshare_del(bat->technology);
	eina_stringshare_del(bat->model);
	eina_stringshare_del(bat->vendor);
        E_FREE_FUNC(bat->poll, ecore_poller_del);
        E_FREE_FUNC(bat, free);
     }
}

static void
_batman_udev_event_battery(const char *syspath, Eeze_Udev_Event event, void *data, Eeze_Udev_Watch *watch EINA_UNUSED)
{
   Instance *inst = data;

   if ((event & EEZE_UDEV_EVENT_ADD) ||
       (event & EEZE_UDEV_EVENT_ONLINE))
     _batman_udev_battery_add(syspath, inst);
   else if ((event & EEZE_UDEV_EVENT_REMOVE) ||
            (event & EEZE_UDEV_EVENT_OFFLINE))
     _batman_udev_battery_del(syspath, inst);
   else /* must be change */
     _batman_udev_battery_update(syspath, NULL, inst);
}

static void
_batman_udev_event_ac(const char *syspath, Eeze_Udev_Event event, void *data, Eeze_Udev_Watch *watch EINA_UNUSED)
{
   Instance *inst = data;

   if ((event & EEZE_UDEV_EVENT_ADD) ||
       (event & EEZE_UDEV_EVENT_ONLINE))
     _batman_udev_ac_add(syspath, inst);
   else if ((event & EEZE_UDEV_EVENT_REMOVE) ||
            (event & EEZE_UDEV_EVENT_OFFLINE))
     _batman_udev_ac_del(syspath, inst);
   else /* must be change */
     _batman_udev_ac_update(syspath, NULL, inst);
}

static void
_batman_udev_battery_add(const char *syspath, Instance *inst)
{
   Battery *bat;
   Eina_List *batteries = _batman_battery_find(syspath), *l;
   Eina_Bool exists = EINA_FALSE;

   if (eina_list_count(batteries))
     {
        EINA_LIST_FOREACH(batteries, l, bat)
          {
             if (inst == bat->inst)
               {
                  _batman_udev_battery_update(NULL, bat, inst);
                  exists = EINA_TRUE;
               }
          }
        if (exists)
          {
             eina_stringshare_del(syspath);
             eina_list_free(batteries);
             return;
          }
     }

   if (!(bat = E_NEW(Battery, 1)))
     {
        eina_stringshare_del(syspath);
        return;
     }
   bat->inst = inst;
   bat->last_update = ecore_time_get();
   bat->udi = eina_stringshare_add(syspath);
   bat->poll = ecore_poller_add(ECORE_POLLER_CORE, 
				bat->inst->cfg->batman.poll_interval, 
				_batman_udev_battery_update_poll, bat);
   batman_device_batteries = eina_list_append(batman_device_batteries, bat);
   _batman_udev_battery_update(syspath, bat, inst);
}

static void
_batman_udev_ac_add(const char *syspath, Instance *inst)
{
   Ac_Adapter *ac;
   Eina_List *adapters = _batman_ac_adapter_find(syspath), *l;
   Eina_Bool exists = EINA_FALSE;

   if (eina_list_count(adapters))
     {
        EINA_LIST_FOREACH(adapters, l, ac)
          {
             if (inst == ac->inst)
               {
                  _batman_udev_ac_update(NULL, ac, inst);
                  exists = EINA_TRUE;
               }
          }
        if (exists)
          {
             eina_stringshare_del(syspath);
             eina_list_free(adapters);
             return;
          }
     }

   if (!(ac = E_NEW(Ac_Adapter, 1)))
     {
        eina_stringshare_del(syspath);
        return;
     }
   ac->inst = inst;
   ac->udi = eina_stringshare_add(syspath);
   batman_device_ac_adapters = eina_list_append(batman_device_ac_adapters, ac);
   _batman_udev_ac_update(syspath, ac, inst);
}

static void
_batman_udev_battery_del(const char *syspath, Instance *inst)
{
   Battery *bat;
   Eina_List *batteries = _batman_battery_find(syspath), *l;
   if (!eina_list_count(batteries))
     {
        eina_stringshare_del(syspath);
        return;
     }

   EINA_LIST_FOREACH(batman_device_batteries, l, bat)
     {
        if (inst == bat->inst)
          {
             batman_device_batteries = eina_list_remove_list(batman_device_batteries, l);
             eina_stringshare_del(bat->udi);
             eina_stringshare_del(bat->technology);
             eina_stringshare_del(bat->model);
             eina_stringshare_del(bat->vendor);
             E_FREE_FUNC(bat->poll, ecore_poller_del);
             E_FREE_FUNC(bat, free);
          }
     }
   eina_stringshare_del(syspath);
   eina_list_free(batteries);
}

static void
_batman_udev_ac_del(const char *syspath, Instance *inst)
{
   Ac_Adapter *ac;
   Eina_List *adapters = _batman_ac_adapter_find(syspath), *l;

   if (!eina_list_count(adapters))
     {
        eina_stringshare_del(syspath);  
        return;
     }
   EINA_LIST_FOREACH(batman_device_ac_adapters, l, ac)
     {
        if (inst == ac->inst)
          {
             batman_device_ac_adapters = eina_list_remove_list(batman_device_ac_adapters, l);
             eina_stringshare_del(ac->udi);
             E_FREE_FUNC(ac, free);
          }
     }
   eina_stringshare_del(syspath);
   eina_list_free(adapters);
}

static Eina_Bool 
_batman_udev_battery_update_poll(void *data)
{
   Battery *bat = data;

   _batman_udev_battery_update(NULL, bat, bat->inst);

   return EINA_TRUE;
}

#define GET_NUM(TYPE, VALUE, PROP) \
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

#define GET_STR(TYPE, VALUE, PROP) TYPE->VALUE = eeze_udev_syspath_get_property(TYPE->udi, #PROP)

static void
_batman_udev_battery_update(const char *syspath, Battery *bat, Instance *inst)
{
   const char *test;
   double t, charge;

   if (!bat)
     {
        _batman_udev_battery_add(syspath, inst);
        return;
     }
   /* update the poller interval */
   ecore_poller_poller_interval_set(bat->poll, bat->inst->cfg->batman.poll_interval);

   GET_NUM(bat, present, POWER_SUPPLY_PRESENT);
   if (!bat->got_prop) /* only need to get these once */
     {
        GET_STR(bat, technology, POWER_SUPPLY_TECHNOLOGY);
        GET_STR(bat, model, POWER_SUPPLY_MODEL_NAME);
        GET_STR(bat, vendor, POWER_SUPPLY_MANUFACTURER);
        GET_NUM(bat, design_charge, POWER_SUPPLY_ENERGY_FULL_DESIGN);
        if (eina_dbl_exact(bat->design_charge, 0))
          GET_NUM(bat, design_charge, POWER_SUPPLY_CHARGE_FULL_DESIGN);
     }
   GET_NUM(bat, last_full_charge, POWER_SUPPLY_ENERGY_FULL);
   if (eina_dbl_exact(bat->last_full_charge, 0))
     GET_NUM(bat, last_full_charge, POWER_SUPPLY_CHARGE_FULL);
   test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_ENERGY_NOW");
   if (!test)
     test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_CHARGE_NOW");
   if (test)
     {
        double charge_rate = 0;

        charge = strtod(test, NULL);
        eina_stringshare_del(test);
        t = ecore_time_get();
        if ((bat->got_prop) && (!eina_dbl_exact(charge, bat->current_charge)) && (!eina_dbl_exact(bat->current_charge, 0)))
          charge_rate = ((charge - bat->current_charge) / (t - bat->last_update));
        if ((!eina_dbl_exact(charge_rate, 0)) || eina_dbl_exact(bat->last_update, 0) || eina_dbl_exact(bat->current_charge, 0))
	  {
	    bat->last_update = t;
	    bat->current_charge = charge;
	    bat->charge_rate = charge_rate;
	  }
        bat->percent = 100 * (bat->current_charge / bat->last_full_charge);
        if (bat->got_prop)
          {
             if (bat->charge_rate > 0)
               {
                  if (bat->inst->cfg->batman.fuzzy && (++bat->inst->cfg->batman.fuzzcount <= 10) && (bat->time_full > 0))
                    bat->time_full = (((bat->last_full_charge - bat->current_charge) / bat->charge_rate) + bat->time_full) / 2;
                  else
                    bat->time_full = (bat->last_full_charge - bat->current_charge) / bat->charge_rate;
                  bat->time_left = -1;
               }
             else
               {
                  if (bat->inst->cfg->batman.fuzzy && (bat->inst->cfg->batman.fuzzcount <= 10) && (bat->time_left > 0))
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
     }
   if (bat->inst->cfg->batman.fuzzcount > 10) bat->inst->cfg->batman.fuzzcount = 0;
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
   if (bat->got_prop)
     _batman_device_update(bat->inst);
   bat->got_prop = 1;
}

static void
_batman_udev_ac_update(const char *syspath, Ac_Adapter *ac, Instance *inst)
{
   const char *test;

   if (!ac)
     {
         _batman_udev_ac_add(syspath, inst);
         return;
     }

   GET_NUM(ac, present, POWER_SUPPLY_ONLINE);
   /* yes, it's really that simple. */

   _batman_device_update(inst);
}

