#include "e.h"

/* local function prototypes */
static void _e_input_capabilities_update(E_Input *seat);

static void _e_input_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id);
static void _e_input_cb_unbind(struct wl_resource *resource);
static void _e_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_cb_touch_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_pointer_cb_cursor_set(struct wl_client *client, struct wl_resource *resource, unsigned int serial, struct wl_resource *surface_resource, int x, int y);
static void _e_input_pointer_grab_cb_focus(E_Input_Pointer_Grab *grab);
static void _e_input_pointer_grab_cb_motion(E_Input_Pointer_Grab *grab, unsigned int timestamp);
static void _e_input_pointer_grab_cb_button(E_Input_Pointer_Grab *grab, unsigned int timestamp, unsigned int button, unsigned int state);

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

/* external functions */
EAPI Eina_Bool 
e_input_init(E_Compositor *comp, E_Input *seat, const char *name)
{
   memset(seat, 0, sizeof(*seat));

   wl_list_init(&seat->resources);
   wl_list_init(&seat->drag_resources);

   wl_signal_init(&seat->signals.selection);
   wl_signal_init(&seat->signals.destroy);

   wl_display_add_global(comp->wl.display, &wl_seat_interface, seat, 
                         _e_input_cb_bind);

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

   /* FIXME: Finish setting up pointer */

   wl_list_init(&ptr->resources);
   wl_signal_init(&ptr->signals.focus);

   ptr->default_grab.interface = &_e_pointer_grab_interface;
   ptr->default_grab.pointer = ptr;
   ptr->grab = &ptr->default_grab;

   wl_list_init(&ptr->grab->surfaces);

   ptr->seat = seat;
   seat->pointer = ptr;

   _e_input_capabilities_update(seat);

   return EINA_TRUE;
}

EAPI Eina_Bool 
e_input_keyboard_init(E_Input *seat)
{
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
   struct wl_resource *resource;
   struct wl_display *disp;
   unsigned int serial = 0;

   resource = pointer->focus_resource;
   if ((resource) && (pointer->focus != surface))
     {
        disp = wl_client_get_display(resource->client);
        serial = e_compositor_get_time();
//        serial = wl_display_next_serial(disp);
        wl_pointer_send_leave(resource, serial, &pointer->focus->wl.resource);
        /* wl_list_remove(&pointer->focus_listener.link); */
     }

   resource = _e_input_surface_resource_get(&pointer->resources, surface);

//   resource = &surface->wl.resource;
   if ((resource) && 
       ((pointer->focus != surface) || 
           (pointer->focus_resource != resource)))
     {
        disp = wl_client_get_display(resource->client);
        serial = e_compositor_get_time();
//        serial = wl_display_next_serial(disp);

        wl_pointer_send_enter(resource, serial, &surface->wl.resource, 
                              wl_fixed_from_int(pointer->x), 
                              wl_fixed_from_int(pointer->y));
        /* wl_signal_add(&resource->destroy_signal, &pointer->focus_listener); */
        /* pointer->focus_serial = serial; */
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

/* local functions */
static void 
_e_input_capabilities_update(E_Input *seat)
{
   struct wl_resource *res;
   enum wl_seat_capability caps = 0;

   if (seat->pointer)
     caps |= WL_SEAT_CAPABILITY_POINTER;

   /* if (seat->keyboard) */
   /*   caps |= WL_SEAT_CAPABILITY_KEYBOARD; */
   /* if (seat->touch) */
   /*   caps |= WL_SEAT_CAPABILITY_TOUCH; */

   wl_list_for_each(res, &seat->resources, link)
     wl_seat_send_capabilities(res, caps);
}

static void 
_e_input_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id)
{
   E_Input *seat;
   struct wl_resource *res;
   enum wl_seat_capability caps = 0;

   if (!(seat = data)) return;

   res = wl_client_add_object(client, &wl_seat_interface, 
                              &_e_input_interface, id, data);

   wl_list_insert(&seat->resources, &res->link);

   res->destroy = _e_input_cb_unbind;

   if (seat->pointer)
     caps |= WL_SEAT_CAPABILITY_POINTER;
   /* if (seat->keyboard) */
   /*   caps |= WL_SEAT_CAPABILITY_KEYBOARD; */
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

   if (!(seat = resource->data)) return;
   if (!seat->pointer) return;

   res = wl_client_add_object(client, &wl_pointer_interface, 
                              &_e_pointer_interface, id, seat->pointer);

   wl_list_insert(&seat->pointer->resources, &res->link);

   res->destroy = _e_input_cb_unbind;

   if ((seat->pointer->focus) && 
       (seat->pointer->focus->wl.resource.client == client))
     {
        E_Surface *es;

        es = seat->pointer->focus;
        e_input_pointer_focus_set(seat->pointer, es, 
                                  seat->pointer->x, seat->pointer->y);
     }
}

static void 
_e_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{

}

static void 
_e_input_cb_touch_get(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{

}

static void 
_e_input_pointer_cb_cursor_set(struct wl_client *client, struct wl_resource *resource, unsigned int serial, struct wl_resource *surface_resource, int x, int y)
{

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

static struct wl_resource *
_e_input_surface_resource_get(struct wl_list *list, E_Surface *surface)
{
   struct wl_resource *ret;

   if (!surface) return NULL;

   wl_list_for_each(ret, list, link)
     if (ret->client == surface->wl.resource.client)
       return ret;

   return NULL;
}
