#include "e.h"

EINTERN void wl_wl_init(void);

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Wl" };

static void
_cb_delete_request(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

E_API void *
e_modapi_init(E_Module *m)
{
   int w = 0, h = 0;

   printf("LOAD WL_WL MODULE\n");

   if (e_comp_config_get()->engine == E_COMP_ENGINE_GL)
     {
        e_comp->ee = ecore_evas_new("wayland_egl", 0, 0, 1, 1, NULL);
        e_comp_gl_set(!!e_comp->ee);
     }
   if (!e_comp->ee)
     {
        if ((e_comp->ee = ecore_evas_new("wayland_shm", 0, 0, 1, 1, NULL)))
          {
             e_comp_gl_set(EINA_FALSE);
             elm_config_accel_preference_set("none");
             elm_config_accel_preference_override_set(EINA_TRUE);
             elm_config_all_flush();
             elm_config_save();
          }
        else
          {
             fprintf(stderr, "Could not create ecore_evas_drm canvas");
             return NULL;
          }
     }
   ecore_evas_callback_delete_request_set(e_comp->ee, _cb_delete_request);
   ecore_evas_title_set(e_comp->ee, "Enlightenment: WL-WL");
   ecore_evas_name_class_set(e_comp->ee, "E", "compositor");

   ecore_evas_screen_geometry_get(e_comp->ee, NULL, NULL, &w, &h);

   if (!e_comp_wl_init()) return NULL;
   if (!e_comp_canvas_init(w * 3 / 4, h * 3 / 4)) return NULL;

   ecore_evas_pointer_xy_get(e_comp->ee, &e_comp_wl->ptr.x,
                             &e_comp_wl->ptr.y);
   e_comp_wl_input_pointer_enabled_set(EINA_TRUE);
   e_comp_wl_input_keyboard_enabled_set(EINA_TRUE);
   e_comp_wl_input_touch_enabled_set(EINA_TRUE);

   /* e_comp->pointer =  */
   /*   e_pointer_window_new(ecore_evas_window_get(e_comp->ee), EINA_TRUE); */
   e_comp->pointer = e_pointer_canvas_new(e_comp->ee, EINA_TRUE);
   e_comp->pointer->color = EINA_TRUE;

   e_comp_wl_input_keymap_set(NULL, NULL, NULL);
   wl_wl_init();

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   return 1;
}
