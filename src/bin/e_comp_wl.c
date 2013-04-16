#include "e.h"
#include "e_comp_wl.h"
#include "e_surface.h"
#include <sys/mman.h>

/* compositor function prototypes */
static void _e_comp_wl_cb_bind(struct wl_client *client, void *data EINA_UNUSED, unsigned int version EINA_UNUSED, unsigned int id);
static Eina_Bool _e_comp_wl_cb_read(void *data EINA_UNUSED, Ecore_Fd_Handler *hdl EINA_UNUSED);
static Eina_Bool _e_comp_wl_cb_idle(void *data EINA_UNUSED);
static Eina_Bool _e_comp_wl_cb_module_idle(void *data EINA_UNUSED);

/* compositor interface prototypes */
static void _e_comp_wl_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_comp_wl_cb_surface_destroy(struct wl_resource *resource);
static void _e_comp_wl_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_comp_wl_cb_region_destroy(struct wl_resource *resource);

/* input function prototypes */
static Eina_Bool _e_comp_wl_input_init(void);
static void _e_comp_wl_input_shutdown(void);
static void _e_comp_wl_input_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id);
static void _e_comp_wl_input_cb_unbind(struct wl_resource *resource);
static struct xkb_keymap *_e_comp_wl_input_keymap_get(void);
static int _e_comp_wl_input_keymap_fd_get(off_t size);
static E_Wayland_Keyboard_Info *_e_comp_wl_input_keyboard_info_get(struct xkb_keymap *keymap);

/* input interface prototypes */
static void _e_comp_wl_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_comp_wl_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_comp_wl_input_cb_touch_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);

/* pointer function prototypes */
static void _e_comp_wl_pointer_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_comp_wl_pointer_configure(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
static void _e_comp_wl_pointer_unmap(E_Wayland_Surface *ews);

/* pointer interface prototypes */
static void _e_comp_wl_pointer_cb_cursor_set(struct wl_client *client, struct wl_resource *resource, unsigned int serial, struct wl_resource *surface_resource, int x, int y);

/* region interface prototypes */
static void _e_comp_wl_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_comp_wl_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);
static void _e_comp_wl_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);

/* surface function prototypes */
static void _e_comp_wl_surface_cb_pending_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_comp_wl_surface_cb_frame_destroy(struct wl_resource *resource);
static void _e_comp_wl_surface_buffer_reference(E_Wayland_Surface *ews, struct wl_buffer *buffer);
static void _e_comp_wl_surface_buffer_reference_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);

/* surface interface prototypes */
static void _e_comp_wl_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_comp_wl_surface_cb_attach(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *buffer_resource, int x, int y);
static void _e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);
static void _e_comp_wl_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, unsigned int callback);
static void _e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource);
static void _e_comp_wl_surface_cb_input_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource);
static void _e_comp_wl_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);

/* local wayland interfaces */
static const struct wl_compositor_interface _e_compositor_interface = 
{
   _e_comp_wl_cb_surface_create,
   _e_comp_wl_cb_region_create
};

static const struct wl_seat_interface _e_input_interface = 
{
   _e_comp_wl_input_cb_pointer_get,
   _e_comp_wl_input_cb_keyboard_get,
   _e_comp_wl_input_cb_touch_get,
};

static const struct wl_pointer_interface _e_pointer_interface = 
{
   _e_comp_wl_pointer_cb_cursor_set
};

static const struct wl_region_interface _e_region_interface = 
{
   _e_comp_wl_region_cb_destroy,
   _e_comp_wl_region_cb_add,
   _e_comp_wl_region_cb_subtract
};

static const struct wl_surface_interface _e_surface_interface = 
{
   _e_comp_wl_surface_cb_destroy,
   _e_comp_wl_surface_cb_attach,
   _e_comp_wl_surface_cb_damage,
   _e_comp_wl_surface_cb_frame,
   _e_comp_wl_surface_cb_opaque_region_set,
   _e_comp_wl_surface_cb_input_region_set,
   _e_comp_wl_surface_cb_commit,
   NULL // cb_buffer_transform_set
};

/* local variables */
static Ecore_Idler *_module_idler = NULL;

/* external variables */
E_Wayland_Compositor *_e_wl_comp;

/* external functions */
EINTERN Eina_Bool 
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

   /* add an idler for deferred shell module loading */
   _module_idler = ecore_idler_add(_e_comp_wl_cb_module_idle, NULL);

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

   /* try to add a display socket */
   if (wl_display_add_socket(_e_wl_comp->wl.display, NULL) < 0)
     {
        ERR("Could not add a Wayland Display socket: %m");
        goto err;
     }

   /* return success */
   return EINA_TRUE;

err:
   /* remove the module idler */
   if (_module_idler) ecore_idler_del(_module_idler);

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

EINTERN void 
e_comp_wl_shutdown(void)
{
   E_Module *mod = NULL;

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

   /* disable the loaded shell module */
   /* TODO: we should have a config variable somewhere to store which 
    * shell we want to unload (tablet, mobile, etc) */
   if ((mod = e_module_find("wl_desktop_shell")))
     e_module_disable(mod);
}

EAPI unsigned int 
e_comp_wl_time_get(void)
{
   struct timeval tm;

   gettimeofday(&tm, NULL);
   return (tm.tv_sec * 1000 + tm.tv_usec / 1000);
}

EAPI void 
e_comp_wl_input_modifiers_update(unsigned int serial)
{
   struct wl_keyboard *kbd;
   struct wl_keyboard_grab *grab;
   unsigned int pressed = 0, latched = 0, locked = 0, group = 0;
   Eina_Bool changed = EINA_FALSE;

   /* check for valid keyboard */
   if (!(kbd = _e_wl_comp->input->wl.seat.keyboard)) 
     return;

   /* try to get the current keyboard's grab interface. 
    * Fallback to the default grab */
   if (!(grab = kbd->grab)) grab = &kbd->default_grab;

   pressed = xkb_state_serialize_mods(_e_wl_comp->input->xkb.state, 
                                      XKB_STATE_DEPRESSED);
   latched = xkb_state_serialize_mods(_e_wl_comp->input->xkb.state, 
                                      XKB_STATE_LATCHED);
   locked = xkb_state_serialize_mods(_e_wl_comp->input->xkb.state, 
                                     XKB_STATE_LOCKED);
   group = xkb_state_serialize_group(_e_wl_comp->input->xkb.state, 
                                     XKB_STATE_EFFECTIVE);

   if ((pressed != kbd->modifiers.mods_depressed) || 
       (latched != kbd->modifiers.mods_latched) || 
       (locked != kbd->modifiers.mods_locked) || 
       (group != kbd->modifiers.group))
     changed = EINA_TRUE;

   kbd->modifiers.mods_depressed = pressed;
   kbd->modifiers.mods_latched = latched;
   kbd->modifiers.mods_locked = locked;
   kbd->modifiers.group = group;

   /* TODO: update leds ? */

   if (changed)
     grab->interface->modifiers(grab, serial, 
                                kbd->modifiers.mods_depressed, 
                                kbd->modifiers.mods_latched, 
                                kbd->modifiers.mods_locked,
                                kbd->modifiers.group);
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

static Eina_Bool 
_e_comp_wl_cb_module_idle(void *data EINA_UNUSED)
{
   E_Module *mod = NULL;

   /* if we are still in the process of loading modules, then we will wait */
   if (e_module_loading_get()) return ECORE_CALLBACK_RENEW;

   /* try to find the shell module, and create it if not found
    * 
    * TODO: we should have a config variable somewhere to store which 
    * shell we want to load (tablet, mobile, etc) */
   if (!(mod = e_module_find("wl_desktop_shell")))
     mod = e_module_new("wl_desktop_shell");

   /* if we have the module now, load it */
   if (mod) 
     {
        e_module_enable(mod);
        _module_idler = NULL;

        /* flush any pending events
         * 
         * NB: This advertises out any globals so it needs to be deferred 
         * until after the shell has been loaded */
        wl_event_loop_dispatch(_e_wl_comp->wl.loop, 0);

        return ECORE_CALLBACK_CANCEL;
     }

   return ECORE_CALLBACK_RENEW;
}

/* compositor interface functions */
static void 
_e_comp_wl_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Wayland_Surface *ews = NULL;

   /* try to allocate space for a new surface */
   if (!(ews = E_NEW(E_Wayland_Surface, 1)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   /* initialize the destroy signal */
   wl_signal_init(&ews->wl.surface.resource.destroy_signal);

   /* initialize the link */
   wl_list_init(&ews->wl.link);

   /* initialize the lists of frames */
   wl_list_init(&ews->wl.frames);
   wl_list_init(&ews->pending.frames);

   ews->wl.surface.resource.client = NULL;

   /* set destroy function for pending buffers */
   ews->pending.buffer_destroy.notify = 
     _e_comp_wl_surface_cb_pending_buffer_destroy;

   /* initialize regions */
   pixman_region32_init(&ews->region.opaque);
   pixman_region32_init(&ews->region.damage);
   pixman_region32_init(&ews->region.clip);
   pixman_region32_init_rect(&ews->region.input, INT32_MIN, INT32_MIN, 
                             UINT32_MAX, UINT32_MAX);

   /* initialize pending regions */
   pixman_region32_init(&ews->pending.opaque);
   pixman_region32_init(&ews->pending.damage);
   pixman_region32_init_rect(&ews->pending.input, INT32_MIN, INT32_MIN, 
                             UINT32_MAX, UINT32_MAX);

   /* set some properties of the surface */
   ews->wl.surface.resource.destroy = _e_comp_wl_cb_surface_destroy;
   ews->wl.surface.resource.object.id = id;
   ews->wl.surface.resource.object.interface = &wl_surface_interface;
   ews->wl.surface.resource.object.implementation = 
     (void (**)(void))&_e_surface_interface;
   ews->wl.surface.resource.data = ews;

   /* add this surface to the client */
   wl_client_add_resource(client, &ews->wl.surface.resource);

   /* add this surface to the list of surfaces */
   _e_wl_comp->surfaces = eina_list_append(_e_wl_comp->surfaces, ews);
}

static void 
_e_comp_wl_cb_surface_destroy(struct wl_resource *resource)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Surface_Frame_Callback *cb = NULL, *ncb = NULL;

   /* try to get the surface from this resource */
   if (!(ews = container_of(resource, E_Wayland_Surface, wl.surface.resource)))
     return;

   /* if this surface is mapped, unmap it */
   if (ews->mapped)
     {
        if (ews->unmap) ews->unmap(ews);
     }

   /* loop any pending surface frame callbacks and destroy them */
   wl_list_for_each_safe(cb, ncb, &ews->pending.frames, wl.link)
     wl_resource_destroy(&cb->wl.resource);

   /* clear any pending regions */
   pixman_region32_fini(&ews->pending.damage);
   pixman_region32_fini(&ews->pending.opaque);
   pixman_region32_fini(&ews->pending.input);

   /* remove the pending buffer from the list */
   if (ews->pending.buffer)
     wl_list_remove(&ews->pending.buffer_destroy.link);

   /* dereference any existing buffers */
   _e_comp_wl_surface_buffer_reference(ews, NULL);

   /* clear any active regions */
   pixman_region32_fini(&ews->region.damage);
   pixman_region32_fini(&ews->region.opaque);
   pixman_region32_fini(&ews->region.input);
   pixman_region32_fini(&ews->region.clip);

   /* loop any active surface frame callbacks and destroy them */
   wl_list_for_each_safe(cb, ncb, &ews->wl.frames, wl.link)
     wl_resource_destroy(&cb->wl.resource);

   /* remove this surface from the compositor's list of surfaces */
   _e_wl_comp->surfaces = eina_list_remove(_e_wl_comp->surfaces, ews);

   /* free the allocated surface structure */
   E_FREE(ews);
}

static void 
_e_comp_wl_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Wayland_Region *ewr = NULL;

   /* try to allocate space for a new region */
   if (!(ewr = E_NEW_RAW(E_Wayland_Region, 1)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   pixman_region32_init(&ewr->region);

   /* set some properties of the region */
   ewr->wl.resource.destroy = _e_comp_wl_cb_region_destroy;
   ewr->wl.resource.object.id = id;
   ewr->wl.resource.object.interface = &wl_region_interface;
   ewr->wl.resource.object.implementation = 
     (void (**)(void))&_e_region_interface;
   ewr->wl.resource.data = ewr;

   /* add this region to the client */
   wl_client_add_resource(client, &ewr->wl.resource);
}

static void 
_e_comp_wl_cb_region_destroy(struct wl_resource *resource)
{
   E_Wayland_Region *ewr = NULL;

   /* try to get the region from this resource */
   if (!(ewr = container_of(resource, E_Wayland_Region, wl.resource)))
     return;

   /* tell pixman we are finished with this region */
   pixman_region32_fini(&ewr->region);

   /* free the allocated region structure */
   E_FREE(ewr);
}

/* input functions */
static Eina_Bool 
_e_comp_wl_input_init(void)
{
   struct xkb_keymap *keymap;

   /* try to allocate space for a new compositor */
   if (!(_e_wl_comp->input = E_NEW(E_Wayland_Input, 1)))
     return EINA_FALSE;

   /* initialize the seat */
   wl_seat_init(&_e_wl_comp->input->wl.seat);

   /* try to add this input to the diplay's list of globals */
   if (!wl_display_add_global(_e_wl_comp->wl.display, &wl_seat_interface, 
                              _e_wl_comp->input, _e_comp_wl_input_cb_bind))
     {
        ERR("Could not add Input to Wayland Display Globals: %m");
        goto err;
     }

   _e_wl_comp->input->pointer.surface = NULL;
   _e_wl_comp->input->pointer.surface_destroy.notify = 
     _e_comp_wl_pointer_cb_destroy;
   _e_wl_comp->input->pointer.hot.x = 16;
   _e_wl_comp->input->pointer.hot.y = 16;

   /* initialize wayland pointer */
   wl_pointer_init(&_e_wl_comp->input->wl.pointer);

   /* tell the seat about this pointer */
   wl_seat_set_pointer(&_e_wl_comp->input->wl.seat, 
                       &_e_wl_comp->input->wl.pointer);

   /* set flag to indicate we have a pointer */
   _e_wl_comp->input->has_pointer = EINA_TRUE;

   /* create the xkb context */
   _e_wl_comp->xkb.context = xkb_context_new(0);

   /* try to fetch the keymap */
   if ((keymap = _e_comp_wl_input_keymap_get()))
     {
        /* try to create new keyboard info */
        _e_wl_comp->input->xkb.info = 
          _e_comp_wl_input_keyboard_info_get(keymap);

        /* create new xkb state */
        _e_wl_comp->input->xkb.state = xkb_state_new(keymap);

        /* unreference the keymap */
        xkb_map_unref(keymap);
     }

   /* initialize the keyboard */
   wl_keyboard_init(&_e_wl_comp->input->wl.keyboard);

   /* tell the seat about this keyboard */
   wl_seat_set_keyboard(&_e_wl_comp->input->wl.seat, 
                        &_e_wl_comp->input->wl.keyboard);

   /* set flag to indicate we have a keyboard */
   _e_wl_comp->input->has_keyboard = EINA_TRUE;

   wl_list_init(&_e_wl_comp->input->wl.link);

   /* append this input to the list */
   _e_wl_comp->seats = eina_list_append(_e_wl_comp->seats, _e_wl_comp->input);

   /* emit a signal saying that input has been initialized */
   wl_signal_emit(&_e_wl_comp->signals.seat, _e_wl_comp->input);

   /* return success */
   return EINA_TRUE;

err:
   /* release the wl_seat */
   wl_seat_release(&_e_wl_comp->input->wl.seat);

   /* free the allocated input structure */
   E_FREE(_e_wl_comp->input);

   /* return failure */
   return EINA_FALSE;
}

static void 
_e_comp_wl_input_shutdown(void)
{
   /* safety check */
   if (!_e_wl_comp->input) return;

   /* destroy keyboard */
   if (_e_wl_comp->input->xkb.info)
     {
        /* if we have a keymap, unreference it */
        if (_e_wl_comp->input->xkb.info->keymap)
          xkb_map_unref(_e_wl_comp->input->xkb.info->keymap);

        /* if we have a keymap mmap'd area, unmap it */
        if (_e_wl_comp->input->xkb.info->area)
          munmap(_e_wl_comp->input->xkb.info->area, 
                 _e_wl_comp->input->xkb.info->size);

        /* if we created an fd for keyboard input, close it */
        if (_e_wl_comp->input->xkb.info->fd) 
          close(_e_wl_comp->input->xkb.info->fd);

        /* free the allocated keyboard info structure */
        E_FREE(_e_wl_comp->input->xkb.info);
     }

   /* unreference the xkb state we created */
   if (_e_wl_comp->input->xkb.state) 
     xkb_state_unref(_e_wl_comp->input->xkb.state);

   /* unreference the xkb context we created */
   if (_e_wl_comp->xkb.context)
     xkb_context_unref(_e_wl_comp->xkb.context);

   /* TODO: destroy pointer surface
    * 
    * NB: Currently, we do not create one */

   wl_list_remove(&_e_wl_comp->input->wl.link);

   /* release the seat */
   wl_seat_release(&_e_wl_comp->input->wl.seat);

   /* free the allocated input structure */
   E_FREE(_e_wl_comp->input);
}

static void 
_e_comp_wl_input_cb_bind(struct wl_client *client, void *data, unsigned int version EINA_UNUSED, unsigned int id)
{
   struct wl_seat *seat = NULL;
   struct wl_resource *resource = NULL;
   enum wl_seat_capability caps = 0;

   /* try to cast data to our seat */
   if (!(seat = data)) return;

   /* add the seat object to the client */
   resource = 
     wl_client_add_object(client, &wl_seat_interface, 
                          &_e_input_interface, id, data);
   wl_list_insert(&seat->base_resource_list, &resource->link);

   /* set resource destroy callback */
   resource->destroy = _e_comp_wl_input_cb_unbind;

   /* set capabilities based on seat */
   if (seat->pointer) caps |= WL_SEAT_CAPABILITY_POINTER;
   if (seat->keyboard) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
   if (seat->touch) caps |= WL_SEAT_CAPABILITY_TOUCH;

   /* inform the resource about the seat capabilities */
   wl_seat_send_capabilities(resource, caps);
}

static void 
_e_comp_wl_input_cb_unbind(struct wl_resource *resource)
{
   /* remove the link */
   wl_list_remove(&resource->link);

   /* free the resource */
   free(resource);
}

static struct xkb_keymap *
_e_comp_wl_input_keymap_get(void)
{
   struct xkb_rule_names names;

   memset(&names, 0, sizeof(names));

   /* if we are running under X11, try to get the xkb rule names atom */
   if (getenv("DISPLAY"))
     {
        Ecore_X_Atom rules = 0;
        Ecore_X_Window root = 0;
        int len = 0;
        unsigned char *data;

        /* TODO: FIXME: NB:
         * 
         * The below commented out code is for using the "already" configured 
         * E xkb settings in the wayland clients. The only Major problem with 
         * that is: That the E_Config_XKB_Layout does not define a 
         * 'RULES' which we need ....
         * 
         */

        /* E_Config_XKB_Layout *kbd_layout; */
        /* kbd_layout = e_xkb_layout_get(); */
        /* names.model = strdup(kbd_layout->model); */
        /* names.layout = strdup(kbd_layout->name); */

        root = ecore_x_window_root_first_get();
        rules = ecore_x_atom_get("_XKB_RULES_NAMES");
        ecore_x_window_prop_property_get(root, rules, ECORE_X_ATOM_STRING, 
                                         1024, &data, &len);

        if ((data) && (len > 0))
          {
             names.rules = strdup((const char *)data);
             data += strlen((const char *)data) + 1;
             names.model = strdup((const char *)data);
             data += strlen((const char *)data) + 1;
             names.layout = strdup((const char *)data);
//             free(data);
          }
     }

   printf("Keymap\n");
   printf("\tRules: %s\n", names.rules);
   printf("\tModel: %s\n", names.model);
   printf("\tLayout: %s\n", names.layout);

   return xkb_map_new_from_names(_e_wl_comp->xkb.context, &names, 0);
}

static int 
_e_comp_wl_input_keymap_fd_get(off_t size)
{
   char tmp[PATH_MAX];
   const char *path;
   int fd = 0;
   long flags;

   if (!(path = getenv("XDG_RUNTIME_DIR")))
     return -1;

   strcpy(tmp, path);
   strcat(tmp, "/e-wl-keymap-XXXXXX");

   if ((fd = mkstemp(tmp)) < 0)
     return -1;

   /* TODO: really should error check the returns here */

   flags = fcntl(fd, F_GETFD);
   fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

   if (ftruncate(fd, size) < 0)
     {
        close(fd);
        return -1;
     }

   unlink(tmp);
   return fd;
}

static E_Wayland_Keyboard_Info *
_e_comp_wl_input_keyboard_info_get(struct xkb_keymap *keymap)
{
   E_Wayland_Keyboard_Info *info = NULL;
   char *tmp;

   /* try to allocate space for the keyboard info structure */
   if (!(info = E_NEW(E_Wayland_Keyboard_Info, 1)))
     return NULL;

   info->keymap = xkb_map_ref(keymap);

   /* init modifiers */
   info->mod_shift = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
   info->mod_caps = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
   info->mod_ctrl = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_CTRL);
   info->mod_alt = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_ALT);
   info->mod_super = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_LOGO);

   /* try to get a string of this keymap */
   if (!(tmp = xkb_map_get_as_string(keymap)))
     {
        printf("Could not map get as string\n");
     }

   info->size = strlen(tmp) + 1;

   /* try to create an fd we can listen on for input */
   info->fd = _e_comp_wl_input_keymap_fd_get(info->size);
   if (info->fd < 0)
     {
        printf("Could not create keymap fd\n");
     }

   info->area = 
     mmap(NULL, info->size, PROT_READ | PROT_WRITE, MAP_SHARED, info->fd, 0);

   /* TODO: error check mmap */

   strcpy(info->area, tmp);
   free(tmp);

   return info;
}

/* input interface functions */
static void 
_e_comp_wl_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Wayland_Input *input = NULL;
   struct wl_resource *ptr = NULL;

   /* try to cast the resource data to our input structure */
   if (!(input = resource->data)) return;

   /* check that input has a pointer */
   if (!input->has_pointer) return;

   /* add a pointer object to the client */
   ptr = wl_client_add_object(client, &wl_pointer_interface, 
                              &_e_pointer_interface, id, input);
   wl_list_insert(&input->wl.seat.pointer->resource_list, &ptr->link);

   /* set pointer destroy callback */
   ptr->destroy = _e_comp_wl_input_cb_unbind;

   /* if the pointer has a focused surface, set it */
   if ((input->wl.seat.pointer->focus) && 
       (input->wl.seat.pointer->focus->resource.client == client))
     {
        /* tell pointer which surface is focused */
        wl_pointer_set_focus(input->wl.seat.pointer, 
                             input->wl.seat.pointer->focus, 
                             input->wl.seat.pointer->x, 
                             input->wl.seat.pointer->y);
     }
}

static void 
_e_comp_wl_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Wayland_Input *input = NULL;
   struct wl_resource *kbd = NULL;

   /* try to cast the resource data to our input structure */
   if (!(input = resource->data)) return;

   /* check that input has a keyboard */
   if (!input->has_keyboard) return;

   /* add a keyboard object to the client */
   kbd = wl_client_add_object(client, &wl_keyboard_interface, NULL, id, input);
   wl_list_insert(&input->wl.seat.keyboard->resource_list, &kbd->link);

   /* set keyboard destroy callback */
   kbd->destroy = _e_comp_wl_input_cb_unbind;

   /* send the current keymap to the keyboard object */
   wl_keyboard_send_keymap(kbd, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, 
                           input->xkb.info->fd, input->xkb.info->size);

   /* test if keyboard has a focused client */
   if ((input->wl.seat.keyboard->focus) && 
       (input->wl.seat.keyboard->focus->resource.client == client))
     {
        /* set keyboard focus */
        wl_keyboard_set_focus(input->wl.seat.keyboard, 
                              input->wl.seat.keyboard->focus);

        /* update the keyboard focus in the data device */
        wl_data_device_set_keyboard_focus(&input->wl.seat);
     }
}

static void 
_e_comp_wl_input_cb_touch_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Wayland_Input *input = NULL;
   struct wl_resource *tch = NULL;

   /* try to cast the resource data to our input structure */
   if (!(input = resource->data)) return;

   /* check that input has a touch */
   if (!input->has_touch) return;

   /* add a touch object to the client */
   tch = wl_client_add_object(client, &wl_touch_interface, NULL, id, input);
   wl_list_insert(&input->wl.seat.touch->resource_list, &tch->link);

   /* set touch destroy callback */
   tch->destroy = _e_comp_wl_input_cb_unbind;
}

/* pointer functions */
static void 
_e_comp_wl_pointer_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Wayland_Input *input = NULL;

   /* try to get the input from this listener */
   if (!(input = container_of(listener, E_Wayland_Input, pointer.surface_destroy)))
     return;

   input->pointer.surface = NULL;
}

static void 
_e_comp_wl_pointer_configure(E_Wayland_Surface *ews, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Wayland_Input *input = NULL;
   E_Wayland_Surface *focus = NULL;

   /* safety check */
   if (!ews) return;

   /* see if this surface has 'input' */
   if (!(input = ews->input)) return;

   input->pointer.hot.x -= x;
   input->pointer.hot.y -= y;

   /* configure the surface geometry */
   ews->geometry.x = x;
   ews->geometry.h = h;
   ews->geometry.w = w;
   ews->geometry.h = h;
   ews->geometry.changed = EINA_TRUE;

   /* tell pixman we are finished with this region */
   pixman_region32_fini(&ews->pending.input);

   /* reinitalize the pending input region */
   pixman_region32_init(&ews->pending.input);

   /* do we have a focused surface ? */
   if ((focus = (E_Wayland_Surface *)input->wl.seat.pointer->focus))
     {
        /* NB: Ideally, I wanted to use the e_pointer methods here so that 
         * the cursor would match the E theme, however Wayland currently 
         * provides NO Method to get the cursor name :( so we are stuck 
         * using the pixels from their cursor surface */

        /* is it mapped ? */
        if ((focus->mapped) && (focus->ee))
          {
             Ecore_Window win;

             /* try to get the ecore_window */
             if ((win = ecore_evas_window_get(focus->ee)))
               {
                  void *pixels;

                  /* grab the pixels from the cursor surface */
                  if ((pixels = wl_shm_buffer_get_data(ews->reference.buffer)))
                    {
                       Ecore_X_Cursor cur;

                       /* create the new X cursor with this image */
                       cur = ecore_x_cursor_new(win, pixels, w, h,
                                                input->pointer.hot.x, 
                                                input->pointer.hot.y);

                       /* set the cursor on this window */
                       ecore_x_window_cursor_set(win, cur);

                       /* free the cursor */
                       ecore_x_cursor_free(cur);
                    }
                  else
                    ecore_x_window_cursor_set(win, 0);
               }
          }
     }
}

static void 
_e_comp_wl_pointer_unmap(E_Wayland_Surface *ews)
{
   E_Wayland_Input *input = NULL;

   /* safety check */
   if (!ews) return;

   if (!(input = ews->input)) return;

   wl_list_remove(&input->pointer.surface_destroy.link);

   if (input->pointer.surface)
     input->pointer.surface->configure = NULL;

   input->pointer.surface = NULL;
}

/* pointer interface functions */
static void 
_e_comp_wl_pointer_cb_cursor_set(struct wl_client *client, struct wl_resource *resource, unsigned int serial, struct wl_resource *surface_resource, int x, int y)
{
   E_Wayland_Input *input = NULL;
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource data to our input structure */
   if (!(input = resource->data)) return;

   /* if we were passed in a surface, try to cast it to our structure */
   if (surface_resource) ews = (E_Wayland_Surface *)surface_resource->data;

   /* if this input has no pointer, get out */
   if (!input->has_pointer) return;

   /* if the input has no current focus, get out */
   if (!input->wl.seat.pointer->focus) return;

   if (input->wl.seat.pointer->focus->resource.client != client) return;
   if ((input->wl.seat.pointer->focus_serial - serial) > (UINT32_MAX / 2))
     return;

   /* is the passed in surface the same as our pointer surface ? */
   if ((ews) && (ews != input->pointer.surface))
     {
        if (ews->configure)
          {
             wl_resource_post_error(&ews->wl.surface.resource, 
                                    WL_DISPLAY_ERROR_INVALID_OBJECT, 
                                    "Surface already configured");
             return;
          }
     }

   /* if we have an existing pointer surface, unmap it */
   if (input->pointer.surface) 
     {
        /* call the unmap function */
        if (input->pointer.surface->unmap)
          input->pointer.surface->unmap(input->pointer.surface);
     }

   input->pointer.surface = ews;

   /* if we don't have a pointer surface, we are done here */
   if (!ews) return;

   /* set the destroy listener */
   wl_signal_add(&ews->wl.surface.resource.destroy_signal, 
                 &input->pointer.surface_destroy);

   /* set some properties on this surface */
   ews->unmap = _e_comp_wl_pointer_unmap;
   ews->configure = _e_comp_wl_pointer_configure;
   ews->input = input;

   /* update input structure with new values */
   input->pointer.hot.x = x;
   input->pointer.hot.y = y;

   if (ews->reference.buffer)
     {
        Evas_Coord bw = 0, bh = 0;

        /* grab the size of the buffer */
        bw = ews->reference.buffer->width;
        bh = ews->reference.buffer->height;

        /* configure the pointer surface */
        _e_comp_wl_pointer_configure(ews, 0, 0, bw, bh);
     }
}

/* region interface functions */
static void 
_e_comp_wl_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Wayland_Region *ewr = NULL;

   /* try to cast the resource data to our region structure */
   if (!(ewr = resource->data)) return;

   /* tell pixman to union this region with any previous one */
   pixman_region32_union_rect(&ewr->region, &ewr->region, x, y, w, h);
}

static void 
_e_comp_wl_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Wayland_Region *ewr = NULL;
   pixman_region32_t region;

   /* try to cast the resource data to our region structure */
   if (!(ewr = resource->data)) return;

   /* ask pixman to create a new temporary rect */
   pixman_region32_init_rect(&region, x, y, w, h);

   /* ask pixman to subtract this temp rect from the existing region */
   pixman_region32_subtract(&ewr->region, &ewr->region, &region);

   /* tell pixman we are finished with the temporary rect */
   pixman_region32_fini(&region);
}

/* surface functions */
static void 
_e_comp_wl_surface_cb_pending_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;

   /* try to get the surface from this listener */
   if (!(ews = container_of(listener, E_Wayland_Surface, 
                            pending.buffer_destroy)))
     return;

   /* set surface pending buffer to null */
   ews->pending.buffer = NULL;
}

static void 
_e_comp_wl_surface_cb_frame_destroy(struct wl_resource *resource)
{
   E_Wayland_Surface_Frame_Callback *cb = NULL;

   /* try to cast the resource data to our surface frame callback structure */
   if (!(cb = resource->data)) return;

   wl_list_remove(&cb->wl.link);

   /* free the allocated callback structure */
   E_FREE(cb);
}

static void 
_e_comp_wl_surface_buffer_reference(E_Wayland_Surface *ews, struct wl_buffer *buffer)
{
   /* check for valid surface */
   if (!ews) return;

   /* if the surface already has a buffer referenced and it is not the 
    * same as the one passed in */
   if ((ews->reference.buffer) && (buffer != ews->reference.buffer))
     {
        /* decrement the reference buffer busy count */
        ews->reference.buffer->busy_count--;

        /* if the compositor is finished with this referenced buffer, then 
         * we need to release it */
        if (ews->reference.buffer->busy_count == 0)
          {
             if (ews->reference.buffer->resource.client)
               wl_resource_queue_event(&ews->reference.buffer->resource, 
                                       WL_BUFFER_RELEASE);
          }

        /* remove the destroy link on the referenced buffer */
        wl_list_remove(&ews->reference.buffer_destroy.link);
     }

   /* if we are passed in a buffer and it is not the one referenced */
   if ((buffer) && (buffer != ews->reference.buffer))
     {
        /* increment busy count */
        buffer->busy_count++;

        /* setup destroy signal */
        wl_signal_add(&buffer->resource.destroy_signal, 
                      &ews->reference.buffer_destroy);
     }

   /* set buffer reference */
   ews->reference.buffer = buffer;

   /* setup destroy listener */
   ews->reference.buffer_destroy.notify = 
     _e_comp_wl_surface_buffer_reference_cb_destroy;
}

static void 
_e_comp_wl_surface_buffer_reference_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Wayland_Surface *ews = NULL;

   /* try to get the surface from this listener */
   ews = container_of(listener, E_Wayland_Surface, reference.buffer_destroy);
   if (!ews) return;

   /* set referenced buffer to null */
   ews->reference.buffer = NULL;
}

/* surface interface functionss */
static void 
_e_comp_wl_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_surface_cb_attach(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *buffer_resource, int x, int y)
{
   E_Wayland_Surface *ews = NULL;
   struct wl_buffer *buffer = NULL;

   /* try to cast the resource data to our surface structure */
   if (!(ews = resource->data)) return;

   if (buffer_resource) buffer = buffer_resource->data;

   /* reference any existing buffers */
   _e_comp_wl_surface_buffer_reference(ews, buffer);

   /* if we are setting a null buffer, then unmap the surface */
   if (!buffer)
     {
        if (ews->mapped)
          {
             if (ews->unmap) ews->unmap(ews);
          }
     }

   /* if we already have a pending buffer, remove the listener */
   if (ews->pending.buffer)
     wl_list_remove(&ews->pending.buffer_destroy.link);

   /* set some pending values */
   ews->pending.x = x;
   ews->pending.y = y;
   ews->pending.buffer = buffer;
//   if (buffer)
   ews->pending.new_buffer = EINA_TRUE;

   /* if we were given a buffer, initialize the destroy signal */
   if (buffer)
     wl_signal_add(&buffer->resource.destroy_signal, 
                   &ews->pending.buffer_destroy);
}

static void 
_e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource data to our surface structure */
   if (!(ews = resource->data)) return;

   /* tell pixman to add this damage to pending */
   pixman_region32_union_rect(&ews->pending.damage, &ews->pending.damage, 
                              x, y, w, h);
}

static void 
_e_comp_wl_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, unsigned int callback)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Surface_Frame_Callback *cb = NULL;

   /* try to cast the resource data to our surface structure */
   if (!(ews = resource->data)) return;

   /* try to allocate space for a new frame callback */
   if (!(cb = E_NEW(E_Wayland_Surface_Frame_Callback, 1)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   /* set some properties on the callback */
   cb->wl.resource.object.interface = &wl_callback_interface;
   cb->wl.resource.object.id = callback;
   cb->wl.resource.destroy = _e_comp_wl_surface_cb_frame_destroy;
   cb->wl.resource.client = client;
   cb->wl.resource.data = cb;

   /* add frame callback to client */
   wl_client_add_resource(client, &cb->wl.resource);

   /* add this callback to the surface list of pending frames */
   wl_list_insert(ews->pending.frames.prev, &cb->wl.link);
}

static void 
_e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource data to our surface structure */
   if (!(ews = resource->data)) return;

   if (region_resource)
     {
        E_Wayland_Region *ewr = NULL;

        /* copy this region to the pending opaque region */
        if ((ewr = region_resource->data))
          pixman_region32_copy(&ews->pending.opaque, &ewr->region);
     }
   else
     {
        /* tell pixman we are finished with this region */
        pixman_region32_fini(&ews->pending.opaque);

        /* reinitalize the pending opaque region */
        pixman_region32_init(&ews->pending.opaque);
     }
}

static void 
_e_comp_wl_surface_cb_input_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource data to our surface structure */
   if (!(ews = resource->data)) return;

   if (region_resource)
     {
        E_Wayland_Region *ewr = NULL;

        /* copy this region to the pending input region */
        if ((ewr = region_resource->data))
          pixman_region32_copy(&ews->pending.input, &ewr->region);
     }
   else
     {
        /* tell pixman we are finished with this region */
        pixman_region32_fini(&ews->pending.input);

        /* reinitalize the pending input region */
        pixman_region32_init_rect(&ews->pending.input, INT32_MIN, INT32_MIN, 
                                  UINT32_MAX, UINT32_MAX);
     }
}

static void 
_e_comp_wl_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Wayland_Surface *ews = NULL;
   Evas_Coord bw = 0, bh = 0;
   pixman_region32_t opaque;
   pixman_box32_t *rects;
   int n = 0;

   /* FIXME */

   /* try to cast the resource data to our surface structure */
   if (!(ews = resource->data)) return;

   /* if we have a pending buffer or a new pending buffer, attach it */
   if ((ews->pending.buffer) || (ews->pending.new_buffer))
     {
        /* reference the pending buffer */
        _e_comp_wl_surface_buffer_reference(ews, ews->pending.buffer);

        /* if the pending buffer is NULL, unmap the surface */
        if (!ews->pending.buffer)
          {
             if (ews->mapped)
               {
                  if (ews->unmap) ews->unmap(ews);
               }
          }
        else
          {
             if (ews->obj)
               {
                  void *data;

                  bw = ews->pending.buffer->width;
                  bh = ews->pending.buffer->height;

                  /* grab the pixel data from the buffer */
                  data = wl_shm_buffer_get_data(ews->pending.buffer);

                  /* send the pixel data to the smart object */
                  e_surface_image_set(ews->obj, bw, bh, data);
               }
          }
     }

   /* if we have a reference to a buffer, get it's size */
   if (ews->reference.buffer)
     {
        bw = ews->reference.buffer->width;
        bh = ews->reference.buffer->height;
     }

   /* if we have a new pending buffer, call configure */
   if ((ews->configure) && (ews->pending.new_buffer))
//     ews->configure(ews, ews->pending.x, ews->pending.y, bw, bh);
     ews->configure(ews, ews->geometry.x, ews->geometry.y, bw, bh);

   if (ews->pending.buffer)
     wl_list_remove(&ews->pending.buffer_destroy.link);

   /* set some pending values */
   ews->pending.buffer = NULL;
   ews->pending.x = 0;
   ews->pending.y = 0;
   ews->pending.new_buffer = EINA_FALSE;

   /* set surface damage */
   pixman_region32_union(&ews->region.damage, &ews->region.damage, 
                         &ews->pending.damage);
   pixman_region32_intersect_rect(&ews->region.damage, &ews->region.damage, 
                                  0, 0, ews->geometry.w, ews->geometry.h);

   /* empty any pending damage */
   pixman_region32_fini(&ews->pending.damage);
   pixman_region32_init(&ews->pending.damage);

   /* get the extent of the damage region */
   rects = pixman_region32_rectangles(&ews->region.damage, &n);
   while (n--)
     {
        pixman_box32_t *r;

        r = &rects[n];

        /* send damages to the image */
        e_surface_damage_add(ews->obj, r->x1, r->y1, r->x2, r->y2);
     }

   /* tell pixman we are finished with this region */
   /* pixman_region32_fini(&ews->region.damage); */

   /* reinitalize the damage region */
   /* pixman_region32_init(&ews->region.damage); */

   /* calculate new opaque region */
   pixman_region32_init_rect(&opaque, 0, 0, ews->geometry.w, ews->geometry.h);
   pixman_region32_intersect(&opaque, &opaque, &ews->pending.opaque);

   /* if new opaque region is not equal to the current one, then update */
   if (!pixman_region32_equal(&opaque, &ews->region.opaque))
     {
        pixman_region32_copy(&ews->region.opaque, &opaque);
        ews->geometry.changed = EINA_TRUE;
     }

   /* tell pixman we are done with this temporary region */
   pixman_region32_fini(&opaque);

   /* clear any existing input region */
   pixman_region32_fini(&ews->region.input);

   /* initialize a new input region */
   pixman_region32_init_rect(&ews->region.input, 0, 0, 
                             ews->geometry.w, ews->geometry.h);

   /* put pending input region into new input region */
   pixman_region32_intersect(&ews->region.input, &ews->region.input, 
                             &ews->pending.input);

   /* check for valid input region */
//   if (pixman_region32_not_empty(&ews->region.input))
     {
        /* get the extent of the input region */
        rects = pixman_region32_extents(&ews->region.input);

        /* update the smart object's input region */
        if (ews->obj)
          e_surface_input_set(ews->obj, rects->x1, rects->y1, 
                              (rects->x2 - rects->x1), 
                              (rects->y2 - rects->y1));
     }

   /* put any pending frame callbacks into active list */
   wl_list_insert_list(&ews->wl.frames, &ews->pending.frames);

   /* clear list of pending frame callbacks */
   wl_list_init(&ews->pending.frames);

   /* TODO: schedule repaint ?? */
}
