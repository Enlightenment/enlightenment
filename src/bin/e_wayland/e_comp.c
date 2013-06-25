#include "e.h"

/* local function prototypes */
static void _e_comp_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_comp_cb_bind_manager(struct wl_client *client, void *data EINA_UNUSED, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_comp_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_comp_cb_surface_destroy(struct wl_resource *resource);
static void _e_comp_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_comp_cb_region_destroy(struct wl_resource *resource);
static Eina_Bool _e_comp_cb_read(void *data, Ecore_Fd_Handler *hdl EINA_UNUSED);
static Eina_Bool _e_comp_cb_idle(void *data);

static void _e_comp_data_device_cb_get(struct wl_client *client, struct wl_resource *resource EINA_UNUSED, unsigned int id, struct wl_resource *seat_resource);
static void _e_comp_data_device_cb_unbind(struct wl_resource *resource);

static Eina_Bool _e_comp_xkb_init(E_Compositor *comp);

/* local interfaces */
static const struct wl_compositor_interface _e_compositor_interface = 
{
   _e_comp_cb_surface_create,
   _e_comp_cb_region_create
};

static const struct wl_data_device_manager_interface _e_manager_interface = 
{
   NULL, // create data source
   _e_comp_data_device_cb_get
};

static const struct wl_data_device_interface _e_data_device_interface = 
{
   NULL, // start_drag
   NULL, // set_selection
};

/* local variables */
EAPI E_Compositor *_e_comp = NULL;

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
     {
        if ((mod = e_module_new(modname)))
          {
             if (e_module_enable(mod))
               return 1;
          }
     }
   else
     return 1;

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
e_compositor_init(E_Compositor *comp, void *display)
{
   E_Plane *p;
   int fd = 0;

   if (_e_comp)
     {
        ERR("Compositor already exists");
        return EINA_FALSE;
     }

   /* try to create a wayland display */
   if (!(comp->wl.display = wl_display_create()))
     {
        ERR("Could not create a wayland display: %m");
        return EINA_FALSE;
     }

   comp->output_pool = 0;

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

   /* initialize hardware plane */
   e_plane_init(&comp->plane, 0, 0);
   e_compositor_plane_stack(comp, &comp->plane, NULL);

   /* init xkb */
   if (!_e_comp_xkb_init(comp))
     {
        ERR("Could not initialize xkb: %m");
        goto global_err;
     }

   /* initialize the data device manager */
   if (!wl_display_add_global(comp->wl.display, 
                              &wl_data_device_manager_interface, NULL, 
                              _e_comp_cb_bind_manager))
     {
        ERR("Could not add data device manager to globals: %m");
        goto global_err;
     }

   /* try to initialize the shm mechanism */
   if (wl_display_init_shm(comp->wl.display) < 0)
     ERR("Could not initialize SHM mechanism: %m");

#ifdef HAVE_WAYLAND_EGL
   /* try to get the egl display
    * 
    * NB: This is interesting....if we try to eglGetDisplay and pass in the 
    * wayland display, then EGL fails due to XCB not owning the event queue.
    * If we pass it a NULL, it inits just fine
    * 
    * NB: This is apparently a Mesa bug because it works now :/ Leaving this 
    * note here in case it fails again in the future */
   comp->egl.display = eglGetDisplay((EGLNativeDisplayType)display);
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

   /* create the input loop */
   comp->wl.input_loop = wl_event_loop_create();

   /* get the display event loop */
   comp->wl.loop = wl_display_get_event_loop(comp->wl.display);

   /* get the event loop fd */
   fd = wl_event_loop_get_fd(comp->wl.loop);

   /* add the event loop fd to main ecore */
   comp->fd_hdlr = 
     ecore_main_fd_handler_add(fd, ECORE_FD_READ, 
                               _e_comp_cb_read, comp, NULL, NULL);

   /* add an idler for flushing clients */
   comp->idler = ecore_idle_enterer_add(_e_comp_cb_idle, comp);

   /* add the socket */
   if (wl_display_add_socket(comp->wl.display, NULL))
     {
        ERR("Failed to add socket: %m");
        goto sock_err;
     }

   wl_event_loop_dispatch(comp->wl.loop, 0);

   /* set a reference to the compositor */
   _e_comp = comp;

   return EINA_TRUE;

sock_err:
   /* destroy the idler */
   if (comp->idler) ecore_idler_del(comp->idler);

   /* destroy the fd handler */
   if (comp->fd_hdlr) ecore_main_fd_handler_del(comp->fd_hdlr);

   /* destroy the input loop */
   if (comp->wl.input_loop) wl_event_loop_destroy(comp->wl.input_loop);

#ifdef HAVE_WAYLAND_EGL
   /* destroy the egl display */
   if (comp->egl.display) eglTerminate(comp->egl.display);
   eglReleaseThread();
#endif

   /* free any planes */
   EINA_LIST_FREE(comp->planes, p)
     e_plane_shutdown(p);

global_err:
   /* destroy the previously created display */
   if (comp->wl.display) wl_display_destroy(comp->wl.display);

   return EINA_FALSE;
}

EAPI Eina_Bool 
e_compositor_shutdown(E_Compositor *comp)
{
   E_Surface *es;
   E_Plane *p;

   /* free any surfaces */
   EINA_LIST_FREE(comp->surfaces, es)
     e_surface_destroy(es);

   /* free any planes */
   EINA_LIST_FREE(comp->planes, p)
     e_plane_shutdown(p);

#ifdef HAVE_WAYLAND_EGL
   /* destroy the egl display */
   if (comp->egl.display) eglTerminate(comp->egl.display);
   eglReleaseThread();
#endif

   /* destroy the input loop */
   if (comp->wl.input_loop) wl_event_loop_destroy(comp->wl.input_loop);

   /* destroy the fd handler */
   if (comp->fd_hdlr) ecore_main_fd_handler_del(comp->fd_hdlr);

   /* destroy the previously created display */
   if (comp->wl.display) wl_display_terminate(comp->wl.display);

   return EINA_TRUE;
}

EAPI E_Compositor *
e_compositor_get(void)
{
   return _e_comp;
}

EAPI void 
e_compositor_plane_stack(E_Compositor *comp, E_Plane *plane, E_Plane *above)
{
   /* add this plane to the compositors list */
   comp->planes = eina_list_prepend_relative(comp->planes, plane, above);
}

EAPI int 
e_compositor_input_read(int fd EINA_UNUSED, unsigned int mask EINA_UNUSED, void *data)
{
   E_Compositor *comp;

   if (!(comp = data)) return 1;
   wl_event_loop_dispatch(comp->wl.input_loop, 0);
   return 1;
}

EAPI void 
e_compositor_damage_calculate(E_Compositor *comp)
{
   E_Plane *plane;
   E_Surface *es;
   Eina_List *l;
   pixman_region32_t clip, opaque;

   /* check for valid compositor */
   if (!comp) return;

   pixman_region32_init(&clip);

   /* loop the planes */
   EINA_LIST_FOREACH(comp->planes, l, plane)
     {
        pixman_region32_copy(&plane->clip, &clip);

        pixman_region32_init(&opaque);

        /* loop the surfaces */
        EINA_LIST_FOREACH(comp->surfaces, l, es)
          {
             if (es->plane != plane) continue;
             e_surface_damage_calculate(es, &opaque);
          }

        pixman_region32_union(&clip, &clip, &opaque);
        pixman_region32_fini(&opaque);
     }

   pixman_region32_fini(&clip);

   /* loop the surfaces */
   EINA_LIST_FOREACH(comp->surfaces, l, es)
     {
        if (!es->buffer.keep)
          e_buffer_reference(&es->buffer.reference, NULL);
     }
}

EAPI unsigned int 
e_compositor_get_time(void)
{
   struct timeval tv;

   gettimeofday(&tv, NULL);
   return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

EAPI E_Surface *
e_compositor_surface_find(E_Compositor *comp, Evas_Coord x, Evas_Coord y)
{
   E_Surface *es;
   Eina_List *l;

   /* loop the surfaces and try to find one which has these
    * coordinates contained in it's input region */
   EINA_LIST_REVERSE_FOREACH(comp->surfaces, l, es)
     {
        if (pixman_region32_contains_point(&es->input, x, y, NULL))
          return es;
     }

   return NULL;
}

EAPI void 
e_compositor_repick(E_Compositor *comp)
{
   E_Input *seat;
   Eina_List *l;

   if (!comp->focus) return;
   EINA_LIST_FOREACH(comp->inputs, l, seat)
     e_input_repick(seat);
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
_e_comp_cb_bind_manager(struct wl_client *client, void *data EINA_UNUSED, unsigned int version EINA_UNUSED, unsigned int id)
{
   /* add the data device manager to the client */
   wl_client_add_object(client, &wl_data_device_manager_interface, 
                        &_e_manager_interface, id, NULL);
}

static void 
_e_comp_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Compositor *comp;
   E_Surface *es;

   printf("Comp Surface Create: %d\n", id);

   /* try to cast to our compositor */
   if (!(comp = resource->data)) return;

   /* try to create a new surface */
   if (!(es = e_surface_new(client, id)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   /* set destroy callback */
   wl_resource_set_destructor(es->wl.resource, _e_comp_cb_surface_destroy);

   /* ask the renderer to create any internal representation of this surface 
    * that it may need */
   if (!comp->renderer->surface_create(es))
     {
        e_surface_destroy(es);
        return;
     }

   /* set surface plane to compositor primary plane */
   es->plane = &comp->plane;

   printf("\tAdding Surface: %p\n", es);

   /* add this surface to the compositors list */
   comp->surfaces = eina_list_append(comp->surfaces, es);
}

static void 
_e_comp_cb_surface_destroy(struct wl_resource *resource)
{
   E_Surface *es;

   if (!(es = wl_resource_get_user_data(resource)))
     return;

   printf("Surface Destroy: %p\n", es);

   /* remove this surface from the compositor */
   _e_comp->surfaces = eina_list_remove(_e_comp->surfaces, es);

   es->wl.resource = NULL;
   e_surface_destroy(es);
}

static void 
_e_comp_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Compositor *comp;
   E_Region *reg;

   /* try to cast to our compositor */
   if (!(comp = resource->data)) return;

   /* try to create a new region */
   if (!(reg = e_region_new(client, id)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_resource_set_destructor(reg->resource, _e_comp_cb_region_destroy);
}

static void 
_e_comp_cb_region_destroy(struct wl_resource *resource)
{
   E_Region *reg;

   /* try to get the region from this resource */
   if (!(reg = wl_resource_get_user_data(resource)))
     return;

   /* free the region */
   pixman_region32_fini(&reg->region);

   /* free the structure */
   E_FREE(reg);
}

static Eina_Bool 
_e_comp_cb_read(void *data, Ecore_Fd_Handler *hdl EINA_UNUSED)
{
   E_Compositor *comp;

   if ((comp = data))
     {
        wl_event_loop_dispatch(comp->wl.loop, 0);
        wl_display_flush_clients(comp->wl.display);
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_comp_cb_idle(void *data)
{
   E_Compositor *comp;

   if ((comp = data))
     {
        /* flush any clients before we idle */
        wl_display_flush_clients(comp->wl.display);
     }

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_data_device_cb_get(struct wl_client *client, struct wl_resource *resource EINA_UNUSED, unsigned int id, struct wl_resource *seat_resource)
{
   E_Input *seat;
   struct wl_resource *res;

   if (!(seat = wl_resource_get_user_data(seat_resource))) return;

   res = wl_client_add_object(client, &wl_data_device_interface, 
                              &_e_data_device_interface, id, seat);

   wl_list_insert(&seat->drag_resources, wl_resource_get_link(res));
   wl_resource_set_destructor(res, _e_comp_data_device_cb_unbind);
}

static void 
_e_comp_data_device_cb_unbind(struct wl_resource *resource)
{
   wl_list_remove(wl_resource_get_link(resource));
   free(resource);
}

static Eina_Bool 
_e_comp_xkb_init(E_Compositor *comp)
{
   if (!(comp->xkb_context = xkb_context_new(0)))
     return EINA_FALSE;

   if (!comp->xkb_names.rules)
     comp->xkb_names.rules = strdup("evdev");
   if (!comp->xkb_names.model)
     comp->xkb_names.model = strdup("pc105");
   if (!comp->xkb_names.layout)
     comp->xkb_names.layout = strdup("us");

   return EINA_TRUE;
}
