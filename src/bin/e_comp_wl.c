#include "e.h"
#include "e_comp_wl.h"
#include <sys/mman.h>

/* local function prototypes */
static void _e_comp_wl_cb_bind(struct wl_client *client, void *data EINA_UNUSED, unsigned int version EINA_UNUSED, unsigned int id);
static Eina_Bool _e_comp_wl_cb_read(void *data EINA_UNUSED, Ecore_Fd_Handler *hdl EINA_UNUSED);
static Eina_Bool _e_comp_wl_cb_idle(void *data EINA_UNUSED);

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
static void _e_comp_wl_input_modifiers_update(unsigned int serial);

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

/* compositor interface functions */
static void 
_e_comp_wl_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{

}

static void 
_e_comp_wl_cb_surface_destroy(struct wl_resource *resource)
{

}

static void 
_e_comp_wl_cb_region_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{

}

static void 
_e_comp_wl_cb_region_destroy(struct wl_resource *resource)
{

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

   /* printf("Names\n"); */
   /* printf("\tRules: %s\n", names.rules); */
   /* printf("\tModel: %s\n", names.model); */
   /* printf("\tLayout: %s\n", names.layout); */

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

static void 
_e_comp_wl_input_modifiers_update(unsigned int serial)
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
        /* FIXME: what to pass to wl_pointer_set_focus (x/y) */
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
                  Ecore_X_Cursor cur;

                  /* grab the pixels from the cursor surface */
                  pixels = wl_shm_buffer_get_data(ews->reference.buffer);

                  /* create the new X cursor with this image */
                  cur = ecore_x_cursor_new(win, pixels, w, h,
                                           input->pointer.hot.x, 
                                           input->pointer.hot.y);

                  /* set the cursor on this window */
                  ecore_x_window_cursor_set(win, cur);

                  /* free the cursor */
                  ecore_x_cursor_free(cur);
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

   if (!ews) return;

   /* set the destroy listener */
   wl_signal_add(&ews->wl.surface.resource.destroy_signal, 
                 &input->pointer.surface_destroy);

   /* set some properties on this surface */
   ews->unmap = _e_comp_wl_pointer_unmap;
   ews->configure = _e_comp_wl_pointer_configure;
   ews->input = input;

   /* update input structure with new values */
   input->pointer.surface = ews;
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
