#include "e.h"

/* local function prototypes */
static void _e_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_surface_cb_attach(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *buffer_resource, int x, int y);
static void _e_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h);
static void _e_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource);
static void _e_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, unsigned int id);
static void _e_surface_cb_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED);

static void _e_surface_frame_cb_destroy(struct wl_resource *resource);

/* local wayland interfaces */
static const struct wl_surface_interface _e_surface_interface = 
{
   _e_surface_cb_destroy,
   _e_surface_cb_attach,
   _e_surface_cb_damage,
   _e_surface_cb_frame,
   NULL, // cb_opaque_set
   NULL, // cb_input_set
   _e_surface_cb_commit,
   NULL // cb_buffer_transform_set
};

EAPI E_Surface *
e_surface_new(unsigned int id)
{
   E_Surface *es;

   /* try to allocate space for a new surface */
   if (!(es = E_NEW(E_Surface, 1))) return NULL;

   /* initialize the destroy signal */
   wl_signal_init(&es->wl.surface.resource.destroy_signal);

   /* initialize the link */
   wl_list_init(&es->wl.link);

   /* TODO: finish me */
   es->damage = eina_rectangle_new(0, 0, 0, 0);
   es->opaque = eina_rectangle_new(0, 0, 0, 0);
   es->input = eina_rectangle_new(0, 0, 0, 0);

   es->pending.buffer_destroy.notify = _e_surface_cb_buffer_destroy;

   /* setup the surface object */
   es->wl.surface.resource.object.id = id;
   es->wl.surface.resource.object.interface = &wl_surface_interface;
   es->wl.surface.resource.object.implementation = 
     (void (**)(void))&_e_surface_interface;
   es->wl.surface.resource.data = es;

   return es;
}

EAPI void 
e_surface_attach(E_Surface *es, struct wl_buffer *buffer)
{
   /* check for valid surface */
   if (!es) return;

   /* reference this buffer */
   e_buffer_reference(&es->buffer.reference, buffer);

   if (!buffer)
     {
        /* if we have no buffer to attach (buffer == NULL), 
         * then we assume that we want to unmap this surface (hide it) */

        /* FIXME: This may need to call the surface->unmap function if a 
         * shell wants to do something "different" when it unmaps */
        if (es->mapped) e_surface_unmap(es);
     }

   /* call renderer attach */
   if (_e_comp->attach) _e_comp->attach(es, buffer);
}

EAPI void 
e_surface_unmap(E_Surface *es)
{
   /* TODO */
}

EAPI void 
e_surface_damage(E_Surface *es)
{
   /* check for valid surface */
   if (!es) return;

   /* add this damage rectangle */
   eina_rectangle_union(es->damage, 
                        &(Eina_Rectangle){ 0, 0, es->geometry.w, es->geometry.h });

   /* TODO: schedule repaint */
}

EAPI void 
e_surface_destroy(E_Surface *es)
{
   E_Surface_Frame *cb;

   /* check for valid surface */
   if (!es) return;

   /* emit the destroy signal */
   wl_signal_emit(&es->wl.surface.resource.destroy_signal, 
                  &es->wl.surface.resource);

   /* if this surface is mapped, unmap it */
   if (es->mapped) e_surface_unmap(es);

   /* remove any pending frame callbacks */
   EINA_LIST_FREE(es->pending.frames, cb)
     wl_resource_destroy(&cb->resource);

   /* destroy pending buffer */
   if (es->pending.buffer)
     wl_list_remove(&es->pending.buffer_destroy.link);

   /* remove any buffer references */
   e_buffer_reference(&es->buffer.reference, NULL);

   /* free rectangles */
   eina_rectangle_free(es->damage);
   eina_rectangle_free(es->opaque);
   eina_rectangle_free(es->input);

   /* remove any active frame callbacks */
   EINA_LIST_FREE(es->frames, cb)
     wl_resource_destroy(&cb->resource);

   /* free the surface structure */
   E_FREE(es);
}

EAPI void 
e_surface_damage_calculate(E_Surface *es)
{
   /* check for valid surface */
   if (!es) return;

   /* check for referenced buffer */
   if (es->buffer.reference)
     {
        /* if this is an shm buffer, flush any pending damage */
        if (wl_buffer_is_shm(es->buffer.reference->buffer))
          e_compositor_damage_flush(_e_comp, es);
     }
}

/* local functions */
static void 
_e_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void 
_e_surface_cb_attach(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *buffer_resource, int x, int y)
{
   E_Surface *es;
   struct wl_buffer *buffer = NULL;

   /* try to cast the resource to our surface */
   if (!(es = resource->data)) return;

   /* if we have a buffer resource, get a wl_buffer from it */
   if (buffer_resource) buffer = buffer_resource->data;

   /* if we have a previous pending buffer, remove it
    * 
    * NB: This means that attach was called more than once without calling 
    * a commit in between, so we disgard any old buffers and just deal with 
    * the most current request */
   if (es->pending.buffer) wl_list_remove(&es->pending.buffer_destroy.link);

   /* set surface pending properties */
   es->pending.x = x;
   es->pending.y = y;
   es->pending.buffer = buffer;
   es->pending.new_attach = EINA_TRUE;

   /* if we have a valid pending buffer, setup a destroy listener */
   if (buffer) 
     wl_signal_add(&buffer->resource.destroy_signal, 
                   &es->pending.buffer_destroy);
}

static void 
_e_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int x, int y, int w, int h)
{
   E_Surface *es;
   /* Eina_Rectangle *dmg; */

   /* try to cast the resource to our surface */
   if (!(es = resource->data)) return;

   /* EINA_RECTANGLE_SET(dmg, x, y, w, h); */

   /* add this damage rectangle */
   eina_rectangle_union(&es->pending.damage, &(Eina_Rectangle){ x, y, w, h });
   /* evas_damage_rectangle_add(es->evas, x, y, w, h); */

   /* eina_rectangle_free(dmg); */
}

static void 
_e_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Surface *es;
   E_Surface_Frame *cb;
   Evas_Coord bw = 0, bh = 0;
   Eina_Rectangle *opaque;
   Eina_Bool ret = EINA_FALSE;

   /* try to cast the resource to our surface */
   if (!(es = resource->data)) return;

   /* if we have a pending buffer, attach it */
   if ((es->pending.buffer) || (es->pending.new_attach))
     e_surface_attach(es, es->pending.buffer);

   /* if we have a referenced buffer, get it's size */
   if (es->buffer.reference.buffer)
     {
        bw = es->buffer.reference.buffer->width;
        bh = es->buffer.reference.buffer->height;
     }

   /* if we attached a new buffer, call the surface configure function */
   if ((es->pending.new_attach) && (es->configure))
     es->configure(es, es->pending.x, es->pending.y, bw, bh);

   /* remove the destroy listener for the pending buffer */
   if (es->pending.buffer) wl_list_remove(&es->pending.buffer_destroy.link);

   /* clear surface pending properties */
   es->pending.buffer = NULL;
   es->pending.x = 0;
   es->pending.y = 0;
   es->pending.new_attach = EINA_FALSE;

   /* combine any pending damage */
   eina_rectangle_union(es->damage, &es->pending.damage);
   ret = 
     eina_rectangle_intersection(es->damage, 
                                 &(Eina_Rectangle){ 0, 0, es->geometry.w, es->geometry.h});

   /* free any pending damage */
   eina_rectangle_free(&es->pending.damage);

   /* combine any pending opaque */
   opaque = eina_rectangle_new(0, 0, es->geometry.w, es->geometry.h);
   ret = eina_rectangle_intersection(opaque, &es->pending.opaque);
   if (!((opaque->x == es->opaque->x) && (opaque->y == es->opaque->y) && 
         (opaque->w == es->opaque->w) && (opaque->h == es->opaque->h)))
     {
        /* copy opaque into surface */
        EINA_RECTANGLE_SET(es->opaque, opaque->x, opaque->y, opaque->w, opaque->h);

        /* mark dirty */
        es->geometry.changed = EINA_TRUE;
     }

   if (es->input) eina_rectangle_free(es->input);
   es->input = eina_rectangle_new(0, 0, es->geometry.w, es->geometry.h);
   ret = eina_rectangle_intersection(es->input, &es->pending.input);

   /* add any pending frame callbacks to main list and free pending */
   EINA_LIST_FREE(es->pending.frames, cb)
     es->frames = eina_list_append(es->frames, cb);

   /* schedule repaint */
   /* evas_damage_rectangle_add(es->evas, es->damage->x, es->damage->y,  */
   /*                           es->damage->w, es->damage->h); */
}

static void 
_e_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, unsigned int id)
{
   E_Surface *es;
   E_Surface_Frame *cb;

   /* try to cast the resource to our surface */
   if (!(es = resource->data)) return;

   /* try to create a new frame callback */
   if (!(cb = E_NEW_RAW(E_Surface_Frame, 1)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   cb->surface = es;

   /* setup the callback object */
   cb->resource.object.interface = &wl_callback_interface;
   cb->resource.object.id = id;
   cb->resource.destroy = _e_surface_frame_cb_destroy;
   cb->resource.client = client;
   cb->resource.data = cb;

   /* add this callback to the client */
   wl_client_add_resource(client, &cb->resource);

   /* append the callback to pending frames */
   es->pending.frames = eina_list_prepend(es->pending.frames, cb);
}

static void 
_e_surface_cb_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Surface *es;

   /* try to get the surface structure from the listener */
   if (!(es = container_of(listener, E_Surface, pending.buffer_destroy)))
     return;

   /* clear the pending buffer */
   es->pending.buffer = NULL;
}

static void 
_e_surface_frame_cb_destroy(struct wl_resource *resource)
{
   E_Surface *es;
   E_Surface_Frame *cb;

   /* try to cast the resource to our callback */
   if (!(cb = resource->data)) return;

   es = cb->surface;

   /* remove this callback from the pending frames callback list */
   es->pending.frames = eina_list_remove(es->pending.frames, cb);

   /* remove this callback from the frames callback list */
   es->frames = eina_list_remove(es->frames, cb);

   /* free the callback structure */
   E_FREE(cb);
}
