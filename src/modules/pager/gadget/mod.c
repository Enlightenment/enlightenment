#include "pager.h"
static E_Config_DD *conf_edd = NULL;
Config *pager_config;
E_Module *gmodule;
Evas_Object *cfg_dialog;
Eina_List *ginstances, *ghandlers;

EINTERN void *
e_modapi_gadget_init(E_Module *m)
{
   conf_edd = E_CONFIG_DD_NEW("Pager_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_VAL(D, T, popup, UINT);
   E_CONFIG_VAL(D, T, popup_speed, DOUBLE);
   E_CONFIG_VAL(D, T, popup_urgent, UINT);
   E_CONFIG_VAL(D, T, popup_urgent_stick, UINT);
   E_CONFIG_VAL(D, T, popup_urgent_speed, DOUBLE);
   E_CONFIG_VAL(D, T, show_desk_names, UINT);
   E_CONFIG_VAL(D, T, popup_height, INT);
   E_CONFIG_VAL(D, T, popup_act_height, INT);
   E_CONFIG_VAL(D, T, drag_resist, UINT);
   E_CONFIG_VAL(D, T, btn_drag, UCHAR);
   E_CONFIG_VAL(D, T, btn_noplace, UCHAR);
   E_CONFIG_VAL(D, T, btn_desk, UCHAR);
   E_CONFIG_VAL(D, T, flip_desk, UCHAR);

   pager_config = e_config_domain_load("module.pager", conf_edd);

   if (!pager_config)
     {
        pager_config = E_NEW(Config, 1);
        pager_config->popup = 1;
        pager_config->popup_speed = 1.0;
        pager_config->popup_urgent = 0;
        pager_config->popup_urgent_stick = 0;
        pager_config->popup_urgent_speed = 1.5;
        pager_config->show_desk_names = 0;
        pager_config->popup_height = 60;
        pager_config->popup_act_height = 60;
        pager_config->drag_resist = 3;
        pager_config->btn_drag = 1;
        pager_config->btn_noplace = 2;
        pager_config->btn_desk = 2;
        pager_config->flip_desk = 0;
     }
   E_CONFIG_LIMIT(pager_config->popup, 0, 1);
   E_CONFIG_LIMIT(pager_config->popup_speed, 0.1, 10.0);
   E_CONFIG_LIMIT(pager_config->popup_urgent, 0, 1);
   E_CONFIG_LIMIT(pager_config->popup_urgent_stick, 0, 1);
   E_CONFIG_LIMIT(pager_config->popup_urgent_speed, 0.1, 10.0);
   E_CONFIG_LIMIT(pager_config->show_desk_names, 0, 1);
   E_CONFIG_LIMIT(pager_config->popup_height, 20, 200);
   E_CONFIG_LIMIT(pager_config->popup_act_height, 20, 200);
   E_CONFIG_LIMIT(pager_config->drag_resist, 0, 50);
   E_CONFIG_LIMIT(pager_config->flip_desk, 0, 1);
   E_CONFIG_LIMIT(pager_config->btn_drag, 0, 32);
   E_CONFIG_LIMIT(pager_config->btn_noplace, 0, 32);
   E_CONFIG_LIMIT(pager_config->btn_desk, 0, 32);

   gmodule = m;

   pager_init();

   e_gadget_type_add("Pager Gadget", pager_create, NULL);

   return m;
}

EINTERN int
e_modapi_gadget_shutdown(E_Module *m EINA_UNUSED)
{
   if (cfg_dialog)
     e_object_del(E_OBJECT(cfg_dialog));

   if (pager_config && ghandlers)
     E_FREE_LIST(ghandlers, ecore_event_handler_del);

   e_action_del("pager_gadget_show");
   e_action_del("pager_gadget_switch");

   e_action_predef_name_del("Pager Gadget", "Show Pager Popup");
   e_action_predef_name_del("Pager Gadget", "Popup Desk Right");
   e_action_predef_name_del("Pager Gadget", "Popup Desk Left");
   e_action_predef_name_del("Pager Gadget", "Popup Desk Up");
   e_action_predef_name_del("Pager Gadget", "Popup Desk Down");
   e_action_predef_name_del("Pager Gadget", "Popup Desk Next");
   e_action_predef_name_del("Pager Gadget", "Popup Desk Previous");

   e_gadget_type_del("Pager Gadget");

   E_FREE(pager_config);
   E_CONFIG_DD_FREE(conf_edd);
   return 1;
}

EINTERN int
e_modapi_gadget_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.pager", conf_edd, pager_config);
   return 1;
}
