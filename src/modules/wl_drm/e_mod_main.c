#include "e.h"
/* #include <Ecore_Drm.h> */

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

   if ((e_comp_gl_get()) && 
       (e_comp_config_get()->engine == E_COMP_ENGINE_GL))
     {
        /* TOOD: create ecore_evas for new drm gl backend */
        /* NB: If that fails, call e_comp_gl_set(EINA_FALSE) */
     }

   /* fallback to framebuffer drm (non-accel) */
   if (!comp->ee)
     comp->ee = ecore_evas_drm_new(NULL, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   if (!comp->ee)
     {
        fprintf(stderr, "Could not create ecore_evas_drm canvas");
        return NULL;
     }

   /* TODO: hook ecore_evas_callback_resize_set */

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
   if (!e_comp_canvas_init(comp)) return NULL;
   e_comp_canvas_fake_layers_init(comp);

   /* NB: This needs to be called AFTER the comp canvas has been setup */
   if (!e_comp_wl_init()) return NULL;

   e_comp_wl_input_pointer_enabled_set(comp->wl_comp_data, EINA_TRUE);
   e_comp_wl_input_keyboard_enabled_set(comp->wl_comp_data, EINA_TRUE);

   /* comp->pointer =  */
   /*   e_pointer_window_new(ecore_evas_window_get(comp->ee), 1); */
   comp->pointer = e_pointer_canvas_new(comp->ee, EINA_TRUE);
   comp->pointer->color = EINA_TRUE;

   /* FIXME: We need a way to trap for user changing the keymap inside of E 
    *        without the event coming from X11 */

   /* FIXME: We should make a decision here ...
    * 
    * Fetch the keymap from drm, OR set this to what the E config is....
    */

   /* FIXME: This is just for testing at the moment....
    * happens to jive with what drm does */
   e_comp_wl_input_keymap_set(comp->wl_comp_data, NULL, NULL, NULL);

   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* shutdown ecore_drm */
   /* ecore_drm_shutdown(); */

   return 1;
}
