#include "e.h"

/* local function prototypes */
static void _e_comp_cb_bind(struct wl_client *client, void *data EINA_UNUSED, unsigned int version EINA_UNUSED, unsigned int id);

/* local interfaces */
static const struct wl_compositor_interface _e_compositor_interface = 
{
   NULL, // surface_create
   NULL // region_create
};

/* local variables */
static E_Compositor *_e_comp;

EINTERN int 
e_comp_init(void)
{
   int fd = 0;

   /* try to allocate space for a new compositor */
   if (!(_e_comp = E_NEW(E_Compositor, 1)))
     {
        ERR("Could not allocate space for compositor");
        return 0;
     }

   /* try to create a wayland display */
   if (!(_e_comp->wl.display = wl_display_create()))
     {
        ERR("Could not create a wayland display: %m");
        goto err;
     }

   /* initialize signals */
   wl_signal_init(&_e_comp->signals.destroy);
   wl_signal_init(&_e_comp->signals.activate);
   wl_signal_init(&_e_comp->signals.kill);
   wl_signal_init(&_e_comp->signals.seat);

   /* try to add the compositor to the displays global list */
   if (!wl_display_add_global(_e_comp->wl.display, &wl_compositor_interface, 
                              _e_comp, _e_comp_cb_bind))
     {
        ERR("Could not add compositor to globals: %m");
        goto global_err;
     }

   /* initialize the data device manager */
   wl_data_device_manager_init(_e_comp->wl.display);

   /* try to initialize the shm mechanism */
   if (wl_display_init_shm(_e_comp->wl.display) < 0)
     ERR("Could not initialize SHM mechanism: %m");

#ifdef HAVE_WAYLAND_EGL
   /* try to get the egl display
    * 
    * NB: This is interesting....if we try to eglGetDisplay and pass in the 
    * wayland display, then EGL fails due to XCB not owning the event queue.
    * If we pass it a NULL, it inits just fine */
   _e_comp->egl.display = eglGetDisplay(NULL);
   if (_e_comp->egl.display == EGL_NO_DISPLAY)
     ERR("Could not get EGL display: %m");
   else
     {
        EGLint major, minor;

        /* try to initialize EGL */
        if (!eglInitialize(_e_comp->egl.display, &major, &minor))
          {
             ERR("Could not initialize EGL: %m");
             eglTerminate(_e_comp->egl.display);
          }
        else
          {
             EGLint n;
             EGLint attribs[] = 
               {
                  EGL_SURFACE_TYPE, EGL_WINDOW_BIT, 
                  EGL_RED_SIZE, 1, EGL_GREEN_SIZE, 1, EGL_BLUE_SIZE, 1, 
                  EGL_ALPHA_SIZE, 1, EGL_RENDERABLE_TYPE, 
                  EGL_OPENGL_ES2_BIT, EGL_NONE
               };

             /* try to find a matching egl config */
             if ((!eglChooseConfig(_e_comp->egl.display, attribs, 
                                   &_e_comp->egl.config, 1, &n) || (n == 0)))
               {
                  ERR("Could not choose EGL config: %m");
                  eglTerminate(_e_comp->egl.display);
               }
          }
     }
#endif

   return 1;

global_err:
   /* destroy the previously created display */
   if (_e_comp->wl.display) wl_display_destroy(_e_comp->wl.display);
err:
   /* free the compositor object */
   E_FREE(_e_comp);

   return 0;
}

EINTERN int 
e_comp_shutdown(void)
{
#ifdef HAVE_WAYLAND_EGL
   /* destroy the egl display */
   if (_e_comp->egl.display) eglTerminate(_e_comp->egl.display);
#endif

   /* destroy the previously created display */
   if (_e_comp->wl.display) wl_display_destroy(_e_comp->wl.display);

   /* free the compositor object */
   E_FREE(_e_comp);

   return 1;
}

/* local functions */
static void 
_e_comp_cb_bind(struct wl_client *client, void *data EINA_UNUSED, unsigned int version EINA_UNUSED, unsigned int id)
{
   /* add the compositor to the client */
   wl_client_add_object(client, &wl_compositor_interface, 
                        &_e_compositor_interface, id, _e_comp);
}
