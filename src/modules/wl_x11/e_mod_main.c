#include "e.h"
#include "e_comp_wl.h"
#include <Ecore_Wayland.h>

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_X11" };

#define SCREEN_W 1024
#define SCREEN_H 768

static void
_cb_delete_request(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

EAPI void *
e_modapi_init(E_Module *m)
{
   Ecore_Evas *ee;
   E_Screen *screen;
   E_Comp *comp;

   printf("LOAD WL_X11 MODULE\n");

   /* try to init ecore_x */
   if (!ecore_x_init(NULL))
     {
        fprintf(stderr, "Could not initialize ecore_x\n");
        return NULL;
     }

   ee = ecore_evas_software_x11_new(NULL, 0, 0, 0, SCREEN_W, SCREEN_H);
   comp = e_comp_new();
   comp->comp_type = E_PIXMAP_TYPE_WL;
   comp->ee = ee;
   if (!e_xinerama_fake_screens_exist())
     {
        screen = E_NEW(E_Screen, 1);
        screen->escreen = screen->screen = 0;
        screen->x = 0;
        screen->y = 0;
        screen->w = SCREEN_W;
        screen->h = SCREEN_H;
        e_xinerama_screens_set(eina_list_append(NULL, screen));
     }
   comp->man = e_manager_new(0, comp, SCREEN_W, SCREEN_H);
   e_comp_wl_init();
   e_comp_canvas_init(comp);
   e_comp_canvas_fake_layers_init(comp);
   comp->pointer = e_pointer_canvas_new(comp->evas, 1);

   ecore_evas_callback_delete_request_set(ee, _cb_delete_request);

   /* setup keymap_change event handler */
   if (!_e_wl_comp->kbd_handler)
     _e_wl_comp->kbd_handler = 
       ecore_event_handler_add(ECORE_X_EVENT_XKB_STATE_NOTIFY, 
                               e_comp_wl_cb_keymap_changed, NULL);

   ecore_wl_init(NULL);
   ecore_wl_server_mode_set(1);
   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* shutdown ecore_x */
   ecore_x_shutdown();

   return 1;
}
