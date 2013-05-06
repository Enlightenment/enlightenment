#include "e.h"

/* local function prototypes */
static void _e_buffer_cb_destroy(struct wl_listener *listener, void *data);

EAPI void 
e_buffer_reference(E_Buffer_Reference *br, struct wl_buffer *buffer)
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
             wl_resource_queue_event(&br->buffer->resource, 
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
        wl_signal_add(&buffer->resource.destroy_signal, &br->buffer_destroy);
     }

   br->buffer = buffer;
   br->buffer_destroy.notify = _e_buffer_cb_destroy;
}

/* local functions */
static void 
_e_buffer_cb_destroy(struct wl_listener *listener, void *data)
{
   E_Buffer_Reference *br;

   /* try to get the buffer_reference structure from the listener */
   if (!(br = container_of(listener, E_Buffer_Reference, buffer_destroy)))
     return;

   /* if this buffer is not equal to the one referenced, then get out */
   if ((struct wl_buffer *)data != br->buffer) return;

   br->buffer = NULL;
}
