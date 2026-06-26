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

extern Eina_List *device_batteries;
extern Eina_List *device_ac_adapters;
extern double init_time;

static Ecore_Timer *_poll_timer = NULL;
static Ecore_Thread *_poll_thread = NULL;
static Ecore_Pipe *_poll_pipe = NULL;
static Eina_Lock _poll_lock;

typedef struct
{
  const char   *udi;

  const char   *technology;
  const char   *model;
  const char   *vendor;
  double        power_now;
  double        design_charge;
  double        last_full_charge;
  double        charge;
  int           charge_lim;
  Eina_Bool     present E_BITFIELD;
  Eina_Bool     charging E_BITFIELD;
  Eina_Bool     is_micro_watts E_BITFIELD;

  Eina_Bool     is_bat E_BITFIELD;
  Eina_Bool     is_ac E_BITFIELD;
} Bat_Poll_Info;

# define GET_NUM(TYPE, VALUE, PROP) \
  do                                                                                        \
    {                                                                                       \
      const char *_test = eeze_udev_syspath_get_property(TYPE->udi, #PROP);                              \
      if (_test)                                                                             \
        {                                                                                   \
           TYPE->VALUE = strtod(_test, NULL);                                                \
           eina_stringshare_del(_test);                                                      \
        }                                                                                   \
    }                                                                                       \
  while (0)
# define GET_STR(TYPE, VALUE, PROP) TYPE->VALUE = eeze_udev_syspath_get_property(TYPE->udi, #PROP)

//////////////////////////////////////////////////////////////////////////////
////////// in thread
static void
_cb_poll_pipe(void *data EINA_UNUSED, void *buf EINA_UNUSED, unsigned int bytes EINA_UNUSED)
{ // called in thread when data arrives from main loop in thread loop
  // don't care about the data - it just woke us up
}

static void
_cb_poll_thread(void *data EINA_UNUSED, Ecore_Thread *thread)
{ // thread loop function sitting there waiting on input
  while (!ecore_thread_check(thread))
    {
      if (ecore_pipe_wait(_poll_pipe, 1, 3600.0) < 1)
        { // timeout waiting for msg from main loop
          L("BAT: timeout waiting for mainloop poll message in udev thread");
        }
      else
        { // poll bat data and send results back to mainloop
          Eina_List *l;
          Battery *bat;
          Ac_Adapter *ac;

          eina_lock_take(&_poll_lock);
          // we only read bat and ac - don't write
          EINA_LIST_FOREACH(device_batteries, l, bat)
            {
              Bat_Poll_Info *bi = calloc(1, sizeof(Bat_Poll_Info));

              if (bi)
                {
                  const char *test;
                  double voltage_now;

                  bi->udi = eina_stringshare_add(bat->udi);
                  bi->is_bat = EINA_TRUE;
                  GET_NUM(bi, present, POWER_SUPPLY_PRESENT);
                  if (!bat->got_prop) /* only need to get these once */
                    {
                      GET_STR(bi, technology, POWER_SUPPLY_TECHNOLOGY);
                      GET_STR(bi, model, POWER_SUPPLY_MODEL_NAME);
                      GET_STR(bi, vendor, POWER_SUPPLY_MANUFACTURER);
                      GET_NUM(bi, design_charge, POWER_SUPPLY_ENERGY_FULL_DESIGN);
                      if (eina_dbl_exact(bi->design_charge, 0))
                        {
                          GET_NUM(bi, design_charge, POWER_SUPPLY_CHARGE_FULL_DESIGN);
                          if (bat->design_voltage > 0.0)
                            {
                              bi->design_charge = bi->design_charge * bat->design_voltage / 1000000.0;
                              bi->is_micro_watts = EINA_TRUE;
                            }
                        }
                    }
                  voltage_now = bat->design_voltage;
                  test = eeze_udev_syspath_get_property(bi->udi, "POWER_SUPPLY_VOLTAGE_NOW");
                  if (test)
                    {
                      voltage_now = (double)strtod(test, NULL);
                      eina_stringshare_del(test);
                    }
                  GET_NUM(bi, power_now, POWER_SUPPLY_POWER_NOW);
                  if (eina_dbl_exact(bi->power_now, 0))
                    {
                      test = eeze_udev_syspath_get_property(bi->udi, "POWER_SUPPLY_CURRENT_NOW");
                      if (test)
                        {
                          double current_now = strtod(test, NULL);

                          eina_stringshare_del(test);
                          if (voltage_now > 0.0)
                            bi->power_now = current_now * voltage_now / 1000000.0;
                        }
                    }
                  GET_NUM(bi, last_full_charge, POWER_SUPPLY_ENERGY_FULL);
                  if (eina_dbl_exact(bi->last_full_charge, 0))
                    {
                      GET_NUM(bi, last_full_charge, POWER_SUPPLY_CHARGE_FULL);
                      if (bat->design_voltage > 0.0)
                        {
                          bi->last_full_charge = bi->last_full_charge * bat->design_voltage / 1000000.0;
                          bi->is_micro_watts = EINA_TRUE;
                        }
                    }
                  else bi->is_micro_watts = EINA_TRUE;
                  test = eeze_udev_syspath_get_property(bi->udi, "POWER_SUPPLY_STATUS");
                  if (test)
                    {
                      if (!strcmp(test, "Charging"))
                        bi->charging = 1;
                      else if ((!strcmp(test, "Unknown")) && (bat->charge_rate > 0))
                        bi->charging = 1;
                      else
                        bi->charging = 0;
                      eina_stringshare_del(test);
                    }
                  else bi->charging = 0;

                  test = eeze_udev_syspath_get_sysattr(bi->udi, "charge_control_end_threshold");
                  if (!test) bi->charge_lim = -1;
                  else bi->charge_lim = atoi(test);

                  bi->charge = -1.0;
                  test = eeze_udev_syspath_get_property(bi->udi, "POWER_SUPPLY_ENERGY_NOW");
                  if (!test)
                    test = eeze_udev_syspath_get_property(bi->udi, "POWER_SUPPLY_CHARGE_NOW");
                  if (!test)
                    {
                      if (eina_dbl_exact(bi->last_full_charge, 0))
                        {
                          bi->last_full_charge = 10000;
                          bi->design_charge = 10000;
                        }
                      test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_CAPACITY");
                    }
                  if (test) bi->charge = strtod(test, NULL);
                  ecore_thread_feedback(thread, bi); // send poll info to mainloop
                }
            }
          EINA_LIST_FOREACH(device_ac_adapters, l, ac)
            {
              Bat_Poll_Info *bi = calloc(1, sizeof(Bat_Poll_Info));

              if (bi)
                {
                  bi->udi = eina_stringshare_add(ac->udi);
                  bi->is_ac = EINA_TRUE;
                  GET_NUM(bi, present, POWER_SUPPLY_ONLINE);
                  ecore_thread_feedback(thread, bi); // send poll info to mainloop
                }
            }
          eina_lock_release(&_poll_lock);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
////////// in main loop
static void
_poll_wakeup(void)
{ // in main loop
  int dummy = 1;
  if (_poll_pipe) ecore_pipe_write(_poll_pipe, &dummy, sizeof(dummy));
}

static void
_devices_timer_clean(void)
{ // in main loop
  if ((!device_batteries) && (!device_ac_adapters))
    {
      if (_poll_timer)
        {
          ecore_timer_del(_poll_timer);
          _poll_timer = NULL;
        }
    }
}

static void
_devices_timer_ensure(void)
{ // in main loop
  if (_poll_timer) return;
  _poll_timer = ecore_timer_add(10.0, _battery_udev_battery_update_poll, NULL);
}

static void
_cb_poll_thread_reply(void *data EINA_UNUSED, Ecore_Thread *thread EINA_UNUSED, void *msg)
{ // in main loop
  // feedback reply from thread loop called in main loop
  Bat_Poll_Info *bi = msg;

  eina_lock_take(&_poll_lock);
  if (bi->is_bat)
    {
      Battery *bat = _battery_battery_find(bi->udi);

      if (bat)
        {
          int pcharging = bat->charging;

          if (!bat->got_prop)
            {
#define REPSTR(_x) if (bi->_x) eina_stringshare_replace(&(bat->_x), bi->_x);
              REPSTR(technology);
              REPSTR(model);
              REPSTR(vendor);
              bat->design_charge = bi->design_charge;
            }
          bat->power_now = bi->power_now;
          bat->last_full_charge = bi->last_full_charge;
          bat->charge_lim = bi->charge_lim;
          bat->present = bi->present;
          bat->charging = bi->charging;
          bat->is_micro_watts = bi->is_micro_watts;
          if (bi->charge >= 0.0)
            {
              double charge;
              double charge_rate = 0;
              double last_charge_rate;
              double t, td;

              charge = bi->charge;
              last_charge_rate = bat->charge_rate;
              if (bat->design_voltage > 0.0)
                charge = charge * bat->design_voltage / 1000000.0;
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
                charge_rate = ((charge - bat->current_charge) / td);
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
                      if (battery_config->fuzzy && (battery_config->fuzzcount <= 10) && (bat->time_left > 0))
                        bat->time_left = (((0 - bat->current_charge) / charge_rate) + bat->time_left) / 2;
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
                  if (pcharging == bat->charging)
                    bat->charge_rate = charge_rate;
                }
              else
                {
                  bat->time_full = -1;
                  bat->time_left = -1;
                }
            }
          if (battery_config->fuzzcount > 10) battery_config->fuzzcount = 0;
          if (bat->got_prop) _battery_device_update();
          bat->got_prop = EINA_TRUE;
        }
    }
  else if (bi->is_ac)
    {
      Ac_Adapter *ac = _battery_ac_adapter_find(bi->udi);

      if (ac)
        {
          ac->present = bi->present;
          _battery_device_update();
        }
    }
  eina_lock_release(&_poll_lock);
  eina_stringshare_replace(&(bi->udi), NULL);
  eina_stringshare_replace(&(bi->technology), NULL);
  eina_stringshare_replace(&(bi->vendor), NULL);
  eina_stringshare_replace(&(bi->model), NULL);
  free(bi);
}

static void
_cb_poll_thread_end(void *data EINA_UNUSED, Ecore_Thread *thread EINA_UNUSED)
{ // in main loop
  // when feedback thread exits and finishes inn main loop
  ecore_pipe_del(_poll_pipe);
  _poll_pipe = NULL;
  _poll_thread = NULL;
  eina_lock_free(&_poll_lock);
}

int
_battery_udev_start(void)
{ // in main loop
  Eina_List *devices;
  const char *dev;

  devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_BAT, NULL);
  EINA_LIST_FREE(devices, dev)
    {
      _battery_udev_battery_add(dev);
      eina_stringshare_del(dev);
    }

  devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_AC, NULL);
  EINA_LIST_FREE(devices, dev)
    {
      _battery_udev_ac_add(dev);
      eina_stringshare_del(dev);
    }

  if (!battery_config->batwatch)
    battery_config->batwatch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_BAT, EEZE_UDEV_EVENT_NONE, _battery_udev_event_battery, NULL);
  if (!battery_config->acwatch)
    battery_config->acwatch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_AC, EEZE_UDEV_EVENT_NONE, _battery_udev_event_ac, NULL);

  init_time = ecore_time_get();
  eina_lock_new(&_poll_lock);
  _poll_pipe = ecore_pipe_add(_cb_poll_pipe, NULL);
  ecore_pipe_freeze(_poll_pipe);
  _poll_thread = ecore_thread_feedback_run
     (_cb_poll_thread, _cb_poll_thread_reply, _cb_poll_thread_end,
      NULL, NULL, EINA_TRUE);
  _poll_wakeup();
  return 1;
}

void
_battery_udev_stop(void)
{ // in main loop
  Ac_Adapter *ac;
  Battery *bat;

  if (battery_config->batwatch)
  eeze_udev_watch_del(battery_config->batwatch);
  if (battery_config->acwatch)
  eeze_udev_watch_del(battery_config->acwatch);

  eina_lock_take(&_poll_lock);
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
      _battery_history_close(bat);
      free(bat);
    }
  _devices_timer_clean();
  eina_lock_release(&_poll_lock);
  if (_poll_thread) ecore_thread_cancel(_poll_thread);
  _poll_wakeup();
}

static void
_battery_udev_event_battery(const char *syspath, Eeze_Udev_Event event, void *data EINA_UNUSED, Eeze_Udev_Watch *watch EINA_UNUSED)
{ // in main loop
  eina_lock_take(&_poll_lock);
  if ((event & EEZE_UDEV_EVENT_ADD) ||
      (event & EEZE_UDEV_EVENT_ONLINE))
    _battery_udev_battery_add(syspath);
  else if ((event & EEZE_UDEV_EVENT_REMOVE) ||
           (event & EEZE_UDEV_EVENT_OFFLINE))
    _battery_udev_battery_del(syspath);
  else /* must be change */
    _poll_wakeup();
  eina_lock_release(&_poll_lock);
  eina_stringshare_del(syspath); // yes - eeze wants us to do this
}

static void
_battery_udev_event_ac(const char *syspath, Eeze_Udev_Event event, void *data EINA_UNUSED, Eeze_Udev_Watch *watch EINA_UNUSED)
{ // in main loop
  eina_lock_take(&_poll_lock);
  if ((event & EEZE_UDEV_EVENT_ADD) ||
      (event & EEZE_UDEV_EVENT_ONLINE))
    _battery_udev_ac_add(syspath);
  else if ((event & EEZE_UDEV_EVENT_REMOVE) ||
           (event & EEZE_UDEV_EVENT_OFFLINE))
    _battery_udev_ac_del(syspath);
  else /* must be change */
    _poll_wakeup();
  eina_lock_release(&_poll_lock);
  eina_stringshare_del(syspath); // yes - eeze wants us to do this
}

static void
_battery_udev_battery_add(const char *syspath)
{ // in main loop
  Battery *bat;
  const char *type, *test;
  double full_design = 0.0;
  double voltage_min_design = 0.0;
  double full = 0.0;
  Config_Battery *cb;
  Eina_List *l;

  if ((bat = _battery_battery_find(syspath)))
    {
      _poll_wakeup();
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

  // ignore this battery - no full and no full design
  if ((eina_dbl_exact(full_design, 0)) && (eina_dbl_exact(full, 0))) return;

  if (!(bat = E_NEW(Battery, 1))) return;
  bat->design_voltage = voltage_min_design;
  bat->last_update = ecore_time_get();
  bat->udi = eina_stringshare_add(syspath);
  _devices_timer_ensure();

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
  _poll_wakeup();
}

static void
_battery_udev_ac_add(const char *syspath)
{ // in main loop
  Ac_Adapter *ac;

  if ((ac = _battery_ac_adapter_find(syspath)))
    {
      _poll_wakeup();
      return;
    }
  if (!(ac = E_NEW(Ac_Adapter, 1))) return;
  ac->udi = eina_stringshare_add(syspath);
  _devices_timer_ensure();
  device_ac_adapters = eina_list_append(device_ac_adapters, ac);
  _poll_wakeup();
}

static void
_battery_udev_battery_del(const char *syspath)
{ // in main loop
  Battery *bat;

  if (!(bat = _battery_battery_find(syspath)))
    {
      _poll_wakeup();
      return;
    }
  device_batteries = eina_list_remove(device_batteries, bat);
  eina_stringshare_del(bat->udi);
  eina_stringshare_del(bat->technology);
  eina_stringshare_del(bat->model);
  eina_stringshare_del(bat->vendor);
  free(bat);
  _poll_wakeup();
  _devices_timer_clean();
}

static void
_battery_udev_ac_del(const char *syspath)
{ // in main loop
  Ac_Adapter *ac;

  if (!(ac = _battery_ac_adapter_find(syspath)))
    {
      _poll_wakeup();
      return;
    }
  device_ac_adapters = eina_list_remove(device_ac_adapters, ac);
  eina_stringshare_del(ac->udi);
  free(ac);
  _poll_wakeup();
  _devices_timer_clean();
}

static Eina_Bool
_battery_udev_battery_update_poll(void *data EINA_UNUSED)
{ // in main loop
  _poll_wakeup();
  return EINA_TRUE;
}
#endif
