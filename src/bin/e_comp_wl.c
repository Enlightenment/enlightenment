#include "e.h"
#include "e_comp_wl.h"
#include <sys/mman.h>

#define e_pixmap_parent_window_set(X, Y) e_pixmap_parent_window_set(X, (Ecore_Window)(uintptr_t)Y)

/* compositor function prototypes */
static void _seat_send_updated_caps(struct wl_seat *seat);
static void _move_resources(struct wl_list *dest, struct wl_list *src);
static void _move_resources_for_client(struct wl_list *dest, struct wl_list *src, struct wl_client *client);

static struct wl_resource *_find_resource_for_surface(struct wl_list *list, struct wl_resource *surface);

static void _default_grab_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x, wl_fixed_t y);
static void _default_grab_motion(struct wl_pointer_grab *grab, uint32_t timestamp, wl_fixed_t x, wl_fixed_t y);
static void _default_grab_button(struct wl_pointer_grab *grab, uint32_t timestamp, uint32_t button, uint32_t state_w);

static void _default_grab_touch_down(struct wl_touch_grab *grab, uint32_t timestamp, int touch_id, wl_fixed_t sx, wl_fixed_t sy);
static void _default_grab_touch_up(struct wl_touch_grab *grab, uint32_t timestamp, int touch_id);
static void _default_grab_touch_motion(struct wl_touch_grab *grab, uint32_t timestamp, int touch_id, wl_fixed_t sx, wl_fixed_t sy);

static void _data_offer_accept(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t serial, const char *mime_type);
static void _data_offer_receive(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *mime_type, int32_t fd);
static void _data_offer_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);

static void _destroy_data_offer(struct wl_resource *resource);
static void _destroy_offer_data_source(struct wl_listener *listener, void *data EINA_UNUSED);

static void _create_data_source(struct wl_client *client, struct wl_resource *resource, uint32_t id);
static void _get_data_device(struct wl_client *client, struct wl_resource *manager_resource EINA_UNUSED, uint32_t id, struct wl_resource *seat_resource);

static void _current_surface_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _bind_manager(struct wl_client *client, void *data EINA_UNUSED, uint32_t version EINA_UNUSED, uint32_t id);

static void _default_grab_key(struct wl_keyboard_grab *grab, uint32_t timestamp, uint32_t key, uint32_t state);
static void _default_grab_modifiers(struct wl_keyboard_grab *grab, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);

static void _data_device_start_drag(struct wl_client *client, struct wl_resource *resource, struct wl_resource *source_resource, struct wl_resource *origin_resource EINA_UNUSED, struct wl_resource *icon_resource, uint32_t serial EINA_UNUSED);
static void _data_device_set_selection(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *source_resource, uint32_t serial);
static void _destroy_data_device_icon(struct wl_listener *listener, void *data EINA_UNUSED);

static void _destroy_selection_data_source(struct wl_listener *listener, void *data EINA_UNUSED);
static void _data_source_offer(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *type);
static void _data_source_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _destroy_data_source(struct wl_resource *resource);
static void _destroy_data_device_source(struct wl_listener *listener, void *data EINA_UNUSED);
static void _data_device_end_drag_grab(struct wl_seat *seat);

static void _drag_grab_button(struct wl_pointer_grab *grab, uint32_t timestamp EINA_UNUSED, uint32_t button, uint32_t state_w);
static void _drag_grab_motion(struct wl_pointer_grab *grab, uint32_t timestamp, wl_fixed_t x, wl_fixed_t y);

static void _destroy_drag_focus(struct wl_listener *listener, void *data EINA_UNUSED);
static void _drag_grab_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x, wl_fixed_t y);

static void _client_source_accept(struct wl_data_source *source, uint32_t timestamp EINA_UNUSED, const char *mime_type);
static void _client_source_send(struct wl_data_source *source, const char *mime_type, int32_t fd);
static void _client_source_cancel(struct wl_data_source *source);

static void _e_comp_wl_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id);
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
static void _e_comp_wl_pointer_cb_release(struct wl_client *client, struct wl_resource *resource);

/* keyboard interface prototypes */
static void _e_comp_wl_keyboard_cb_release(struct wl_client *client, struct wl_resource *resource);

/* touch interface prototypes */
static void _e_comp_wl_touch_cb_release(struct wl_client *client, struct wl_resource *resource);

/* region interface prototypes */
static void _e_comp_wl_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_comp_wl_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);
static void _e_comp_wl_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);

/* surface function prototypes */
static void _e_comp_wl_surface_cb_pending_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_comp_wl_surface_cb_frame_destroy(struct wl_resource *resource);
static void _e_comp_wl_surface_buffer_reference(E_Wayland_Buffer_Reference *ref, E_Wayland_Buffer *buffer);
static void _e_comp_wl_surface_buffer_reference_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static E_Wayland_Buffer *_e_comp_wl_surface_buffer_resource(struct wl_resource *resource);
static void _e_comp_wl_surface_buffer_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);

/* surface interface prototypes */
static void _e_comp_wl_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_comp_wl_surface_cb_attach(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *buffer_resource, int x, int y);
static void _e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);
static void _e_comp_wl_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, unsigned int callback);
static void _e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource);
static void _e_comp_wl_surface_cb_input_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource);
static void _e_comp_wl_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_comp_wl_surface_cb_buffer_transform_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int transform EINA_UNUSED);
static void _e_comp_wl_surface_cb_buffer_scale_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int scale EINA_UNUSED);

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
   _e_comp_wl_pointer_cb_cursor_set,
   _e_comp_wl_pointer_cb_release
};

static const struct wl_keyboard_interface _e_keyboard_interface = 
{
   _e_comp_wl_keyboard_cb_release
};

static const struct wl_touch_interface _e_touch_interface = 
{
   _e_comp_wl_touch_cb_release
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
   _e_comp_wl_surface_cb_buffer_transform_set,
   _e_comp_wl_surface_cb_buffer_scale_set
};

static const struct wl_pointer_grab_interface _e_pointer_grab_interface = 
{
   _default_grab_focus,
   _default_grab_motion,
   _default_grab_button
};

static const struct wl_keyboard_grab_interface _e_default_keyboard_grab_interface = 
{
   _default_grab_key,
   _default_grab_modifiers,
};

static const struct wl_data_offer_interface _e_data_offer_interface = 
{
   _data_offer_accept,
   _data_offer_receive,
   _data_offer_destroy,
};

static const struct wl_data_device_manager_interface _e_manager_interface = 
{
   _create_data_source,
   _get_data_device
};

static const struct wl_data_device_interface _e_data_device_interface = 
{
   _data_device_start_drag,
   _data_device_set_selection,
};

static const struct wl_touch_grab_interface _e_default_touch_grab_interface = 
{
   _default_grab_touch_down,
   _default_grab_touch_up,
   _default_grab_touch_motion
};

static struct wl_data_source_interface _e_data_source_interface = 
{
   _data_source_offer,
   _data_source_destroy
};

static const struct wl_pointer_grab_interface _e_drag_grab_interface = 
{
   _drag_grab_focus,
   _drag_grab_motion,
   _drag_grab_button,
};

/* local variables */
static Ecore_Idler *_module_idler = NULL;

/* external variables */
EAPI E_Wayland_Compositor *_e_wl_comp;

/* external functions */
EAPI Eina_Bool 
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
   if (!wl_global_create(_e_wl_comp->wl.display, &wl_compositor_interface, 3, 
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
             /* const char *exts; */

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

#ifndef WAYLAND_ONLY
   /* setup keymap_change event handler */
   _e_wl_comp->kbd_handler = 
     ecore_event_handler_add(ECORE_X_EVENT_XKB_STATE_NOTIFY, 
                             e_comp_wl_cb_keymap_changed, NULL);
#endif
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

   wl_event_loop_dispatch(_e_wl_comp->wl.loop, 0);
#ifndef WAYLAND_ONLY
   /* add an idler for deferred shell module loading */
   _module_idler = ecore_idler_add(_e_comp_wl_cb_module_idle, NULL);
#endif
   /* return success */
   return EINA_TRUE;

err:
   /* remove kbd handler */
   if (_e_wl_comp->kbd_handler) 
     ecore_event_handler_del(_e_wl_comp->kbd_handler);
#ifndef WAYLAND_ONLY
   /* remove the module idler */
   if (_module_idler) ecore_idler_del(_module_idler);
#endif

#ifdef HAVE_WAYLAND_EGL
   /* unbind wayland display */
   if (_e_wl_comp->egl.bound)
     _e_wl_comp->egl.unbind_display(_e_wl_comp->egl.display, _e_wl_comp->wl.display);

   /* terminate the egl display */
   if (_e_wl_comp->egl.display) eglTerminate(_e_wl_comp->egl.display);

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

   if (_e_wl_comp)
     {
        /* remove the idler */
        if (_e_wl_comp->idler) ecore_idler_del(_e_wl_comp->idler);

        /* remove the fd handler */
        if (_e_wl_comp->fd_handler)
          ecore_main_fd_handler_del(_e_wl_comp->fd_handler);

        /* shutdown input */
        _e_comp_wl_input_shutdown();

#ifdef HAVE_WAYLAND_EGL
        /* unbind wayland display */
        if (_e_wl_comp->egl.bound)
          _e_wl_comp->egl.unbind_display(_e_wl_comp->egl.display, _e_wl_comp->wl.display);

        /* terminate the egl display */
        if (_e_wl_comp->egl.display) eglTerminate(_e_wl_comp->egl.display);

        eglReleaseThread();
#endif

        /* if we have a display, destroy it */
        if (_e_wl_comp->wl.display) wl_display_destroy(_e_wl_comp->wl.display);

        /* free the compositor */
        E_FREE(_e_wl_comp);
     }

   /* disable the loaded shell module */
   /* TODO: we should have a config variable somewhere to store which 
    * shell we want to unload (tablet, mobile, etc) */
   if ((mod = e_module_find("wl_desktop_shell")))
     e_module_disable(mod);
}

#ifdef WAYLAND_ONLY
EAPI int 
e_comp_wl_input_read(int fd EINA_UNUSED, unsigned int mask EINA_UNUSED, void *data)
{
   E_Wayland_Compositor *wl_comp = data;

   wl_event_loop_dispatch(wl_comp->wl.input_loop, 0);
   return 1;
}
#endif

EAPI void 
wl_seat_init(struct wl_seat *seat)
{
   memset(seat, 0, sizeof *seat);

   wl_signal_init(&seat->destroy_signal);

   seat->selection_data_source = NULL;
   wl_list_init(&seat->base_resource_list);
   wl_signal_init(&seat->selection_signal);
   wl_list_init(&seat->drag_resource_list);
   wl_signal_init(&seat->drag_icon_signal);
}

EAPI void 
wl_seat_release(struct wl_seat *seat)
{
   /* if (seat->pointer) */
   /*   { */
   /*      if (seat->pointer->focus) */
   /*        wl_list_remove(&seat->pointer->focus_listener.link); */
   /*   } */

   /* if (seat->keyboard) */
   /*   { */
   /*      if (seat->keyboard->focus) */
   /*        wl_list_remove(&seat->keyboard->focus_listener.link); */
   /*   } */

   /* if (seat->touch) */
   /*   { */
   /*      if (seat->touch->focus) */
   /*        wl_list_remove(&seat->touch->focus_listener.link); */
   /*   } */

   wl_signal_emit(&seat->destroy_signal, seat);
}

EAPI void 
wl_seat_set_pointer(struct wl_seat *seat, struct wl_pointer *pointer)
{
   if (pointer && (seat->pointer || pointer->seat)) return; /* XXX: error? */
   if (!pointer && !seat->pointer) return;

   seat->pointer = pointer;
   if (pointer) pointer->seat = seat;

   _seat_send_updated_caps(seat);
}

EAPI void 
wl_seat_set_keyboard(struct wl_seat *seat, struct wl_keyboard *keyboard)
{
   if (keyboard && (seat->keyboard || keyboard->seat)) return; /* XXX: error? */
   if (!keyboard && !seat->keyboard) return;

   seat->keyboard = keyboard;
   if (keyboard) keyboard->seat = seat;

   _seat_send_updated_caps(seat);
}

EAPI void 
wl_seat_set_touch(struct wl_seat *seat, struct wl_touch *touch)
{
   if (touch && (seat->touch || touch->seat)) return; /* XXX: error? */
   if (!touch && !seat->touch) return;

   seat->touch = touch;
   if (touch) touch->seat = seat;

   _seat_send_updated_caps(seat);
}

EAPI void 
wl_pointer_init(struct wl_pointer *pointer)
{
   memset(pointer, 0, sizeof *pointer);
   wl_list_init(&pointer->resource_list);
   wl_list_init(&pointer->focus_resource_list);
   pointer->default_grab.interface = &_e_pointer_grab_interface;
   pointer->default_grab.pointer = pointer;
   pointer->grab = &pointer->default_grab;
   wl_signal_init(&pointer->focus_signal);

   pointer->x = wl_fixed_from_int(100);
   pointer->y = wl_fixed_from_int(100);
}

EAPI void 
wl_pointer_set_focus(struct wl_pointer *pointer, struct wl_resource *surface, wl_fixed_t sx, wl_fixed_t sy)
{
   struct wl_keyboard *kbd = pointer->seat->keyboard;
   struct wl_resource *res;
   struct wl_list *lst;
   uint32_t serial;

   lst = &pointer->focus_resource_list;
   if ((!wl_list_empty(lst)) && (pointer->focus != surface))
     {
        serial = wl_display_next_serial(_e_wl_comp->wl.display);
        wl_resource_for_each(res, lst)
          wl_pointer_send_leave(res, serial, pointer->focus);

        _move_resources(&pointer->resource_list, lst);
     }

   if ((_find_resource_for_surface(&pointer->resource_list, surface)) && 
       (pointer->focus != surface))
     {
        struct wl_client *client;

        client = wl_resource_get_client(surface);
        _move_resources_for_client(lst, &pointer->resource_list, client);

        wl_resource_for_each(res, lst)
          wl_pointer_send_enter(res, serial, surface, sx, sy);

        pointer->focus_serial = serial;
     }

   if ((kbd) && (surface) && (kbd->focus != pointer->focus))
     {
        struct wl_client *client;

        client = wl_resource_get_client(surface);

        wl_resource_for_each(res, &kbd->resource_list)
          {
             if (wl_resource_get_client(res) == client)
               wl_keyboard_send_modifiers(res, serial,
                                          kbd->modifiers.mods_depressed,
                                          kbd->modifiers.mods_latched,
                                          kbd->modifiers.mods_locked,
                                          kbd->modifiers.group);
          }
     }

   pointer->focus = surface;
   pointer->default_grab.focus = surface;
   wl_signal_emit(&pointer->focus_signal, pointer);
}

EAPI void 
wl_pointer_start_grab(struct wl_pointer *pointer, struct wl_pointer_grab *grab)
{
   const struct wl_pointer_grab_interface *interface;

   pointer->grab = grab;
   interface = pointer->grab->interface;
   grab->pointer = pointer;

   if (pointer->current)
     interface->focus(pointer->grab, pointer->current,
                       pointer->current_x, pointer->current_y);
}

EAPI void 
wl_pointer_end_grab(struct wl_pointer *pointer)
{
   const struct wl_pointer_grab_interface *interface;

   pointer->grab = &pointer->default_grab;
   interface = pointer->grab->interface;
   interface->focus(pointer->grab, pointer->current,
                    pointer->current_x, pointer->current_y);
}

EAPI void 
wl_keyboard_init(struct wl_keyboard *keyboard)
{
   memset(keyboard, 0, sizeof *keyboard);
   wl_list_init(&keyboard->resource_list);
   wl_array_init(&keyboard->keys);
   wl_list_init(&keyboard->focus_resource_list);
   keyboard->default_grab.interface = &_e_default_keyboard_grab_interface;
   keyboard->default_grab.keyboard = keyboard;
   keyboard->grab = &keyboard->default_grab;
   wl_signal_init(&keyboard->focus_signal);
}

EAPI void 
wl_keyboard_set_focus(struct wl_keyboard *keyboard, struct wl_resource *surface)
{
   struct wl_resource *res;
   struct wl_list *lst;
   uint32_t serial;

   lst = &keyboard->focus_resource_list;

   if ((!wl_list_empty(lst)) && (keyboard->focus != surface))
     {
        serial = wl_display_next_serial(_e_wl_comp->wl.display);

        wl_resource_for_each(res, lst)
          wl_keyboard_send_leave(res, serial, keyboard->focus);

        _move_resources(&keyboard->resource_list, lst);
     }

   if ((_find_resource_for_surface(&keyboard->resource_list, surface)) && 
       (keyboard->focus != surface))
     {       
        struct wl_client *client;

        client = wl_resource_get_client(surface);
        serial = wl_display_next_serial(_e_wl_comp->wl.display);

        _move_resources_for_client(lst, &keyboard->resource_list, client);

        wl_resource_for_each(res, lst)
          {
             wl_keyboard_send_modifiers(res, serial,
                                        keyboard->modifiers.mods_depressed,
                                        keyboard->modifiers.mods_latched,
                                        keyboard->modifiers.mods_locked,
                                        keyboard->modifiers.group);
             wl_keyboard_send_enter(res, serial, surface, &keyboard->keys);
          }

        keyboard->focus_serial = serial;
     }

   keyboard->focus = surface;
   wl_signal_emit(&keyboard->focus_signal, keyboard);
}

EAPI void 
wl_keyboard_start_grab(struct wl_keyboard *device, struct wl_keyboard_grab *grab)
{
   device->grab = grab;
   grab->keyboard = device;
}

EAPI void 
wl_keyboard_end_grab(struct wl_keyboard *keyboard)
{
   keyboard->grab = &keyboard->default_grab;
}

EAPI void 
wl_touch_init(struct wl_touch *touch)
{
   memset(touch, 0, sizeof *touch);
   wl_list_init(&touch->resource_list);
   wl_list_init(&touch->focus_resource_list);
   touch->default_grab.interface = &_e_default_touch_grab_interface;
   touch->default_grab.touch = touch;
   touch->grab = &touch->default_grab;
   wl_signal_init(&touch->focus_signal);
}

EAPI void 
wl_touch_start_grab(struct wl_touch *device, struct wl_touch_grab *grab)
{
   device->grab = grab;
   grab->touch = device;
}

EAPI void 
wl_touch_end_grab(struct wl_touch *touch)
{
   touch->grab = &touch->default_grab;
}

EAPI void 
wl_data_device_set_keyboard_focus(struct wl_seat *seat)
{
   struct wl_resource *data_device, *focus, *offer;
   struct wl_data_source *source;

   if (!seat->keyboard) return;

   focus = seat->keyboard->focus;
   if (!focus) return;

   data_device = 
     wl_resource_find_for_client(&seat->drag_resource_list, 
                                 wl_resource_get_client(focus));
   if (!data_device) return;

   source = seat->selection_data_source;
   if (source) 
     {
        offer = wl_data_source_send_offer(source, data_device);
        wl_data_device_send_selection(data_device, offer);
     }
}

EAPI int 
wl_data_device_manager_init(struct wl_display *display)
{
   if (!wl_global_create(display, &wl_data_device_manager_interface, 1, 
                         NULL, _bind_manager))
     return -1;
   return 0;
}

EAPI struct wl_resource *
wl_data_source_send_offer(struct wl_data_source *source, struct wl_resource *target)
{
   struct wl_data_offer *offer;
   char **p;

   offer = malloc(sizeof *offer);
   if (offer == NULL) return NULL;

   offer->resource = 
     wl_resource_create(wl_resource_get_client(target),
                        &wl_data_offer_interface, 1, 0);
   if (!offer->resource)
     {
        free(offer);
        return NULL;
     }

   wl_resource_set_implementation(offer->resource, &_e_data_offer_interface, 
                                  offer, _destroy_data_offer);

   offer->source = source;
   offer->source_destroy_listener.notify = _destroy_offer_data_source;
   wl_signal_add(&source->destroy_signal,
                 &offer->source_destroy_listener);

   wl_data_device_send_data_offer(target, offer->resource);

   wl_array_for_each(p, &source->mime_types)
     wl_data_offer_send_offer(offer->resource, *p);

   return offer->resource;
}

EAPI void
wl_seat_set_selection(struct wl_seat *seat, struct wl_data_source *source, uint32_t serial)
{
   struct wl_resource *data_device, *offer;
   struct wl_resource *focus = NULL;

   if (seat->selection_data_source &&
       seat->selection_serial - serial < UINT32_MAX / 2)
     return;

   if (seat->selection_data_source) 
     {
        seat->selection_data_source->cancel(seat->selection_data_source);
        wl_list_remove(&seat->selection_data_source_listener.link);
        seat->selection_data_source = NULL;
     }

   seat->selection_data_source = source;
   seat->selection_serial = serial;
   if (seat->keyboard)
     focus = seat->keyboard->focus;
   if (focus) 
     {
        data_device = 
          wl_resource_find_for_client(&seat->drag_resource_list, 
                                      wl_resource_get_client(focus));
        if (data_device && source) 
          {
             offer = wl_data_source_send_offer(seat->selection_data_source,
                                               data_device);
             wl_data_device_send_selection(data_device, offer);
          }
        else if (data_device) 
          {
             wl_data_device_send_selection(data_device, NULL);
          }
     }

   wl_signal_emit(&seat->selection_signal, seat);
   if (source) 
     {
        seat->selection_data_source_listener.notify =
          _destroy_selection_data_source;
        wl_signal_add(&source->destroy_signal,
                      &seat->selection_data_source_listener);
     }
}

EAPI unsigned int 
e_comp_wl_time_get(void)
{
   struct timeval tm;

   gettimeofday(&tm, NULL);
   return (tm.tv_sec * 1000 + tm.tv_usec / 1000);
}

EAPI void
e_comp_wl_mouse_button(struct wl_resource *resource, uint32_t serial, uint32_t timestamp, uint32_t button, uint32_t state_w)
{
   switch (button)
     {
      case BTN_LEFT:
      case BTN_MIDDLE:
      case BTN_RIGHT:
        wl_pointer_send_button(resource, serial, timestamp, 
                               button, state_w);
        break;
      case 4:
        if (state_w)
          wl_pointer_send_axis(resource, timestamp, 
                               WL_POINTER_AXIS_VERTICAL_SCROLL, 
                               -wl_fixed_from_int(1));
        break;
      case 5:
        if (state_w)
          wl_pointer_send_axis(resource, timestamp, 
                               WL_POINTER_AXIS_VERTICAL_SCROLL, 
                               wl_fixed_from_int(1));
        break;
      case 6:
        if (state_w)
          wl_pointer_send_axis(resource, timestamp, 
                               WL_POINTER_AXIS_HORIZONTAL_SCROLL, 
                               -wl_fixed_from_int(1));
        break;
      case 7:
        if (state_w)
          wl_pointer_send_axis(resource, timestamp, 
                               WL_POINTER_AXIS_HORIZONTAL_SCROLL, 
                               wl_fixed_from_int(1));
        break;
     }
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
_seat_send_updated_caps(struct wl_seat *seat)
{
   struct wl_resource *res;
   enum wl_seat_capability caps = 0;

   if (seat->pointer)
     caps |= WL_SEAT_CAPABILITY_POINTER;
   if (seat->keyboard)
     caps |= WL_SEAT_CAPABILITY_KEYBOARD;
   if (seat->touch)
     caps |= WL_SEAT_CAPABILITY_TOUCH;

   wl_resource_for_each(res, &seat->base_resource_list)
     wl_seat_send_capabilities(res, caps);
}

static void 
_move_resources(struct wl_list *dest, struct wl_list *src)
{
   wl_list_insert_list(dest, src);
   wl_list_init(src);
}

static void 
_move_resources_for_client(struct wl_list *dest, struct wl_list *src, struct wl_client *client)
{
   struct wl_resource *res, *tmp;

   wl_resource_for_each_safe(res, tmp, src)
     {
        if (wl_resource_get_client(res) == client)
          {
             wl_list_remove(wl_resource_get_link(res));
             wl_list_insert(dest, wl_resource_get_link(res));
          }
     }
}

static struct wl_resource *
_find_resource_for_surface(struct wl_list *list, struct wl_resource *surface)
{
   if (!surface) return NULL;

   return wl_resource_find_for_client(list, wl_resource_get_client(surface));
}

static void
_default_grab_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x, wl_fixed_t y)
{
   struct wl_pointer *pointer = grab->pointer;

   if (pointer->button_count > 0) return;

   wl_pointer_set_focus(pointer, surface, x, y);
}

static void
_default_grab_motion(struct wl_pointer_grab *grab, uint32_t timestamp, wl_fixed_t x, wl_fixed_t y)
{
   struct wl_list *lst;
   struct wl_resource *res;

   lst = &grab->pointer->focus_resource_list;
   wl_resource_for_each(res, lst)
     wl_pointer_send_motion(res, timestamp, x, y);
}

static void
_default_grab_button(struct wl_pointer_grab *grab, uint32_t timestamp, uint32_t button, uint32_t state_w)
{
   struct wl_pointer *pointer = grab->pointer;
   struct wl_list *lst;
   struct wl_resource *res;
   enum wl_pointer_button_state state = state_w;

   lst = &pointer->focus_resource_list;
   if (!wl_list_empty(lst))
     {
        uint32_t serial;

        serial = wl_display_next_serial(_e_wl_comp->wl.display);

        wl_resource_for_each(res, lst)
          e_comp_wl_mouse_button(res, serial, timestamp, button, state_w);
     }
}

static void 
_default_grab_touch_down(struct wl_touch_grab *grab, uint32_t timestamp, int touch_id, wl_fixed_t sx, wl_fixed_t sy)
{
   struct wl_resource *res;
   struct wl_list *lst;
   struct wl_touch *touch = grab->touch;

   lst = &touch->focus_resource_list;
   if (!wl_list_empty(lst) && (touch->focus))
     {
        uint32_t serial;

        serial = wl_display_next_serial(_e_wl_comp->wl.display);
        wl_resource_for_each(res, lst)
          wl_touch_send_down(res, serial, timestamp,
                             touch->focus, touch_id, sx, sy);
     }
}

static void 
_default_grab_touch_up(struct wl_touch_grab *grab, uint32_t timestamp, int touch_id)
{
   struct wl_resource *res;
   struct wl_list *lst;
   struct wl_touch *touch = grab->touch;

   lst = &touch->focus_resource_list;
   if (!wl_list_empty(lst) && (touch->focus))
     {
        uint32_t serial;

        serial = wl_display_next_serial(_e_wl_comp->wl.display);
        wl_resource_for_each(res, lst)
          wl_touch_send_up(res, serial, timestamp, touch_id);
     }
}

static void 
_default_grab_touch_motion(struct wl_touch_grab *grab, uint32_t timestamp, int touch_id, wl_fixed_t sx, wl_fixed_t sy)
{
   struct wl_resource *res;
   struct wl_list *lst;
   struct wl_touch *touch = grab->touch;

   lst = &touch->focus_resource_list;
   wl_resource_for_each(res, lst)
     wl_touch_send_motion(res, timestamp, touch_id, sx, sy);
}

static void
_data_offer_accept(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t serial, const char *mime_type)
{
   struct wl_data_offer *offer;

   offer = wl_resource_get_user_data(resource);

   if (offer->source)
     offer->source->accept(offer->source, serial, mime_type);
}

static void
_data_offer_receive(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *mime_type, int32_t fd)
{
   struct wl_data_offer *offer;

   offer = wl_resource_get_user_data(resource);

   if (offer->source)
     offer->source->send(offer->source, mime_type, fd);
   else
     close(fd);
}

static void
_data_offer_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_destroy_data_offer(struct wl_resource *resource)
{
   struct wl_data_offer *offer;

   offer = wl_resource_get_user_data(resource);

   if (offer->source)
     wl_list_remove(&offer->source_destroy_listener.link);
   free(offer);
}

static void
_destroy_offer_data_source(struct wl_listener *listener, void *data EINA_UNUSED)
{
   struct wl_data_offer *offer;

   offer = container_of(listener, struct wl_data_offer,
                        source_destroy_listener);
   offer->source = NULL;
}

static void
_create_data_source(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   struct wl_data_source *source;

   source = malloc(sizeof *source);
   if (source == NULL) 
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_signal_init(&source->destroy_signal);
   source->accept = _client_source_accept;
   source->send = _client_source_send;
   source->cancel = _client_source_cancel;

   wl_array_init(&source->mime_types);

   source->resource = 
     wl_resource_create(client, &wl_data_source_interface, 1, id);

   wl_resource_set_implementation(source->resource, 
                                  &_e_data_source_interface, source, 
                                  _destroy_data_source);
}

static void 
_unbind_data_device(struct wl_resource *resource)
{
   wl_list_remove(wl_resource_get_link(resource));
}

static void
_get_data_device(struct wl_client *client, struct wl_resource *manager_resource, uint32_t id, struct wl_resource *seat_resource)
{
   struct wl_seat *seat;
   struct wl_resource *resource;

   seat = wl_resource_get_user_data(seat_resource);

   resource = 
     wl_resource_create(client, &wl_data_device_interface, 1, id);
   if (!resource)
     {
        wl_resource_post_no_memory(manager_resource);
        return;
     }

   wl_list_insert(&seat->drag_resource_list, wl_resource_get_link(resource));

   wl_resource_set_implementation(resource, &_e_data_device_interface, seat, 
                                  _unbind_data_device);

}

static void
_current_surface_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   struct wl_pointer *pointer =
     container_of(listener, struct wl_pointer, current_listener);
   pointer->current = NULL;
}

static void
_bind_manager(struct wl_client *client, void *data EINA_UNUSED, uint32_t version EINA_UNUSED, uint32_t id)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &wl_data_device_manager_interface, 1, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_manager_interface, NULL, NULL);
}

static void
_default_grab_key(struct wl_keyboard_grab *grab, uint32_t timestamp, uint32_t key, uint32_t state)
{
   struct wl_keyboard *keyboard = grab->keyboard;
   struct wl_resource *res;
   struct wl_list *lst;

   lst = &keyboard->focus_resource_list;
   if (!wl_list_empty(lst))
     {
        uint32_t serial;

        serial = wl_display_next_serial(_e_wl_comp->wl.display);
        wl_resource_for_each(res, lst)
          wl_keyboard_send_key(res, serial, timestamp, key, state);
     }
}

static void
_default_grab_modifiers(struct wl_keyboard_grab *grab, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
   struct wl_keyboard *keyboard = grab->keyboard;
   struct wl_pointer *pointer = keyboard->seat->pointer;
   struct wl_resource *res;
   struct wl_list *lst;

   lst = &keyboard->focus_resource_list;
   wl_resource_for_each(res, lst)
     wl_keyboard_send_modifiers(res, serial, mods_depressed, mods_latched,
                                mods_locked, group);

   if (pointer && pointer->focus && pointer->focus != keyboard->focus) 
     {
        struct wl_client *client;

        lst = &keyboard->resource_list;
        client = wl_resource_get_client(pointer->focus);
        wl_resource_for_each(res, lst)
          {
             if (wl_resource_get_client(res) == client)
               wl_keyboard_send_modifiers(res, serial, mods_depressed, 
                                          mods_latched, mods_locked, group);
          }
     }
}

static void
_data_device_start_drag(struct wl_client *client, struct wl_resource *resource, struct wl_resource *source_resource, struct wl_resource *origin_resource, struct wl_resource *icon_resource, uint32_t serial EINA_UNUSED)
{
   struct wl_seat *seat;

   seat = wl_resource_get_user_data(resource);

   if ((seat->pointer->button_count == 0) || 
       (seat->pointer->grab_serial != serial) || 
       (seat->pointer->focus != wl_resource_get_user_data(origin_resource)))
     return;

   seat->drag_grab.interface = &_e_drag_grab_interface;
   seat->drag_client = client;
   seat->drag_data_source = NULL;

   if (source_resource) 
     {
        struct wl_data_source *source;

        source = wl_resource_get_user_data(source_resource);
        seat->drag_data_source = source;
        seat->drag_data_source_listener.notify =
          _destroy_data_device_source;
        wl_signal_add(&source->destroy_signal,
                      &seat->drag_data_source_listener);
       }

   if (icon_resource) 
     {
        E_Wayland_Surface *icon;

        icon = wl_resource_get_user_data(icon_resource);

        seat->drag_surface = icon->wl.surface;
        seat->drag_icon_listener.notify = _destroy_data_device_icon;
        wl_signal_add(&icon->wl.destroy_signal,
                      &seat->drag_icon_listener);
        /* wl_signal_emit(&seat->drag_icon_signal, icon_resource); */
     }

   wl_pointer_set_focus(seat->pointer, NULL,
                        wl_fixed_from_int(0), wl_fixed_from_int(0));
   wl_pointer_start_grab(seat->pointer, &seat->drag_grab);
}

static void
_data_device_set_selection(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *source_resource, uint32_t serial)
{
   struct wl_data_source *source;
   struct wl_seat *seat;

   if (!(seat = wl_resource_get_user_data(resource))) return;
   source = wl_resource_get_user_data(source_resource);

   wl_seat_set_selection(seat, source, serial);
}

static void
_destroy_data_device_icon(struct wl_listener *listener, void *data EINA_UNUSED)
{
   struct wl_seat *seat = 
     container_of(listener, struct wl_seat, drag_icon_listener);

   seat->drag_surface = NULL;
}

static void
_destroy_selection_data_source(struct wl_listener *listener, void *data EINA_UNUSED)
{
   struct wl_seat *seat = 
     container_of(listener, struct wl_seat, selection_data_source_listener);
   struct wl_resource *data_device;
   struct wl_resource *focus = NULL;

   seat->selection_data_source = NULL;

   if (seat->keyboard)
     focus = seat->keyboard->focus;

   if (focus)
     {
        data_device = 
          wl_resource_find_for_client(&seat->drag_resource_list,
                                    wl_resource_get_client(focus));
        if (data_device)
          wl_data_device_send_selection(data_device, NULL);
     }

   wl_signal_emit(&seat->selection_signal, seat);
}

static void
_destroy_data_device_source(struct wl_listener *listener, void *data EINA_UNUSED)
{
   struct wl_seat *seat = 
     container_of(listener, struct wl_seat, drag_data_source_listener);
   _data_device_end_drag_grab(seat);
}

static void
_data_source_offer(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *type)
{
   struct wl_data_source *source;
   char **p;

   source = wl_resource_get_user_data(resource);
   p = wl_array_add(&source->mime_types, sizeof *p);
   if (p) *p = strdup(type);

   if (!p || !*p) wl_resource_post_no_memory(resource);
}

static void
_data_source_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_destroy_data_source(struct wl_resource *resource)
{
   struct wl_data_source *source;
   char **p;

   source = wl_resource_get_user_data(resource);

   wl_signal_emit(&source->destroy_signal, source);

   wl_array_for_each(p, &source->mime_types)
     free(*p);

   wl_array_release(&source->mime_types);

   source->resource = NULL;
}

static void
_data_device_end_drag_grab(struct wl_seat *seat)
{
   if (seat->drag_surface) 
     {
        seat->drag_surface = NULL;
        wl_signal_emit(&seat->drag_icon_signal, NULL);
        wl_list_remove(&seat->drag_icon_listener.link);
     }

   _drag_grab_focus(&seat->drag_grab, NULL,
                    wl_fixed_from_int(0), wl_fixed_from_int(0));
   wl_pointer_end_grab(seat->pointer);
   seat->drag_data_source = NULL;
   seat->drag_client = NULL;
}

static void
_drag_grab_button(struct wl_pointer_grab *grab, uint32_t timestamp EINA_UNUSED, uint32_t button, uint32_t state_w)
{
   struct wl_seat *seat = container_of(grab, struct wl_seat, drag_grab);
   enum wl_pointer_button_state state = state_w;

   if (seat->drag_focus_resource &&
       seat->pointer->grab_button == button &&
       state == WL_POINTER_BUTTON_STATE_RELEASED)
     wl_data_device_send_drop(seat->drag_focus_resource);

   if (seat->pointer->button_count == 0 &&
       state == WL_POINTER_BUTTON_STATE_RELEASED) 
     {
        if (seat->drag_data_source)
          wl_list_remove(&seat->drag_data_source_listener.link);

        _data_device_end_drag_grab(seat);
     }
}

static void
_drag_grab_motion(struct wl_pointer_grab *grab, uint32_t timestamp, wl_fixed_t x, wl_fixed_t y)
{
   struct wl_seat *seat = container_of(grab, struct wl_seat, drag_grab);

   if (seat->drag_focus_resource)
     wl_data_device_send_motion(seat->drag_focus_resource,
                                timestamp, x, y);
}

static void
_destroy_drag_focus(struct wl_listener *listener, void *data EINA_UNUSED)
{
   struct wl_seat *seat =
     container_of(listener, struct wl_seat, drag_focus_listener);

   seat->drag_focus_resource = NULL;
}

static void
_drag_grab_focus(struct wl_pointer_grab *grab, struct wl_resource *surface, wl_fixed_t x, wl_fixed_t y)
{
   struct wl_seat *seat = container_of(grab, struct wl_seat, drag_grab);
   struct wl_resource *resource, *offer = NULL;
   struct wl_display *display;
   uint32_t serial;
   E_Wayland_Surface *ews;

   if (seat->drag_focus_resource) 
     {
        wl_data_device_send_leave(seat->drag_focus_resource);
        wl_list_remove(&seat->drag_focus_listener.link);
        seat->drag_focus_resource = NULL;
        seat->drag_focus = NULL;
     }

   if (!surface) return;

   if (!seat->drag_data_source &&
       wl_resource_get_client(surface) != seat->drag_client)
     return;

   resource = 
     wl_resource_find_for_client(&seat->drag_resource_list, 
                                 wl_resource_get_client(surface));
   if (!resource) return;

   display = wl_client_get_display(wl_resource_get_client(resource));
   serial = wl_display_next_serial(display);

   if (seat->drag_data_source)
     offer = wl_data_source_send_offer(seat->drag_data_source,
                                       resource);

   wl_data_device_send_enter(resource, serial, surface, x, y, offer);

   ews = wl_resource_get_user_data(surface);

   seat->drag_focus = surface;
   seat->drag_focus_listener.notify = _destroy_drag_focus;
   wl_signal_add(&ews->wl.destroy_signal, &seat->drag_focus_listener);
   seat->drag_focus_resource = resource;
   grab->focus = surface;
}

static void
_client_source_accept(struct wl_data_source *source, uint32_t timestamp EINA_UNUSED, const char *mime_type)
{
   wl_data_source_send_target(source->resource, mime_type);
}

static void
_client_source_send(struct wl_data_source *source, const char *mime_type, int32_t fd)
{
   wl_data_source_send_send(source->resource, mime_type, fd);
   close(fd);
}

static void
_client_source_cancel(struct wl_data_source *source)
{
   wl_data_source_send_cancelled(source->resource);
}

static void 
_e_comp_wl_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id)
{
   E_Wayland_Compositor *comp;
   struct wl_resource *res;

   if (!(comp = data)) return;

   res = 
     wl_resource_create(client, &wl_compositor_interface, MIN(version, 3), id);
   if (res)
     wl_resource_set_implementation(res, &_e_compositor_interface, comp, NULL);
}

static Eina_Bool 
_e_comp_wl_cb_read(void *data EINA_UNUSED, Ecore_Fd_Handler *hdl EINA_UNUSED)
{
   /* flush any events before we sleep */
   wl_display_flush_clients(_e_wl_comp->wl.display);
   wl_event_loop_dispatch(_e_wl_comp->wl.loop, 0);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_comp_wl_cb_idle(void *data EINA_UNUSED)
{
   if ((_e_wl_comp) && (_e_wl_comp->wl.display))
     {
        /* flush any clients before we idle */
        wl_display_flush_clients(_e_wl_comp->wl.display);
     }

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

EAPI Eina_Bool 
e_comp_wl_cb_keymap_changed(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   struct xkb_keymap *keymap;

   /* try to fetch the keymap */
   if (!(keymap = _e_comp_wl_input_keymap_get())) 
     return ECORE_CALLBACK_PASS_ON;

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

   /* create the xkb context */
   _e_wl_comp->xkb.context = xkb_context_new(0);

   /* try to fetch the keymap */
//   if ((keymap = _e_comp_wl_input_keymap_get()))
     {
        /* try to create new keyboard info */
        _e_wl_comp->input->xkb.info = 
          _e_comp_wl_input_keyboard_info_get(keymap);

        /* create new xkb state */
        _e_wl_comp->input->xkb.state = xkb_state_new(keymap);

        /* unreference the keymap */
        xkb_map_unref(keymap);
     }

   /* check for valid keyboard */
   if (!_e_wl_comp->input->wl.keyboard_resource) 
     return ECORE_CALLBACK_PASS_ON;

   /* send the current keymap to the keyboard object */
   wl_keyboard_send_keymap(_e_wl_comp->input->wl.keyboard_resource, 
                           WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, 
                           _e_wl_comp->input->xkb.info->fd, 
                           _e_wl_comp->input->xkb.info->size);

   return ECORE_CALLBACK_PASS_ON;
}

/* compositor interface functions */
static void 
_e_comp_wl_cb_surface_create(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Wayland_Surface *ews = NULL;
   uint64_t wid;
   pid_t pid;

   /* try to allocate space for a new surface */
   if (!(ews = E_NEW(E_Wayland_Surface, 1)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   ews->wl.client = client;
   wl_client_get_credentials(client, &pid, NULL, NULL);
   wid = e_comp_wl_id_get(pid, id);
   ews->pixmap = e_pixmap_find(E_PIXMAP_TYPE_WL, wid);
   if (!ews->pixmap)
     ews->pixmap = e_pixmap_new(E_PIXMAP_TYPE_WL, wid);
   e_pixmap_parent_window_set(ews->pixmap, ews);
   e_pixmap_usable_set(ews->pixmap, 1);
   /* initialize the destroy signal */
   wl_signal_init(&ews->wl.destroy_signal);

   /* initialize the link */
   wl_list_init(&ews->wl.link);

   /* initialize the lists of frames */
   wl_list_init(&ews->wl.frames);
   wl_list_init(&ews->pending.frames);

   ews->wl.surface = NULL;

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

   ews->wl.surface = 
     wl_resource_create(client, &wl_surface_interface, 
                        wl_resource_get_version(resource), id);
   wl_resource_set_implementation(ews->wl.surface, &_e_surface_interface, 
                                  ews, _e_comp_wl_cb_surface_destroy);

   /* add this surface to the list of surfaces */
   _e_wl_comp->surfaces = eina_inlist_append(_e_wl_comp->surfaces, EINA_INLIST_GET(ews));
}

static void 
_e_comp_wl_cb_surface_destroy(struct wl_resource *resource)
{
   E_Wayland_Surface *ews = NULL;
   E_Wayland_Surface_Frame_Callback *cb = NULL, *ncb = NULL;
   struct wl_pointer *pointer;
   Eina_Inlist *l;
   E_Wayland_Buffer *buffer;

   /* try to get the surface from this resource */
   if (!(ews = wl_resource_get_user_data(resource)))
     return;

   pointer = &_e_wl_comp->input->wl.pointer;
   if (pointer->focus == resource)
     {
        wl_pointer_set_focus(pointer, NULL,
                             wl_fixed_from_int(0), wl_fixed_from_int(0));

        _e_wl_comp->input->pointer.surface = NULL;
     }

   /* if this surface is mapped, unmap it */
   if (ews->mapped)
     {
        if (ews->unmap) ews->unmap(ews);
     }
   if (ews->buffer_reference.buffer)
     ews->buffer_reference.buffer->ews = NULL;
   if (ews->pending.buffer)
     ews->pending.buffer->ews = NULL;

   /* loop any pending surface frame callbacks and destroy them */
   wl_list_for_each_safe(cb, ncb, &ews->pending.frames, wl.link)
     wl_resource_destroy(cb->wl.resource);

   /* clear any pending regions */
   pixman_region32_fini(&ews->pending.damage);
   pixman_region32_fini(&ews->pending.opaque);
   pixman_region32_fini(&ews->pending.input);

   /* remove the pending buffer from the list */
   if (ews->pending.buffer)
     wl_list_remove(&ews->pending.buffer_destroy.link);

   /* dereference any existing buffers */
   _e_comp_wl_surface_buffer_reference(&ews->buffer_reference, NULL);

   /* clear any active regions */
   pixman_region32_fini(&ews->region.damage);
   pixman_region32_fini(&ews->region.opaque);
   pixman_region32_fini(&ews->region.input);
   pixman_region32_fini(&ews->region.clip);

   /* loop any active surface frame callbacks and destroy them */
   wl_list_for_each_safe(cb, ncb, &ews->wl.frames, wl.link)
     wl_resource_destroy(cb->wl.resource);

   EINA_INLIST_FOREACH_SAFE(ews->buffers, l, buffer)
     {
        buffer->ews = NULL;
        ews->buffers = eina_inlist_remove(ews->buffers, EINA_INLIST_GET(buffer));
     }
   if (e_comp_get(NULL)->pointer->pixmap == ews->pixmap)
     {
        e_pointer_image_set(e_comp_get(NULL)->pointer, NULL, 0, 0, 0, 0);
     }
   e_pixmap_parent_window_set(ews->pixmap, NULL);
   e_pixmap_free(ews->pixmap);

   /* remove this surface from the compositor's list of surfaces */
   _e_wl_comp->surfaces = eina_inlist_remove(_e_wl_comp->surfaces, EINA_INLIST_GET(ews));

   /* free the allocated surface structure */
   free(ews);
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

   ewr->wl.resource = 
     wl_resource_create(client, &wl_region_interface, 
                        wl_resource_get_version(resource), id);
   wl_resource_set_implementation(ewr->wl.resource, &_e_region_interface, ewr, 
                                  _e_comp_wl_cb_region_destroy);
}

static void 
_e_comp_wl_cb_region_destroy(struct wl_resource *resource)
{
   E_Wayland_Region *ewr = NULL;

   /* try to get the region from this resource */
   if (!(ewr = wl_resource_get_user_data(resource)))
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
   if (!wl_global_create(_e_wl_comp->wl.display, &wl_seat_interface, 2, 
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
_e_comp_wl_input_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id)
{
   struct wl_seat *seat = NULL;
   struct wl_resource *resource = NULL;
   enum wl_seat_capability caps = 0;

   /* try to cast data to our seat */
   if (!(seat = data)) return;

   /* add the seat object to the client */
   resource = 
     wl_resource_create(client, &wl_seat_interface, MIN(version, 2), id);

   wl_list_insert(&seat->base_resource_list, wl_resource_get_link(resource));

   wl_resource_set_implementation(resource, &_e_input_interface, data, 
                                  _e_comp_wl_input_cb_unbind);

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
   wl_list_remove(wl_resource_get_link(resource));
}

static struct xkb_keymap *
_e_comp_wl_input_keymap_get(void)
{
   E_Config_XKB_Layout *kbd_layout;
   struct xkb_keymap *keymap;
   struct xkb_rule_names names;

   memset(&names, 0, sizeof(names));

   if ((kbd_layout = e_xkb_layout_get()))
     {
        names.model = strdup(kbd_layout->model);
        names.layout = strdup(kbd_layout->name);
     }

#ifndef WAYLAND_ONLY
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

        root = ecore_x_window_root_first_get();
        rules = ecore_x_atom_get("_XKB_RULES_NAMES");
        ecore_x_window_prop_property_get(root, rules, ECORE_X_ATOM_STRING, 
                                         1024, &data, &len);

        if ((data) && (len > 0))
          {
             names.rules = (char*)data;
             data += strlen((const char *)data) + 1;
             if (!names.model)
               names.model = strdup((const char *)data);
             data += strlen((const char *)data) + 1;
             if (!names.layout)
               names.layout = strdup((const char *)data);
          }
     }
#endif

   printf("Keymap\n");
   printf("\tRules: %s\n", names.rules);
   printf("\tModel: %s\n", names.model);
   printf("\tLayout: %s\n", names.layout);

   keymap = xkb_map_new_from_names(_e_wl_comp->xkb.context, &names, 0);

   free((char *)names.rules);
   free((char *)names.model);
   free((char *)names.layout);

   return keymap;
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

   if (!(info->keymap = xkb_map_ref(keymap)))
     {
        E_FREE(info);
        return NULL;
     }

   /* init modifiers */
   info->mod_shift = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
   info->mod_caps = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
   info->mod_ctrl = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_CTRL);
   info->mod_alt = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_ALT);
   info->mod_super = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_LOGO);

   /* try to get a string of this keymap */
   if (!(tmp = xkb_map_get_as_string(keymap)))
     {
        printf("Could not get keymap as string\n");
        E_FREE(info);
        return NULL;
     }

   info->size = strlen(tmp) + 1;

   /* try to create an fd we can listen on for input */
   info->fd = _e_comp_wl_input_keymap_fd_get(info->size);
   if (info->fd < 0)
     {
        printf("Could not create keymap fd\n");
        E_FREE(info);
        return NULL;
     }

   info->area = 
     mmap(NULL, info->size, PROT_READ | PROT_WRITE, MAP_SHARED, info->fd, 0);

   /* TODO: error check mmap */

   if ((info->area) && (tmp))
     {
        strcpy(info->area, tmp);
        free(tmp);
     }

   return info;
}

/* input interface functions */
static void 
_e_comp_wl_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Wayland_Input *input = NULL;
   struct wl_resource *ptr = NULL;

   /* try to cast the resource data to our input structure */
   if (!(input = wl_resource_get_user_data(resource)))
     return;

   /* check that input has a pointer */
   if (!input->has_pointer) return;

   /* add a pointer object to the client */
   ptr = wl_resource_create(client, &wl_pointer_interface, 
                            wl_resource_get_version(resource), id);
   wl_list_insert(&input->wl.seat.pointer->resource_list, 
                  wl_resource_get_link(ptr));
   wl_resource_set_implementation(ptr, &_e_pointer_interface, 
                                  input, _e_comp_wl_input_cb_unbind);

   /* if the pointer has a focused surface, set it */
   if ((input->wl.seat.pointer->focus) && 
       (wl_resource_get_client(input->wl.seat.pointer->focus) == client))
     {
        /* tell pointer which surface is focused */
        wl_list_remove(wl_resource_get_link(ptr));
        wl_list_insert(&input->wl.seat.pointer->focus_resource_list, 
                       wl_resource_get_link(ptr));

        wl_pointer_send_enter(ptr, input->wl.seat.pointer->focus_serial, 
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
   if (!(input = wl_resource_get_user_data(resource)))
     return;

   /* check that input has a keyboard */
   if (!input->has_keyboard) return;

   /* add a keyboard object to the client */
   kbd = wl_resource_create(client, &wl_keyboard_interface, 
                            wl_resource_get_version(resource), id);
   wl_list_insert(&input->wl.seat.keyboard->resource_list, 
                  wl_resource_get_link(kbd));
   wl_resource_set_implementation(kbd, &_e_keyboard_interface, input, 
                                  _e_comp_wl_input_cb_unbind);

   /* send the current keymap to the keyboard object */
   wl_keyboard_send_keymap(kbd, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, 
                           input->xkb.info->fd, input->xkb.info->size);

   if (((input->wl.seat.keyboard) && (input->wl.seat.keyboard->focus) && 
        (wl_resource_get_client(input->wl.seat.keyboard->focus) == client)) || 
       ((input->wl.seat.pointer) && (input->wl.seat.pointer->focus) && 
           (wl_resource_get_client(input->wl.seat.pointer->focus) == client)))
     {
        wl_keyboard_send_modifiers(kbd, input->wl.seat.keyboard->focus_serial,
                                   input->wl.seat.keyboard->modifiers.mods_depressed,
                                   input->wl.seat.keyboard->modifiers.mods_latched,
                                   input->wl.seat.keyboard->modifiers.mods_locked,
                                   input->wl.seat.keyboard->modifiers.group);
     }

   /* test if keyboard has a focused client */
   if ((input->wl.seat.keyboard->focus) && 
       (wl_resource_get_client(input->wl.seat.keyboard->focus) == client))
     {
        wl_list_remove(wl_resource_get_link(kbd));
        wl_list_insert(&input->wl.seat.keyboard->focus_resource_list, 
                       wl_resource_get_link(kbd));
        wl_keyboard_send_enter(kbd, input->wl.seat.keyboard->focus_serial, 
                               input->wl.seat.keyboard->focus, 
                               &input->wl.seat.keyboard->keys);

        if (input->wl.seat.keyboard->focus_resource_list.prev == 
            wl_resource_get_link(kbd))
          {
             /* update the keyboard focus in the data device */
             wl_data_device_set_keyboard_focus(&input->wl.seat);
          }
     }

   input->wl.keyboard_resource = kbd;
}

static void 
_e_comp_wl_input_cb_touch_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Wayland_Input *input = NULL;
   struct wl_resource *tch = NULL;

   /* try to cast the resource data to our input structure */
   if (!(input = wl_resource_get_user_data(resource)))
     return;

   /* check that input has a touch */
   if (!input->has_touch) return;

   /* add a touch object to the client */
   tch = wl_resource_create(client, &wl_touch_interface, 
                            wl_resource_get_version(resource), id);
   if ((input->wl.seat.touch->focus) && 
       (wl_resource_get_client(input->wl.seat.touch->focus) == client))
     {
        wl_list_insert(&input->wl.seat.touch->resource_list, 
                       wl_resource_get_link(tch));
     }
   else
     wl_list_insert(&input->wl.seat.touch->focus_resource_list, 
                    wl_resource_get_link(tch));

   wl_resource_set_implementation(tch, &_e_touch_interface, input, 
                                  _e_comp_wl_input_cb_unbind);
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
   if (!input->wl.seat.pointer->focus) return;

   focus = wl_resource_get_user_data(input->wl.seat.pointer->focus);
   if (!focus) return;
   /* NB: Ideally, I wanted to use the e_pointer methods here so that 
    * the cursor would match the E theme, however Wayland currently 
    * provides NO Method to get the cursor name :( so we are stuck 
    * using the pixels from their cursor surface */

   /* is it mapped ? */
   if ((!focus->mapped) || (!focus->ec)) return;
   e_pixmap_dirty(ews->pixmap);
   e_pointer_image_set(focus->ec->comp->pointer, ews->pixmap, w, h, input->pointer.hot.x, input->pointer.hot.y);
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
   if (!(input = wl_resource_get_user_data(resource)))
     return;

   /* if we were passed in a surface, try to cast it to our structure */
   if (surface_resource) ews = wl_resource_get_user_data(surface_resource);

   /* if this input has no pointer, get out */
   if (!input->has_pointer) return;

   /* if the input has no current focus, get out */
   if (!input->wl.seat.pointer->focus) 
     {
        /* if we have an existing pointer surface, unmap it */
        if (input->pointer.surface) 
          {
             /* call the unmap function */
             if (input->pointer.surface->unmap)
               input->pointer.surface->unmap(input->pointer.surface);
          }

        input->pointer.surface = NULL;

        return;
     }

   if (wl_resource_get_client(input->wl.seat.pointer->focus) != client) return;
   if ((input->wl.seat.pointer->focus_serial - serial) > (UINT32_MAX / 2))
     return;

   /* is the passed in surface the same as our pointer surface ? */
   if ((ews) && (ews != input->pointer.surface))
     {
        if (ews->configure)
          {
             wl_resource_post_error(ews->wl.surface, 
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
   if (!ews)
     {
        e_pointer_hide(e_comp_get(NULL)->pointer);
        return;
     }

   /* set the destroy listener */
   wl_signal_add(&ews->wl.destroy_signal, 
                 &input->pointer.surface_destroy);

   /* set some properties on this surface */
   ews->unmap = _e_comp_wl_pointer_unmap;
   ews->configure = _e_comp_wl_pointer_configure;
   ews->input = input;

   /* update input structure with new values */
   input->pointer.hot.x = x;
   input->pointer.hot.y = y;

   if (&ews->buffer_reference)
     {
        E_Wayland_Buffer *buf;

        if ((buf = ews->buffer_reference.buffer))
          {
             Evas_Coord bw = 0, bh = 0;

             bw = buf->w;
             bh = buf->h;

             /* configure the pointer surface */
             _e_comp_wl_pointer_configure(ews, 0, 0, bw, bh);
          }
     }
}

static void 
_e_comp_wl_pointer_cb_release(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

/* keyboard interface functions */
static void 
_e_comp_wl_keyboard_cb_release(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

/* touch interface functions */
static void 
_e_comp_wl_touch_cb_release(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
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
   if (!(ewr = wl_resource_get_user_data(resource))) return;

   /* tell pixman to union this region with any previous one */
   pixman_region32_union_rect(&ewr->region, &ewr->region, x, y, w, h);
}

static void 
_e_comp_wl_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Wayland_Region *ewr = NULL;
   pixman_region32_t region;

   /* try to cast the resource data to our region structure */
   if (!(ewr = wl_resource_get_user_data(resource))) return;

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
   if (!(cb = wl_resource_get_user_data(resource))) return;

   wl_list_remove(&cb->wl.link);

   /* free the allocated callback structure */
   E_FREE(cb);
}

static void 
_e_comp_wl_surface_buffer_reference(E_Wayland_Buffer_Reference *ref, E_Wayland_Buffer *buffer)
{
   if ((ref->buffer) && (buffer != ref->buffer))
     {
        ref->buffer->busy_count--;
        if (ref->buffer->busy_count == 0)
          wl_resource_queue_event(ref->buffer->wl.resource, WL_BUFFER_RELEASE);
        wl_list_remove(&ref->destroy_listener.link);
     }

   if ((buffer) && (buffer != ref->buffer))
     {
        buffer->busy_count++;
        wl_signal_add(&buffer->wl.destroy_signal, &ref->destroy_listener);
     }

   //INF("CURRENT BUFFER SWAP: %p->%p", ref->buffer, buffer);
   ref->buffer = buffer;
   ref->destroy_listener.notify = 
     _e_comp_wl_surface_buffer_reference_cb_destroy;
}

static void 
_e_comp_wl_surface_buffer_reference_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Wayland_Buffer_Reference *ref;

   /* try to get the surface from this listener */
   ref = container_of(listener, E_Wayland_Buffer_Reference, destroy_listener);
   if (!ref) return;

   /* set referenced buffer to null */
   ref->buffer = NULL;
}

static E_Wayland_Buffer *
_e_comp_wl_surface_buffer_resource(struct wl_resource *resource)
{
   E_Wayland_Buffer *buffer;
   struct wl_listener *listener;

   listener = 
     wl_resource_get_destroy_listener(resource, 
                                      _e_comp_wl_surface_buffer_cb_destroy);
   if (listener)
     buffer = container_of(listener, E_Wayland_Buffer, wl.destroy_listener);
   else
     {
        buffer = E_NEW_RAW(E_Wayland_Buffer, 1);
        memset(buffer, 0, sizeof(*buffer));
        buffer->wl.resource = resource;
        wl_signal_init(&buffer->wl.destroy_signal);
        buffer->wl.destroy_listener.notify = 
          _e_comp_wl_surface_buffer_cb_destroy;
        wl_resource_add_destroy_listener(resource, 
                                         &buffer->wl.destroy_listener);
     }

   return buffer;
}

static void 
_e_comp_wl_surface_buffer_cb_destroy(struct wl_listener *listener, void *data)
{
   E_Wayland_Buffer *buffer;

   buffer = container_of(listener, E_Wayland_Buffer, wl.destroy_listener);

   wl_signal_emit(&buffer->wl.destroy_signal, buffer);
   if (buffer->ews)
     {
        if (buffer->ews->ec && buffer->ews->pixmap && (e_pixmap_resource_get(buffer->ews->pixmap) == data) &&
            evas_object_visible_get(buffer->ews->ec->frame))
          {
             //INF("DESTROYED CURRENT BUFFER: %s", e_pixmap_dirty_get(buffer->ews->pixmap) ? "DIRTY" : "CLEAN");
             e_pixmap_usable_set(buffer->ews->pixmap, 0);
             if (!e_pixmap_image_exists(buffer->ews->pixmap))
               {
                  e_pixmap_image_refresh(buffer->ews->pixmap);
               }
             
             e_pixmap_image_clear(buffer->ews->pixmap, 0);
             e_comp_object_damage(buffer->ews->ec->frame, 0, 0, buffer->ews->ec->client.w, buffer->ews->ec->client.h);
             e_comp_object_render(buffer->ews->ec->frame);
             e_comp_object_render_update_del(buffer->ews->ec->frame);
          }
        buffer->ews->buffers = eina_inlist_remove(buffer->ews->buffers, EINA_INLIST_GET(buffer));
     }
   E_FREE(buffer);
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
   E_Wayland_Buffer *buffer = NULL;

   /* try to cast the resource data to our surface structure */
   if (!(ews = wl_resource_get_user_data(resource))) return;

   if (buffer_resource) 
     {
        buffer = _e_comp_wl_surface_buffer_resource(buffer_resource);
        if (ews->ec && (!ews->buffer_reference.buffer))
          {
             e_pixmap_usable_set(ews->pixmap, 1);
          }
     }

   /* reference any existing buffers */
   _e_comp_wl_surface_buffer_reference(&ews->buffer_reference, buffer);
   if (buffer)
     {
        if (!buffer->ews)
          ews->buffers = eina_inlist_append(ews->buffers, EINA_INLIST_GET(buffer));
        buffer->ews = ews;
     }
   //INF("ATTACHED NEW BUFFER");
   e_pixmap_dirty(ews->pixmap);
   //if (ews->ec)
     //e_comp_object_damage(ews->ec->frame, 0, 0, ews->ec->client.w, ews->ec->client.h);
     

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
     wl_signal_add(&buffer->wl.destroy_signal, &ews->pending.buffer_destroy);
}

static void 
_e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource data to our surface structure */
   if (!(ews = wl_resource_get_user_data(resource))) return;
   e_pixmap_image_clear(ews->pixmap, 1);

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
   if (!(ews = wl_resource_get_user_data(resource))) return;

   /* try to allocate space for a new frame callback */
   if (!(cb = E_NEW(E_Wayland_Surface_Frame_Callback, 1)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   cb->wl.resource = 
     wl_resource_create(client, &wl_callback_interface, 1, callback);
   wl_resource_set_implementation(cb->wl.resource, NULL, cb, 
                                  _e_comp_wl_surface_cb_frame_destroy);

   /* add this callback to the surface list of pending frames */
   wl_list_insert(ews->pending.frames.prev, &cb->wl.link);
}

static void 
_e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Wayland_Surface *ews = NULL;

   /* try to cast the resource data to our surface structure */
   if (!(ews = wl_resource_get_user_data(resource))) return;

   if (region_resource)
     {
        E_Wayland_Region *ewr = NULL;

        /* copy this region to the pending opaque region */
        if ((ewr = wl_resource_get_user_data(region_resource)))
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
   if (!(ews = wl_resource_get_user_data(resource))) return;

   if (region_resource)
     {
        E_Wayland_Region *ewr = NULL;

        /* copy this region to the pending input region */
        if ((ewr = wl_resource_get_user_data(region_resource)))
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
   if (!(ews = wl_resource_get_user_data(resource))) return;

   /* if we have a pending buffer or a new pending buffer, attach it */
   if ((ews->pending.buffer) || (ews->pending.new_buffer))
     {
        /* reference the pending buffer */
        _e_comp_wl_surface_buffer_reference(&ews->buffer_reference, 
                                            ews->pending.buffer);

        /* if the pending buffer is NULL, unmap the surface */
        if (!ews->pending.buffer)
          {
             if (ews->mapped)
               {
                  if (ews->unmap) ews->unmap(ews);
               }
          }
     }
   e_pixmap_dirty(ews->pixmap);
   e_pixmap_refresh(ews->pixmap);
   e_pixmap_size_get(ews->pixmap, &bw, &bh);

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
   if (ews->ec)
     {
        rects = pixman_region32_rectangles(&ews->region.damage, &n);
        while (n--)
          {
             pixman_box32_t *r;

             r = &rects[n];

             /* send damages to the image */
             e_comp_object_damage(ews->ec->frame, r->x1, r->y1, r->x2, r->y2);
          }
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
        if (ews->ec)
          e_comp_object_input_area_set(ews->ec->frame, rects->x1, rects->y1, 
                                       rects->x2, rects->y2); 
     }

   /* put any pending frame callbacks into active list */
   wl_list_insert_list(&ews->wl.frames, &ews->pending.frames);

   /* clear list of pending frame callbacks */
   wl_list_init(&ews->pending.frames);

   ews->updates = 1;

   _e_wl_comp->surfaces = eina_inlist_promote(_e_wl_comp->surfaces, EINA_INLIST_GET(ews));

   /* TODO: schedule repaint ?? */
}

static void 
_e_comp_wl_surface_cb_buffer_transform_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int transform EINA_UNUSED)
{

}

static void 
_e_comp_wl_surface_cb_buffer_scale_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int scale EINA_UNUSED)
{

}
