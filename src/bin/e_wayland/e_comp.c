#include "e.h"

/* local function prototypes */
static void _e_comp_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_comp_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_comp_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id);

/* local interfaces */
static const struct wl_compositor_interface _e_compositor_interface = 
{
   _e_comp_cb_surface_create,
   _e_comp_cb_region_create
};

/* local variables */

EINTERN int 
e_comp_init(void)
{
   E_Module *mod = NULL;
   const char *modname;

   /* NB: Basically, this function needs to load the appropriate 
    * compositor module (drm, fb, x11, etc) */

   if (getenv("DISPLAY"))
     modname = "wl_x11";
   else
     modname = "wl_drm";

   if (!(mod = e_module_find(modname)))
     mod = e_module_new(modname);

   if (mod) 
     {
        e_module_enable(mod);
        return 1;
     }

   return 0;
}

EINTERN int 
e_comp_shutdown(void)
{
   E_Module *mod = NULL;
   const char *modname;

   /* NB: This function needs to unload the compositor module */

   if (getenv("DISPLAY"))
     modname = "wl_x11";
   else
     modname = "wl_drm";

   if ((mod = e_module_find(modname)))
     e_module_disable(mod);

   return 1;
}

EAPI Eina_Bool 
e_compositor_init(E_Compositor *comp)
{
   /* try to create a wayland display */
   if (!(comp->wl.display = wl_display_create()))
     {
        ERR("Could not create a wayland display: %m");
        return EINA_FALSE;
     }

   /* initialize signals */
   wl_signal_init(&comp->signals.destroy);
   wl_signal_init(&comp->signals.activate);
   wl_signal_init(&comp->signals.kill);
   wl_signal_init(&comp->signals.seat);

   /* try to add the compositor to the displays global list */
   if (!wl_display_add_global(comp->wl.display, &wl_compositor_interface, 
                              comp, _e_comp_cb_bind))
     {
        ERR("Could not add compositor to globals: %m");
        goto global_err;
     }

   /* TODO: e_plane */

   /* TODO: init xkb */

   /* initialize the data device manager */
   wl_data_device_manager_init(comp->wl.display);

   /* try to initialize the shm mechanism */
   if (wl_display_init_shm(comp->wl.display) < 0)
     ERR("Could not initialize SHM mechanism: %m");

#ifdef HAVE_WAYLAND_EGL
   /* try to get the egl display
    * 
    * NB: This is interesting....if we try to eglGetDisplay and pass in the 
    * wayland display, then EGL fails due to XCB not owning the event queue.
    * If we pass it a NULL, it inits just fine */
   comp->egl.display = eglGetDisplay(NULL);
   if (comp->egl.display == EGL_NO_DISPLAY)
     ERR("Could not get EGL display: %m");
   else
     {
        EGLint major, minor;

        /* try to initialize EGL */
        if (!eglInitialize(comp->egl.display, &major, &minor))
          {
             ERR("Could not initialize EGL: %m");
             eglTerminate(comp->egl.display);
             eglReleaseThread();
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
             if ((!eglChooseConfig(comp->egl.display, attribs, 
                                   &comp->egl.config, 1, &n) || (n == 0)))
               {
                  ERR("Could not choose EGL config: %m");
                  eglTerminate(comp->egl.display);
                  eglReleaseThread();
               }
          }
     }
#endif

   return EINA_TRUE;

global_err:
   /* destroy the previously created display */
   if (comp->wl.display) wl_display_destroy(comp->wl.display);

   return EINA_FALSE;
}

EAPI Eina_Bool 
e_compositor_shutdown(E_Compositor *comp)
{
#ifdef HAVE_WAYLAND_EGL
   /* destroy the egl display */
   if (comp->egl.display) eglTerminate(comp->egl.display);
   eglReleaseThread();
#endif

   /* destroy the previously created display */
   if (comp->wl.display) wl_display_destroy(comp->wl.display);

   return EINA_TRUE;
}

/* local functions */
static void 
_e_comp_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id)
{
   E_Compositor *comp;

   if (!(comp = data)) return;

   /* add the compositor to the client */
   wl_client_add_object(client, &wl_compositor_interface, 
                        &_e_compositor_interface, id, comp);
}

static void 
_e_comp_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Compositor *comp;

   if (!(comp = resource->data)) return;
}

static void 
_e_comp_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Compositor *comp;

   if (!(comp = resource->data)) return;
}
