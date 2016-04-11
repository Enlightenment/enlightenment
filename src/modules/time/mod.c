#include "clock.h"
static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;
static E_Action *act = NULL;

static void
_e_mod_action_cb(E_Object *obj EINA_UNUSED, const char *params, ...)
{
   Eina_List *l;
   Instance *inst;

   if (!eina_streq(params, "show_calendar")) return;

   EINA_LIST_FOREACH(clock_instances, l, inst)
     if (inst->popup)
       {
          elm_ctxpopup_dismiss(inst->popup);
          inst->popup = NULL;
       }
     else
       clock_popup_new(inst);
}

EINTERN void
clock_init(void)
{
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, weekend.start, INT);
   E_CONFIG_VAL(D, T, weekend.len, INT);
   E_CONFIG_VAL(D, T, week.start, INT);
   E_CONFIG_VAL(D, T, digital_clock, INT);
   E_CONFIG_VAL(D, T, digital_24h, INT);
   E_CONFIG_VAL(D, T, show_seconds, INT);
   E_CONFIG_VAL(D, T, show_date, INT);
   E_CONFIG_VAL(D, T, advanced, UCHAR);
   E_CONFIG_VAL(D, T, timezone, STR);
   E_CONFIG_VAL(D, T, time_str[0], STR);
   E_CONFIG_VAL(D, T, time_str[1], STR);
   E_CONFIG_VAL(D, T, colorclass[0], STR);
   E_CONFIG_VAL(D, T, colorclass[1], STR);

   conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   time_config = e_config_domain_load("module.time", conf_edd);

   if (!time_config)
     time_config = E_NEW(Config, 1);

   act = e_action_add("clock");
   if (act)
     {
        act->func.go = (void*)_e_mod_action_cb;
        act->func.go_key = (void*)_e_mod_action_cb;
        act->func.go_mouse = (void*)_e_mod_action_cb;
        act->func.go_edge = (void*)_e_mod_action_cb;

        e_action_predef_name_set(N_("Clock"), N_("Toggle calendar"), "clock", "show_calendar", NULL, 0);
     }

   e_gadget_type_add("Digital Clock", digital_clock_create, digital_clock_wizard);
   e_gadget_type_add("Analog Clock", analog_clock_create, analog_clock_wizard);
   time_init();
}

EINTERN void
clock_shutdown(void)
{
   if (act)
     {
        e_action_predef_name_del("Clock", "Toggle calendar");
        e_action_del("clock");
        act = NULL;
     }
   if (time_config)
     {
        Config_Item *ci;

        if (time_config->config_dialog)
          {
             evas_object_hide(time_config->config_dialog);
             evas_object_del(time_config->config_dialog);
          }

        EINA_LIST_FREE(time_config->items, ci)
          {
             eina_stringshare_del(ci->timezone);
             eina_stringshare_del(ci->time_str[0]);
             eina_stringshare_del(ci->time_str[1]);
             eina_stringshare_del(ci->colorclass[0]);
             eina_stringshare_del(ci->colorclass[1]);
             free(ci);
          }

        E_FREE(time_config);
     }
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);

   e_gadget_type_del("Digital Clock");
   e_gadget_type_del("Analog Clock");
   time_shutdown();
}

/* module setup */
E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Time"
};

E_API void *
e_modapi_init(E_Module *m)
{
   if (!E_EFL_VERSION_MINIMUM(1, 17, 99)) return NULL;
   clock_init();

   time_config->module = m;
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   clock_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.time", conf_edd, time_config);
   return 1;
}
