#include "e.h"

/* local function prototypes */
static void _e_input_capabilities_update(E_Input *seat);

static void _e_input_cb_bind(struct wl_client *client, void *data, unsigned int version, unsigned int id);
static void _e_input_cb_unbind(struct wl_resource *resource);
static void _e_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_cb_touch_get(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_input_pointer_cb_cursor_set(struct wl_client *client, struct wl_resource *resource, unsigned int serial, struct wl_resource *surface_resource, int x, int y);

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

/* local functions */
static void 
_e_input_capabilities_update(E_Input *seat)
{
   struct wl_resource *res;
   enum wl_seat_capability caps = 0;

   if (seat->base.pointer)
     caps |= WL_SEAT_CAPABILITY_POINTER;
   if (seat->base.keyboard)
     caps |= WL_SEAT_CAPABILITY_KEYBOARD;
   if (seat->base.touch)
     caps |= WL_SEAT_CAPABILITY_TOUCH;

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

   if (seat->base.pointer)
     caps |= WL_SEAT_CAPABILITY_POINTER;
   if (seat->base.keyboard)
     caps |= WL_SEAT_CAPABILITY_KEYBOARD;
   if (seat->base.touch)
     caps |= WL_SEAT_CAPABILITY_TOUCH;

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

   /* TODO handle focus */
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
