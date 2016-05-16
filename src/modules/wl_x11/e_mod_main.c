#include "e.h"
#include <Ecore_X.h>

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_X11" };

static Ecore_Event_Handler *kbd_hdlr;

static void
_cb_delete_request(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

E_API void *
e_modapi_init(E_Module *m)
{
   int w = 0, h = 0;

   printf("LOAD WL_X11 MODULE\n");

   if (!ecore_x_init(NULL))
     {
        fprintf(stderr, "X11 connect failed!\n");
        return NULL;
     }
   e_comp_x_randr_canvas_new(ecore_x_window_root_first_get(), 1, 1);

   if (!e_comp->ee)
     {
        ERR("Could not create ecore_evas canvas");
        return NULL;
     }
   ecore_evas_callback_delete_request_set(e_comp->ee, _cb_delete_request);
   ecore_evas_title_set(e_comp->ee, "Enlightenment: WL-X11");
   ecore_evas_name_class_set(e_comp->ee, "E", "compositor");

   ecore_evas_screen_geometry_get(e_comp->ee, NULL, NULL, &w, &h);
   if (!ecore_x_screen_is_composited(0))
     e_comp_x_randr_screen_iface_set();
   if (!e_comp_wl_init()) return NULL;
   if (!e_comp_canvas_init(w, h)) return NULL;

   e_comp_wl_input_pointer_enabled_set(EINA_TRUE);
   e_comp_wl_input_keyboard_enabled_set(EINA_TRUE);
   e_comp_wl_input_touch_enabled_set(EINA_TRUE);

   /* e_comp->pointer =  */
   /*   e_pointer_window_new(ecore_evas_window_get(e_comp->ee), EINA_TRUE); */
   e_comp->pointer = e_pointer_canvas_new(e_comp->ee, EINA_TRUE);
   e_comp->pointer->color = EINA_TRUE;

   e_comp_wl->dmabuf_disable = EINA_TRUE;

   return m;
}

E_API int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* delete handler for keymap change */
   if (kbd_hdlr) ecore_event_handler_del(kbd_hdlr);
   ecore_x_shutdown();

   return 1;
}
