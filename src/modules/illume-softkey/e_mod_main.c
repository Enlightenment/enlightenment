#include "e.h"
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "e_mod_sft_win.h"

/* local variables */
static Eina_List *swins = NULL;

/* external variables */
const char *_sft_mod_dir = NULL;

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Illume-Softkey" };

E_API void *
e_modapi_init(E_Module *m) 
{
   const Eina_List *l;
   E_Comp *comp;

   /* set module priority so we load before others */
   e_module_priority_set(m, 85);

   /* set module directory variable */
   _sft_mod_dir = eina_stringshare_add(m->dir);

   /* init config subsystem */
   if (!il_sft_config_init()) 
     {
        /* clear module directory variable */
        if (_sft_mod_dir) eina_stringshare_del(_sft_mod_dir);
        _sft_mod_dir = NULL;
        return NULL;
     }

   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     {
        E_Zone *zone;
        Eina_List *zl;

        /* for each zone, create a softkey window */
        EINA_LIST_FOREACH(comp->zones, zl, zone) 
          {
             Sft_Win *swin;

             /* try to create new softkey window */
             if (!(swin = e_mod_sft_win_new(zone))) continue;
             swins = eina_list_append(swins, swin);
          }
     }

   return m;
}

E_API int 
e_modapi_shutdown(E_Module *m __UNUSED__) 
{
   Sft_Win *swin;

   /* destroy the softkey windows */
   EINA_LIST_FREE(swins, swin)
     e_object_del(E_OBJECT(swin));

   /* reset softkey geometry for conformant apps */
   ecore_x_e_illume_softkey_geometry_set(ecore_x_window_root_first_get(), 
                                         0, 0, 0, 0);

   /* shutdown config */
   il_sft_config_shutdown();

   /* clear module directory variable */
   if (_sft_mod_dir) eina_stringshare_del(_sft_mod_dir);
   _sft_mod_dir = NULL;

   return 1;
}

E_API int 
e_modapi_save(E_Module *m __UNUSED__) 
{
   return il_sft_config_save();
}
