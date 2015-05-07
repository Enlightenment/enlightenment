#define E_COMP_WL
#include "e.h"


#define E_COMP_WL_PIXMAP_CHECK \
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return

/* local variables */
static Eina_List *handlers = NULL;
static Eina_List *subcomp_handlers = NULL;
static Eina_Hash *clients_win_hash = NULL;
static Ecore_Idle_Enterer *_client_idler = NULL;
static Eina_List *_idle_clients = NULL;
static Eina_Bool restacking = EINA_FALSE;

static void 
_e_comp_wl_focus_down_set(E_Client *ec)
{
   Ecore_Window win = 0;

   win = e_client_util_pwin_get(ec);
   e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, win);
   e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, win);
}

static void 
_e_comp_wl_focus_check(E_Comp *comp)
{
   E_Client *ec;

   if (stopping) return;
   ec = e_client_focused_get();
   if ((!ec) || (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL))
     e_grabinput_focus(comp->ee_win, E_FOCUS_METHOD_PASSIVE);
}

static void
_e_comp_wl_client_event_free(void *d EINA_UNUSED, void *event)
{
   E_Event_Client *ev;

   if (!(ev = event)) return;
   e_object_unref(E_OBJECT(ev->ec));
   free(ev);
}

static void 
_e_comp_wl_buffer_pending_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Client_Data *cd;

   if (!(cd = container_of(listener, E_Comp_Client_Data, 
                           pending.buffer_destroy)))
     return;

   cd->pending.buffer = NULL;
}

static void 
_e_comp_wl_buffer_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Wl_Buffer *buffer;

   if (!(buffer = container_of(listener, E_Comp_Wl_Buffer, destroy_listener)))
     return;

   wl_signal_emit(&buffer->destroy_signal, buffer);
   free(buffer);
}

static E_Comp_Wl_Buffer *
_e_comp_wl_buffer_get(struct wl_resource *resource)
{
   E_Comp_Wl_Buffer *buffer;
   struct wl_listener *listener;

   listener = 
     wl_resource_get_destroy_listener(resource, _e_comp_wl_buffer_cb_destroy);

   if (listener)
     return container_of(listener, E_Comp_Wl_Buffer, destroy_listener);

   if (!(buffer = E_NEW(E_Comp_Wl_Buffer, 1))) return NULL;

   buffer->resource = resource;
   wl_signal_init(&buffer->destroy_signal);
   buffer->destroy_listener.notify = _e_comp_wl_buffer_cb_destroy;
   wl_resource_add_destroy_listener(resource, &buffer->destroy_listener);

   return buffer;
}

static void 
_e_comp_wl_buffer_reference_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Wl_Buffer_Ref *ref;

   if (!(ref = container_of(listener, E_Comp_Wl_Buffer_Ref, destroy_listener)))
     return;

   ref->buffer = NULL;
}

static void 
_e_comp_wl_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static E_Comp_Wl_Subsurf *
_e_comp_wl_client_subsurf_data_get(E_Client *ec)
{
   return ec->comp_data->sub.cdata;
}

static E_Client *
_e_comp_wl_subsurface_parent_get(E_Client *ec)
{
   E_Comp_Wl_Subsurf *sub_cdata;

   if (!(sub_cdata = _e_comp_wl_client_subsurf_data_get(ec))) return NULL;

   return sub_cdata->parent;
}

static void 
_e_comp_wl_surface_cb_attach(struct wl_client *client, struct wl_resource *resource, struct wl_resource *buffer_resource, int32_t sx, int32_t sy)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Buffer *buffer = NULL;

   if (!(cp = wl_resource_get_user_data(resource))) return;

   /* try to find the E client for this surface */
   if (!(ec = e_pixmap_client_get(cp)))
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(cp));

   if ((!ec) || (e_object_is_del(E_OBJECT(ec)))) return;

   if (buffer_resource)
     {
        if (!(buffer = _e_comp_wl_buffer_get(buffer_resource)))
          {
             wl_client_post_no_memory(client);
             return;
          }
     }

   if (!ec->comp_data) return;

   if (ec->comp_data->pending.buffer)
     wl_list_remove(&ec->comp_data->pending.buffer_destroy.link);

   ec->comp_data->pending.x = sx;
   ec->comp_data->pending.y = sy;
   ec->comp_data->pending.w = 0;
   ec->comp_data->pending.h = 0;
   ec->comp_data->pending.buffer = buffer;
   ec->comp_data->pending.new_attach = EINA_TRUE;

   if (buffer)
     {
        struct wl_shm_buffer *b;

        /* trap return of shm_buffer_get as it Can fail if the buffer is not 
         * an shm buffer (ie: egl buffer) */
        if (!(b = wl_shm_buffer_get(buffer_resource))) return;
        ec->comp_data->pending.w = wl_shm_buffer_get_width(b);
        ec->comp_data->pending.h = wl_shm_buffer_get_height(b);

        wl_signal_add(&buffer->destroy_signal, 
                      &ec->comp_data->pending.buffer_destroy);
     }
}

static void 
_e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   E_Pixmap *cp;
   E_Client *ec;
   Eina_Tiler *tmp;

   if (!(cp = wl_resource_get_user_data(resource))) return;

   /* try to find the E client for this surface */
   if (!(ec = e_pixmap_client_get(cp)))
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(cp));

   if ((!ec) || (e_object_is_del(E_OBJECT(ec)))) return;

   tmp = eina_tiler_new(ec->w ?: w, ec->h ?: h);
   eina_tiler_tile_size_set(tmp, 1, 1);
   eina_tiler_rect_add(tmp, &(Eina_Rectangle){x, y, w, h});

   eina_tiler_union(ec->comp_data->pending.damage, tmp);

   eina_tiler_free(tmp);
}

static void 
_e_comp_wl_surface_cb_frame_destroy(struct wl_resource *resource)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource))) return;

   if (e_object_is_del(E_OBJECT(ec))) return;

   /* remove this frame callback from the list */
   ec->comp_data->frames = 
     eina_list_remove(ec->comp_data->frames, resource);
}

static void 
_e_comp_wl_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, uint32_t callback)
{
   E_Pixmap *cp;
   E_Client *ec;
   struct wl_resource *res;

   if (!(cp = wl_resource_get_user_data(resource))) return;

   /* try to find the E client for this surface */
   if (!(ec = e_pixmap_client_get(cp)))
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(cp));

   if ((!ec) || (e_object_is_del(E_OBJECT(ec)))) return;

   /* create frame callback */
   res = wl_resource_create(client, &wl_callback_interface, 1, callback);
   if (!res)
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_resource_set_implementation(res, NULL, ec, 
                                  _e_comp_wl_surface_cb_frame_destroy);

   /* add this frame callback to the client */
   ec->comp_data->frames = eina_list_prepend(ec->comp_data->frames, res);
}

static void 
_e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Pixmap *cp;
   E_Client *ec;

   /* DBG("E_Surface Opaque Region Set"); */

   if (!(cp = wl_resource_get_user_data(resource))) return;

   /* try to find the E client for this surface */
   if (!(ec = e_pixmap_client_get(cp)))
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(cp));

   if ((!ec) || (e_object_is_del(E_OBJECT(ec)))) return;

   if (region_resource)
     {
        Eina_Tiler *tmp;

        if (!(tmp = wl_resource_get_user_data(region_resource)))
          return;

        eina_tiler_union(ec->comp_data->pending.opaque, tmp);
     }
   else
     {
        eina_tiler_clear(ec->comp_data->pending.opaque);
        eina_tiler_rect_add(ec->comp_data->pending.opaque, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});
     }
}

static void 
_e_comp_wl_surface_cb_input_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Pixmap *cp;
   E_Client *ec;

   if (!(cp = wl_resource_get_user_data(resource))) return;

   /* try to find the E client for this surface */
   if (!(ec = e_pixmap_client_get(cp)))
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(cp));

   if ((!ec) || (e_object_is_del(E_OBJECT(ec)))) return;

   if (region_resource)
     {
        Eina_Tiler *tmp;

        if (!(tmp = wl_resource_get_user_data(region_resource)))
          return;

        eina_tiler_union(ec->comp_data->pending.input, tmp);
     }
   else
     {
        eina_tiler_clear(ec->comp_data->pending.input);
        eina_tiler_rect_add(ec->comp_data->pending.input, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});
     }
}

static void
_e_comp_wl_subsurface_destroy_internal(E_Client *ec)
{
   E_Comp_Wl_Subsurf *sub_cdata;

   sub_cdata = _e_comp_wl_client_subsurf_data_get(ec);
   if (!sub_cdata) return;

   if (sub_cdata->resource)
     {
        if (sub_cdata->parent)
          {
             sub_cdata->parent->comp_data->sub.list
                = eina_list_remove(sub_cdata->parent->comp_data->sub.list, ec);
             sub_cdata->parent = NULL;
          }

        e_comp_wl_buffer_reference(&sub_cdata->cached.buffer_ref, NULL);

        if (sub_cdata->cached.damage)
          eina_tiler_free(sub_cdata->cached.damage);
        if (sub_cdata->cached.input)
          eina_tiler_free(sub_cdata->cached.input);
        if (sub_cdata->cached.opaque)
          eina_tiler_free(sub_cdata->cached.opaque);
     }

   free(sub_cdata);
   ec->comp_data->sub.cdata = NULL;
   e_object_del(E_OBJECT(ec));
}

static void
_e_comp_wl_subsurface_destroy(struct wl_resource *resource)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource))) return;

   _e_comp_wl_subsurface_destroy_internal(ec);
}

static E_Client *
_e_comp_wl_subsurface_root_get(E_Client *ec)
{
   E_Client *parent = NULL;

   if (!_e_comp_wl_client_subsurf_data_get(ec)) return ec;
   if (!(parent = _e_comp_wl_subsurface_parent_get(ec))) return ec;

   return _e_comp_wl_subsurface_root_get(parent);
}

static Eina_Bool
_e_comp_wl_subsurface_is_synchronized(E_Comp_Wl_Subsurf *sub_cdata)
{
   while (sub_cdata)
     {
        if (sub_cdata->synchronized) return EINA_TRUE;

        if (!sub_cdata->parent) return EINA_FALSE;

        sub_cdata = _e_comp_wl_client_subsurf_data_get(sub_cdata->parent);
     }

   return EINA_FALSE;
}

static void
_e_comp_wl_subsurface_commit_to_cache(E_Client *ec)
{
   E_Comp_Client_Data *cdata;
   E_Comp_Wl_Subsurf *sub_cdata;

   if (!(cdata = ec->comp_data)) return;
   if (!(sub_cdata = cdata->sub.cdata)) return;

   // copy pending damage to cache, and clear pending damage.
   eina_tiler_union(sub_cdata->cached.damage, cdata->pending.damage);
   eina_tiler_clear(cdata->pending.damage);

   if (cdata->pending.new_attach)
     {
        sub_cdata->cached.new_attach = EINA_TRUE;
        e_comp_wl_buffer_reference(&sub_cdata->cached.buffer_ref,
                                    cdata->pending.buffer);
     }

   sub_cdata->cached.x = cdata->pending.x;
   sub_cdata->cached.y = cdata->pending.y;
   cdata->pending.x = 0;
   cdata->pending.y = 0;
   cdata->pending.new_attach = EINA_FALSE;

   sub_cdata->cached.opaque = eina_tiler_new(ec->w, ec->h);
   eina_tiler_union(sub_cdata->cached.opaque, cdata->pending.opaque);

   sub_cdata->cached.input = eina_tiler_new(ec->w, ec->h);
   eina_tiler_union(sub_cdata->cached.input, cdata->pending.input);

   sub_cdata->cached.has_data = EINA_TRUE;
}

static void
_e_comp_wl_subsurface_commit_from_cache(E_Client *ec)
{
   E_Comp_Client_Data *cdata;
   E_Comp_Wl_Subsurf *sub_cdata;
   E_Pixmap *cp;
   Eina_Tiler *src, *tmp;

   if (!(cdata = ec->comp_data)) return;
   if (!(sub_cdata = cdata->sub.cdata)) return;
   if (!(cp = ec->pixmap)) return;

   if (sub_cdata->cached.new_attach)
     {
        e_comp_wl_buffer_reference(&cdata->buffer_ref,
                                    sub_cdata->cached.buffer_ref.buffer);
        if (cdata->pending.buffer)
          e_pixmap_resource_set(cp,
                                cdata->pending.buffer->resource);
        else
          e_pixmap_resource_set(cp, NULL);
        e_pixmap_usable_set(cp, (cdata->pending.buffer != NULL));
     }

   e_pixmap_dirty(cp);
   e_pixmap_refresh(cp);

   if (sub_cdata->cached.new_attach)
     {
        if (sub_cdata->cached.buffer_ref.buffer)
          {
             if (cdata->mapped)
               {
                  if ((cdata->shell.surface) &&
                      (cdata->shell.unmap))
                    cdata->shell.unmap(cdata->shell.surface);
               }
          }
        else
          {
             if (!cdata->mapped)
               {
                  if ((cdata->shell.surface) &&
                      (cdata->shell.map))
                    cdata->shell.map(cdata->shell.surface);
               }
          }
     }

   sub_cdata->cached.x = 0;
   sub_cdata->cached.y = 0;
   sub_cdata->cached.has_data = EINA_FALSE;
   sub_cdata->cached.new_attach = EINA_FALSE;

   if ((!ec->comp->nocomp) && (ec->frame))
     {
        tmp = eina_tiler_new(ec->w, ec->h);
        eina_tiler_tile_size_set(tmp, 1, 1);
        eina_tiler_rect_add(tmp,
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

        src = eina_tiler_intersection(sub_cdata->cached.damage, tmp);
        if (src)
          {
             Eina_Rectangle *rect;
             Eina_Iterator *itr;

             itr = eina_tiler_iterator_new(src);
             EINA_ITERATOR_FOREACH(itr, rect)
               {
                  e_comp_object_damage(ec->frame,
                                       rect->x, rect->y, rect->w, rect->h);
               }
             eina_iterator_free(itr);
             eina_tiler_free(src);
          }
        eina_tiler_free(tmp);
        eina_tiler_clear(sub_cdata->cached.damage);
     }

   /* handle surface opaque region */
   tmp = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(tmp, 1, 1);
   eina_tiler_rect_add(tmp, &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

   src = eina_tiler_intersection(sub_cdata->cached.opaque, tmp);
   if (src)
     {
        Eina_Rectangle *rect;
        Eina_Iterator *itr;
        int i = 0;

        ec->shape_rects_num = 0;

        itr = eina_tiler_iterator_new(src);
        EINA_ITERATOR_FOREACH(itr, rect)
          {
             ec->shape_rects_num += 1;
          }

        ec->shape_rects =
          malloc(sizeof(Eina_Rectangle) * ec->shape_rects_num);

        if (!ec->shape_rects)
          {
             eina_iterator_free(itr);
             eina_tiler_free(src);
             eina_tiler_free(tmp);
             return;
          }
        EINA_ITERATOR_FOREACH(itr, rect)
          {
             ec->shape_rects[i] = *(Eina_Rectangle *)((char *)rect);

             ec->shape_rects[i].x = rect->x;
             ec->shape_rects[i].y = rect->y;
             ec->shape_rects[i].w = rect->w;
             ec->shape_rects[i].h = rect->h;

             i++;
          }

        eina_iterator_free(itr);
        eina_tiler_free(src);
     }

   eina_tiler_free(tmp);
   eina_tiler_clear(sub_cdata->cached.opaque);

   tmp = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(tmp, 1, 1);
   eina_tiler_rect_add(tmp, &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

   src = eina_tiler_intersection(sub_cdata->cached.input, tmp);
   if (src)
     {
        Eina_Rectangle *rect;
        Eina_Iterator *itr;
        int i = 0;

        ec->shape_input_rects_num = 0;

        itr = eina_tiler_iterator_new(src);
        EINA_ITERATOR_FOREACH(itr, rect)
          {
             ec->shape_input_rects_num += 1;
          }

        ec->shape_input_rects = 
          malloc(sizeof(Eina_Rectangle) * ec->shape_input_rects_num);

        if (!ec->shape_input_rects)
          {
             eina_iterator_free(itr);
             eina_tiler_free(src);
             eina_tiler_free(tmp);
             return;
          }

        EINA_ITERATOR_FOREACH(itr, rect)
          {
             ec->shape_input_rects[i] = 
               *(Eina_Rectangle *)((char *)rect);

             ec->shape_input_rects[i].x = rect->x;
             ec->shape_input_rects[i].y = rect->y;
             ec->shape_input_rects[i].w = rect->w;
             ec->shape_input_rects[i].h = rect->h;

             i++;
          }

        eina_iterator_free(itr);
        eina_tiler_free(src);
     }

   eina_tiler_free(tmp);
   eina_tiler_clear(sub_cdata->cached.input);

   ec->changes.shape_input = EINA_TRUE;
   EC_CHANGED(ec);
}

static void
_e_comp_wl_surface_commit(E_Client *ec)
{
   E_Pixmap *cp;
   Eina_Tiler *src, *tmp;

   if (!(cp = ec->pixmap)) return;

   if (ec->comp_data->pending.new_attach)
     {
        e_comp_wl_buffer_reference(&ec->comp_data->buffer_ref,
                                    ec->comp_data->pending.buffer);
        if (ec->comp_data->pending.buffer)
          e_pixmap_resource_set(cp,
                                ec->comp_data->pending.buffer->resource);
        else
          e_pixmap_resource_set(cp, NULL);
        e_pixmap_usable_set(cp, (ec->comp_data->pending.buffer != NULL));
     }

   e_pixmap_dirty(cp);
   e_pixmap_refresh(cp);

   if ((ec->comp_data->shell.surface) &&
       (ec->comp_data->shell.configure))
     {
        if (ec->comp_data->pending.new_attach)
          {
             if ((ec->client.w != ec->comp_data->pending.w) ||
                 (ec->client.h != ec->comp_data->pending.h))
               ec->comp_data->shell.configure(ec->comp_data->shell.surface,
                                                 ec->client.x, ec->client.y,
                                                 ec->comp_data->pending.w,
                                                 ec->comp_data->pending.h);
          }
     }

   if (ec->comp_data)
     {
        if (ec->comp_data->pending.damage)
          eina_tiler_area_size_set(ec->comp_data->pending.damage,
                                   ec->client.w, ec->client.h);

        if (ec->comp_data->pending.input)
          eina_tiler_area_size_set(ec->comp_data->pending.input,
                                   ec->client.w, ec->client.h);

        if (ec->comp_data->pending.opaque)
          eina_tiler_area_size_set(ec->comp_data->pending.opaque,
                                   ec->client.w, ec->client.h);
     }

   if (ec->comp_data->pending.new_attach)
     {
        if (!ec->comp_data->pending.buffer)
          {
             if (ec->comp_data->mapped)
               {
                  if ((ec->comp_data->shell.surface) &&
                      (ec->comp_data->shell.unmap))
                    ec->comp_data->shell.unmap(ec->comp_data->shell.surface);
               }
          }
        else
          {
             if (!ec->comp_data->mapped)
               {
                  if ((ec->comp_data->shell.surface) &&
                      (ec->comp_data->shell.map))
                    ec->comp_data->shell.map(ec->comp_data->shell.surface);
               }
          }
     }

   /* reset pending buffer */
   if (ec->comp_data->pending.buffer)
     wl_list_remove(&ec->comp_data->pending.buffer_destroy.link);

   ec->comp_data->pending.x = 0;
   ec->comp_data->pending.y = 0;
   ec->comp_data->pending.w = 0;
   ec->comp_data->pending.h = 0;
   ec->comp_data->pending.buffer = NULL;
   ec->comp_data->pending.new_attach = EINA_FALSE;

   /* handle surface opaque region */
   tmp = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(tmp, 1, 1);
   eina_tiler_rect_add(tmp, &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

   src = eina_tiler_intersection(ec->comp_data->pending.opaque, tmp);
   if (src)
     {
        Eina_Rectangle *rect;
        Eina_Iterator *itr;
        int i = 0;

        ec->shape_rects_num = 0;

        itr = eina_tiler_iterator_new(src);
        EINA_ITERATOR_FOREACH(itr, rect)
          {
             ec->shape_rects_num += 1;
          }

        ec->shape_rects = 
          malloc(sizeof(Eina_Rectangle) * ec->shape_rects_num);

        if (ec->shape_rects)
          {
             EINA_ITERATOR_FOREACH(itr, rect)
               {
                  ec->shape_rects[i] = *(Eina_Rectangle *)((char *)rect);

                  ec->shape_rects[i].x = rect->x;
                  ec->shape_rects[i].y = rect->y;
                  ec->shape_rects[i].w = rect->w;
                  ec->shape_rects[i].h = rect->h;

                  i++;
               }
          }

        eina_iterator_free(itr);
        eina_tiler_free(src);
     }

   eina_tiler_free(tmp);
   eina_tiler_clear(ec->comp_data->pending.opaque);

   /* handle surface damages */
   if ((!ec->comp->nocomp) && (ec->frame))
     {
        tmp = eina_tiler_new(ec->w, ec->h);
        eina_tiler_tile_size_set(tmp, 1, 1);
        eina_tiler_rect_add(tmp,
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

        src = eina_tiler_intersection(ec->comp_data->pending.damage, tmp);
        if (src)
          {
             Eina_Rectangle *rect;
             Eina_Iterator *itr;

             itr = eina_tiler_iterator_new(src);
             EINA_ITERATOR_FOREACH(itr, rect)
               {
                  e_comp_object_damage(ec->frame, 
                                       rect->x, rect->y, rect->w, rect->h);
               }
             eina_iterator_free(itr);
             eina_tiler_free(src);
          }

        eina_tiler_free(tmp);

        eina_tiler_clear(ec->comp_data->pending.damage);
     }

   /* handle input regions */
   tmp = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(tmp, 1, 1);
   eina_tiler_rect_add(tmp, &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

   src = eina_tiler_intersection(ec->comp_data->pending.input, tmp);
   if (src)
     {
        Eina_Rectangle *rect;
        Eina_Iterator *itr;
        int i = 0;

        ec->shape_input_rects_num = 0;

        itr = eina_tiler_iterator_new(src);
        EINA_ITERATOR_FOREACH(itr, rect)
          {
             ec->shape_input_rects_num += 1;
          }

        ec->shape_input_rects = 
          malloc(sizeof(Eina_Rectangle) * ec->shape_input_rects_num);

        if (ec->shape_input_rects)
          {
             EINA_ITERATOR_FOREACH(itr, rect)
               {
                  ec->shape_input_rects[i] = 
                    *(Eina_Rectangle *)((char *)rect);

                  ec->shape_input_rects[i].x = rect->x;
                  ec->shape_input_rects[i].y = rect->y;
                  ec->shape_input_rects[i].w = rect->w;
                  ec->shape_input_rects[i].h = rect->h;

                  i++;
               }
          }

        eina_iterator_free(itr);
        eina_tiler_free(src);
     }

   eina_tiler_free(tmp);
   eina_tiler_clear(ec->comp_data->pending.input);

/* #ifndef HAVE_WAYLAND_ONLY */
/*              e_comp_object_input_area_set(ec->frame, rect->x, rect->y,  */
/*                                           rect->w, rect->h); */
/* #endif */
}

static void
_e_comp_wl_subsurface_parent_commit(E_Client *ec, Eina_Bool parent_is_synchronized)
{
   E_Client *parent, *subc;
   E_Comp_Wl_Subsurf *sub_cdata;
   Eina_List *l;

   if (!(parent = _e_comp_wl_subsurface_parent_get(ec))) return;
   if (!(sub_cdata = _e_comp_wl_client_subsurf_data_get(ec))) return;

   if (sub_cdata->position.set)
     {
        evas_object_move(ec->frame,
                         parent->x + sub_cdata->position.x,
                         parent->y + sub_cdata->position.y);
        sub_cdata->position.set = EINA_FALSE;
     }

   if ((parent_is_synchronized) || (sub_cdata->synchronized))
     {
        if (sub_cdata->cached.has_data)
          _e_comp_wl_subsurface_commit_from_cache(ec);

        EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
          {
             if (ec != subc)
               _e_comp_wl_subsurface_parent_commit(subc, EINA_TRUE);
          }
     }
}

static void
_e_comp_wl_subsurface_commit(E_Client *ec)
{
   E_Client *subc;
   E_Comp_Wl_Subsurf *sub_cdata;
   Eina_List *l;
   int w, h;

   sub_cdata = _e_comp_wl_client_subsurf_data_get(ec);
   if (!sub_cdata) return;

   ec->client.w = ec->comp_data->pending.w;
   ec->client.h = ec->comp_data->pending.h;
   evas_object_geometry_get(ec->frame, NULL, NULL, &w, &h);

   if ((ec->client.w != w) || (ec->client.h != h))
     evas_object_resize(ec->frame, ec->client.w, ec->client.h);

   if ((!ec->iconic) &&
       (!evas_object_visible_get(ec->frame)))
     evas_object_show(ec->frame);

   if (_e_comp_wl_subsurface_is_synchronized(sub_cdata))
     _e_comp_wl_subsurface_commit_to_cache(ec);
   else
     {
        if (sub_cdata->cached.has_data)
          {
             _e_comp_wl_subsurface_commit_to_cache(ec);
             _e_comp_wl_subsurface_commit_from_cache(ec);
          }
        else _e_comp_wl_surface_commit(ec);

        EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
          {
             if (ec != subc)
               _e_comp_wl_subsurface_parent_commit(subc, EINA_FALSE);
          }
     }
}

static void 
_e_comp_wl_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Pixmap *cp;
   E_Client *ec, *subc;
   Eina_List *l;

   if (!(cp = wl_resource_get_user_data(resource))) return;

   /* try to find the E client for this surface */
   if (!(ec = e_pixmap_client_get(cp)))
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(cp));

   if ((!ec) || (e_object_is_del(E_OBJECT(ec))))
     return;

   if (_e_comp_wl_client_subsurf_data_get(ec))
     {
        // ec for subsurface.
        _e_comp_wl_subsurface_commit(ec);
        return;
     }

   // ec for main surface.
   _e_comp_wl_surface_commit(ec);

   EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
     {
        if (subc != ec)
          _e_comp_wl_subsurface_parent_commit(subc, EINA_FALSE);
     }
}

static void 
_e_comp_wl_surface_cb_buffer_transform_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t transform EINA_UNUSED)
{
   DBG("Surface Buffer Transform");
}

static void 
_e_comp_wl_surface_cb_buffer_scale_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t scale EINA_UNUSED)
{
   DBG("Surface Buffer Scale");
}

static const struct wl_surface_interface _e_comp_wl_surface_interface = 
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

static void 
_e_comp_wl_comp_cb_surface_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp *comp;
   E_Pixmap *cp;
   struct wl_resource *res;
   uint64_t wid;
   pid_t pid;

   /* DBG("COMP_WL: Create Surface: %d", id); */

   if (!(comp = wl_resource_get_user_data(resource))) return;

   res = 
     e_comp_wl_surface_create(client, wl_resource_get_version(resource), id);
   if (!res)
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   /* get the client pid and generate a pixmap id */
   wl_client_get_credentials(client, &pid, NULL, NULL);
   wid = e_comp_wl_id_get(pid, id);

   /* see if we already have a pixmap for this surface */
   if (!(cp = e_pixmap_find(E_PIXMAP_TYPE_WL, wid)))
     {
        /* try to create a new pixmap for this surface */
        if (!(cp = e_pixmap_new(E_PIXMAP_TYPE_WL, wid)))
          {
             wl_resource_destroy(res);
             wl_resource_post_no_memory(resource);
             return;
          }
     }

   /* set reference to pixmap so we can fetch it later */
   wl_resource_set_user_data(res, cp);
}

static void 
_e_comp_wl_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   Eina_Tiler *tiler;

   if ((tiler = wl_resource_get_user_data(resource)))
     {
        Eina_Tiler *src;

        src = eina_tiler_new(w, h);
        eina_tiler_tile_size_set(src, 1, 1);
        eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});

        eina_tiler_union(tiler, src);
        eina_tiler_free(src);
     }
}

static void 
_e_comp_wl_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   Eina_Tiler *tiler;

   if ((tiler = wl_resource_get_user_data(resource)))
     {
        Eina_Tiler *src;

        src = eina_tiler_new(w, h);
        eina_tiler_tile_size_set(src, 1, 1);
        eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});

        eina_tiler_subtract(tiler, src);
        eina_tiler_free(src);
     }
}

static const struct wl_region_interface _e_region_interface = 
{
   _e_comp_wl_region_cb_destroy,
   _e_comp_wl_region_cb_add,
   _e_comp_wl_region_cb_subtract
};

static void 
_e_comp_wl_comp_cb_region_destroy(struct wl_resource *resource)
{
   Eina_Tiler *tiler;

   if ((tiler = wl_resource_get_user_data(resource)))
     eina_tiler_free(tiler);
}

static void 
_e_comp_wl_comp_cb_region_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp  *c;
   Eina_Tiler *tiler;
   struct wl_resource *res;

   if (!(c = e_comp_get(NULL)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   if (!(tiler = eina_tiler_new(c->man->w, c->man->h)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   eina_tiler_tile_size_set(tiler, 1, 1);
   eina_tiler_rect_add(tiler, 
                       &(Eina_Rectangle){0, 0, c->man->w, c->man->h});

   if (!(res = wl_resource_create(client, &wl_region_interface, 1, id)))
     {
        eina_tiler_free(tiler);
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_resource_set_implementation(res, &_e_region_interface, tiler, 
                                  _e_comp_wl_comp_cb_region_destroy);
}

static void 
_e_comp_wl_subcomp_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);

   E_FREE_LIST(subcomp_handlers, ecore_event_handler_del);
}

static void
_e_comp_wl_subsurface_restack(E_Client *ec)
{
   E_Client *subc, *below = NULL;
   Eina_List *l;

   if (!ec->comp_data->sub.list) return;

   EINA_LIST_REVERSE_FOREACH(ec->comp_data->sub.list, l, subc)
     {
        if (subc->iconic) continue;
        if (below)
          evas_object_stack_below(subc->frame, below->frame);
        else
          evas_object_stack_above(subc->frame, ec->frame);
        below = subc;

        if (subc->comp_data->sub.list)
          _e_comp_wl_subsurface_restack(subc);
     }
}

static Eina_Bool 
_e_comp_wl_client_idler(void *data EINA_UNUSED)
{
   E_Client *ec;
   E_Comp *comp;
   const Eina_List *l;

   EINA_LIST_FREE(_idle_clients, ec)
     {
        if ((e_object_is_del(E_OBJECT(ec))) || (!ec->comp_data)) continue;

//        E_COMP_WL_PIXMAP_CHECK continue;

        if (ec->post_resize)
          {
             if ((ec->comp_data) && (ec->comp_data->shell.configure_send))
               ec->comp_data->shell.configure_send(ec->comp_data->shell.surface, 
                                                      ec->comp->wl_comp_data->resize.edges,
                                                      ec->client.w, ec->client.h);
          }
        ec->post_move = 0;
        ec->post_resize = 0;

        if (ec->comp_data->sub.restack_target)
          {
             if (ec->layer_block) continue;

             // for blocking evas object restack callback.
             restacking = EINA_TRUE;
             if (ec->comp_data->sub.restack_target != ec)
               evas_object_stack_below(ec->frame, ec->comp_data->sub.restack_target->frame);
             _e_comp_wl_subsurface_restack(ec);
             ec->comp_data->sub.restack_target = NULL;
             restacking = EINA_FALSE;
          }
     }

   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     {
        if ((comp->wl_comp_data->restack) && (!comp->new_clients))
          {
             e_hints_client_stacking_set();
             comp->wl_comp_data->restack = EINA_FALSE;
          }
     }

   _client_idler = NULL;
   return EINA_FALSE;
}

static void 
_e_comp_wl_client_idler_add(E_Client *ec)
{
   if (!_client_idler) 
     _client_idler = ecore_idle_enterer_add(_e_comp_wl_client_idler, NULL);

   if (!ec) CRI("ACK!");

   if (!eina_list_data_find(_idle_clients, ec))
     _idle_clients = eina_list_append(_idle_clients, ec);
}

static void
_e_comp_wl_evas_cb_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec;
   E_Client *subc;
   Eina_List *l;
   int x, y;

   if (!(ec = data)) return;

   E_COMP_WL_PIXMAP_CHECK;

   EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
     {
        x = ec->x + subc->comp_data->sub.cdata->position.x;
        y = ec->y + subc->comp_data->sub.cdata->position.y;
        evas_object_move(subc->frame, x, y);
     }

   ec->post_move = EINA_FALSE;
   _e_comp_wl_client_idler_add(ec);
}

static Eina_Bool
_e_comp_wl_cb_client_iconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec, *subc;
   Eina_List *l;

   ev = event;
   if (!(ec = ev->ec)) return ECORE_CALLBACK_PASS_ON;

   E_COMP_WL_PIXMAP_CHECK ECORE_CALLBACK_PASS_ON;

   if (_e_comp_wl_client_subsurf_data_get(ec))
     {
        ec = _e_comp_wl_subsurface_root_get(ec);
        if (!ec->iconic) e_client_iconify(ec);
     }

   EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
     {
        if (subc->iconic) continue;
        e_client_iconify(subc);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_wl_cb_client_uniconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec, *subc;
   Eina_List *l;

   ev = event;
   if (!(ec = ev->ec)) return ECORE_CALLBACK_PASS_ON;

   E_COMP_WL_PIXMAP_CHECK ECORE_CALLBACK_PASS_ON;

   if (_e_comp_wl_client_subsurf_data_get(ec))
     {
        ec = _e_comp_wl_subsurface_root_get(ec);
        if (ec->iconic) e_client_uniconify(ec);
     }

   EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
     {
        if (!subc->iconic) continue;
        e_client_uniconify(subc);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static const struct wl_compositor_interface _e_comp_interface = 
{
   _e_comp_wl_comp_cb_surface_create,
   _e_comp_wl_comp_cb_region_create
};

static void 
_e_comp_wl_cb_bind_compositor(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;

   if (!(comp = data)) return;

   res = 
     wl_resource_create(client, &wl_compositor_interface, MIN(version, 3), id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_comp_interface, comp, NULL);
}

static void 
_e_comp_wl_cb_del(E_Comp *comp)
{
   E_Comp_Data *cdata;

   cdata = comp->wl_comp_data;

   e_comp_wl_data_manager_shutdown(cdata);
   e_comp_wl_input_shutdown(cdata);

   /* delete idler to flush clients */
   if (cdata->idler) ecore_idler_del(cdata->idler);

   /* delete fd handler to listen for wayland main loop events */
   if (cdata->fd_hdlr) ecore_main_fd_handler_del(cdata->fd_hdlr);

   /* delete the wayland display */
   if (cdata->wl.disp) wl_display_destroy(cdata->wl.disp);

   free(cdata);
}

static Eina_Bool 
_e_comp_wl_cb_read(void *data, Ecore_Fd_Handler *hdl EINA_UNUSED)
{
   E_Comp_Data *cdata;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;
   if (!cdata->wl.disp) return ECORE_CALLBACK_RENEW;

   /* flush any pending client events */
   wl_display_flush_clients(cdata->wl.disp);

   /* dispatch any pending main loop events */
   wl_event_loop_dispatch(cdata->wl.loop, 0);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_comp_wl_cb_idle(void *data)
{
   E_Comp_Data *cdata;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;
   if (!cdata->wl.disp) return ECORE_CALLBACK_RENEW;

   /* flush any pending client events */
   wl_display_flush_clients(cdata->wl.disp);

   /* dispatch any pending main loop events */
   wl_event_loop_dispatch(cdata->wl.loop, 0);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_comp_wl_cb_module_idle(void *data)
{
   E_Module *mod = NULL;
   E_Comp_Data *cdata;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;

   if (e_module_loading_get()) return ECORE_CALLBACK_RENEW;

   /* FIXME: make which shell to load configurable */
   if (!(mod = e_module_find("wl_desktop_shell")))
     mod = e_module_new("wl_desktop_shell");

   if (mod)
     {
        e_module_enable(mod);

        /* dispatch any pending main loop events */
        wl_event_loop_dispatch(cdata->wl.loop, 0);

        return ECORE_CALLBACK_CANCEL;
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_comp_wl_cb_first_draw(void *data)
{
   E_Client *ec;

   if (!(ec = data)) return EINA_TRUE;
   ec->comp_data->first_draw_tmr = NULL;
   e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
   return EINA_FALSE;
}

static void
_e_comp_wl_evas_cb_restack(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec;
   E_Client *parent;

   if (restacking) return;
   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if ((!ec->comp_data->sub.list) &&
       (!_e_comp_wl_client_subsurf_data_get(ec))) return;

   parent = ec;
   if (_e_comp_wl_client_subsurf_data_get(ec))
     parent = _e_comp_wl_subsurface_root_get(ec);

   parent->comp_data->sub.restack_target = ec;
   _e_comp_wl_client_idler_add(parent);
}

static void
_e_comp_wl_subsurface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_comp_wl_subsurface_cb_position_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y)
{
   E_Client *ec;
   E_Comp_Wl_Subsurf *sub_cdata;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (!(sub_cdata = _e_comp_wl_client_subsurf_data_get(ec))) return;

   sub_cdata->position.x = x;
   sub_cdata->position.y = y;
   sub_cdata->position.set = EINA_TRUE;
}

static void
_e_comp_wl_subsurface_cb_place_above(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *sibling_resource)
{
   E_Client *parent;
   E_Client *ec;
   E_Client *sibling;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (!(sibling = wl_resource_get_user_data(sibling_resource))) return;
   if ((!_e_comp_wl_client_subsurf_data_get(ec)) ||
       (!_e_comp_wl_client_subsurf_data_get(sibling))) return;
   if (_e_comp_wl_subsurface_parent_get(ec) !=
       _e_comp_wl_subsurface_parent_get(sibling)) return;
   if (!(parent = _e_comp_wl_subsurface_parent_get(ec))) return;

   parent->comp_data->sub.list =
      eina_list_remove(parent->comp_data->sub.list, ec);
   parent->comp_data->sub.list =
      eina_list_append_relative(parent->comp_data->sub.list, ec, sibling);
   parent->comp_data->sub.restack_target = parent;
   _e_comp_wl_client_idler_add(parent);
}

static void
_e_comp_wl_subsurface_cb_place_below(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *sibling_resource)
{
   E_Client *parent;
   E_Client *ec;
   E_Client *sibling;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (!(sibling = wl_resource_get_user_data(sibling_resource))) return;
   if ((!_e_comp_wl_client_subsurf_data_get(ec)) ||
       (!_e_comp_wl_client_subsurf_data_get(sibling))) return;
   if (_e_comp_wl_subsurface_parent_get(ec) !=
       _e_comp_wl_subsurface_parent_get(sibling)) return;
   if (!(parent = _e_comp_wl_subsurface_parent_get(ec))) return;

   parent->comp_data->sub.list =
      eina_list_remove(parent->comp_data->sub.list, ec);
   parent->comp_data->sub.list =
      eina_list_prepend_relative(parent->comp_data->sub.list, ec, sibling);
   parent->comp_data->sub.restack_target = parent;
   _e_comp_wl_client_idler_add(parent);
}

static void
_e_comp_wl_subsurface_cb_sync_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;
   E_Comp_Wl_Subsurf *sub_cdata;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (!(sub_cdata = _e_comp_wl_client_subsurf_data_get(ec))) return;

   sub_cdata->synchronized = EINA_TRUE;
}

static void
_e_comp_wl_subsurface_cb_desync_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;
   E_Comp_Wl_Subsurf *sub_cdata;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (!(sub_cdata = _e_comp_wl_client_subsurf_data_get(ec))) return;

   sub_cdata->synchronized = EINA_FALSE;
}

static const struct wl_subsurface_interface _e_comp_wl_subsurface_interface =
{
   _e_comp_wl_subsurface_cb_destroy,
   _e_comp_wl_subsurface_cb_position_set,
   _e_comp_wl_subsurface_cb_place_above,
   _e_comp_wl_subsurface_cb_place_below,
   _e_comp_wl_subsurface_cb_sync_set,
   _e_comp_wl_subsurface_cb_desync_set
};

static Eina_Bool
_e_comp_wl_subsurface_create(E_Client *ec, E_Client *pc, uint32_t id, struct wl_resource *surface_resource)
{
   E_Comp_Wl_Subsurf *sub_cdata;
   struct wl_client *client;

   if (!(client = wl_resource_get_client(surface_resource))) return EINA_FALSE;

   sub_cdata = E_NEW(E_Comp_Wl_Subsurf, 1);
   if (!sub_cdata) return EINA_FALSE;

   sub_cdata->resource = wl_resource_create(client, &wl_subsurface_interface, 1, id);
   if (!sub_cdata->resource)
     {
        free(sub_cdata);
        return EINA_FALSE;
     }
   wl_resource_set_implementation(sub_cdata->resource,
                                  &_e_comp_wl_subsurface_interface, ec,
                                  _e_comp_wl_subsurface_destroy);

   sub_cdata->synchronized = EINA_TRUE;
   sub_cdata->parent = pc;

   sub_cdata->cached.damage = eina_tiler_new(1, 1);
   eina_tiler_tile_size_set(sub_cdata->cached.damage, 1, 1);

   sub_cdata->cached.input = eina_tiler_new(1, 1);
   eina_tiler_tile_size_set(sub_cdata->cached.damage, 1, 1);

   sub_cdata->cached.opaque = eina_tiler_new(1, 1);
   eina_tiler_tile_size_set(sub_cdata->cached.damage, 1, 1);

   ec->borderless = EINA_TRUE;
   ec->argb = EINA_TRUE;
   ec->lock_border = EINA_TRUE;
   ec->lock_focus_in = ec->lock_focus_out = EINA_TRUE;
   ec->netwm.state.skip_taskbar = EINA_TRUE;
   ec->netwm.state.skip_pager = EINA_TRUE;
   ec->comp_data->surface = surface_resource;
   ec->comp_data->sub.cdata = sub_cdata;

   evas_object_pass_events_set(ec->frame, EINA_TRUE);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_RESTACK,
                                  _e_comp_wl_evas_cb_restack, ec);

   if (pc)
     {
        E_Comp_Client_Data *cdata;

        cdata = pc->comp_data;
        if (cdata)
          cdata->sub.list = eina_list_append(cdata->sub.list, ec);

        evas_object_event_callback_add(pc->frame, EVAS_CALLBACK_MOVE,
                                       _e_comp_wl_evas_cb_move, pc);
        evas_object_event_callback_add(pc->frame, EVAS_CALLBACK_RESTACK,
                                       _e_comp_wl_evas_cb_restack, pc);
     }

   return EINA_TRUE;
}

static void 
_e_comp_wl_subcomp_cb_subsurface_get(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource, struct wl_resource *parent_resource)
{
   E_Pixmap *pp, *ep;
   E_Client *ec, *pc;
   static const char where[] = "get_subsurface: wl_subsurface@";

   if (!(pp = wl_resource_get_user_data(parent_resource)))
     {
        wl_resource_post_error(resource,
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "%s%d: wl_surface@%d is invalid.",
                               where, id, wl_resource_get_id(parent_resource));
        return;
     }

   if (!(ep = wl_resource_get_user_data(surface_resource)))
     {
        wl_resource_post_error(resource,
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "%s%d: wl_surface@%d is invalid.",
                               where, id, wl_resource_get_id(surface_resource));
        return;
     }

   if (pp == ep)
     {
        wl_resource_post_error(resource,
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "%s%d: wl_surface@%d cannot be its own parent",
                               where, id, wl_resource_get_id(surface_resource));
        return;
     }

   if (!(ec = e_pixmap_client_get(ep)))
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(ep));

   if (!ec)
     {
        if (!(ec = e_client_new(e_util_comp_current_get(), ep, 1, 0)))
          {
             wl_resource_post_no_memory(resource);
             return;
          }
     }
   else if (_e_comp_wl_client_subsurf_data_get(ec))
     {
        wl_resource_post_error(resource,
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "%s%d: wl_surface@%d is already a sub-surface",
                               where, id, wl_resource_get_id(surface_resource));
        return;
     }

   if (!ec->comp_data)
     {
        e_object_del(E_OBJECT(ec));
        return;
     }

   pc = e_pixmap_client_get(pp);
   if (!_e_comp_wl_subsurface_create(ec, pc, id, surface_resource))
     {
        e_object_del(E_OBJECT(ec));
        return;
     }
}

static const struct wl_subcompositor_interface _e_subcomp_interface = 
{
   _e_comp_wl_subcomp_cb_destroy,
   _e_comp_wl_subcomp_cb_subsurface_get
};

static void 
_e_comp_wl_cb_bind_subcompositor(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;

   if (!(comp = data)) return;

   res = 
     wl_resource_create(client, &wl_subcompositor_interface, 
                        MIN(version, 1), id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_subcomp_interface, comp, NULL);

   E_LIST_HANDLER_APPEND(subcomp_handlers, E_EVENT_CLIENT_ICONIFY,
                         _e_comp_wl_cb_client_iconify, NULL);
   E_LIST_HANDLER_APPEND(subcomp_handlers, E_EVENT_CLIENT_UNICONIFY,
                         _e_comp_wl_cb_client_uniconify, NULL);
}

static Eina_Bool 
_e_comp_wl_compositor_create(void)
{
   E_Comp *comp;
   E_Comp_Data *cdata;
   char buff[PATH_MAX];
   /* char *rules, *model, *layout; */
   int fd = 0;

   /* get the current compositor */
   if (!(comp = e_comp_get(NULL)))
     {
        comp = e_comp_new();
        comp->comp_type = E_PIXMAP_TYPE_WL;
        E_OBJECT_DEL_SET(comp, _e_comp_wl_cb_del);
     }

   /* check compositor type and make sure it's Wayland */
   /* if (comp->comp_type != E_PIXMAP_TYPE_WL) return EINA_FALSE; */

   cdata = E_NEW(E_Comp_Data, 1);
   comp->wl_comp_data = cdata;

   /* setup wayland display environment variable */
   snprintf(buff, sizeof(buff), "%s/wayland-0", e_ipc_socket);
   e_env_set("WAYLAND_DISPLAY", buff);

   /* try to create wayland display */
   if (!(cdata->wl.disp = wl_display_create()))
     {
        ERR("Could not create a Wayland Display: %m");
        goto disp_err;
     }

   if (wl_display_add_socket(cdata->wl.disp, buff))
     {
        ERR("Could not create a Wayland Display: %m");
        goto disp_err;
     }

   /* setup wayland compositor signals */
   /* NB: So far, we don't need ANY of these... */
   /* wl_signal_init(&cdata->signals.destroy); */
   /* wl_signal_init(&cdata->signals.activate); */
   /* wl_signal_init(&cdata->signals.transform); */
   /* wl_signal_init(&cdata->signals.kill); */
   /* wl_signal_init(&cdata->signals.idle); */
   /* wl_signal_init(&cdata->signals.wake); */
   /* wl_signal_init(&cdata->signals.session); */
   /* wl_signal_init(&cdata->signals.seat.created); */
   /* wl_signal_init(&cdata->signals.seat.destroyed); */
   /* wl_signal_init(&cdata->signals.seat.moved); */
   /* wl_signal_init(&cdata->signals.output.created); */
   /* wl_signal_init(&cdata->signals.output.destroyed); */
   /* wl_signal_init(&cdata->signals.output.moved); */

   /* try to add compositor to wayland display globals */
   if (!wl_global_create(cdata->wl.disp, &wl_compositor_interface, 3, 
                         comp, _e_comp_wl_cb_bind_compositor))
     {
        ERR("Could not add compositor to globals: %m");
        goto disp_err;
     }

   /* try to add subcompositor to wayland display globals */
   if (!wl_global_create(cdata->wl.disp, &wl_subcompositor_interface, 1, 
                         comp, _e_comp_wl_cb_bind_subcompositor))
     {
        ERR("Could not add compositor to globals: %m");
        goto disp_err;
     }

   /* try to init data manager */
   if (!e_comp_wl_data_manager_init(cdata))
     {
        ERR("Could not initialize data manager");
        goto disp_err;
     }

   /* try to init input (keyboard & pointer) */
   if (!e_comp_wl_input_init(cdata))
     {
        ERR("Could not initialize input");
        goto disp_err;
     }

/* #ifndef HAVE_WAYLAND_ONLY */
/*    if (getenv("DISPLAY")) */
/*      { */
/*         E_Config_XKB_Layout *ekbd; */
/*         Ecore_X_Atom xkb = 0; */
/*         Ecore_X_Window root = 0; */
/*         int len = 0; */
/*         unsigned char *dat; */

/*         if ((ekbd = e_xkb_layout_get())) */
/*           { */
/*              model = strdup(ekbd->model); */
/*              layout = strdup(ekbd->name); */
/*           } */

/*         root = ecore_x_window_root_first_get(); */
/*         xkb = ecore_x_atom_get("_XKB_RULES_NAMES"); */
/*         ecore_x_window_prop_property_get(root, xkb, ECORE_X_ATOM_STRING,  */
/*                                          1024, &dat, &len); */
/*         if ((dat) && (len > 0)) */
/*           { */
/*              rules = (char *)dat; */
/*              dat += strlen((const char *)dat) + 1; */
/*              if (!model) model = strdup((const char *)dat); */
/*              dat += strlen((const char *)dat) + 1; */
/*              if (!layout) layout = strdup((const char *)dat); */
/*           } */
/*      } */
/* #endif */

   /* fallback */
   /* if (!rules) rules = strdup("evdev"); */
   /* if (!model) model = strdup("pc105"); */
   /* if (!layout) layout = strdup("us"); */

   /* update compositor keymap */
   /* e_comp_wl_input_keymap_set(cdata, rules, model, layout); */

   /* TODO: init text backend */

   /* initialize shm mechanism */
   wl_display_init_shm(cdata->wl.disp);

   /* check for gl rendering */
   if ((e_comp_gl_get()) && 
       (e_comp_config_get()->engine == E_COMP_ENGINE_GL))
     {
        /* TODO: setup gl ? */
     }

   /* get the wayland display's event loop */
   cdata->wl.loop= wl_display_get_event_loop(cdata->wl.disp);

   /* get the file descriptor of the main loop */
   fd = wl_event_loop_get_fd(cdata->wl.loop);

   /* add an fd handler to listen for wayland main loop events */
   cdata->fd_hdlr = 
     ecore_main_fd_handler_add(fd, ECORE_FD_READ | ECORE_FD_WRITE, 
                               _e_comp_wl_cb_read, cdata, NULL, NULL);

   /* add an idler to flush clients */
   cdata->idler = ecore_idle_enterer_add(_e_comp_wl_cb_idle, cdata);

   /* setup module idler to load shell module */
   ecore_idler_add(_e_comp_wl_cb_module_idle, cdata);

   if (comp->comp_type == E_PIXMAP_TYPE_X)
     {
        e_comp_wl_input_pointer_enabled_set(comp->wl_comp_data, EINA_TRUE);
        e_comp_wl_input_keyboard_enabled_set(comp->wl_comp_data, EINA_TRUE);
     }

   return EINA_TRUE;

disp_err:
   e_env_unset("WAYLAND_DISPLAY");
   return EINA_FALSE;
}

static void 
_e_comp_wl_client_priority_adjust(int pid, int set, int adj, Eina_Bool use_adj, Eina_Bool adj_child, Eina_Bool do_child)
{
   int n;

   n = set;
   if (use_adj) n = (getpriority(PRIO_PROCESS, pid) + adj);
   setpriority(PRIO_PROCESS, pid, n);

   if (do_child)
     {
        Eina_List *files;
        char *file, buff[PATH_MAX];
        FILE *f;
        int pid2, ppid;

        files = ecore_file_ls("/proc");
        EINA_LIST_FREE(files, file)
          {
             if (isdigit(file[0]))
               {
                  snprintf(buff, sizeof(buff), "/proc/%s/stat", file);
                  if ((f = fopen(buff, "r")))
                    {
                       pid2 = -1;
                       ppid = -1;
                       if (fscanf(f, "%i %*s %*s %i %*s", &pid2, &ppid) == 2)
                         {
                            fclose(f);
                            if (ppid == pid)
                              {
                                 if (adj_child)
                                   _e_comp_wl_client_priority_adjust(pid2, set, 
                                                                     adj, EINA_TRUE,
                                                                     adj_child, do_child);
                                 else
                                   _e_comp_wl_client_priority_adjust(pid2, set, 
                                                                     adj, use_adj,
                                                                     adj_child, do_child);
                              }
                         }
                       else 
                         fclose(f);
                    }
               }
             free(file);
          }
     }
}

static void 
_e_comp_wl_client_priority_raise(E_Client *ec)
{
   if (ec->netwm.pid <= 0) return;
   if (ec->netwm.pid == getpid()) return;
   _e_comp_wl_client_priority_adjust(ec->netwm.pid, 
                                     e_config->priority - 1, -1, 
                                     EINA_FALSE, EINA_TRUE, EINA_FALSE);
}

static void 
_e_comp_wl_client_priority_normal(E_Client *ec)
{
   if (ec->netwm.pid <= 0) return;
   if (ec->netwm.pid == getpid()) return;
   _e_comp_wl_client_priority_adjust(ec->netwm.pid, e_config->priority, 1, 
                                     EINA_FALSE, EINA_TRUE, EINA_FALSE);
}

static void 
_e_comp_wl_evas_cb_show(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec, *tmp;
   Eina_List *l;

   if (!(ec = data)) return;

   E_COMP_WL_PIXMAP_CHECK;

   if (!ec->override) 
     e_hints_window_visible_set(ec);

   if (ec->comp_data->frame_update)
     ec->comp_data->frame_update = EINA_FALSE;

   EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
     evas_object_show(tmp->frame);
}

static void 
_e_comp_wl_evas_cb_hide(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec, *tmp;
   Eina_List *l;

   if (!(ec = data)) return;

   EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
     evas_object_hide(tmp->frame);
}

static void 
_e_comp_wl_evas_cb_mouse_in(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_In *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial;

   ev = event;
   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (e_object_is_del(E_OBJECT(ec))) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_enter(res, serial, ec->comp_data->surface, 
                              wl_fixed_from_int(ev->canvas.x), 
                              wl_fixed_from_int(ev->canvas.y));
     }
}

static void 
_e_comp_wl_evas_cb_mouse_out(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial;

   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_leave(res, serial, ec->comp_data->surface);
     }
}

static void 
_e_comp_wl_evas_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_Down *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial, btn;

   ev = event;
   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_client_util_ignored_get(ec)) return;

   switch (ev->button)
     {
      case 1:
        btn = BTN_LEFT;
        break;
      case 2:
        btn = BTN_MIDDLE;
        break;
      case 3:
        btn = BTN_RIGHT;
        break;
      default:
        btn = ev->button;
        break;
     }

   ec->comp->wl_comp_data->ptr.button = btn;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_button(res, serial, ev->timestamp, btn, 
                               WL_POINTER_BUTTON_STATE_PRESSED);
     }
}

static void 
_e_comp_wl_evas_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_Up *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial, btn;

   ev = event;
   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_client_util_ignored_get(ec)) return;

   switch (ev->button)
     {
      case 1:
        btn = BTN_LEFT;
        break;
      case 2:
        btn = BTN_MIDDLE;
        break;
      case 3:
        btn = BTN_RIGHT;
        break;
      default:
        btn = ev->button;
        break;
     }

   ec->comp->wl_comp_data->resize.resource = NULL;
   ec->comp->wl_comp_data->ptr.button = btn;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_button(res, serial, ev->timestamp, btn, 
                               WL_POINTER_BUTTON_STATE_RELEASED);
     }
}

static void 
_e_comp_wl_evas_cb_mouse_wheel(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_Wheel *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t axis, dir;

   ev = event;
   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_client_util_ignored_get(ec)) return;

   if (ev->direction == 0)
     axis = WL_POINTER_AXIS_VERTICAL_SCROLL;
   else
     axis = WL_POINTER_AXIS_HORIZONTAL_SCROLL;

   if (ev->z < 0)
     dir = -wl_fixed_from_int(abs(ev->z));
   else
     dir = wl_fixed_from_int(ev->z);

   wc = wl_resource_get_client(ec->comp_data->surface);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_axis(res, ev->timestamp, axis, dir);
     }
}

static void 
_e_comp_wl_evas_cb_mouse_move(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_Move *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;

   ev = event;
   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_client_util_ignored_get(ec)) return;

   ec->comp->wl_comp_data->ptr.x = 
     wl_fixed_from_int(ev->cur.canvas.x - ec->client.x);
   ec->comp->wl_comp_data->ptr.y = 
     wl_fixed_from_int(ev->cur.canvas.y - ec->client.y);

   wc = wl_resource_get_client(ec->comp_data->surface);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_motion(res, ev->timestamp, 
                               ec->comp->wl_comp_data->ptr.x, 
                               ec->comp->wl_comp_data->ptr.y);
     }
}

static void 
_e_comp_wl_evas_cb_key_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   E_Comp_Data *cdata;
   Evas_Event_Key_Down *ev;
   uint32_t serial, *end, *k, keycode;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;

   ev = event;
   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (!ec->focused) return;

   keycode = (ev->keycode - 8);
   if (!(cdata = ec->comp->wl_comp_data)) return;

   end = (uint32_t *)cdata->kbd.keys.data + (cdata->kbd.keys.size / sizeof(*k));

   for (k = cdata->kbd.keys.data; k < end; k++)
     {
        /* ignore server-generated key repeats */
        if (*k == keycode) return;
     }

   cdata->kbd.keys.size = (const char *)end - (const char *)cdata->kbd.keys.data;
   k = wl_array_add(&cdata->kbd.keys, sizeof(*k));
   *k = keycode;

   /* update modifier state */
   e_comp_wl_input_keyboard_state_update(cdata, keycode, EINA_TRUE);

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->kbd.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        wl_keyboard_send_key(res, serial, ev->timestamp, 
                             keycode, WL_KEYBOARD_KEY_STATE_PRESSED);
     }
}

static void 
_e_comp_wl_evas_cb_key_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   E_Comp_Data *cdata;
   Evas_Event_Key_Up *ev;
   uint32_t serial, *end, *k, keycode;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;

   ev = event;
   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (!ec->focused) return;

   keycode = (ev->keycode - 8);
   if (!(cdata = ec->comp->wl_comp_data)) return;

   end = (uint32_t *)cdata->kbd.keys.data + (cdata->kbd.keys.size / sizeof(*k));
   for (k = cdata->kbd.keys.data; k < end; k++)
     if (*k == keycode) *k = *--end;

   cdata->kbd.keys.size = (const char *)end - (const char *)cdata->kbd.keys.data;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(cdata->wl.disp);
   EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        wl_keyboard_send_key(res, serial, ev->timestamp, 
                             keycode, WL_KEYBOARD_KEY_STATE_RELEASED);
     }

   /* update modifier state */
   e_comp_wl_input_keyboard_state_update(cdata, keycode, EINA_FALSE);
}

static void 
_e_comp_wl_evas_cb_focus_in(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec, *focused;
   E_Comp_Data *cdata;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial, *k;

   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (e_object_is_del(E_OBJECT(ec))) return;

   /* block refocus attempts on iconic clients
    * these result from iconifying a client during a grab */
   if (ec->iconic) return;

   /* block spurious focus events
    * not sure if correct, but seems necessary to use pointer focus... */
   focused = e_client_focused_get();
   if (focused && (ec != focused)) return;

   cdata = ec->comp->wl_comp_data;

   /* priority raise */
   _e_comp_wl_client_priority_raise(ec);

   /* update modifier state */
   wl_array_for_each(k, &cdata->kbd.keys)
     e_comp_wl_input_keyboard_state_update(cdata, *k, EINA_TRUE);

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(cdata->wl.disp);
   EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        wl_keyboard_send_enter(res, serial, ec->comp_data->surface, 
                               &cdata->kbd.keys);
     }
}

static void 
_e_comp_wl_evas_cb_focus_out(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial, *k;

   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (e_object_is_del(E_OBJECT(ec))) return;

   /* priority normal */
   _e_comp_wl_client_priority_normal(ec);

   /* update modifier state */
   wl_array_for_each(k, &ec->comp->wl_comp_data->kbd.keys)
     e_comp_wl_input_keyboard_state_update(ec->comp->wl_comp_data, *k, EINA_FALSE);

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->kbd.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        wl_keyboard_send_leave(res, serial, ec->comp_data->surface);
     }
}

static void 
_e_comp_wl_evas_cb_resize(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if ((ec->shading) || (ec->shaded)) return;
   if (!e_pixmap_size_changed(ec->pixmap, ec->client.w, ec->client.h))
     return;

   /* DBG("COMP_WL: Evas Resize: %d %d", ec->client.w, ec->client.h); */

   ec->post_resize = EINA_TRUE;
   e_pixmap_dirty(ec->pixmap);
   e_comp_object_render_update_del(ec->frame);
   _e_comp_wl_client_idler_add(ec);
}

static void 
_e_comp_wl_evas_cb_frame_recalc(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;

   E_COMP_WL_PIXMAP_CHECK;
   if (evas_object_visible_get(obj))
     ec->comp_data->frame_update = EINA_FALSE;
   else
     ec->comp_data->frame_update = EINA_TRUE;
   ec->post_move = ec->post_resize = EINA_TRUE;
   _e_comp_wl_client_idler_add(ec);
}

/* static void  */
/* _e_comp_wl_evas_cb_comp_hidden(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED) */
/* { */
/*    E_Client *ec; */

/*    if (!(ec = data)) return; */
/* #warning FIXME Implement Evas Comp Hidden */
/* } */

static void 
_e_comp_wl_evas_cb_delete_request(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;
   E_Comp *comp;

   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   if (ec->netwm.ping) e_client_ping(ec);

   /* FIXME !!!
    * 
    * This is a HUGE problem for internal windows...
    * 
    * IF we delete the client here, then we cannot reopen some internal 
    * dialogs (configure, etc, etc) ...
    * 
    * BUT, if we don't handle delete_request Somehow, then the close button on 
    * the frame does Nothing
    * 
    */

   comp = ec->comp;

   e_comp_ignore_win_del(E_PIXMAP_TYPE_WL, e_pixmap_window_get(ec->pixmap));
   if (ec->comp_data)
     {
        if (ec->comp_data->reparented)
          e_client_comp_hidden_set(ec, EINA_TRUE);
     }

   evas_object_pass_events_set(ec->frame, EINA_TRUE);
   if (ec->visible) evas_object_hide(ec->frame);
   if (!ec->internal) e_object_del(E_OBJECT(ec));

   _e_comp_wl_focus_check(comp);

   /* TODO: Delete request send ??
    * NB: No such animal wrt wayland */
}

static void 
_e_comp_wl_evas_cb_kill_request(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;
   E_Comp *comp;

   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   /* if (ec->netwm.ping) e_client_ping(ec); */

   comp = ec->comp;

   e_comp_ignore_win_del(E_PIXMAP_TYPE_WL, e_pixmap_window_get(ec->pixmap));
   if (ec->comp_data)
     {
        if (ec->comp_data->reparented)
          e_client_comp_hidden_set(ec, EINA_TRUE);
     }

   evas_object_pass_events_set(ec->frame, EINA_TRUE);
   if (ec->visible) evas_object_hide(ec->frame);
   if (!ec->internal) e_object_del(E_OBJECT(ec));

   _e_comp_wl_focus_check(comp);
}

static void 
_e_comp_wl_evas_cb_ping(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;

   E_COMP_WL_PIXMAP_CHECK;
   if (ec->comp_data->shell.ping)
     {
        if (ec->comp_data->shell.surface)
          ec->comp_data->shell.ping(ec->comp_data->shell.surface);
     }
}

static void 
_e_comp_wl_evas_cb_color_set(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Client *ec;
   int a = 0;

   if (!(ec = data)) return;
   E_COMP_WL_PIXMAP_CHECK;
   evas_object_color_get(obj, NULL, NULL, NULL, &a);
   if (ec->netwm.opacity == a) return;
   ec->netwm.opacity = a;
   ec->netwm.opacity_changed = EINA_TRUE;
   _e_comp_wl_client_idler_add(ec);
}

static void 
_e_comp_wl_client_evas_init(E_Client *ec)
{
   if (ec->comp_data->evas_init) return;
   ec->comp_data->evas_init = EINA_TRUE;

   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW, 
                                  _e_comp_wl_evas_cb_show, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_HIDE, 
                                  _e_comp_wl_evas_cb_hide, ec);

   /* we need to hook evas mouse events for wayland clients */
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_IN, 
                                  _e_comp_wl_evas_cb_mouse_in, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_OUT, 
                                  _e_comp_wl_evas_cb_mouse_out, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_DOWN, 
                                  _e_comp_wl_evas_cb_mouse_down, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_UP, 
                                  _e_comp_wl_evas_cb_mouse_up, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_WHEEL, 
                                  _e_comp_wl_evas_cb_mouse_wheel, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_MOVE, 
                                  _e_comp_wl_evas_cb_mouse_move, ec);

   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_KEY_DOWN, 
                                  _e_comp_wl_evas_cb_key_down, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_KEY_UP, 
                                  _e_comp_wl_evas_cb_key_up, ec);

   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_FOCUS_IN, 
                                  _e_comp_wl_evas_cb_focus_in, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_FOCUS_OUT, 
                                  _e_comp_wl_evas_cb_focus_out, ec);

   if (!ec->override)
     {
        evas_object_smart_callback_add(ec->frame, "client_resize", 
                                       _e_comp_wl_evas_cb_resize, ec);
     }

   evas_object_smart_callback_add(ec->frame, "frame_recalc_done", 
                                  _e_comp_wl_evas_cb_frame_recalc, ec);
   evas_object_smart_callback_add(ec->frame, "delete_request", 
                                  _e_comp_wl_evas_cb_delete_request, ec);
   evas_object_smart_callback_add(ec->frame, "kill_request", 
                                  _e_comp_wl_evas_cb_kill_request, ec);
   evas_object_smart_callback_add(ec->frame, "ping", 
                                  _e_comp_wl_evas_cb_ping, ec);
   evas_object_smart_callback_add(ec->frame, "color_set", 
                                  _e_comp_wl_evas_cb_color_set, ec);

   /* TODO: these will need to send_configure */
   /* evas_object_smart_callback_add(ec->frame, "fullscreen_zoom",  */
   /*                                _e_comp_wl_evas_cb_resize, ec); */
   /* evas_object_smart_callback_add(ec->frame, "unfullscreen_zoom",  */
   /*                                _e_comp_wl_evas_cb_resize, ec); */
}

static Eina_Bool 
_e_comp_wl_client_new_helper(E_Client *ec)
{
   /* FIXME: No Way to get "initial attributes" of a wayland window */
   ec->border_size = 0;

   ec->placed |= ec->override;
   ec->icccm.accepts_focus = ((!ec->override) && (!ec->input_only));

   if ((ec->override) && ((ec->x == -77) && (ec->y == -777)))
     {
        e_comp_ignore_win_add(E_PIXMAP_TYPE_WL, e_client_util_win_get(ec));
        e_object_del(E_OBJECT(ec));
        return EINA_FALSE;
     }

   if ((!e_client_util_ignored_get(ec)) && 
       (!ec->internal) && (!ec->internal_ecore_evas))
     {
        ec->comp_data->need_reparent = EINA_TRUE;
        EC_CHANGED(ec);
        ec->take_focus = !starting;
     }

   ec->new_client ^= ec->override;

   if (e_pixmap_size_changed(ec->pixmap, ec->client.w, ec->client.h))
     {
        ec->changes.size = EINA_TRUE;
        EC_CHANGED(ec);
     }

   return EINA_TRUE;
}

static Eina_Bool 
_e_comp_wl_client_shape_check(E_Client *ec)
{
   /* FIXME: need way to determine if shape has changed */
   ec->shape_changed = EINA_TRUE;
   e_comp_shape_queue(ec->comp);
   return EINA_TRUE;
}

static Eina_Bool 
_e_comp_wl_cb_comp_object_add(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Comp_Object *ev)
{
   E_Client *ec;

   ec = e_comp_object_client_get(ev->comp_object);

   /* NB: Don't check re_manage here as we need evas events for mouse */
   if ((!ec) || (e_object_is_del(E_OBJECT(ec))))
     return ECORE_CALLBACK_RENEW;

   E_COMP_WL_PIXMAP_CHECK ECORE_CALLBACK_RENEW;

   _e_comp_wl_client_evas_init(ec);

   return ECORE_CALLBACK_RENEW;
}

/* static Eina_Bool  */
/* _e_comp_wl_cb_client_zone_set(void *data EINA_UNUSED, int type EINA_UNUSED, void *event) */
/* { */
/*    E_Event_Client *ev; */
/*    E_Client *ec; */

/*    DBG("CLIENT ZONE SET !!!"); */

/*    ev = event; */
/*    if (!(ec = ev->ec)) return ECORE_CALLBACK_RENEW; */
/*    if (e_object_is_del(E_OBJECT(ec))) return ECORE_CALLBACK_RENEW; */
/*    E_COMP_WL_PIXMAP_CHECK ECORE_CALLBACK_RENEW; */

/*    DBG("\tClient Zone: %d", (ec->zone != NULL)); */

/*    return ECORE_CALLBACK_RENEW; */
/* } */

static Eina_Bool 
_e_comp_wl_cb_client_prop(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Client *ec;
   E_Event_Client_Property *ev;

   ev = event;
   /* if (!(ev->property & E_CLIENT_PROPERTY_ICON)) return ECORE_CALLBACK_RENEW; */

   ec = ev->ec;
   E_COMP_WL_PIXMAP_CHECK ECORE_CALLBACK_RENEW;

   if (ec->desktop)
     {
        if (!ec->exe_inst) e_exec_phony(ec);
     }

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_cb_hook_client_move_end(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   /* DBG("COMP_WL HOOK CLIENT MOVE END !!"); */

   /* unset pointer */
   /* e_pointer_type_pop(e_comp_get(ec)->pointer, ec, "move"); */

   /* ec->post_move = EINA_TRUE; */
   /* _e_comp_wl_client_idler_add(ec); */
}

static void 
_e_comp_wl_cb_hook_client_del(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t win;

   E_COMP_WL_PIXMAP_CHECK;

   if ((!ec->already_unparented) && (ec->comp_data->reparented))
     _e_comp_wl_focus_down_set(ec);

   ec->already_unparented = EINA_TRUE;
   win = e_pixmap_window_get(ec->pixmap);
   eina_hash_del_by_key(clients_win_hash, &win);

   if (ec->comp_data->pending.damage)
     eina_tiler_free(ec->comp_data->pending.damage);
   if (ec->comp_data->pending.input)
     eina_tiler_free(ec->comp_data->pending.input);
   if (ec->comp_data->pending.opaque)
     eina_tiler_free(ec->comp_data->pending.opaque);

   if (ec->comp_data->reparented)
     {
        win = e_client_util_pwin_get(ec);
        eina_hash_del_by_key(clients_win_hash, &win);
        e_pixmap_parent_window_set(ec->pixmap, 0);
     }

   if ((ec->parent) && (ec->parent->modal == ec))
     {
        ec->parent->lock_close = EINA_FALSE;
        ec->parent->modal = NULL;
     }

   if (ec->comp_data->sub.list)
     {
        E_Client *subc;

        EINA_LIST_FREE(ec->comp_data->sub.list, subc)
          _e_comp_wl_subsurface_destroy_internal(subc);
     }

   E_FREE_FUNC(ec->comp_data->first_draw_tmr, ecore_timer_del);

   E_FREE(ec->comp_data);
   ec->comp_data = NULL;

   _e_comp_wl_focus_check(ec->comp);
}

static void 
_e_comp_wl_cb_hook_client_new(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t win;

   E_COMP_WL_PIXMAP_CHECK;

   win = e_pixmap_window_get(ec->pixmap);
   ec->ignored = e_comp_ignore_win_find(win);

   /* NB: could not find a better place to do this, BUT for internal windows, 
    * we need to set delete_request else the close buttons on the frames do 
    * basically nothing */
   if (ec->internal) ec->icccm.delete_request = EINA_TRUE;

   ec->comp_data = E_NEW(E_Comp_Client_Data, 1);

   ec->comp_data->pending.damage = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(ec->comp_data->pending.damage, 1, 1);

   ec->comp_data->pending.input = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(ec->comp_data->pending.input, 1, 1);

   ec->comp_data->pending.opaque = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(ec->comp_data->pending.opaque, 1, 1);

   ec->comp_data->pending.buffer_destroy.notify = 
     _e_comp_wl_buffer_pending_cb_destroy;

   ec->comp_data->mapped = EINA_FALSE;
   ec->changes.shape = EINA_TRUE;
   ec->changes.shape_input = EINA_TRUE;

   if (!_e_comp_wl_client_new_helper(ec)) return;

   ec->comp_data->first_damage = ec->internal;
   /* ec->comp_data->first_damage = ((ec->internal) || (ec->override)); */

   eina_hash_add(clients_win_hash, &win, ec);
   e_hints_client_list_set();

   ec->comp_data->first_draw_tmr = 
     ecore_timer_add(e_comp_config_get()->first_draw_delay, 
                     _e_comp_wl_cb_first_draw, ec);
}

static void 
_e_comp_wl_cb_hook_client_eval_fetch(void *data EINA_UNUSED, E_Client *ec)
{
   E_Event_Client_Property *ev;
   Eina_Bool move = EINA_FALSE;
   Eina_Bool resize = EINA_FALSE;
   int x, y, w, h;

   E_COMP_WL_PIXMAP_CHECK;

   if ((ec->changes.prop) && (ec->netwm.fetch.state))
     {
        e_hints_window_state_get(ec);
        ec->netwm.fetch.state = EINA_FALSE;
     }

   if ((ec->changes.prop) && (ec->e.fetch.state))
     {
        e_hints_window_e_state_get(ec);
        ec->e.fetch.state = EINA_FALSE;
     }

   if ((ec->changes.prop) && (ec->netwm.fetch.type))
     {
        e_hints_window_type_get(ec);
        if (((!ec->lock_border) || (!ec->border.name)) && 
            (ec->comp_data->reparented))
          {
             ec->border.changed = EINA_TRUE;
             EC_CHANGED(ec);
          }

        if ((ec->netwm.type == E_WINDOW_TYPE_DOCK) || (ec->tooltip))
          {
             if (!ec->netwm.state.skip_pager)
               {
                  ec->netwm.state.skip_pager = EINA_TRUE;
                  ec->netwm.update.state = EINA_TRUE;
               }
             if (!ec->netwm.state.skip_taskbar)
               {
                  ec->netwm.state.skip_taskbar = EINA_TRUE;
                  ec->netwm.update.state = EINA_TRUE;
               }
          }
        else if (ec->netwm.type == E_WINDOW_TYPE_DESKTOP)
          {
             ec->focus_policy_override = E_FOCUS_CLICK;
             if (!ec->netwm.state.skip_pager)
               {
                  ec->netwm.state.skip_pager = EINA_TRUE;
                  ec->netwm.update.state = EINA_TRUE;
               }
             if (!ec->netwm.state.skip_taskbar)
               {
                  ec->netwm.state.skip_taskbar = EINA_TRUE;
                  ec->netwm.update.state = EINA_TRUE;
               }
             if (!e_client_util_ignored_get(ec))
               ec->border.changed = ec->borderless = EINA_TRUE;
          }
        else if ((ec->netwm.type == E_WINDOW_TYPE_MENU) || 
                 (ec->netwm.type == E_WINDOW_TYPE_POPUP_MENU) || 
                 (ec->netwm.type == E_WINDOW_TYPE_DROPDOWN_MENU))
          {
             ec->netwm.state.skip_pager = EINA_TRUE;
             ec->netwm.update.state = EINA_TRUE;
             ec->netwm.state.skip_taskbar = EINA_TRUE;
             ec->netwm.update.state = EINA_TRUE;
             ec->focus_policy_override = E_FOCUS_CLICK;
             ec->icccm.accepts_focus = EINA_FALSE;
             eina_stringshare_replace(&ec->bordername, "borderless");
          }

        if (ec->tooltip)
          {
             ec->focus_policy_override = E_FOCUS_CLICK;
             ec->icccm.accepts_focus = EINA_FALSE;
             eina_stringshare_replace(&ec->bordername, "borderless");
          }
        else if (ec->internal)
          {
             Ecore_Wl_Window *wwin;

             DBG("CLIENT EVAL INTERNAL WINDOW");

             /* FIXME: BORDERLESS INTERNAL OVERRIDE WINDOWS
              * 
              * ie: logout window */
             wwin = ecore_evas_wayland_window_get(ec->internal_ecore_evas);
             if (ec->dialog)
               {
                  Ecore_Wl_Window *pwin;

                  pwin = ecore_evas_wayland_window_get(ec->comp->ee);
                  ecore_wl_window_parent_set(wwin, pwin);
               }
             else
               ecore_wl_window_parent_set(wwin, NULL);
          }

        ev = E_NEW(E_Event_Client_Property, 1);

        ev->ec = ec;
        e_object_ref(E_OBJECT(ec));
        ev->property = E_CLIENT_PROPERTY_NETWM_STATE;
        ecore_event_add(E_EVENT_CLIENT_PROPERTY, ev, 
                        _e_comp_wl_client_event_free, NULL);

        ec->netwm.fetch.type = EINA_FALSE;
     }

   /* TODO: vkbd, etc */

   /* FIXME: Update ->changes.shape code for recent switch to eina_tiler */

   if ((ec->changes.prop) && (ec->netwm.update.state))
     {
        e_hints_window_state_set(ec);
        if (((!ec->lock_border) || (!ec->border.name)) && 
            (!(((ec->maximized & E_MAXIMIZE_TYPE) == E_MAXIMIZE_FULLSCREEN))) && 
               (ec->comp_data->reparented))
          {
             ec->border.changed = EINA_TRUE;
             EC_CHANGED(ec);
          }

        if (ec->parent)
          {
             if (ec->netwm.state.modal)
               {
                  ec->parent->modal = ec;
                  if (ec->parent->focused)
                    evas_object_focus_set(ec->frame, EINA_TRUE);
               }
          }
        else if (ec->leader)
          {
             if (ec->netwm.state.modal)
               {
                  ec->leader->modal = ec;
                  if (ec->leader->focused)
                    evas_object_focus_set(ec->frame, EINA_TRUE);
                  else
                    {
                       Eina_List *l;
                       E_Client *child;

                       EINA_LIST_FOREACH(ec->leader->group, l, child)
                         {
                            if ((child != ec) && (child->focused))
                              evas_object_focus_set(ec->frame, EINA_TRUE);
                         }
                    }
               }
          }
        ec->netwm.update.state = EINA_FALSE;
     }

   x = ec->x;
   y = ec->y;
   w = ec->client.w;
   h = ec->client.h;

   if ((ec->changes.pos) && (!ec->lock_client_location))
     {
        int zx, zy, zw, zh;

        e_zone_useful_geometry_get(ec->zone, &zx, &zy, &zw, &zh);

        if (e_config->screen_limits == E_CLIENT_OFFSCREEN_LIMIT_ALLOW_NONE)
          {
             x = E_CLAMP(ec->x, zx, zx + zw - ec->w);
             y = E_CLAMP(ec->y, zy, zy + zh - ec->h);
          }
     }

   e_comp_object_frame_wh_adjust(ec->frame, w, h, &w, &h);
   move = ((x != ec->x) || (y != ec->y));
   resize = ((w != ec->w) || (h != ec->h));

   if ((move) && (!ec->lock_client_location))
     {
        if ((ec->maximized & E_MAXIMIZE_TYPE) != E_MAXIMIZE_NONE)
          {
             E_Zone *zone;

             ec->saved.x = x;
             ec->saved.y = y;

             zone = e_comp_zone_xy_get(ec->comp, x, y);
             if (zone && ((zone->x) || (zone->y)))
               {
                  ec->saved.x -= zone->x;
                  ec->saved.y -= zone->y;
               }
          }
        else
          {
             /* client is completely outside the screen, policy does not allow */
             if (((!E_INTERSECTS(x, y, ec->w, ec->h, ec->comp->man->x, ec->comp->man->y, ec->comp->man->w - 5, ec->comp->man->h - 5)) &&
                  (e_config->screen_limits != E_CLIENT_OFFSCREEN_LIMIT_ALLOW_FULL)) ||
                 /* client is partly outside the zone, policy does not allow */
                 (((!E_INSIDE(x, y, ec->comp->man->x, ec->comp->man->y, ec->comp->man->w - 5, ec->comp->man->h - 5)) &&
                   (!E_INSIDE(x + ec->w, y, ec->comp->man->x, ec->comp->man->y, ec->comp->man->w - 5, ec->comp->man->h - 5)) &&
                   (!E_INSIDE(x, y + ec->h, ec->comp->man->x, ec->comp->man->y, ec->comp->man->w - 5, ec->comp->man->h - 5)) &&
                   (!E_INSIDE(x + ec->w, y + ec->h, ec->comp->man->x, ec->comp->man->y, ec->comp->man->w - 5, ec->comp->man->h - 5))) &&
                     (e_config->screen_limits == E_CLIENT_OFFSCREEN_LIMIT_ALLOW_NONE))
                )
               e_comp_object_util_center(ec->frame);
             else
               evas_object_move(ec->frame, x, y);
          }
     }

   if (((resize) && (!ec->lock_client_size)) && 
       ((move) || ((!ec->maximized) && (!ec->fullscreen))))
     {
        if ((ec->maximized & E_MAXIMIZE_TYPE) != E_MAXIMIZE_NONE)
          {
             ec->saved.w = w;
             ec->saved.h = h;
          }
        else
          evas_object_resize(ec->frame, w, h);
     }

   if (ec->icccm.fetch.transient_for)
     {
        if (ec->parent)
          {
             evas_object_layer_set(ec->frame, ec->parent->layer);
             if (ec->netwm.state.modal)
               {
                  ec->parent->modal = ec;
                  ec->parent->lock_close = EINA_TRUE;
               }

             if ((e_config->focus_setting == E_FOCUS_NEW_DIALOG) ||
                 (ec->parent->focused && 
                     (e_config->focus_setting == 
                         E_FOCUS_NEW_DIALOG_IF_OWNER_FOCUSED)))
               ec->take_focus = EINA_TRUE;
          }

        ec->icccm.fetch.transient_for = EINA_FALSE;
     }

   ec->changes.prop = EINA_FALSE;
   if (ec->changes.icon)
     {
        if (ec->comp_data->reparented) return;
        ec->comp_data->change_icon = EINA_TRUE;
        ec->changes.icon = EINA_FALSE;
     }
}

static void 
_e_comp_wl_cb_hook_client_pre_frame(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t parent;
   
   E_COMP_WL_PIXMAP_CHECK;

   if (!ec->comp_data->need_reparent) return;

   /* WRN("Client Needs New Parent in Pre Frame"); */

   parent = e_client_util_pwin_get(ec);

   ec->border_size = 0;
   e_pixmap_parent_window_set(ec->pixmap, parent);

   /* if (ec->shaped_input) */
   /*   { */
   /*      DBG("\tClient Shaped Input"); */
   /*   } */

   eina_hash_add(clients_win_hash, &parent, ec);

   if (ec->visible)
     {
        if (ec->comp_data->set_win_type)
          {
             if (ec->internal_ecore_evas)
               {
                  int type = ECORE_WL_WINDOW_TYPE_TOPLEVEL;

                  switch (ec->netwm.type)
                    {
                     case E_WINDOW_TYPE_DIALOG:
                       /* NB: If there is No transient set, then dialogs get 
                        * treated as Normal Toplevel windows */
                       if (ec->icccm.transient_for)
                         type = ECORE_WL_WINDOW_TYPE_TRANSIENT;
                       break;
                     case E_WINDOW_TYPE_DESKTOP:
                       type = ECORE_WL_WINDOW_TYPE_FULLSCREEN;
                       break;
                     case E_WINDOW_TYPE_DND:
                       type = ECORE_WL_WINDOW_TYPE_DND;
                       break;
                     case E_WINDOW_TYPE_MENU:
                     case E_WINDOW_TYPE_DROPDOWN_MENU:
                     case E_WINDOW_TYPE_POPUP_MENU:
                       type = ECORE_WL_WINDOW_TYPE_MENU;
                       break;
                     case E_WINDOW_TYPE_NORMAL:
                     default:
                         break;
                    }

                  ecore_evas_wayland_type_set(ec->internal_ecore_evas, type);
               }

             ec->comp_data->set_win_type = EINA_FALSE;
          }
     }

   /* TODO */
   /* focus_setup */
   e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, parent);
   e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, parent);

   _e_comp_wl_client_evas_init(ec);

   if ((ec->netwm.ping) && (!ec->ping_poller)) 
     e_client_ping(ec);

   if (ec->visible) evas_object_show(ec->frame);

   ec->comp_data->need_reparent = EINA_FALSE;
   ec->redirected = EINA_TRUE;

   if (ec->comp_data->change_icon)
     {
        ec->changes.icon = EINA_TRUE;
        EC_CHANGED(ec);
     }

   ec->comp_data->change_icon = EINA_FALSE;
   ec->comp_data->reparented = EINA_TRUE;

   /* _e_comp_wl_evas_cb_comp_hidden(ec, NULL, NULL); */
}

static void 
_e_comp_wl_cb_hook_client_post_new(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   if (ec->need_shape_merge) ec->need_shape_merge = EINA_FALSE;

   if (ec->changes.internal_state)
     {
        E_Win *win;

        if ((win = ecore_evas_data_get(ec->internal_ecore_evas, "E_Win")))
          {
             if (win->state.centered)
               e_comp_object_util_center(ec->frame);
          }

        ec->changes.internal_state = EINA_FALSE;
     }

   if (ec->changes.internal_props)
     {
        E_Win *win;

        if ((win = ecore_evas_data_get(ec->internal_ecore_evas, "E_Win")))
          {
             ecore_evas_size_min_set(ec->internal_ecore_evas, 
                                     win->min_w, win->min_h);
             ecore_evas_size_max_set(ec->internal_ecore_evas, 
                                     win->max_w, win->max_h);
             ecore_evas_size_base_set(ec->internal_ecore_evas, 
                                      win->base_w, win->base_h);
             ecore_evas_size_step_set(ec->internal_ecore_evas, 
                                      win->step_x, win->step_y);
             /* TODO: handle aspect */

             ec->changes.internal_props = EINA_FALSE;
          }
     }

   if (ec->need_shape_export) 
     {
        _e_comp_wl_client_shape_check(ec);
        ec->need_shape_export = EINA_FALSE;
     }
}

static void 
_e_comp_wl_cb_hook_client_eval_end(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   if ((ec->comp->wl_comp_data->restack) && (!ec->comp->new_clients))
     {
        e_hints_client_stacking_set();
        ec->comp->wl_comp_data->restack = EINA_FALSE;
     }
}

static void 
_e_comp_wl_cb_hook_client_focus_set(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   if (ec->comp_data->shell.activate)
     {
        if (ec->comp_data->shell.surface)
          ec->comp_data->shell.activate(ec->comp_data->shell.surface);
     }

   /* FIXME: This seems COMPLETELY wrong !! (taken from e_comp_x)
    * 
    * We are getting focus on the client, WHY ON EARTH would we want to focus 
    * the compositor window Even IF the client pixmap is not Wl ?? */

   /* if ((e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL)) */
   /*   { */
   /*      e_grabinput_focus(ec->comp->ee_win, E_FOCUS_METHOD_PASSIVE); */
   /*      return; */
   /*   } */

   if ((ec->icccm.take_focus) && (ec->icccm.accepts_focus))
     e_grabinput_focus(e_client_util_win_get(ec), 
                       E_FOCUS_METHOD_LOCALLY_ACTIVE);
   else if (!ec->icccm.accepts_focus)
     e_grabinput_focus(e_client_util_win_get(ec), 
                       E_FOCUS_METHOD_GLOBALLY_ACTIVE);
   else if (!ec->icccm.take_focus)
     e_grabinput_focus(e_client_util_win_get(ec), E_FOCUS_METHOD_PASSIVE);

   if (ec->comp->wl_comp_data->kbd.focus != ec->comp_data->surface)
     {
        ec->comp->wl_comp_data->kbd.focus = ec->comp_data->surface;
        e_comp_wl_data_device_keyboard_focus_set(ec->comp->wl_comp_data);
     }
}

static void 
_e_comp_wl_cb_hook_client_focus_unset(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   if (ec->comp_data->shell.deactivate)
     {
        if (ec->comp_data->shell.surface)
          ec->comp_data->shell.deactivate(ec->comp_data->shell.surface);
     }

   _e_comp_wl_focus_check(ec->comp);

   if (ec->comp->wl_comp_data->kbd.focus == ec->comp_data->surface)
     ec->comp->wl_comp_data->kbd.focus = NULL;
}

E_API Eina_Bool 
e_comp_wl_init(void)
{
   /* set gl available */
   if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_EGL))
     e_comp_gl_set(EINA_TRUE);

   /* try to create a wayland compositor */
   if (!_e_comp_wl_compositor_create())
     {
        e_error_message_show(_("Enlightenment cannot create a Wayland Compositor!\n"));
        return EINA_FALSE;
     }

   /* set ecore_wayland in server mode
    * NB: this is done before init so that ecore_wayland does not stall while 
    * waiting for compositor to be created */
   /* ecore_wl_server_mode_set(EINA_TRUE); */

   /* try to initialize ecore_wayland */
   if (!ecore_wl_init(NULL))
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Wayland!\n"));
        return EINA_FALSE;
     }

   /* create hash to store client windows */
   clients_win_hash = eina_hash_int64_new(NULL);

   /* setup event handlers for e events */
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMP_OBJECT_ADD, 
                         _e_comp_wl_cb_comp_object_add, NULL);
   /* E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_ZONE_SET,  */
   /*                       _e_comp_wl_cb_client_zone_set, NULL); */
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_PROPERTY, 
                         _e_comp_wl_cb_client_prop, NULL);

   /* setup client hooks */
   /* e_client_hook_add(E_CLIENT_HOOK_DESK_SET, _cb, NULL); */
   /* e_client_hook_add(E_CLIENT_HOOK_RESIZE_BEGIN, _cb, NULL); */
   /* e_client_hook_add(E_CLIENT_HOOK_RESIZE_END, _cb, NULL); */
   /* e_client_hook_add(E_CLIENT_HOOK_MOVE_BEGIN,  */
   /*                   _e_comp_wl_cb_hook_client_move_begin, NULL); */
   e_client_hook_add(E_CLIENT_HOOK_MOVE_END, 
                     _e_comp_wl_cb_hook_client_move_end, NULL);
   e_client_hook_add(E_CLIENT_HOOK_DEL, 
                     _e_comp_wl_cb_hook_client_del, NULL);
   e_client_hook_add(E_CLIENT_HOOK_NEW_CLIENT, 
                     _e_comp_wl_cb_hook_client_new, NULL);
   e_client_hook_add(E_CLIENT_HOOK_EVAL_FETCH, 
                     _e_comp_wl_cb_hook_client_eval_fetch, NULL);
   e_client_hook_add(E_CLIENT_HOOK_EVAL_PRE_FRAME_ASSIGN, 
                     _e_comp_wl_cb_hook_client_pre_frame, NULL);
   e_client_hook_add(E_CLIENT_HOOK_EVAL_POST_NEW_CLIENT, 
                     _e_comp_wl_cb_hook_client_post_new, NULL);
   e_client_hook_add(E_CLIENT_HOOK_EVAL_END, 
                     _e_comp_wl_cb_hook_client_eval_end, NULL);
   /* e_client_hook_add(E_CLIENT_HOOK_UNREDIRECT, _cb, NULL); */
   /* e_client_hook_add(E_CLIENT_HOOK_REDIRECT, _cb, NULL); */
   e_client_hook_add(E_CLIENT_HOOK_FOCUS_SET, 
                     _e_comp_wl_cb_hook_client_focus_set, NULL);
   e_client_hook_add(E_CLIENT_HOOK_FOCUS_UNSET, 
                     _e_comp_wl_cb_hook_client_focus_unset, NULL);

   /* TODO: e_desklock_hooks ?? */

   return EINA_TRUE;
}

EINTERN void 
e_comp_wl_shutdown(void)
{
   /* delete event handlers */
   E_FREE_LIST(handlers, ecore_event_handler_del);

   /* delete client window hash */
   E_FREE_FUNC(clients_win_hash, eina_hash_free);

   /* shutdown ecore_wayland */
   ecore_wl_shutdown();
}

EINTERN struct wl_resource *
e_comp_wl_surface_create(struct wl_client *client, int version, uint32_t id)
{
   struct wl_resource *ret = NULL;

   if ((ret = wl_resource_create(client, &wl_surface_interface, version, id)))
     wl_resource_set_implementation(ret, &_e_comp_wl_surface_interface, NULL, 
                                    e_comp_wl_surface_destroy);

   return ret;
}

EINTERN void 
e_comp_wl_surface_destroy(struct wl_resource *resource)
{
   E_Pixmap *cp;
   E_Client *ec;

   if (!(cp = wl_resource_get_user_data(resource))) return;

   /* try to find the E client for this surface */
   if (!(ec = e_pixmap_client_get(cp)))
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, e_pixmap_window_get(cp));

   if (!ec)
     {
        /* client was already deleted */
        e_pixmap_free(cp);
        return;
     }

   e_object_del(E_OBJECT(ec));
}

EINTERN void
e_comp_wl_buffer_reference(E_Comp_Wl_Buffer_Ref *ref, E_Comp_Wl_Buffer *buffer)
{
   if ((ref->buffer) && (buffer != ref->buffer))
     {
        ref->buffer->busy--;

        if (ref->buffer->busy == 0)
          wl_resource_queue_event(ref->buffer->resource, WL_BUFFER_RELEASE);

        wl_list_remove(&ref->destroy_listener.link);
     }

   if ((buffer) && (buffer != ref->buffer))
     {
        buffer->busy++;
        wl_signal_add(&buffer->destroy_signal, &ref->destroy_listener);
     }

   ref->buffer = buffer;
   ref->destroy_listener.notify = _e_comp_wl_buffer_reference_cb_destroy;
}
