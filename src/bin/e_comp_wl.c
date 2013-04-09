#include "e.h"
#include "e_comp_wl.h"
#include <sys/mman.h>

/* local function prototypes */
static void _e_comp_wl_cb_bind(struct wl_client *client, void *data EINA_UNUSED, unsigned int version EINA_UNUSED, unsigned int id);
static Eina_Bool _e_comp_wl_cb_read(void *data EINA_UNUSED, Ecore_Fd_Handler *hdl EINA_UNUSED);
static Eina_Bool _e_comp_wl_cb_idle(void *data EINA_UNUSED);

/* input function prototypes */
static Eina_Bool _e_comp_wl_input_init(void);
static void _e_comp_wl_input_shutdown(void);

/* compositor interface prototypes */
static void _e_comp_wl_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_comp_wl_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id);

/* local wayland interfaces */
static const struct wl_compositor_interface _e_compositor_interface = 
{
   _e_comp_wl_cb_surface_create,
   _e_comp_wl_cb_region_create
};

/* local variables */

/* external variables */
E_Wayland_Compositor *_e_wl_comp;

/* external functions */
Eina_Bool 
e_comp_wl_init(void)
{
   int fd = 0;

   /* try to allocate space for a new compositor */
   if (!(_e_wl_comp = E_NEW(E_Wayland_Compositor, 1)))
     return EINA_FALSE;

   /* try to create a wayland display */
   if (!(_e_wl_comp->wl.display = wl_display_create()))
     {
        ERR("Could not create a Wayland Display: %m");
        goto err;
     }

   /* try to add a display socket */
   if (wl_display_add_socket(_e_wl_comp->wl.display, NULL) < 0)
     {
        ERR("Could not add a Wayland Display socket: %m");
        goto err;
     }

   /* init compositor signals */
   wl_signal_init(&_e_wl_comp->signals.destroy);
   wl_signal_init(&_e_wl_comp->signals.activate);
   wl_signal_init(&_e_wl_comp->signals.kill);
   wl_signal_init(&_e_wl_comp->signals.seat);

   /* try to add compositor to the displays globals */
   if (!wl_display_add_global(_e_wl_comp->wl.display, &wl_compositor_interface,
                              _e_wl_comp, _e_comp_wl_cb_bind))
     {
        ERR("Could not add compositor to globals: %m");
        goto err;
     }

   /* init data device manager */
   wl_data_device_manager_init(_e_wl_comp->wl.display);

   /* try to init shm mechanism */
   if (wl_display_init_shm(_e_wl_comp->wl.display) < 0)
     ERR("Could not initialize SHM: %m");

#ifdef HAVE_WAYLAND_EGL
   /* try to get the egl display
    * 
    * NB: This is interesting....if we try to eglGetDisplay and pass in the 
    * wayland display, then EGL fails due to XCB not owning the event queue.
    * If we pass it a NULL, it inits just fine */
   _e_wl_comp->egl.display = eglGetDisplay(NULL);
   if (_e_wl_comp->egl.display == EGL_NO_DISPLAY)
     ERR("Could not get EGL display: %m");
   else
     {
        EGLint major, minor;

        /* try to initialize egl */
        if (!eglInitialize(_e_wl_comp->egl.display, &major, &minor))
          {
             ERR("Could not initialize EGL: %m");
             eglTerminate(_e_wl_comp->egl.display);
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

             if ((!eglChooseConfig(_e_wl_comp->egl.display, attribs, 
                                   &_e_wl_comp->egl.config, 1, &n) || (n == 0)))
               {
                  ERR("Could not choose EGL config: %m");
                  eglTerminate(_e_wl_comp->egl.display);
               }
          }
     }
#endif

   /* try to initialize input */
   if (!_e_comp_wl_input_init())
     {
        ERR("Could not initialize input: %m");
        goto err;
     }

   /* TODO: module idler */

   /* get the displays event loop */
   _e_wl_comp->wl.loop = wl_display_get_event_loop(_e_wl_comp->wl.display);

   /* get the event loop's file descriptor so we can listen on it */
   fd = wl_event_loop_get_fd(_e_wl_comp->wl.loop);

   /* add the fd to E's main loop */
   _e_wl_comp->fd_handler = 
     ecore_main_fd_handler_add(fd, ECORE_FD_READ, 
                               _e_comp_wl_cb_read, NULL, NULL, NULL);

   /* add an idler for flushing clients */
   _e_wl_comp->idler = ecore_idle_enterer_add(_e_comp_wl_cb_idle, NULL);

   /* TODO: event handlers ?? */

   /* flush any pending events */
   wl_event_loop_dispatch(_e_wl_comp->wl.loop, 0);

   /* return success */
   return EINA_TRUE;

err:
#ifdef HAVE_WAYLAND_EGL
   /* terminate the egl display */
   if (_e_wl_comp->egl.display)
     eglTerminate(_e_wl_comp->egl.display);
   eglReleaseThread();
#endif

   /* if we have a display, destroy it */
   if (_e_wl_comp->wl.display) wl_display_destroy(_e_wl_comp->wl.display);

   /* free the compositor */
   E_FREE(_e_wl_comp);

   /* return failure */
   return EINA_FALSE;
}

void 
e_comp_wl_shutdown(void)
{
   /* remove the idler */
   if (_e_wl_comp->idler) ecore_idler_del(_e_wl_comp->idler);

   /* remove the fd handler */
   if (_e_wl_comp->fd_handler)
     ecore_main_fd_handler_del(_e_wl_comp->fd_handler);

   /* shutdown input */
   _e_comp_wl_input_shutdown();

#ifdef HAVE_WAYLAND_EGL
   /* terminate the egl display */
   if (_e_wl_comp->egl.display)
     eglTerminate(_e_wl_comp->egl.display);
   eglReleaseThread();
#endif

   /* if we have a display, destroy it */
   if (_e_wl_comp->wl.display) wl_display_destroy(_e_wl_comp->wl.display);

   /* free the compositor */
   E_FREE(_e_wl_comp);

   /* TODO: unload shell module */
}

/* local functions */
static void 
_e_comp_wl_cb_bind(struct wl_client *client, void *data EINA_UNUSED, unsigned int version EINA_UNUSED, unsigned int id)
{
   /* add the compositor object to the client */
   wl_client_add_object(client, &wl_compositor_interface, 
                        &_e_compositor_interface, id, _e_wl_comp);
}

static Eina_Bool 
_e_comp_wl_cb_read(void *data EINA_UNUSED, Ecore_Fd_Handler *hdl EINA_UNUSED)
{
   /* flush any events before we sleep */
   wl_event_loop_dispatch(_e_wl_comp->wl.loop, 0);
   wl_display_flush_clients(_e_wl_comp->wl.display);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_comp_wl_cb_idle(void *data EINA_UNUSED)
{
   /* flush any clients before we idle */
   wl_display_flush_clients(_e_wl_comp->wl.display);

   return ECORE_CALLBACK_RENEW;
}

/* input functions */
static Eina_Bool 
_e_comp_wl_input_init(void)
{
   return EINA_TRUE;
}

static void 
_e_comp_wl_input_shutdown(void)
{

}

/* compositor interface functions */
static void 
_e_comp_wl_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{

}

static void 
_e_comp_wl_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{

}

