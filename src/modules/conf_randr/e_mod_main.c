#include "e.h"
#include "e_mod_main.h"
#include "e_int_config_randr2.h"

E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION, "Settings - Screen Setup"
};

static Ecore_Event_Handler *randr_event_hand = NULL;

static Eina_Bool
_cb_randr(void *data EINA_UNUSED, int type EINA_UNUSED, void *info EINA_UNUSED)
{
   Eina_List *l;
   E_Randr2_Screen *s;

   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!s->config.configured)
          {
             // XXX: we should put up a dialog asking to configure screen s
             printf("RRR: New unconfigured screen on %s\n", s->info.name);
          }
     }
   return EINA_TRUE;
}

E_API void *
e_modapi_init(E_Module *m)
{
   /* create Screen configuration category
    * 
    * NB: If the category already exists, this function just returns */
   e_configure_registry_category_add("screen", 30, _("Screen"),
                                     NULL, "preferences-desktop-display");

   /* add the randr dialog to the screen category and provide 
    * the configure category with the function to call */
   e_configure_registry_item_add("screen/screen_setup", 20, _("Screen Setup"),
                                 NULL, "preferences-system-screen-resolution",
                                 e_int_config_randr2);
   randr_event_hand = ecore_event_handler_add(E_EVENT_RANDR_CHANGE,
                                              _cb_randr, NULL);
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   E_Config_Dialog *cfd;

   if (randr_event_hand)
     {
        ecore_event_handler_del(randr_event_hand);
        randr_event_hand = NULL;
     }

   while ((cfd = e_config_dialog_get("E", "screen/screen_setup")))
     e_object_del(E_OBJECT(cfd));

   e_configure_registry_item_del("screen/screen_setup");
   e_configure_registry_category_del("screen");
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}
