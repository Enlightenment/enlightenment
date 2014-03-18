#include "e.h"
#include <Ecore_Drm.h>
#include "e_comp_wl.h"
#include <Ecore_Wayland.h>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1200

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Drm" };

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Comp *comp;

   printf("LOAD WL_DRM MODULE\n");

   /* try to init ecore_drm */
   /* if (!ecore_drm_init()) */
   /*   { */
   /*      fprintf(stderr, "Could not initialize ecore_drm"); */
   /*      return NULL; */
   /*   } */

   comp = e_comp_new();
   comp->comp_type = E_PIXMAP_TYPE_WL;
   comp->ee = ecore_evas_drm_new(NULL, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
   if (!comp->ee)
     {
        fprintf(stderr, "Could not create ecore_evas_drm canvas");
        return NULL;
     }

   if (!e_xinerama_fake_screens_exist())
     {
        E_Screen *screen;

        screen = E_NEW(E_Screen, 1);
        screen->escreen = screen->screen = 0;
        screen->x = 0;
        screen->y = 0;
        screen->w = SCREEN_WIDTH;
        screen->h = SCREEN_HEIGHT;
        e_xinerama_screens_set(eina_list_append(NULL, screen));
     }
   comp->man = e_manager_new(0, comp, SCREEN_WIDTH, SCREEN_HEIGHT);
   if (!e_comp_wl_init()) return NULL;
   e_comp_canvas_init(comp);
   e_comp_canvas_fake_layers_init(comp);
   comp->pointer = e_pointer_canvas_new(comp->evas, 1);

   ecore_wl_server_mode_set(1);
   ecore_wl_init(NULL);

   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* shutdown ecore_drm */
   /* ecore_drm_shutdown(); */

   return 1;
}
