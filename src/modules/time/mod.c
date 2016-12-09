#include "clock.h"

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
   config_descriptor_init();
   time_config = e_config_domain_load("module.time", config_descriptor_get());

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
   config_descriptor_shutdown();

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
   e_config_domain_save("module.time", config_descriptor_get(), time_config);
   return 1;
}
