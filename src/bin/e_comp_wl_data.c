#define E_COMP_WL
#include "e.h"

static struct wl_resource *
_e_comp_wl_data_find_for_client(Eina_List *list, struct wl_client *client)
{
   Eina_List *l;
   struct wl_resource *res;

   EINA_LIST_FOREACH(list, l, res)
     {
        if (wl_resource_get_client(res) == client)
          return res;
     }

   return NULL;
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
_e_comp_wl_data_device_destroy_selection_data_source(struct wl_listener *listener, void *data)
{
   E_Comp_Data *cdata;
   E_Comp_Wl_Data_Source *source;
   struct wl_resource *data_device_res, *focus = NULL;

   if (!(source = (E_Comp_Wl_Data_Source*)data))
     return;

   if (!(cdata = container_of(listener, E_Comp_Data, 
                              selection.data_source_listener)))
     return;

   cdata->selection.data_source = NULL;

   if (cdata->kbd.enabled)
     focus = cdata->kbd.focus;

   if (focus)
     {
        data_device_res = 
           _e_comp_wl_data_find_for_client(cdata->mgr.data_resources, 
                                           wl_resource_get_client(source->resource));

        if (data_device_res)
          wl_data_device_send_selection(data_device_res, NULL);
     }

   wl_signal_emit(&cdata->selection.signal, cdata);
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
_e_comp_wl_data_device_selection_set(E_Comp_Data *cdata, E_Comp_Wl_Data_Source *source, uint32_t serial)
{
   E_Comp_Wl_Data_Source *sel_source;
   struct wl_resource *offer_res, *data_device_res, *focus = NULL;

   sel_source = (E_Comp_Wl_Data_Source*)cdata->selection.data_source;

   if ((sel_source) &&
       (cdata->selection.serial - serial < UINT32_MAX / 2))
     {
        /* TODO: elm_entry is sending too many request on now,
         * for those requests, selection.signal is being emitted also a lot.
         * when it completes to optimize the entry, it should be checked more.
         */
        if (cdata->clipboard.source)
          wl_signal_emit(&cdata->selection.signal, cdata);

        return;
     }

   if (sel_source)
     {
        if (sel_source->cancelled)
          sel_source->cancelled(sel_source);
        wl_list_remove(&cdata->selection.data_source_listener.link);
        cdata->selection.data_source = NULL;
     }

   cdata->selection.data_source = sel_source = source;
   cdata->selection.serial = serial;

   if (cdata->kbd.enabled)
     focus = cdata->kbd.focus;

   if (focus)
     {
        data_device_res =
           _e_comp_wl_data_find_for_client(cdata->mgr.data_resources,
                                           wl_resource_get_client(focus));
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

   wl_signal_emit(&cdata->selection.signal, cdata);

   if (source)
     {
        cdata->selection.data_source_listener.notify = 
          _e_comp_wl_data_device_destroy_selection_data_source;
        wl_signal_add(&source->destroy_signal, 
                      &cdata->selection.data_source_listener);
     }
}

static void
_e_comp_wl_data_device_cb_drag_start(struct wl_client *client, struct wl_resource *resource, struct wl_resource *source_resource, struct wl_resource *origin_resource, struct wl_resource *icon_resource, uint32_t serial)
{
   E_Comp_Data *cdata;
   E_Comp_Wl_Data_Source *source;
   Eina_List *l;
   struct wl_resource *res;

   DBG("Data Device Drag Start");

   if (!(cdata = wl_resource_get_user_data(resource))) return;
   if ((cdata->kbd.focus) && (cdata->kbd.focus != origin_resource)) return;

   if (!(source = wl_resource_get_user_data(source_resource))) return;

   /* TODO: create icon for pointer ?? */
   if (icon_resource)
     {
        E_Pixmap *cp;

        DBG("\tHave Icon Resource: %p", icon_resource);
        cp = wl_resource_get_user_data(icon_resource);
     }

   EINA_LIST_FOREACH(cdata->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != client) continue;
        wl_pointer_send_leave(res, serial, cdata->kbd.focus);
     }

   /* TODO: pointer start drag */
}

static void
_e_comp_wl_data_device_cb_selection_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *source_resource, uint32_t serial)
{
   E_Comp_Data *cdata;
   E_Comp_Wl_Data_Source *source;

   if (!source_resource) return;
   if (!(cdata = wl_resource_get_user_data(resource))) return;
   if (!(source = wl_resource_get_user_data(source_resource))) return;

   _e_comp_wl_data_device_selection_set(cdata, source, serial);
}

static const struct wl_data_device_interface _e_data_device_interface =
{
   _e_comp_wl_data_device_cb_drag_start,
   _e_comp_wl_data_device_cb_selection_set,
};

static void
_e_comp_wl_data_device_cb_unbind(struct wl_resource *resource)
{
   E_Comp_Data *cdata;

   if(!(cdata = wl_resource_get_user_data(resource)))
     return;

   cdata->mgr.data_resources = 
     eina_list_remove(cdata->mgr.data_resources, resource);
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
_e_comp_wl_data_manager_cb_device_get(struct wl_client *client, struct wl_resource *manager_resource, uint32_t id, struct wl_resource *seat_resource)
{
   E_Comp_Data *cdata;
   struct wl_resource *res;

   /* DBG("Data Manager Device Get"); */

   /* try to get the compositor data */
   if (!(cdata = wl_resource_get_user_data(seat_resource))) return;

   /* try to create the data device resource */
   res = wl_resource_create(client, &wl_data_device_interface, 1, id);
   if (!res)
     {
        ERR("Could not create data device resource: %m");
        wl_resource_post_no_memory(manager_resource);
        return;
     }

   cdata->mgr.data_resources = 
     eina_list_append(cdata->mgr.data_resources, res);
   wl_resource_set_implementation(res, &_e_data_device_interface, cdata, 
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
/*    E_Comp_Data *cdata; */

/*    DBG("Comp_Wl_Data: Unbind Manager"); */

/*    if (!(cdata = wl_resource_get_user_data(resource))) return; */

/*    cdata->mgr.resource = NULL; */
/* } */

static void 
_e_comp_wl_data_cb_bind_manager(struct wl_client *client, void *data, uint32_t version EINA_UNUSED, uint32_t id)
{
   E_Comp_Data *cdata;
   struct wl_resource *res;

   cdata = data;

   /* try to create data manager resource */
   res = wl_resource_create(client, &wl_data_device_manager_interface, 1, id);
   if (!res)
     {
        ERR("Could not create data device manager: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_manager_interface, cdata, NULL);
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
_e_comp_wl_clipboard_source_save(void *data, Ecore_Fd_Handler *handler)
{
   E_Comp_Data *cdata;
   E_Comp_Wl_Clipboard_Source *source;
   char *p;
   int len, size;

   if (!(cdata = data)) return ECORE_CALLBACK_CANCEL;
   if (!(source = (E_Comp_Wl_Clipboard_Source*)cdata->clipboard.source))
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
        cdata->clipboard.source = NULL;
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
_e_comp_wl_clipboard_source_create(E_Comp_Data *cdata, const char *mime_type, uint32_t serial, int fd)
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
                                cdata, NULL, NULL);
   if (!source->fd_handler) return NULL;

   source->fd = fd;

   return source;
}

static void
_e_comp_wl_clipboard_selection_set(struct wl_listener *listener EINA_UNUSED, void *data)
{
   E_Comp_Data *cdata;
   E_Comp_Wl_Data_Source *sel_source;
   E_Comp_Wl_Clipboard_Source *clip_source;
   int p[2];
   char *mime_type;

   cdata = data;
   sel_source = (E_Comp_Wl_Data_Source*) cdata->selection.data_source;
   clip_source = (E_Comp_Wl_Clipboard_Source*) cdata->clipboard.source;

   if (!sel_source)
     {
        if (clip_source)
          _e_comp_wl_data_device_selection_set(cdata,
                                               &clip_source->data_source,
                                               clip_source->serial);
        return;
     }
   else if (sel_source->target == _e_comp_wl_clipboard_source_target_send)
     return;

   if (clip_source)
     _e_comp_wl_clipboard_source_unref(clip_source);

   cdata->clipboard.source = NULL;
   mime_type = eina_list_nth(sel_source->mime_types, 0);

   if (pipe2(p, O_CLOEXEC) == -1)
     return;

   sel_source->send(sel_source, mime_type, p[1]);

   cdata->clipboard.source =
      _e_comp_wl_clipboard_source_create(cdata, mime_type,
                                         cdata->selection.serial, p[0]);

   if (!cdata->clipboard.source)
     close(p[0]);
}

static void
_e_comp_wl_clipboard_destroy(E_Comp_Data *cdata)
{
   wl_list_remove(&cdata->clipboard.listener.link);
}

static void
_e_comp_wl_clipboard_create(E_Comp_Data *cdata)
{
   cdata->clipboard.listener.notify = _e_comp_wl_clipboard_selection_set;
   wl_signal_add(&cdata->selection.signal, &cdata->clipboard.listener);
}

EINTERN void
e_comp_wl_data_device_keyboard_focus_set(E_Comp_Data *cdata)
{
   struct wl_resource *data_device_res, *offer_res, *focus;
   E_Comp_Wl_Data_Source *source;

   if (!cdata->kbd.enabled) 
     {
        ERR("Keyboard not enabled");
        return;
     }

   if (!(focus = cdata->kbd.focus))
     {
        ERR("No focused resource");
        return;
     }

   data_device_res =
      _e_comp_wl_data_find_for_client(cdata->mgr.data_resources,
                                      wl_resource_get_client(focus));
   if (!data_device_res) return;

   source = (E_Comp_Wl_Data_Source*)cdata->selection.data_source;
   if (source)
     {
        uint32_t serial;

        serial = wl_display_next_serial(cdata->wl.disp);

        offer_res = _e_comp_wl_data_device_data_offer_create(source,
                                                             data_device_res);
        wl_data_device_send_enter(data_device_res, serial, focus, 
                                  cdata->ptr.x, cdata->ptr.y, offer_res);
        wl_data_device_send_selection(data_device_res, offer_res);
     }
}

EINTERN Eina_Bool
e_comp_wl_data_manager_init(E_Comp_Data *cdata)
{
   /* check for valid compositor data */
   if (!cdata) 
     {
        ERR("No Compositor Data");
        return EINA_FALSE;
     }

   /* try to create global data manager */
   cdata->mgr.global =
     wl_global_create(cdata->wl.disp, &wl_data_device_manager_interface, 1,
                      cdata, _e_comp_wl_data_cb_bind_manager);
   if (!cdata->mgr.global)
     {
        ERR("Could not create global for data device manager: %m");
        return EINA_FALSE;
     }

   wl_signal_init(&cdata->selection.signal);

   /* create clipboard */
   _e_comp_wl_clipboard_create(cdata);

   return EINA_TRUE;
}

EINTERN void 
e_comp_wl_data_manager_shutdown(E_Comp_Data *cdata)
{
   /* destroy the global manager resource */
   /* if (cdata->mgr.global) wl_global_destroy(cdata->mgr.global); */

   _e_comp_wl_clipboard_destroy(cdata);
}
