#include "e.h"
#include "e_mod_main.h"

/* local function prototypes */

/* local variables */
static E_Compositor_X11 *_e_comp;

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_X11" };

EAPI void *
e_modapi_init(E_Module *m)
{
   Evas_Coord w, h;

   /* try to init ecore_x */
   if (!ecore_x_init(NULL)) return NULL;

   /* try to allocate space for comp structure */
   if (!(_e_comp = E_NEW(E_Compositor_X11, 1)))
     {
        ERR("Could not allocate space for compositor: %m");
        goto err;
     }

   /* get the X display */
   _e_comp->display = ecore_x_display_get();

   /* try to initialize generic compositor */
   if (!e_compositor_init(&_e_comp->base))
     {
        ERR("Could not initialize compositor: %m");
        goto err;
     }

   /* FIXME: make these sizes configurable ? */
   w = 1024;
   h = 768;

   /* try to create the output window */
   _e_comp->win = ecore_x_window_new(0, 0, 0, w, h);
   ecore_x_window_background_color_set(_e_comp->win, 0, 0, 0);
   ecore_x_icccm_size_pos_hints_set(_e_comp->win, EINA_FALSE, 
                                    ECORE_X_GRAVITY_NW, w, h, w, h,
                                    0, 0, 1, 1, 0.0, 0.0);
   ecore_x_icccm_title_set(_e_comp->win, "E Wayland X11 Compositor");
   ecore_x_icccm_name_class_set(_e_comp->win, "E Wayland X11 Compositor", 
                                "e_wayland/X11 Compositor");
   ecore_x_window_show(_e_comp->win);

   /* flush any pending events
    * 
    * NB: This advertises out any globals so it needs to be deferred 
    * until after the module has finished initialize */
   /* wl_event_loop_dispatch(_e_comp->base.wl.loop, 0); */

   return m;

err:
   /* shutdown ecore_x */
   ecore_x_shutdown();

   /* free the structure */
   E_FREE(_e_comp);

   return NULL;
}

EAPI int 
e_modapi_shutdown(E_Module *m)
{
   /* destroy the window */
   if (_e_comp->win) ecore_x_window_free(_e_comp->win);

   /* shutdown generic compositor */
   if (&_e_comp->base) e_compositor_shutdown(&_e_comp->base);

   /* shutdown ecore_x */
   ecore_x_shutdown();

   /* free the structure */
   E_FREE(_e_comp);

   return 1;
}
