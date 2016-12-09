#include "pager.h"
Config *pager_config;
E_Module *gmodule;
Evas_Object *cfg_dialog;
Eina_List *ginstances, *ghandlers;

E_API void *
e_modapi_gadget_init(E_Module *m)
{
   config_descriptor_init();
   pager_config = e_config_domain_load("module.pager", config_descriptor_get());

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

E_API int
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
   config_descriptor_shutdown();
   return 1;
}

E_API int
e_modapi_gadget_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.pager", config_descriptor_get(), pager_config);
   return 1;
}
