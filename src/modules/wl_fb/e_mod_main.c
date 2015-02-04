#include "e.h"
#include <Ecore_Fb.h>
#include <Ecore_Wayland.h>

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_FB" };

EAPI void *
e_modapi_init(E_Module *m)
{
   Ecore_Evas *ee;
   E_Screen *screen;
   E_Comp *comp;
   int w, h;

   printf("LOAD WL_FB MODULE\n");

   /* try to init ecore_x */
   if (!ecore_fb_init(NULL))
     {
        fprintf(stderr, "Could not initialize ecore_fb");
        return NULL;
     }

   ecore_fb_size_get(&w, &h);
   ee = ecore_evas_fb_new(NULL, 0, w, h);

   if (!(comp = e_comp))
     {
        comp = e_comp_new();
        comp->comp_type = E_PIXMAP_TYPE_WL;
     }

   comp->ee = ee;

   if (!e_xinerama_fake_screens_exist())
     {
        screen = E_NEW(E_Screen, 1);
        screen->escreen = screen->screen = 0;
        screen->x = 0;
        screen->y = 0;
        screen->w = w;
        screen->h = h;
        e_xinerama_screens_set(eina_list_append(NULL, screen));
     }
   comp->man = e_manager_new(ecore_evas_window_get(comp->ee), comp, w, h);
   e_comp_wl_init();
   e_comp_canvas_init(comp);
   e_comp_canvas_fake_layers_init(comp);
   comp->pointer = e_pointer_canvas_new(comp->ee, EINA_TRUE);

   ecore_wl_init(NULL);
   ecore_wl_server_mode_set(1);
   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* shutdown ecore_x */
   ecore_fb_shutdown();

   return 1;
}
