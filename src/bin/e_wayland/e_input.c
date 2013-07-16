#include "e.h"
#include <sys/mman.h>
#include <fcntl.h>

/* local function prototypes */
static void _e_input_capabilities_update(E_Input *seat);

static void _e_input_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id);
static void _e_input_cb_unbind(struct wl_resource *resource);
static void _e_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_cb_touch_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_pointer_cb_cursor_set(struct wl_client *client, struct wl_resource *resource, unsigned int serial, struct wl_resource *surface_resource, int x, int y);
static void _e_input_pointer_cb_focus(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_input_pointer_grab_cb_focus(E_Input_Pointer_Grab *grab);
static void _e_input_pointer_grab_cb_motion(E_Input_Pointer_Grab *grab, unsigned int timestamp);
static void _e_input_pointer_grab_cb_button(E_Input_Pointer_Grab *grab, unsigned int timestamp, unsigned int button, unsigned int state);
static int _e_input_keyboard_keymap_fd_get(off_t size);
static void _e_input_keyboard_cb_focus(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_input_keyboard_grab_cb_key(E_Input_Keyboard_Grab *grab, unsigned int timestamp, unsigned int key, unsigned int state);
static void _e_input_keyboard_grab_cb_modifiers(E_Input_Keyboard_Grab *grab, unsigned int serial, unsigned int pressed, unsigned int latched, unsigned int locked, unsigned int group);

static struct wl_resource *_e_input_surface_resource_get(struct wl_list *list, E_Surface *surface);

/* wayland interfaces */
static const struct wl_seat_interface _e_input_interface = 
{
   _e_input_cb_pointer_get,
   _e_input_cb_keyboard_get,
   _e_input_cb_touch_get,
};

static const struct wl_pointer_interface _e_pointer_interface = 
{
   _e_input_pointer_cb_cursor_set
};

static E_Input_Pointer_Grab_Interface _e_pointer_grab_interface = 
{
   _e_input_pointer_grab_cb_focus,
   _e_input_pointer_grab_cb_motion,
   _e_input_pointer_grab_cb_button
};

static E_Input_Keyboard_Grab_Interface _e_keyboard_grab_interface = 
{
   _e_input_keyboard_grab_cb_key,
   _e_input_keyboard_grab_cb_modifiers,
};

/* external functions */
EAPI Eina_Bool 
e_input_init(E_Compositor *comp, E_Input *seat, const char *name)
{
   memset(seat, 0, sizeof(*seat));

   wl_list_init(&seat->resources);
   wl_list_init(&seat->drag_resources);

   wl_signal_init(&seat->signals.selection);
   wl_signal_init(&seat->signals.destroy);

   wl_global_create(comp->wl.display, &wl_seat_interface, 2, 
                    seat, _e_input_cb_bind);

   seat->name = strdup(name);

   seat->compositor = comp;

   comp->inputs = eina_list_append(comp->inputs, seat);

   wl_signal_emit(&comp->signals.seat, seat);

   return EINA_TRUE;
}

EAPI Eina_Bool 
e_input_shutdown(E_Input *seat)
{
   if (!seat) return EINA_TRUE;

   /* wl_list_remove(&seat->link); */
   free(seat->name);

   wl_signal_emit(&seat->signals.destroy, seat);

   return EINA_TRUE;
}

EAPI Eina_Bool 
e_input_pointer_init(E_Input *seat)
{
   E_Input_Pointer *ptr;

   if (seat->pointer) return EINA_TRUE;

   /* try to allocate space for new pointer structure */
   if (!(ptr = E_NEW(E_Input_Pointer, 1))) return EINA_FALSE;

   wl_list_init(&ptr->resources);
   ptr->focus_listener.notify = _e_input_pointer_cb_focus;
   ptr->default_grab.interface = &_e_pointer_grab_interface;
   ptr->default_grab.pointer = ptr;
   ptr->grab = &ptr->default_grab;
   wl_signal_init(&ptr->signals.focus);

   wl_list_init(&ptr->grab->surfaces);

   ptr->seat = seat;
   seat->pointer = ptr;

   _e_input_capabilities_update(seat);

   return EINA_TRUE;
}

EAPI Eina_Bool 
e_input_keyboard_init(E_Input *seat, struct xkb_keymap *keymap)
{
   if (seat->keyboard) return EINA_TRUE;

   if (keymap)
     {
        seat->kbd_info.keymap = xkb_map_ref(keymap);
        if (!e_input_keyboard_info_keymap_add(&seat->kbd_info))
          {
             xkb_map_unref(seat->kbd_info.keymap);
             return EINA_FALSE;
          }
     }
   else
     {
        /* TODO: build a global keymap */
     }

   if (!(seat->kbd.state = xkb_state_new(seat->kbd_info.keymap)))
     {
        /* TODO: cleanup keymap */
        return EINA_FALSE;
     }

   if (!(seat->keyboard = e_input_keyboard_add(seat)))
     {
        /* TODO: cleanup */
        return EINA_FALSE;
     }

   _e_input_capabilities_update(seat);

   return EINA_TRUE;
}

EAPI Eina_Bool 
e_input_touch_init(E_Input *seat)
{
   return EINA_TRUE;
}

EAPI void 
e_input_pointer_focus_set(E_Input_Pointer *pointer, E_Surface *surface, Evas_Coord x, Evas_Coord y)
{
   E_Input_Keyboard *kbd;
   struct wl_resource *resource;
   struct wl_display *disp;
   unsigned int serial = 0;

   kbd = pointer->seat->keyboard;

   resource = pointer->focus_resource;
   if ((resource) && (pointer->focus != surface))
     {
        disp = wl_client_get_display(wl_resource_get_client(resource));
        serial = wl_display_next_serial(disp);
        wl_pointer_send_leave(resource, serial, pointer->focus->wl.resource);
//        wl_list_remove(&pointer->focus_listener.link);
     }

   if (!surface) return;

   resource = _e_input_surface_resource_get(&pointer->resources, surface);
   if ((resource) && 
       ((pointer->focus != surface) || 
           (pointer->focus_resource != resource)))
     {
        disp = wl_client_get_display(wl_resource_get_client(resource));
        serial = wl_display_next_serial(disp);

        if (kbd)
          {
             struct wl_resource *res;

             res = _e_input_surface_resource_get(&kbd->resources, surface);
             if (res)
               {
                  wl_keyboard_send_modifiers(res, serial, 
                                             kbd->modifiers.pressed,
                                             kbd->modifiers.latched,
                                             kbd->modifiers.locked,
                                             kbd->modifiers.group);
               }
          }

        wl_pointer_send_enter(resource, serial, surface->wl.resource, 
                              wl_fixed_from_int(x), wl_fixed_from_int(y));
        wl_resource_add_destroy_listener(resource, &pointer->focus_listener);
        pointer->focus_serial = serial;
     }

   pointer->focus_resource = resource;
   pointer->focus = surface;
   wl_signal_emit(&pointer->signals.focus, pointer);
}

EAPI void 
e_input_pointer_grab_start(E_Input_Pointer *pointer)
{
   if (!pointer) return;

   printf("Input Pointer Grab Start\n");

   if ((pointer->grab) && (pointer->grab->interface))
     {
        if (pointer->grab->interface->focus)
          pointer->grab->interface->focus(pointer->grab);
     }
}

EAPI void 
e_input_pointer_grab_end(E_Input_Pointer *pointer)
{
   if (!pointer) return;

   printf("Input Pointer Grab End\n");

   pointer->grab = &pointer->default_grab;
   if ((pointer->grab) && (pointer->grab->interface))
     {
        if (pointer->grab->interface->focus)
          pointer->grab->interface->focus(pointer->grab);
     }
}

EAPI Eina_Bool 
e_input_keyboard_info_keymap_add(E_Input_Keyboard_Info *kbd_info)
{
   char *str = NULL;

   kbd_info->mods.shift = 
     xkb_map_mod_get_index(kbd_info->keymap, XKB_MOD_NAME_SHIFT);
   kbd_info->mods.caps = 
     xkb_map_mod_get_index(kbd_info->keymap, XKB_MOD_NAME_CAPS);
   kbd_info->mods.ctrl = 
     xkb_map_mod_get_index(kbd_info->keymap, XKB_MOD_NAME_CTRL);
   kbd_info->mods.alt = 
     xkb_map_mod_get_index(kbd_info->keymap, XKB_MOD_NAME_ALT);
   kbd_info->mods.super = 
     xkb_map_mod_get_index(kbd_info->keymap, XKB_MOD_NAME_LOGO);

   if (!(str = xkb_map_get_as_string(kbd_info->keymap)))
     return EINA_FALSE;

   kbd_info->size = strlen(str) + 1;
   kbd_info->fd = _e_input_keyboard_keymap_fd_get(kbd_info->size);
   if (kbd_info->fd < 0)
     {
        free(str);
        return EINA_FALSE;
     }

   kbd_info->area = 
     mmap(NULL, kbd_info->size, (PROT_READ | PROT_WRITE), 
          MAP_SHARED, kbd_info->fd, 0);
   if (kbd_info->area == MAP_FAILED)
     {
        close(kbd_info->fd);
        kbd_info->fd = -1;
        free(str);
        return EINA_FALSE;
     }

   if ((kbd_info->area) && (str)) strcpy(kbd_info->area, str);
   free(str);

   return EINA_TRUE;
}

EAPI void 
e_input_keyboard_focus_set(E_Input_Keyboard *keyboard, E_Surface *surface)
{
   struct wl_resource *res;
   struct wl_display *disp;
   unsigned int serial = 0;

   if ((keyboard->focus_resource) && (keyboard->focus != surface))
     {
        res = keyboard->focus_resource;
        disp = wl_client_get_display(wl_resource_get_client(res));
        serial = wl_display_next_serial(disp);
        printf("Send Keyboard Leave: %p\n", keyboard->focus);
        if (surface) printf("\tSurface: %p\n", surface);
        else printf("\tNO SURFACE\n");
        wl_keyboard_send_leave(res, serial, keyboard->focus->wl.resource);
        wl_list_remove(&keyboard->focus_listener.link);
     }

   res = _e_input_surface_resource_get(&keyboard->resources, surface);
   if ((res) && 
       ((keyboard->focus != surface) || (keyboard->focus_resource != res)))
     {
        disp = wl_client_get_display(wl_resource_get_client(res));
        serial = wl_display_next_serial(disp);
        wl_keyboard_send_modifiers(res, serial, 
                                   keyboard->modifiers.pressed,
                                   keyboard->modifiers.latched,
                                   keyboard->modifiers.locked,
                                   keyboard->modifiers.group);
        printf("Send Keyboard Enter: %p\n", surface);
        wl_keyboard_send_enter(res, serial, 
                               surface->wl.resource, &keyboard->keys);
        wl_resource_add_destroy_listener(res, &keyboard->focus_listener);
        keyboard->focus_serial = serial;
     }

   keyboard->focus_resource = res;
   keyboard->focus = surface;
   wl_signal_emit(&keyboard->signals.focus, keyboard);
}

EAPI E_Input_Keyboard *
e_input_keyboard_add(E_Input *seat)
{
   E_Input_Keyboard *kbd;

   if (!(kbd = E_NEW(E_Input_Keyboard, 1))) return NULL;

   wl_list_init(&kbd->resources);
   wl_array_init(&kbd->keys);
   kbd->focus_listener.notify = _e_input_keyboard_cb_focus;
   kbd->default_grab.interface = &_e_keyboard_grab_interface;
   kbd->default_grab.keyboard = kbd;
   kbd->grab = &kbd->default_grab;
   wl_signal_init(&kbd->signals.focus);

   return kbd;
}

EAPI void 
e_input_keyboard_del(E_Input_Keyboard *keyboard)
{
   if (!keyboard) return;
   if (keyboard->focus_resource) 
     wl_list_remove(&keyboard->focus_listener.link);
   wl_array_release(&keyboard->keys);
   E_FREE(keyboard);
}

EAPI void 
e_input_repick(E_Input *seat)
{
   E_Input_Pointer_Grab_Interface *interface;

   if (!seat->pointer) return;
   interface = seat->pointer->grab->interface;
   interface->focus(seat->pointer->grab);
}

EAPI void 
e_input_keyboard_focus_destroy(struct wl_listener *listener, void *data)
{
   E_Input *seat;

   printf("Input Keyboard Focus Destroy\n");
   if ((seat = container_of(listener, E_Input, kbd.focus_listener)))
     seat->kbd.saved_focus = NULL;
}

/* local functions */
static void 
_e_input_capabilities_update(E_Input *seat)
{
   struct wl_list *link;
   enum wl_seat_capability caps = 0;

   if (seat->pointer)
     caps |= WL_SEAT_CAPABILITY_POINTER;
   if (seat->keyboard)
     caps |= WL_SEAT_CAPABILITY_KEYBOARD;
   /* if (seat->touch) */
   /*   caps |= WL_SEAT_CAPABILITY_TOUCH; */

   for (link = seat->resources.next; 
        link != &seat->resources; link = link->next)
     {
        wl_seat_send_capabilities(wl_resource_from_link(link), caps);
     }
}

static void 
_e_input_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id)
{
   E_Input *seat;
   struct wl_resource *res;
   enum wl_seat_capability caps = 0;

   if (!(seat = data)) return;

   res = wl_resource_create(client, &wl_seat_interface, MIN(version, 2), id);
   wl_list_insert(&seat->resources, wl_resource_get_link(res));
   wl_resource_set_implementation(res, &_e_input_interface, data, 
                                  _e_input_cb_unbind);

   if (seat->pointer)
     caps |= WL_SEAT_CAPABILITY_POINTER;
   if (seat->keyboard)
     caps |= WL_SEAT_CAPABILITY_KEYBOARD;
   /* if (seat->touch) */
   /*   caps |= WL_SEAT_CAPABILITY_TOUCH; */

   wl_seat_send_capabilities(res, caps);

   if (version >= 2) wl_seat_send_name(res, seat->name);
}

static void 
_e_input_cb_unbind(struct wl_resource *resource)
{
   wl_list_remove(&resource->link);
   free(resource);
}

static void 
_e_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Input *seat;
   struct wl_resource *res;

   if (!(seat = wl_resource_get_user_data(resource))) return;
   if (!seat->pointer) return;

   res = wl_resource_create(client, &wl_pointer_interface, 
                            wl_resource_get_version(resource), id);
   wl_list_insert(&seat->pointer->resources, wl_resource_get_link(res));
   wl_resource_set_implementation(res, &_e_pointer_interface, 
                                  seat->pointer, _e_input_cb_unbind);

   if ((seat->pointer->focus) && 
       (wl_resource_get_client(seat->pointer->focus->wl.resource) == client))
     {
        E_Surface *es;

        es = seat->pointer->focus;
        e_input_pointer_focus_set(seat->pointer, es, 
                                  seat->pointer->x - es->geometry.x, 
                                  seat->pointer->y - es->geometry.y);
     }
}

static void 
_e_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Input *seat;
   struct wl_resource *res;

   if (!(seat = wl_resource_get_user_data(resource))) return;
   if (!seat->keyboard) return;

   res = wl_resource_create(client, &wl_keyboard_interface, 
                            wl_resource_get_version(resource), id);

   wl_list_insert(&seat->keyboard->resources, wl_resource_get_link(res));
   wl_resource_set_implementation(res, NULL, seat, _e_input_cb_unbind);

   wl_keyboard_send_keymap(res, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, 
                           seat->kbd_info.fd, seat->kbd_info.size);

   if ((seat->keyboard->focus) && 
       (seat->kbd.saved_focus != seat->keyboard->focus) && 
       (wl_resource_get_client(seat->keyboard->focus->wl.resource) == client))
     {
        /* printf("Input Keyboard Get. Set Focus %p\n", seat->keyboard->focus); */
        e_input_keyboard_focus_set(seat->keyboard, seat->keyboard->focus);
        /* FIXME */
        /* wl_data_device_set_keyboard_focus(seat); */
     }
}

static void 
_e_input_cb_touch_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{

}

static void 
_e_input_pointer_cb_cursor_set(struct wl_client *client, struct wl_resource *resource, unsigned int serial, struct wl_resource *surface_resource, int x, int y)
{
   E_Input_Pointer *ptr;
   E_Surface *es;

   if (!(ptr = wl_resource_get_user_data(resource))) return;
   if (surface_resource) es = wl_resource_get_user_data(surface_resource);
   if (!ptr->focus) return;
   if (!ptr->focus->wl.resource) return;
   if (wl_resource_get_client(ptr->focus->wl.resource) != client) return;
   if (ptr->focus_serial - serial > UINT32_MAX / 2) return;

   /* TODO */
   /* if ((es) && (es != ptr->sprite)) */
   /*   { */

   /*   } */
}

static void 
_e_input_pointer_cb_focus(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Input_Pointer *ptr;

   ptr = container_of(listener, E_Input_Pointer, focus_listener);
   ptr->focus_resource = NULL;
}

static void 
_e_input_pointer_grab_cb_focus(E_Input_Pointer_Grab *grab)
{
   E_Input_Pointer *ptr;
   E_Surface *es;

   if (!(ptr = grab->pointer)) return;

   es = e_compositor_surface_find(ptr->seat->compositor, ptr->x, ptr->y);
   if (ptr->focus != es)
     e_input_pointer_focus_set(ptr, es, ptr->x, ptr->y);
}

static void 
_e_input_pointer_grab_cb_motion(E_Input_Pointer_Grab *grab, unsigned int timestamp)
{
   E_Input_Pointer *ptr;

   if (!(ptr = grab->pointer)) return;

   if (ptr->focus_resource)
     wl_pointer_send_motion(ptr->focus_resource, timestamp, 
                            wl_fixed_from_int(ptr->x), 
                            wl_fixed_from_int(ptr->y));
}

static void 
_e_input_pointer_grab_cb_button(E_Input_Pointer_Grab *grab, unsigned int timestamp, unsigned int button, unsigned int state)
{
   E_Input_Pointer *ptr;
   struct wl_resource *res;

   if (!(ptr = grab->pointer)) return;

   if ((res = ptr->focus_resource))
     {
        struct wl_display *disp;
        unsigned int serial = 0;

        disp = wl_client_get_display(res->client);
        serial = wl_display_next_serial(disp);
        wl_pointer_send_button(res, serial, timestamp, button, state);
     }

   if ((ptr->grab->button_count == 0) && 
       (state == WL_POINTER_BUTTON_STATE_RELEASED))
     {
        E_Surface *es;

        if ((es = e_compositor_surface_find(ptr->seat->compositor, 
                                            ptr->x, ptr->y)))
          e_input_pointer_focus_set(ptr, es, ptr->x, ptr->y);
     }
}

static int 
_e_input_keyboard_keymap_fd_get(off_t size)
{
   const char *path;
   char tmp[PATH_MAX];
   int fd = 0;
   long flags;

   if (!(path = getenv("XDG_RUNTIME_DIR"))) return -1;

   strcpy(tmp, path);
   strcat(tmp, "/e_wayland-keymap-XXXXXX");

   if ((fd = mkstemp(tmp)) < 0) return -1;

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

static void 
_e_input_keyboard_cb_focus(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Input_Keyboard *kbd;

   printf("Input Keyboard Cb Focus\n");
   kbd = container_of(listener, E_Input_Keyboard, focus_listener);
   kbd->focus_resource = NULL;
}

static void 
_e_input_keyboard_grab_cb_key(E_Input_Keyboard_Grab *grab, unsigned int timestamp, unsigned int key, unsigned int state)
{
   E_Input_Keyboard *kbd;
   struct wl_resource *res;

   if (!(kbd = grab->keyboard)) return;
   if ((res = kbd->focus_resource))
     {
        struct wl_display *disp;
        unsigned int serial = 0;

        disp = wl_client_get_display(res->client);
        serial = wl_display_next_serial(disp);
        wl_keyboard_send_key(res, serial, timestamp, key, state);
     }
}

static void 
_e_input_keyboard_grab_cb_modifiers(E_Input_Keyboard_Grab *grab, unsigned int serial, unsigned int pressed, unsigned int latched, unsigned int locked, unsigned int group)
{
   E_Input_Keyboard *kbd;
   E_Input_Pointer *ptr;
   struct wl_resource *res;

   if (!(kbd = grab->keyboard)) return;
   if (!(res = kbd->focus_resource)) return;

   wl_keyboard_send_modifiers(res, serial, pressed, latched, locked, group);

   if (!(ptr = kbd->seat->pointer)) return;
   if ((ptr->focus) && (ptr->focus != kbd->focus))
     {
        struct wl_resource *pres;

        if ((pres = 
             _e_input_surface_resource_get(&kbd->resources, ptr->focus)))
          {
             wl_keyboard_send_modifiers(pres, serial, 
                                        kbd->modifiers.pressed, 
                                        kbd->modifiers.latched,
                                        kbd->modifiers.locked,
                                        kbd->modifiers.group);
          }
     }
}

static struct wl_resource *
_e_input_surface_resource_get(struct wl_list *list, E_Surface *surface)
{
   if (!surface) return NULL;

   return wl_resource_find_for_client(list, wl_resource_get_client(surface->wl.resource));
}
