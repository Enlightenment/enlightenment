#define E_COMP_WL
#include "e.h"

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
};

static void
_e_comp_wl_data_source_cb_offer(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *mime_type)
{
   E_Comp_Wl_Data_Source *source;

   DBG("Data Source Offer");
   if (!(source = wl_resource_get_user_data(resource)))
     return;

   source->mime_types = 
     eina_list_append(source->mime_types, eina_stringshare_add(mime_type));
}

/* called by wl_data_source_destroy */
static void
_e_comp_wl_data_source_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   DBG("Data Source Destroy");
   wl_resource_destroy(resource);
}

/* called by wl_resource_destroy */
static void
_e_comp_wl_data_source_cb_resource_destroy(struct wl_resource *resource)
{
   E_Comp_Wl_Data_Source *source;
   char *t;

   if (!(source = wl_resource_get_user_data(resource)))
     return;

   wl_signal_emit(&source->destroy_signal, source);

   EINA_LIST_FREE(source->mime_types, t)
      eina_stringshare_del(t);

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
};

static void
_e_comp_wl_data_device_destroy_selection_data_source(struct wl_listener *listener EINA_UNUSED, void *data)
{
   E_Comp_Wl_Data_Source *source;
   struct wl_resource *data_device_res, *focus = NULL;

   DBG("Data Device Destroy Selection Source");
   if (!(source = (E_Comp_Wl_Data_Source*)data))
     return;

   e_comp->wl_comp_data->selection.data_source = NULL;

   if (e_comp->wl_comp_data->kbd.enabled)
     focus = e_comp->wl_comp_data->kbd.focus;

   if (focus)
     {
        data_device_res = 
           e_comp_wl_data_find_for_client(wl_resource_get_client(source->resource));

        if (data_device_res)
          wl_data_device_send_selection(data_device_res, NULL);
     }

   wl_signal_emit(&e_comp->wl_comp_data->selection.signal, e_comp->wl_comp_data);
}

static struct wl_resource*
_e_comp_wl_data_device_data_offer_create(E_Comp_Wl_Data_Source *source, struct wl_resource *data_device_res)
{
   E_Comp_Wl_Data_Offer *offer;
   Eina_List *l;
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

   EINA_LIST_FOREACH(source->mime_types, l, t)
      wl_data_offer_send_offer(offer->resource, t);

   return offer->resource;
}

static void
_e_comp_wl_data_device_selection_set(void *data EINA_UNUSED, E_Comp_Wl_Data_Source *source, uint32_t serial)
{
   E_Comp_Wl_Data_Source *sel_source;
   struct wl_resource *offer_res, *data_device_res, *focus = NULL;

   sel_source = (E_Comp_Wl_Data_Source*)e_comp->wl_comp_data->selection.data_source;

   if ((sel_source) &&
       (e_comp->wl_comp_data->selection.serial - serial < UINT32_MAX / 2))
     {
        /* TODO: elm_entry is sending too many request on now,
         * for those requests, selection.signal is being emitted also a lot.
         * when it completes to optimize the entry, it should be checked more.
         */
        if (e_comp->wl_comp_data->clipboard.source)
          wl_signal_emit(&e_comp->wl_comp_data->selection.signal, e_comp->wl_comp_data);

        return;
     }

   if (sel_source)
     {
        if (sel_source->cancelled)
          sel_source->cancelled(sel_source);
        wl_list_remove(&e_comp->wl_comp_data->selection.data_source_listener.link);
        e_comp->wl_comp_data->selection.data_source = NULL;
     }

   e_comp->wl_comp_data->selection.data_source = sel_source = source;
   e_comp->wl_comp_data->selection.serial = serial;

   if (e_comp->wl_comp_data->kbd.enabled)
     focus = e_comp->wl_comp_data->kbd.focus;

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

   wl_signal_emit(&e_comp->wl_comp_data->selection.signal, e_comp->wl_comp_data);

   if (source)
     {
        e_comp->wl_comp_data->selection.data_source_listener.notify = 
          _e_comp_wl_data_device_destroy_selection_data_source;
        wl_signal_add(&source->destroy_signal, 
                      &e_comp->wl_comp_data->selection.data_source_listener);
     }
}

static void
_e_comp_wl_data_device_cb_drag_start(struct wl_client *client, struct wl_resource *resource EINA_UNUSED, struct wl_resource *source_resource, struct wl_resource *origin_resource, struct wl_resource *icon_resource, uint32_t serial)
{
   E_Comp_Wl_Data_Source *source;
   Eina_List *l;
   struct wl_resource *res;

   DBG("Data Device Drag Start");

   if ((e_comp->wl_comp_data->kbd.focus) && (e_comp->wl_comp_data->kbd.focus != origin_resource)) return;

   if (!(source = wl_resource_get_user_data(source_resource))) return;

   /* TODO: create icon for pointer ?? */
   if (icon_resource)
     {
        E_Pixmap *cp;

        DBG("\tHave Icon Resource: %p", icon_resource);
        cp = wl_resource_get_user_data(icon_resource);
     }

   EINA_LIST_FOREACH(e_comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != client) continue;
        wl_pointer_send_leave(res, serial, e_comp->wl_comp_data->kbd.focus);
     }

   /* TODO: pointer start drag */
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
   e_comp->wl_comp_data->mgr.data_resources =
     eina_list_remove(e_comp->wl_comp_data->mgr.data_resources, resource);
}

static void
_e_comp_wl_data_manager_cb_source_create(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id EINA_UNUSED)
{
   E_Comp_Wl_Data_Source *source;

   DBG("Data Manager Source Create");

   source = E_NEW(E_Comp_Wl_Data_Source, 1);
   if (!source)
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_signal_init(&source->destroy_signal);
   source->target = _e_comp_wl_data_source_target_send;
   source->send = _e_comp_wl_data_source_send_send;
   source->cancelled = _e_comp_wl_data_source_cancelled_send;

   source->resource = 
     wl_resource_create(client, &wl_data_source_interface, 1, id);
   if (!source->resource)
     {
        ERR("Could not create data source resource: %m");
        free(source);
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_resource_set_implementation(source->resource, 
                                  &_e_data_source_interface, source, 
                                  _e_comp_wl_data_source_cb_resource_destroy);
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
        ERR("Could not create data device resource: %m");
        wl_resource_post_no_memory(manager_resource);
        return;
     }

   e_comp->wl_comp_data->mgr.data_resources = 
     eina_list_append(e_comp->wl_comp_data->mgr.data_resources, res);
   wl_resource_set_implementation(res, &_e_data_device_interface, e_comp->wl_comp_data, 
                                  _e_comp_wl_data_device_cb_unbind);
}

static const struct wl_data_device_manager_interface _e_manager_interface = 
{
   _e_comp_wl_data_manager_cb_source_create,
   _e_comp_wl_data_manager_cb_device_get
};

/* static void  */
/* _e_comp_wl_data_cb_unbind_manager(struct wl_resource *resource) */
/* { */
/*    E_Comp_Data *e_comp->wl_comp_data; */

/*    DBG("Comp_Wl_Data: Unbind Manager"); */

/*    if (!(e_comp->wl_comp_data = wl_resource_get_user_data(resource))) return; */

/*    e_comp->wl_comp_data->mgr.resource = NULL; */
/* } */

static void 
_e_comp_wl_data_cb_bind_manager(struct wl_client *client, void *data EINA_UNUSED, uint32_t version EINA_UNUSED, uint32_t id)
{
   struct wl_resource *res;

   /* try to create data manager resource */
   res = wl_resource_create(client, &wl_data_device_manager_interface, 1, id);
   if (!res)
     {
        ERR("Could not create data device manager: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_manager_interface,
                                  e_comp->wl_comp_data, NULL);
}

static void
_e_comp_wl_clipboard_source_unref(E_Comp_Wl_Clipboard_Source *source)
{
   char* t;

   source->ref --;
   if (source->ref > 0) return;

   if (source->fd_handler)
     {
        ecore_main_fd_handler_del(source->fd_handler);
        close(source->fd);
     }

   EINA_LIST_FREE(source->data_source.mime_types, t)
      eina_stringshare_del(t);

   wl_signal_emit(&source->data_source.destroy_signal, &source->data_source);
   wl_array_release(&source->contents);
   free(source);
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
        _e_comp_wl_clipboard_source_unref(offer->source);
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


   if (!(source = (E_Comp_Wl_Clipboard_Source*)e_comp->wl_comp_data->clipboard.source))
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
        _e_comp_wl_clipboard_source_unref(source);
        e_comp->wl_comp_data->clipboard.source = NULL;
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

   t = eina_list_nth(source->mime_types, 0);
   if (!strcmp(mime_type, t))
     _e_comp_wl_clipboard_offer_create(clip_source, fd);
   else
     close(fd);
}

static void
_e_comp_wl_clipboard_source_cancelled_send(E_Comp_Wl_Data_Source *source EINA_UNUSED)
{
}

static E_Comp_Wl_Clipboard_Source*
_e_comp_wl_clipboard_source_create(const char *mime_type, uint32_t serial, int fd)
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

   source->data_source.mime_types =
      eina_list_append(source->data_source.mime_types,
                       eina_stringshare_add(mime_type));

   source->fd_handler =
      ecore_main_fd_handler_add(fd, ECORE_FD_READ,
                                _e_comp_wl_clipboard_source_save,
                                e_comp->wl_comp_data, NULL, NULL);
   if (!source->fd_handler) return NULL;

   source->fd = fd;

   return source;
}

static void
_e_comp_wl_clipboard_selection_set(struct wl_listener *listener EINA_UNUSED, void *data EINA_UNUSED)
{
   E_Comp_Wl_Data_Source *sel_source;
   E_Comp_Wl_Clipboard_Source *clip_source;
   int p[2];
   char *mime_type;

   sel_source = (E_Comp_Wl_Data_Source*) e_comp->wl_comp_data->selection.data_source;
   clip_source = (E_Comp_Wl_Clipboard_Source*) e_comp->wl_comp_data->clipboard.source;

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
     _e_comp_wl_clipboard_source_unref(clip_source);

   e_comp->wl_comp_data->clipboard.source = NULL;
   mime_type = eina_list_nth(sel_source->mime_types, 0);

   if (pipe2(p, O_CLOEXEC) == -1)
     return;

   sel_source->send(sel_source, mime_type, p[1]);

   e_comp->wl_comp_data->clipboard.source =
      _e_comp_wl_clipboard_source_create(mime_type,
                                         e_comp->wl_comp_data->selection.serial, p[0]);

   if (!e_comp->wl_comp_data->clipboard.source)
     close(p[0]);
}

static void
_e_comp_wl_clipboard_create(void)
{
   e_comp->wl_comp_data->clipboard.listener.notify = _e_comp_wl_clipboard_selection_set;
   wl_signal_add(&e_comp->wl_comp_data->selection.signal, &e_comp->wl_comp_data->clipboard.listener);
}

EINTERN void
e_comp_wl_data_device_keyboard_focus_set(void)
{
   struct wl_resource *data_device_res, *offer_res = NULL, *focus;
   E_Comp_Wl_Data_Source *source;


   if (!e_comp->wl_comp_data->kbd.enabled)
     {
        ERR("Keyboard not enabled");
        return;
     }

   if (!(focus = e_comp->wl_comp_data->kbd.focus))
     {
        ERR("No focused resource");
        return;
     }

   data_device_res =
      e_comp_wl_data_find_for_client(wl_resource_get_client(focus));
   if (!data_device_res) return;

   source = (E_Comp_Wl_Data_Source*)e_comp->wl_comp_data->selection.data_source;
   if (source)
     offer_res = _e_comp_wl_data_device_data_offer_create(source, data_device_res);
   wl_data_device_send_selection(data_device_res, offer_res);
}

EINTERN Eina_Bool
e_comp_wl_data_manager_init(void)
{
   /* try to create global data manager */
   e_comp->wl_comp_data->mgr.global =
     wl_global_create(e_comp->wl_comp_data->wl.disp, &wl_data_device_manager_interface, 1,
                      e_comp->wl_comp_data, _e_comp_wl_data_cb_bind_manager);
   if (!e_comp->wl_comp_data->mgr.global)
     {
        ERR("Could not create global for data device manager: %m");
        return EINA_FALSE;
     }

   wl_signal_init(&e_comp->wl_comp_data->selection.signal);

   /* create clipboard */
   _e_comp_wl_clipboard_create();

   return EINA_TRUE;
}

EINTERN void 
e_comp_wl_data_manager_shutdown(void)
{
   /* destroy the global manager resource */
   /* if (e_comp->wl_comp_data->mgr.global) wl_global_destroy(e_comp->wl_comp_data->mgr.global); */

   wl_list_remove(&e_comp->wl_comp_data->clipboard.listener.link);
}

EINTERN struct wl_resource *
e_comp_wl_data_find_for_client(struct wl_client *client)
{
   Eina_List *l;
   struct wl_resource *res;

   EINA_LIST_FOREACH(e_comp->wl_comp_data->mgr.data_resources, l, res)
     {
        if (wl_resource_get_client(res) == client)
          return res;
     }

   return NULL;
}
