#include "e.h"

/* local function prototypes */
static void _e_buffer_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED);
static void _e_buffer_reference_cb_destroy(struct wl_listener *listener, void *data);

EAPI E_Buffer *
e_buffer_resource_get(struct wl_resource *resource)
{
   E_Buffer *buffer;
   struct wl_listener *listener;

   listener = wl_resource_get_destroy_listener(resource, _e_buffer_cb_destroy);
   if (listener)
     buffer = container_of(listener, E_Buffer, buffer_destroy);
   else
     {
        if (!(buffer = E_NEW_RAW(E_Buffer, 1)))
          return NULL;

        buffer->w = 0;
        buffer->h = 0;
        buffer->wl.resource = resource;
        wl_signal_init(&buffer->signals.destroy);
        buffer->buffer_destroy.notify = _e_buffer_cb_destroy;
        wl_resource_add_destroy_listener(resource, &buffer->buffer_destroy);
     }

   return buffer;
}

EAPI void 
e_buffer_reference(E_Buffer_Reference *br, E_Buffer *buffer)
{
   /* check for valid buffer reference */
   if (!br) return;

   /* check if the new buffer is difference than the one we already have 
    * referenced */
   if ((br->buffer) && (br->buffer != buffer))
     {
        br->buffer->busy_count--;
        if (br->buffer->busy_count == 0)
          {
             /* queue a release event */
             wl_resource_queue_event(br->buffer->wl.resource, 
                                     WL_BUFFER_RELEASE);
          }

        /* remove any existing destroy listener */
        wl_list_remove(&br->buffer_destroy.link);
     }

   /* if we have a valid buffer, reference it */
   if ((buffer) && (br->buffer != buffer))
     {
        buffer->busy_count++;

        /* setup destroy listener */
        wl_signal_add(&buffer->signals.destroy, &br->buffer_destroy);
     }

   br->buffer = buffer;
   br->buffer_destroy.notify = _e_buffer_reference_cb_destroy;
}

/* local functions */
static void 
_e_buffer_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Buffer *buffer;

   /* try to get the buffer_reference structure from the listener */
   if (!(buffer = container_of(listener, E_Buffer, buffer_destroy)))
     return;

   wl_signal_emit(&buffer->signals.destroy, buffer);

   E_FREE(buffer);
}

static void 
_e_buffer_reference_cb_destroy(struct wl_listener *listener, void *data)
{
   E_Buffer_Reference *br;

   if (!(br = container_of(listener, E_Buffer_Reference, buffer_destroy)))
     return;

   if ((E_Buffer *)data != br->buffer) return;

   br->buffer = NULL;
}
