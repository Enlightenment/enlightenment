#define EXECUTIVE_MODE_ENABLED
#define E_COMP_WL
#include "e.h"

static void
_mime_types_free(E_Comp_Wl_Data_Source *source)
{
   if (!source->mime_types) return;
   while (eina_array_count(source->mime_types))
     eina_stringshare_del(eina_array_pop(source->mime_types));
   eina_array_free(source->mime_types);
}

static void 
_e_comp_wl_data_offer_cb_accept(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t serial, const char *mime_type)
{
   E_Comp_Wl_Data_Offer *offer;

   DBG("Data Offer Accept");
   if (!(offer = wl_resource_get_user_data(resource)))
     return;

   if (offer->source)
     offer->source->target(offer->source, serial, mime_type);
}

static void
_e_comp_wl_data_offer_cb_receive(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *mime_type, int32_t fd)
{
   E_Comp_Wl_Data_Offer *offer;

   DBG("Data Offer Receive");

   if (!(offer = wl_resource_get_user_data(resource)))
     return;

   if (offer->source)
     offer->source->send(offer->source, mime_type, fd);
   else
     close(fd);
}

/* called by wl_data_offer_destroy */
static void
_e_comp_wl_data_offer_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   DBG("Data Offer Destroy");
   wl_resource_destroy(resource);
}

static void
_e_comp_wl_data_offer_cb_finish(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED)
{
   /* TODO: implement */
}

static void
_e_comp_wl_data_offer_cb_actions_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, uint32_t actions EINA_UNUSED, uint32_t preferred_action EINA_UNUSED)
{
   /* TODO: implement */
}

/* called by wl_resource_destroy */
static void
_e_comp_wl_data_offer_cb_resource_destroy(struct wl_resource *resource)
{
   E_Comp_Wl_Data_Offer *offer;

   if (!(offer = wl_resource_get_user_data(resource)))
     return;

   if (offer->source)
     wl_list_remove(&offer->source_destroy_listener.link);

   free(offer);
}

/* called by emission of source->destroy_signal */
static void
_e_comp_wl_data_offer_cb_source_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Wl_Data_Offer *offer;

   DBG("Data Offer Source Destroy");
   offer = container_of(listener, E_Comp_Wl_Data_Offer,
                        source_destroy_listener);
   if (!offer) return;

   offer->source = NULL;
}

static const struct wl_data_offer_interface _e_data_offer_interface =
{
   _e_comp_wl_data_offer_cb_accept,
   _e_comp_wl_data_offer_cb_receive,
   _e_comp_wl_data_offer_cb_destroy,
   _e_comp_wl_data_offer_cb_finish,
   _e_comp_wl_data_offer_cb_actions_set,
};

static void
_e_comp_wl_data_source_cb_offer(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *mime_type)
{
   E_Comp_Wl_Data_Source *source;

   DBG("Data Source Offer");
   if (!(source = wl_resource_get_user_data(resource)))
     return;

   if (!source->mime_types)
     source->mime_types = eina_array_new(1);
   eina_array_push(source->mime_types, eina_stringshare_add(mime_type));
}

/* called by wl_data_source_destroy */
static void
_e_comp_wl_data_source_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   DBG("Data Source Destroy");
   wl_resource_destroy(resource);
}

static void
_e_comp_wl_data_source_cb_actions_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, uint32_t actions EINA_UNUSED)
{
   /* TODO: implement */
}

/* called by wl_resource_destroy */
static void
_e_comp_wl_data_source_cb_resource_destroy(struct wl_resource *resource)
{
   E_Comp_Wl_Data_Source *source;

   if (!(source = wl_resource_get_user_data(resource)))
     return;

   wl_signal_emit(&source->destroy_signal, source);

   _mime_types_free(source);
   free(source);
}

static void
_e_comp_wl_data_source_target_send(E_Comp_Wl_Data_Source *source, uint32_t serial EINA_UNUSED, const char* mime_type)
{
   DBG("Data Source Target Send");
   wl_data_source_send_target(source->resource, mime_type);
}

static void
_e_comp_wl_data_source_send_send(E_Comp_Wl_Data_Source *source, const char* mime_type, int32_t fd)
{
   DBG("Data Source Source Send");
   wl_data_source_send_send(source->resource, mime_type, fd);
   close(fd);
}

static void
_e_comp_wl_data_source_cancelled_send(E_Comp_Wl_Data_Source *source)
{
   DBG("Data Source Cancelled Send");
   wl_data_source_send_cancelled(source->resource);
}

static const struct wl_data_source_interface _e_data_source_interface =
{
   _e_comp_wl_data_source_cb_offer,
   _e_comp_wl_data_source_cb_destroy,
   _e_comp_wl_data_source_cb_actions_set,
};

static void
_e_comp_wl_data_device_destroy_selection_data_source(struct wl_listener *listener EINA_UNUSED, void *data)
{
   E_Comp_Wl_Data_Source *source;
   struct wl_resource *data_device_res = NULL, *focus = NULL;

   DBG("Data Device Destroy Selection Source");
   if (!(source = (E_Comp_Wl_Data_Source*)data))
     return;

   e_comp_wl->selection.data_source = NULL;

   if (e_comp_wl->kbd.enabled)
     focus = e_comp_wl->kbd.focus;

   if (focus)
     {
        if (source->resource)
          data_device_res =
             e_comp_wl_data_find_for_client(wl_resource_get_client(source->resource));

        if (data_device_res)
          wl_data_device_send_selection(data_device_res, NULL);
     }

   wl_signal_emit(&e_comp_wl->selection.signal, e_comp->wl_comp_data);
}

static struct wl_resource*
_e_comp_wl_data_device_data_offer_create(E_Comp_Wl_Data_Source *source, struct wl_resource *data_device_res)
{
   E_Comp_Wl_Data_Offer *offer;
   Eina_Iterator *it;
   char *t;

   DBG("Data Offer Create");

   offer = E_NEW(E_Comp_Wl_Data_Offer, 1);
   if (!offer) return NULL;

   offer->resource = 
     wl_resource_create(wl_resource_get_client(data_device_res), 
                        &wl_data_offer_interface, 1, 0);
   if (!offer->resource)
     {
        free(offer);
        return NULL;
     }

   wl_resource_set_implementation(offer->resource, 
                                  &_e_data_offer_interface, offer, 
                                  _e_comp_wl_data_offer_cb_resource_destroy);
   offer->source = source;
   offer->source_destroy_listener.notify = 
     _e_comp_wl_data_offer_cb_source_destroy;
   wl_signal_add(&source->destroy_signal, &offer->source_destroy_listener);

   wl_data_device_send_data_offer(data_device_res, offer->resource);

   it = eina_array_iterator_new(source->mime_types);
   EINA_ITERATOR_FOREACH(it, t)
     wl_data_offer_send_offer(offer->resource, t);
   eina_iterator_free(it);

   return offer->resource;
}

static void
_e_comp_wl_data_device_selection_set(void *data EINA_UNUSED, E_Comp_Wl_Data_Source *source, uint32_t serial)
{
   E_Comp_Wl_Data_Source *sel_source;
   struct wl_resource *offer_res, *data_device_res, *focus = NULL;

   sel_source = (E_Comp_Wl_Data_Source*)e_comp_wl->selection.data_source;

   if (sel_source)
     {
        if (sel_source->cancelled)
          sel_source->cancelled(sel_source);
        if (!e_comp_wl->clipboard.xwl_owner)
          wl_list_remove(&e_comp_wl->selection.data_source_listener.link);
        e_comp_wl->selection.data_source = NULL;
     }

   e_comp_wl->selection.data_source = sel_source = source;
   e_comp_wl->clipboard.xwl_owner = NULL;
   e_comp_wl->selection.serial = serial;

   if (e_comp_wl->kbd.enabled)
     focus = e_comp_wl->kbd.focus;

   if (focus)
     {
        data_device_res =
          e_comp_wl_data_find_for_client(wl_resource_get_client(focus));
        if ((data_device_res) && (source))
          {
             offer_res =
               _e_comp_wl_data_device_data_offer_create(source,
                                                        data_device_res);
             wl_data_device_send_selection(data_device_res, offer_res);
          }
        else if (data_device_res)
          wl_data_device_send_selection(data_device_res, NULL);
     }

   wl_signal_emit(&e_comp_wl->selection.signal, e_comp->wl_comp_data);

   if (source)
     {
        e_comp_wl->selection.data_source_listener.notify = 
          _e_comp_wl_data_device_destroy_selection_data_source;
        wl_signal_add(&source->destroy_signal, 
                      &e_comp_wl->selection.data_source_listener);
     }
}

static void
_e_comp_wl_data_device_drag_finished(E_Drag *drag, int dropped)
{
   Evas_Object *o;

   o = edje_object_part_swallow_get(drag->comp_object, "e.swallow.content");
   if (eina_streq(evas_object_type_get(o), "e_comp_object"))
     edje_object_part_unswallow(drag->comp_object, o);
   else
     e_zoomap_child_set(o, NULL);
   evas_object_hide(o);
   evas_object_pass_events_set(o, 1);
   if (e_comp_wl->drag != drag) return;
   e_comp_wl->drag = NULL;
   e_comp_wl->drag_client = NULL;
   e_screensaver_inhibit_toggle(0);
   if (e_comp_wl->selection.target && (!dropped))
     {
#ifndef HAVE_WAYLAND_ONLY
        if (e_client_has_xwindow(e_comp_wl->selection.target))
          {
             ecore_x_client_message32_send(e_client_util_win_get(e_comp_wl->selection.target),
                                           ECORE_X_ATOM_XDND_DROP,
                                           ECORE_X_EVENT_MASK_NONE,
                                           e_comp->cm_selection, 0,
                                           ecore_x_current_time_get(), 0, 0);
          }
        else
#endif
          {
             struct wl_resource *res;

             res = e_comp_wl_data_find_for_client(wl_resource_get_client(e_comp_wl->selection.target->comp_data->surface));
             if (res)
               {
                  wl_data_device_send_drop(res);
                  wl_data_device_send_leave(res);
               }
#ifndef HAVE_WAYLAND_ONLY
             if (e_comp_util_has_xwayland())
               {
                  ecore_x_selection_owner_set(0, ECORE_X_ATOM_SELECTION_XDND,
                                              ecore_x_current_time_get());
                  ecore_x_window_hide(e_comp->cm_selection);
               }
#endif
             e_comp_wl->selection.target = NULL;
             e_comp_wl->drag_source = NULL;
          }
     }
}

static void
_e_comp_wl_data_device_cb_drag_start(struct wl_client *client, struct wl_resource *resource EINA_UNUSED, struct wl_resource *source_resource, struct wl_resource *origin_resource, struct wl_resource *icon_resource, uint32_t serial)
{
   E_Comp_Wl_Data_Source *source;
   Eina_List *l;
   struct wl_resource *res;
   E_Client *ec = NULL;
   int x, y;

   DBG("Data Device Drag Start");

   if ((e_comp_wl->kbd.focus) && (e_comp_wl->kbd.focus != origin_resource))
     return;

   if (!(source = wl_resource_get_user_data(source_resource))) return;
   e_comp_wl->drag_source = source;

   if (icon_resource)
     {
        DBG("\tHave Icon Resource: %p", icon_resource);
        ec = wl_resource_get_user_data(icon_resource);
        if (!ec->re_manage)
          {
             ec->re_manage = 1;

             ec->lock_focus_out = ec->override = 1;
             ec->icccm.title = eina_stringshare_add("noshadow");
             ec->layer = E_LAYER_CLIENT_DRAG;
             evas_object_layer_set(ec->frame, E_LAYER_CLIENT_DRAG);
             e_client_focus_stack_set(eina_list_remove(e_client_focus_stack_get(), ec));
             EC_CHANGED(ec);
             e_comp_wl->drag_client = ec;
          }
     }

   EINA_LIST_FOREACH(e_comp_wl->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != client) continue;
        wl_pointer_send_leave(res, serial, e_comp_wl->kbd.focus);
     }

   evas_pointer_canvas_xy_get(e_comp->evas, &x, &y);
   e_comp_wl->drag = e_drag_new(x, y, NULL, 0, NULL, 0, NULL,
                                _e_comp_wl_data_device_drag_finished);
   e_comp_wl->drag->button_mask =
     evas_pointer_button_down_mask_get(e_comp->evas);
   if (ec)
     e_drag_object_set(e_comp_wl->drag, ec->frame);
   e_drag_start(e_comp_wl->drag, x, y);
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_util_has_xwayland())
     {
        ecore_x_window_show(e_comp->cm_selection);
        ecore_x_selection_owner_set(e_comp->cm_selection,
                                    ECORE_X_ATOM_SELECTION_XDND,
                                    ecore_x_current_time_get());
     }
#endif
   if (e_comp_wl->ptr.ec)
     e_comp_wl_data_device_send_enter(e_comp_wl->ptr.ec);
   e_screensaver_inhibit_toggle(1);
   e_comp_canvas_feed_mouse_up(0);
}

static void
_e_comp_wl_data_device_cb_selection_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *source_resource, uint32_t serial)
{
   E_Comp_Wl_Data_Source *source;

   DBG("Data Device Selection Set");
   if (!source_resource) return;
   if (!(source = wl_resource_get_user_data(source_resource))) return;

   _e_comp_wl_data_device_selection_set(e_comp->wl_comp_data, source, serial);
}

static void 
_e_comp_wl_data_device_cb_release(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   DBG("Data Device Release");
   wl_resource_destroy(resource);
}

static const struct wl_data_device_interface _e_data_device_interface =
{
   _e_comp_wl_data_device_cb_drag_start,
   _e_comp_wl_data_device_cb_selection_set,
   _e_comp_wl_data_device_cb_release
};

static void
_e_comp_wl_data_device_cb_unbind(struct wl_resource *resource)
{
   struct wl_client *wc = wl_resource_get_client(resource);
   eina_hash_del_by_key(e_comp_wl->mgr.data_resources, &wc);
}

static void 
_e_comp_wl_data_manager_cb_device_get(struct wl_client *client, struct wl_resource *manager_resource, uint32_t id, struct wl_resource *seat_resource EINA_UNUSED)
{
   struct wl_resource *res;

   DBG("Data Manager Device Get");


   /* try to create the data device resource */
   res = wl_resource_create(client, &wl_data_device_interface, 1, id);
   if (!res)
     {
        ERR("Could not create data device resource");
        wl_resource_post_no_memory(manager_resource);
        return;
     }

   eina_hash_add(e_comp_wl->mgr.data_resources, &client, res);
   wl_resource_set_implementation(res, &_e_data_device_interface,
                                  e_comp->wl_comp_data, 
                                  _e_comp_wl_data_device_cb_unbind);
}

static const struct wl_data_device_manager_interface _e_manager_interface = 
{
   (void*)e_comp_wl_data_manager_source_create,
   _e_comp_wl_data_manager_cb_device_get
};

/* static void  */
/* _e_comp_wl_data_cb_unbind_manager(struct wl_resource *resource) */
/* { */
/*    E_Comp_Data *e_comp->wl_comp_data; */

/*    DBG("Comp_Wl_Data: Unbind Manager"); */

/*    if (!(e_comp->wl_comp_data = wl_resource_get_user_data(resource))) return; */

/*    e_comp_wl->mgr.resource = NULL; */
/* } */

static void 
_e_comp_wl_data_cb_bind_manager(struct wl_client *client, void *data EINA_UNUSED, uint32_t version EINA_UNUSED, uint32_t id)
{
   struct wl_resource *res;

   /* try to create data manager resource */
   e_comp_wl->mgr.resource = res =
     wl_resource_create(client, &wl_data_device_manager_interface, 1, id);
   if (!res)
     {
        ERR("Could not create data device manager");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_manager_interface,
                                  e_comp->wl_comp_data, NULL);
}

static Eina_Bool
_e_comp_wl_clipboard_offer_load(void *data, Ecore_Fd_Handler *handler)
{
   E_Comp_Wl_Clipboard_Offer *offer;
   char *p;
   size_t size;
   int len;
   int fd;

   if (!(offer = (E_Comp_Wl_Clipboard_Offer*)data))
     return ECORE_CALLBACK_CANCEL;

   fd = ecore_main_fd_handler_fd_get(handler);

   size = offer->source->contents.size;
   p = (char*)offer->source->contents.data;
   len = write(fd, p + offer->offset, size - offer->offset);
   if (len > 0) offer->offset += len;

   if ((offer->offset == size) || (len <= 0))
     {
        close(fd);
        ecore_main_fd_handler_del(handler);
        e_comp_wl_clipboard_source_unref(offer->source);
        free(offer);
     }

   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_wl_clipboard_offer_create(E_Comp_Wl_Clipboard_Source* source, int fd)
{
   E_Comp_Wl_Clipboard_Offer *offer;

   offer = E_NEW(E_Comp_Wl_Clipboard_Offer, 1);

   offer->offset = 0;
   offer->source = source;
   source->ref++;
   offer->fd_handler =
      ecore_main_fd_handler_add(fd, ECORE_FD_WRITE,
                                _e_comp_wl_clipboard_offer_load, offer,
                                NULL, NULL);
}

static Eina_Bool
_e_comp_wl_clipboard_source_save(void *data EINA_UNUSED, Ecore_Fd_Handler *handler)
{
   E_Comp_Wl_Clipboard_Source *source;
   char *p;
   int len, size;


   if (!(source = (E_Comp_Wl_Clipboard_Source*)e_comp_wl->clipboard.source))
     return ECORE_CALLBACK_CANCEL;

   /* extend contents buffer */
   if ((source->contents.alloc - source->contents.size) < CLIPBOARD_CHUNK)
     {
        wl_array_add(&source->contents, CLIPBOARD_CHUNK);
        source->contents.size -= CLIPBOARD_CHUNK;
     }

   p = (char*)source->contents.data + source->contents.size;
   size = source->contents.alloc - source->contents.size;
   len = read(source->fd, p, size);

   if (len == 0)
     {
        ecore_main_fd_handler_del(handler);
        close(source->fd);
        source->fd_handler = NULL;
     }
   else if (len < 0)
     {
        e_comp_wl_clipboard_source_unref(source);
        e_comp_wl->clipboard.source = NULL;
     }
   else
     {
        source->contents.size += len;
     }

   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_wl_clipboard_source_target_send(E_Comp_Wl_Data_Source *source EINA_UNUSED, uint32_t serial EINA_UNUSED, const char *mime_type EINA_UNUSED)
{
}

static void
_e_comp_wl_clipboard_source_send_send(E_Comp_Wl_Data_Source *source, const char *mime_type, int fd)
{
   E_Comp_Wl_Clipboard_Source* clip_source;
   char *t;

   clip_source = container_of(source, E_Comp_Wl_Clipboard_Source, data_source);
   if (!clip_source) return;

   t = eina_array_data_get(source->mime_types, 0);
   if (!strcmp(mime_type, t))
     _e_comp_wl_clipboard_offer_create(clip_source, fd);
   else
     close(fd);
}

static void
_e_comp_wl_clipboard_source_cancelled_send(E_Comp_Wl_Data_Source *source EINA_UNUSED)
{
}

static void
_e_comp_wl_clipboard_selection_set(struct wl_listener *listener EINA_UNUSED, void *data EINA_UNUSED)
{
   E_Comp_Wl_Data_Source *sel_source;
   E_Comp_Wl_Clipboard_Source *clip_source;
   int p[2];
   char *mime_type;

   sel_source = (E_Comp_Wl_Data_Source*) e_comp_wl->selection.data_source;
   clip_source = (E_Comp_Wl_Clipboard_Source*) e_comp_wl->clipboard.source;

   if (!sel_source)
     {
        if (clip_source)
          _e_comp_wl_data_device_selection_set(e_comp->wl_comp_data,
                                               &clip_source->data_source,
                                               clip_source->serial);
        return;
     }
   else if (sel_source->target == _e_comp_wl_clipboard_source_target_send)
     return;

   if (clip_source)
     e_comp_wl_clipboard_source_unref(clip_source);

   e_comp_wl->clipboard.source = NULL;
   mime_type = eina_array_data_get(sel_source->mime_types, 0);

   if (pipe2(p, O_CLOEXEC) == -1)
     return;

   sel_source->send(sel_source, mime_type, p[1]);

   e_comp_wl->clipboard.source =
     e_comp_wl_clipboard_source_create(mime_type,
                                       e_comp_wl->selection.serial, p[0]);

   if (!e_comp_wl->clipboard.source)
     close(p[0]);
}

static void
_e_comp_wl_clipboard_create(void)
{
   e_comp_wl->clipboard.listener.notify = _e_comp_wl_clipboard_selection_set;
   wl_signal_add(&e_comp_wl->selection.signal, &e_comp_wl->clipboard.listener);
}

static void
_e_comp_wl_data_device_target_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;

   if (e_comp_wl->selection.target == ec)
     e_comp_wl->selection.target = NULL;
}

E_API void
e_comp_wl_data_device_send_enter(E_Client *ec)
{
   struct wl_resource *data_device_res, *offer_res;
   uint32_t serial;
   int x, y;

   if (e_client_has_xwindow(ec) &&
       e_client_has_xwindow(e_comp_wl->drag_client))
     return;
   if (!e_client_has_xwindow(ec))
     {
        data_device_res =
          e_comp_wl_data_find_for_client(wl_resource_get_client(ec->comp_data->surface));
        if (!data_device_res) return;
        offer_res = e_comp_wl_data_device_send_offer(ec);
        if (e_comp_wl->drag_source && (!offer_res)) return;
     }
   e_comp_wl->selection.target = ec;
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_DEL,
                                  _e_comp_wl_data_device_target_del, ec);

#ifndef HAVE_WAYLAND_ONLY
   if (e_client_has_xwindow(ec))
     {
        int d1 = 0x5UL, d2, d3, d4;
        E_Comp_Wl_Data_Source *source;

        d2 = d3 = d4 = 0;
        source = e_comp_wl->drag_source;

        if (eina_array_count(source->mime_types) > 3)
          {
             const char *type, *types[eina_array_count(source->mime_types)];
             int i = 0;
             Eina_Iterator *it;

             d1 |= 0x1UL;
             it = eina_array_iterator_new(source->mime_types);
             EINA_ITERATOR_FOREACH(it, type)
               types[i++] = type;
             eina_iterator_free(it);
             ecore_x_dnd_types_set(e_comp->cm_selection, types, i);
          }
        else if (source->mime_types)
          {
             if (eina_array_count(source->mime_types))
               d2 = ecore_x_atom_get(eina_array_data_get(source->mime_types, 0));
             if (eina_array_count(source->mime_types) > 1)
               d3 = ecore_x_atom_get(eina_array_data_get(source->mime_types, 1));
             if (eina_array_count(source->mime_types) > 2)
               d4 = ecore_x_atom_get(eina_array_data_get(source->mime_types, 2));
          }

        ecore_x_client_message32_send(e_client_util_win_get(ec),
                                      ECORE_X_ATOM_XDND_ENTER,
                                      ECORE_X_EVENT_MASK_NONE,
                                      e_comp->cm_selection, d1, d2, d3, d4);

        return;
     }
#endif
   x = wl_fixed_to_int(e_comp_wl->ptr.x) - e_comp_wl->selection.target->client.x;
   y = wl_fixed_to_int(e_comp_wl->ptr.y) - e_comp_wl->selection.target->client.y;
   serial = wl_display_next_serial(e_comp_wl->wl.disp);
   wl_data_device_send_enter(data_device_res, serial, ec->comp_data->surface,
                             wl_fixed_from_int(x), wl_fixed_from_int(y),
                             offer_res);
}

E_API void
e_comp_wl_data_device_send_leave(E_Client *ec)
{
   struct wl_resource *res;

   if (e_client_has_xwindow(ec) &&
       e_client_has_xwindow(e_comp_wl->drag_client))
     return;
   evas_object_event_callback_del_full(ec->frame, EVAS_CALLBACK_DEL,
                                       _e_comp_wl_data_device_target_del, ec);
   if (e_comp_wl->selection.target == ec)
     e_comp_wl->selection.target = NULL;
#ifndef HAVE_WAYLAND_ONLY
   if (e_client_has_xwindow(ec))
     {
        ecore_x_client_message32_send(e_client_util_win_get(ec),
                                      ECORE_X_ATOM_XDND_LEAVE,
                                      ECORE_X_EVENT_MASK_NONE,
                                      e_comp->cm_selection, 0, 0, 0, 0);
        return;
     }
#endif
   res = e_comp_wl_data_find_for_client(wl_resource_get_client(ec->comp_data->surface));
   if (res)
     wl_data_device_send_leave(res);
}

EINTERN void *
e_comp_wl_data_device_send_offer(E_Client *ec)
{
   struct wl_resource *data_device_res, *offer_res = NULL;
   E_Comp_Wl_Data_Source *source;

   data_device_res =
      e_comp_wl_data_find_for_client(wl_resource_get_client(ec->comp_data->surface));
   if (!data_device_res) return NULL;
   source = e_comp_wl->drag_source;
   if (source)
     {
        offer_res =
          _e_comp_wl_data_device_data_offer_create(source, data_device_res);
     }

   return offer_res;
}

E_API void
e_comp_wl_data_device_keyboard_focus_set(void)
{
   struct wl_resource *data_device_res, *offer_res = NULL, *focus;
   E_Comp_Wl_Data_Source *source;

   if (!e_comp_wl->kbd.enabled)
     {
        ERR("Keyboard not enabled");
        return;
     }

   if (!(focus = e_comp_wl->kbd.focus))
     {
        ERR("No focused resource");
        return;
     }
   source = (E_Comp_Wl_Data_Source*)e_comp_wl->selection.data_source;

#ifndef HAVE_WAYLAND_ONLY
   do
     {
        if (!e_comp_util_has_xwayland()) break;
        if (e_comp_wl->clipboard.xwl_owner)
          {
             if (e_client_has_xwindow(e_client_focused_get())) return;
             break;
          }
        else if (source && e_client_has_xwindow(e_client_focused_get()))
          {
             /* wl -> x11 */
             ecore_x_selection_owner_set(e_comp->cm_selection,
                                         ECORE_X_ATOM_SELECTION_CLIPBOARD,
                                         ecore_x_current_time_get());
             return;
          }
     } while (0);
#endif
   data_device_res =
     e_comp_wl_data_find_for_client(wl_resource_get_client(focus));
   if (!data_device_res) return;

   if (source)
     {
        offer_res =
          _e_comp_wl_data_device_data_offer_create(source, data_device_res);
     }

   wl_data_device_send_selection(data_device_res, offer_res);
}

EINTERN Eina_Bool
e_comp_wl_data_manager_init(void)
{
   /* try to create global data manager */
   e_comp_wl->mgr.global =
     wl_global_create(e_comp_wl->wl.disp, &wl_data_device_manager_interface, 1,
                      e_comp->wl_comp_data, _e_comp_wl_data_cb_bind_manager);
   if (!e_comp_wl->mgr.global)
     {
        ERR("Could not create global for data device manager");
        return EINA_FALSE;
     }

   wl_signal_init(&e_comp_wl->selection.signal);

   /* create clipboard */
   _e_comp_wl_clipboard_create();
   e_comp_wl->mgr.data_resources = eina_hash_pointer_new(NULL);

   return EINA_TRUE;
}

EINTERN void 
e_comp_wl_data_manager_shutdown(void)
{
   /* destroy the global manager resource */
   /* if (e_comp_wl->mgr.global) wl_global_destroy(e_comp_wl->mgr.global); */

   wl_list_remove(&e_comp_wl->clipboard.listener.link);
   E_FREE_FUNC(e_comp_wl->mgr.data_resources, eina_hash_free);
}

E_API struct wl_resource *
e_comp_wl_data_find_for_client(struct wl_client *client)
{
   return eina_hash_find(e_comp_wl->mgr.data_resources, &client);
}

E_API E_Comp_Wl_Data_Source *
e_comp_wl_data_manager_source_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp_Wl_Data_Source *source;

   DBG("Data Manager Source Create");

   source = E_NEW(E_Comp_Wl_Data_Source, 1);
   if (!source)
     {
        wl_resource_post_no_memory(resource);
        return NULL;
     }

   wl_signal_init(&source->destroy_signal);
   source->target = _e_comp_wl_data_source_target_send;
   source->send = _e_comp_wl_data_source_send_send;
   source->cancelled = _e_comp_wl_data_source_cancelled_send;

   source->resource =
     wl_resource_create(client, &wl_data_source_interface, 1, id);
   if (!source->resource)
     {
        ERR("Could not create data source resource");
        free(source);
        wl_resource_post_no_memory(resource);
        return NULL;
     }

   wl_resource_set_implementation(source->resource,
                                  &_e_data_source_interface, source,
                                  _e_comp_wl_data_source_cb_resource_destroy);
   return source;
}

E_API E_Comp_Wl_Clipboard_Source *
e_comp_wl_clipboard_source_create(const char *mime_type, uint32_t serial, int fd)
{
   E_Comp_Wl_Clipboard_Source *source;

   source = E_NEW(E_Comp_Wl_Clipboard_Source, 1);
   if (!source) return NULL;

   source->data_source.resource = NULL;
   source->data_source.target = _e_comp_wl_clipboard_source_target_send;
   source->data_source.send = _e_comp_wl_clipboard_source_send_send;
   source->data_source.cancelled = _e_comp_wl_clipboard_source_cancelled_send;

   wl_array_init(&source->contents);
   wl_signal_init(&source->data_source.destroy_signal);

   source->ref = 1;
   source->serial = serial;

   if (mime_type)
     {
        if (!source->data_source.mime_types)
          source->data_source.mime_types = eina_array_new(1);
        eina_array_push(source->data_source.mime_types,
                        eina_stringshare_add(mime_type));
     }

   if (fd > 0)
     {
        source->fd_handler =
          ecore_main_fd_handler_add(fd, ECORE_FD_READ,
                                    _e_comp_wl_clipboard_source_save,
                                    e_comp->wl_comp_data, NULL, NULL);
        if (!source->fd_handler) return NULL;
     }

   source->fd = fd;

   return source;
}

E_API void
e_comp_wl_clipboard_source_unref(E_Comp_Wl_Clipboard_Source *source)
{
   EINA_SAFETY_ON_NULL_RETURN(source);
   source->ref--;
   if (source->ref > 0) return;

   if (source->fd_handler)
     {
        ecore_main_fd_handler_del(source->fd_handler);
        close(source->fd);
     }

   _mime_types_free(&source->data_source);

   wl_signal_emit(&source->data_source.destroy_signal, &source->data_source);
   wl_array_release(&source->contents);
   free(source);
}
