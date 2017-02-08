#include "sysinfo.h"

static void _sysinfo_deleted_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED);

static void
_sysinfo_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;

   if (inst->o_main != event_data) return;

   sysinfo_batman_remove(inst, NULL, NULL, NULL);
   sysinfo_thermal_remove(inst, NULL, NULL, NULL);
   sysinfo_cpuclock_remove(inst, NULL, NULL, NULL);
   sysinfo_cpumonitor_remove(inst, NULL, NULL, NULL);
   sysinfo_memusage_remove(inst, NULL, NULL, NULL);
   sysinfo_netstatus_remove(inst, NULL, NULL, NULL);

   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL, _sysinfo_deleted_cb, data);

   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   E_FREE(inst->cfg);
}

static void
_sysinfo_deleted_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   sysinfo_batman_remove(inst, NULL, NULL, NULL);
   sysinfo_thermal_remove(inst, NULL, NULL, NULL);
   sysinfo_cpuclock_remove(inst, NULL, NULL, NULL);
   sysinfo_cpumonitor_remove(inst, NULL, NULL, NULL);
   sysinfo_memusage_remove(inst, NULL, NULL, NULL);
   sysinfo_netstatus_remove(inst, NULL, NULL, NULL);
}

static void
_sysinfo_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->o_table = elm_table_add(inst->o_main);
   elm_table_homogeneous_set(inst->o_table, EINA_FALSE);
   E_EXPAND(inst->o_table);
   elm_object_content_set(inst->o_main, inst->o_table);
   evas_object_show(inst->o_table);

   inst->cfg->sysinfo.o_batman = sysinfo_batman_create(inst->o_table, inst);
   elm_table_pack(inst->o_table, inst->cfg->sysinfo.o_batman, 0, 0, 1, 1);
   inst->cfg->sysinfo.o_cpuclock = sysinfo_cpuclock_create(inst->o_table, inst);
   elm_table_pack(inst->o_table, inst->cfg->sysinfo.o_cpuclock, 1, 0, 1, 1);
   inst->cfg->sysinfo.o_cpumonitor = sysinfo_cpumonitor_create(inst->o_table, inst);
   elm_table_pack(inst->o_table, inst->cfg->sysinfo.o_cpumonitor, 0, 1, 1, 1);
   inst->cfg->sysinfo.o_memusage = sysinfo_memusage_create(inst->o_table, inst);
   elm_table_pack(inst->o_table, inst->cfg->sysinfo.o_memusage, 1, 1, 1, 1);
   inst->cfg->sysinfo.o_thermal = sysinfo_thermal_create(inst->o_table, inst);
   elm_table_pack(inst->o_table, inst->cfg->sysinfo.o_thermal, 0, 2, 1, 1);
   inst->cfg->sysinfo.o_netstatus = sysinfo_netstatus_create(inst->o_table, inst);
   elm_table_pack(inst->o_table, inst->cfg->sysinfo.o_netstatus, 1, 2, 1, 1);

   evas_object_smart_callback_del_full(obj, "gadget_created", _sysinfo_created_cb, data);
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(sysinfo_config->items, l, ci)
          if (*id == ci->id && ci->esm == E_SYSINFO_MODULE_SYSINFO) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(sysinfo_config->items)+1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_SYSINFO;
   ci->batman.poll_interval = 512;
   ci->batman.alert = 30;
   ci->batman.alert_p = 10;
   ci->batman.alert_timeout = 0;
   ci->batman.suspend_below = 0;
   ci->batman.force_mode = 0;
   ci->batman.full = -2;
   ci->batman.time_left = -2;
   ci->batman.have_battery = -2;
   ci->batman.have_power = -2;
#if defined HAVE_EEZE || defined __OpenBSD__ || defined __NetBSD__
   ci->batman.fuzzy = 0;
#endif
   ci->batman.desktop_notifications = 0;
   ci->batman.popup = NULL;
   ci->batman.configure = NULL;
   ci->thermal.poll_interval = 128;
   ci->thermal.low = 30;
   ci->thermal.high = 80;
   ci->thermal.sensor_type = SENSOR_TYPE_NONE;
   ci->thermal.sensor_name = NULL;
   ci->thermal.units = CELSIUS;
   ci->cpuclock.poll_interval = 32;
   ci->cpuclock.restore_governor = 0;
   ci->cpuclock.auto_powersave = 1;
   ci->cpuclock.powersave_governor = NULL;
   ci->cpuclock.governor = NULL;
   ci->cpuclock.pstate_min = 1;
   ci->cpuclock.pstate_max = 101;
   ci->cpumonitor.poll_interval = 32;
   ci->cpumonitor.total = 0;
   ci->cpumonitor.idle = 0;
   ci->cpumonitor.percent = 0;
   ci->cpumonitor.popup = NULL;
   ci->cpumonitor.configure = NULL;
   ci->memusage.poll_interval = 32;
   ci->memusage.mem_percent = 0;
   ci->memusage.swp_percent = 0;
   ci->memusage.popup = NULL;
   ci->memusage.configure = NULL;
   ci->netstatus.poll_interval = 32;
   ci->netstatus.in = 0;
   ci->netstatus.out = 0;
   ci->netstatus.inmax = 0;
   ci->netstatus.outmax = 0;
   ci->netstatus.incurrent = 0;
   ci->netstatus.outcurrent = 0;
   ci->netstatus.inpercent = 0;
   ci->netstatus.outpercent = 0;
   ci->netstatus.instring = NULL;
   ci->netstatus.outstring = NULL;
   ci->netstatus.popup = NULL;
   ci->netstatus.configure = NULL;
   ci->netstatus.automax = EINA_TRUE;
   ci->netstatus.inmax = 0;
   ci->netstatus.outmax = 0;
   ci->netstatus.receive_units = NETSTATUS_UNIT_BYTES;
   ci->netstatus.send_units = NETSTATUS_UNIT_BYTES;

   sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);

   return ci;
}

Evas_Object *
sysinfo_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;

   inst->o_main = elm_scroller_add(parent);
   elm_object_style_set(inst->o_main, "no_inset_shadow");
   E_EXPAND(inst->o_main);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, _sysinfo_deleted_cb, inst);
   evas_object_show(inst->o_main);

   evas_object_smart_callback_add(parent, "gadget_created", _sysinfo_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _sysinfo_removed_cb, inst);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances =
     eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

