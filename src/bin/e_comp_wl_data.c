#include "e.h"
#include "e_comp_wl.h"
#include "e_comp_wl_data.h"

static void 
_e_comp_wl_data_device_cb_drag_start(struct wl_client *client, struct wl_resource *resource, struct wl_resource *source_resource, struct wl_resource *origin_resource, struct wl_resource *icon_resource, uint32_t serial)
{

}

static void 
_e_comp_wl_data_device_cb_selection_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *source_resource, uint32_t serial)
{

}

static const struct wl_data_device_interface _e_data_device_interface = 
{
   _e_comp_wl_data_device_cb_drag_start,
   _e_comp_wl_data_device_cb_selection_set,
};

static void 
_e_comp_wl_data_manager_cb_source_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   /* NB: New resource */
}

static void 
_e_comp_wl_data_manager_cb_device_get(struct wl_client *client EINA_UNUSED, struct wl_resource *manager_resource, uint32_t id, struct wl_resource *seat_resource)
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

   wl_resource_set_implementation(res, &_e_data_device_interface, cdata, NULL);
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

   return EINA_TRUE;
}

EINTERN void 
e_comp_wl_data_manager_shutdown(E_Comp_Wl_Data *cdata)
{
   /* destroy the global manager resource */
   /* if (cdata->mgr.global) wl_global_destroy(cdata->mgr.global); */
}
