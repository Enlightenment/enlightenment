#include "e.h"
#include "e_comp_wl.h"
#include "e_comp_wl_data.h"

static struct wl_resource *
_e_comp_wl_data_find_for_client(Eina_List *list, struct wl_client *client)
{
   Eina_List *l;
   struct wl_resource *res;

   if ((!list) || (!client))
     return NULL;

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

   if (!(offer = wl_resource_get_user_data(resource)))
     return;

   if (offer->source)
     offer->source->target(offer->source, serial, mime_type);
}

static void
_e_comp_wl_data_offer_cb_receive(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *mime_type, int32_t fd)
{
   E_Comp_Wl_Data_Offer *offer;

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

   if (!(source = wl_resource_get_user_data(resource)))
     return;

   source->mime_types = 
     eina_list_append(source->mime_types, eina_stringshare_add(mime_type));
}

/* called by wl_data_source_destroy */
static void
_e_comp_wl_data_source_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
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
   wl_data_source_send_target(source->resource, mime_type);
}

static void
_e_comp_wl_data_source_send_send(E_Comp_Wl_Data_Source *source, const char* mime_type, int32_t fd)
{
   wl_data_source_send_send(source->resource, mime_type, fd);
   close(fd);
}

static void
_e_comp_wl_data_source_cancelled_send(E_Comp_Wl_Data_Source *source)
{
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
   E_Comp_Wl_Data *cdata;
   E_Comp_Wl_Data_Source *source;
   struct wl_resource *data_device_res, *focus = NULL;

   if (!(source = (E_Comp_Wl_Data_Source*)data))
     return;

   if (!(cdata = container_of(listener, E_Comp_Wl_Data, 
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

   offer = E_NEW(E_Comp_Wl_Data_Offer, 1);
   if (!offer) return NULL;

   offer->resource = wl_resource_create(wl_resource_get_client(data_device_res), &wl_data_offer_interface, 1, 0);
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
_e_comp_wl_data_device_cb_drag_start(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *source_resource EINA_UNUSED, struct wl_resource *origin_resource EINA_UNUSED, struct wl_resource *icon_resource EINA_UNUSED, uint32_t serial EINA_UNUSED)
{

}

static void 
_e_comp_wl_data_device_cb_selection_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *source_resource EINA_UNUSED, uint32_t serial EINA_UNUSED)
{
   E_Comp_Wl_Data *cdata;
   E_Comp_Wl_Data_Source *source, *sel_source;
   struct wl_resource *offer_res, *data_device_res, *focus = NULL;

   if (!source_resource) return;
   if (!(cdata = wl_resource_get_user_data(resource))) return;

   source = wl_resource_get_user_data(source_resource);
   sel_source = (E_Comp_Wl_Data_Source*)cdata->selection.data_source;

   if ((sel_source) &&
       (cdata->selection.serial - serial < UINT32_MAX / 2))
     return;

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
          {
             wl_data_device_send_selection(data_device_res, NULL);
          }
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

static const struct wl_data_device_interface _e_data_device_interface = 
{
   _e_comp_wl_data_device_cb_drag_start,
   _e_comp_wl_data_device_cb_selection_set,
};

static void
_e_comp_wl_data_device_cb_unbind(struct wl_resource *resource)
{
   E_Comp_Wl_Data *cdata;

   if(!(cdata = wl_resource_get_user_data(resource)))
     return;

   cdata->mgr.data_resources = 
     eina_list_remove(cdata->mgr.data_resources, resource);
}

static void
_e_comp_wl_data_manager_cb_source_create(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id EINA_UNUSED)
{
   E_Comp_Wl_Data_Source *source;

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
   E_Comp_Wl_Data *cdata;
   struct wl_resource *res;

   DBG("Comp_Wl_Data: Get Data Device");

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
/*    E_Comp_Wl_Data *cdata; */

/*    DBG("Comp_Wl_Data: Unbind Manager"); */

/*    if (!(cdata = wl_resource_get_user_data(resource))) return; */

/*    cdata->mgr.resource = NULL; */
/* } */

static void 
_e_comp_wl_data_cb_bind_manager(struct wl_client *client, void *data, uint32_t version EINA_UNUSED, uint32_t id)
{
   E_Comp_Wl_Data *cdata;
   struct wl_resource *res;

   if (!(cdata = data)) return;

   DBG("Comp_Wl_Data: Bind Manager");

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

EINTERN void
e_comp_wl_data_device_keyboard_focus_set(E_Comp_Wl_Data *cdata)
{
   struct wl_resource *data_device_res, *offer_res, *focus;
   E_Comp_Wl_Data_Source *source;

   if (!cdata->kbd.enabled) return;

   focus = cdata->kbd.focus;
   if (!focus) return;

   data_device_res =
      _e_comp_wl_data_find_for_client(cdata->mgr.data_resources,
                                      wl_resource_get_client(focus));
   if (!data_device_res) return;

   source = (E_Comp_Wl_Data_Source*)cdata->selection.data_source;
   if (source)
     {
        offer_res = _e_comp_wl_data_device_data_offer_create(source,
                                                             data_device_res);
        wl_data_device_send_selection(data_device_res, offer_res);
     }
}

EINTERN Eina_Bool
e_comp_wl_data_manager_init(E_Comp_Wl_Data *cdata)
{
   /* check for valid compositor data */
   if (!cdata) return EINA_FALSE;

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

   return EINA_TRUE;
}

EINTERN void 
e_comp_wl_data_manager_shutdown(E_Comp_Wl_Data *cdata EINA_UNUSED)
{
   /* destroy the global manager resource */
   /* if (cdata->mgr.global) wl_global_destroy(cdata->mgr.global); */
}
