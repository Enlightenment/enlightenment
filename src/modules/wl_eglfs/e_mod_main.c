#include "e.h"
#include <Ecore_Fb.h>
#include <Ecore_Wayland.h>

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_EGLFS" };

E_API void *
e_modapi_init(E_Module *m)
{
   Ecore_Evas *ee;
   E_Screen *screen;
   int w, h;

   printf("LOAD WL_EGLFS MODULE\n");
   e_util_env_set("HYBRIS_EGLPLATFORM", "hwcomposer");

   /* try to init ecore_fb */
   if (!ecore_fb_init(NULL))
     {
        fprintf(stderr, "Could not initialize ecore_fb");
        return NULL;
     }

   ecore_fb_size_get(&w, &h);
   ee = ecore_evas_eglfs_new(NULL, 0, w, h);

   e_comp->ee = ee;
   e_comp_gl_set(!!e_comp->ee);

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
   e_comp_wl_init();
   e_comp_canvas_init(w, h);
   e_comp->pointer = e_pointer_canvas_new(e_comp->ee, EINA_TRUE);

   e_comp_wl_input_pointer_enabled_set(EINA_TRUE);
   e_comp_wl_input_keyboard_enabled_set(EINA_TRUE);
   e_comp_wl_input_touch_enabled_set(EINA_TRUE);
   e_comp_wl_input_keymap_set(NULL, NULL, NULL);

   ecore_wl_init(NULL);
   ecore_wl_server_mode_set(1);
   e_util_env_set("HYBRIS_EGLPLATFORM", "wayland");
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* shutdown ecore_fb */
   ecore_fb_shutdown();

   return 1;
}
