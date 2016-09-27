#define E_COMP_WL
#include "e.h"
#include "viewporter-server-protocol.h"

static void
_e_viewport_destroy(struct wl_resource *resource)
{
   E_Client *ec = wl_resource_get_user_data(resource);
   E_Comp_Client_Data *cdata;

   if (e_object_is_del(E_OBJECT(ec))) return;

   EINA_SAFETY_ON_NULL_RETURN(cdata = ec->comp_data);
   EINA_SAFETY_ON_NULL_RETURN(cdata->scaler.viewport);

   cdata->scaler.viewport = NULL;
   cdata->pending.buffer_viewport.buffer.src_width = wl_fixed_from_int(-1);
   cdata->pending.buffer_viewport.surface.width = -1;
   cdata->pending.buffer_viewport.changed = 1;
}

static void
_e_viewport_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_viewport_cb_set_source(struct wl_client *client EINA_UNUSED,
                          struct wl_resource *resource,
                          wl_fixed_t src_x,
                          wl_fixed_t src_y,
                          wl_fixed_t src_width,
                          wl_fixed_t src_height)
{
   E_Client *ec = wl_resource_get_user_data(resource);
   E_Comp_Client_Data *cdata;

   if (e_object_is_del(E_OBJECT(ec))) return;

   EINA_SAFETY_ON_NULL_RETURN(cdata = ec->comp_data);
   EINA_SAFETY_ON_NULL_RETURN(cdata->scaler.viewport);

   if (src_width == wl_fixed_from_int(-1) && src_height == wl_fixed_from_int(-1))
     {
        /* unset source size */
        cdata->pending.buffer_viewport.buffer.src_width = wl_fixed_from_int(-1);
        cdata->pending.buffer_viewport.changed = 1;
        return;
     }

   if (src_width <= 0 || src_height <= 0)
     {
        wl_resource_post_error(resource,
                               WP_VIEWPORT_ERROR_BAD_VALUE,
                               "source size must be positive (%fx%f)",
                               wl_fixed_to_double(src_width),
                               wl_fixed_to_double(src_height));
        return;
     }

   cdata->pending.buffer_viewport.buffer.src_x = src_x;
   cdata->pending.buffer_viewport.buffer.src_y = src_y;
   cdata->pending.buffer_viewport.buffer.src_width = src_width;
   cdata->pending.buffer_viewport.buffer.src_height = src_height;
   cdata->pending.buffer_viewport.changed = 1;
}

static void
_e_viewport_cb_set_destination(struct wl_client *client EINA_UNUSED,
                               struct wl_resource *resource,
                               int32_t dst_width,
                               int32_t dst_height)
{
   E_Client *ec = wl_resource_get_user_data(resource);
   E_Comp_Client_Data *cdata;

   if (e_object_is_del(E_OBJECT(ec))) return;

   EINA_SAFETY_ON_NULL_RETURN(cdata = ec->comp_data);
   EINA_SAFETY_ON_NULL_RETURN(cdata->scaler.viewport);

   if (dst_width == -1 && dst_height == -1)
     {
        /* unset destination size */
        cdata->pending.buffer_viewport.surface.width = -1;
        cdata->pending.buffer_viewport.changed = 1;
        return;
     }

   if (dst_width <= 0 || dst_height <= 0)
     {
        wl_resource_post_error(resource,
                               WP_VIEWPORT_ERROR_BAD_VALUE,
                               "destination size must be positive (%dx%d)",
                               dst_width, dst_height);
        return;
     }

   cdata->pending.buffer_viewport.surface.width = dst_width;
   cdata->pending.buffer_viewport.surface.height = dst_height;
   cdata->pending.buffer_viewport.changed = 1;
}

static const struct wp_viewport_interface _e_viewport_interface = {
   _e_viewport_cb_destroy,
   _e_viewport_cb_set_source,
   _e_viewport_cb_set_destination
};

static void
_e_scaler_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_scaler_cb_get_viewport(struct wl_client *client EINA_UNUSED, struct wl_resource *scaler, uint32_t id, struct wl_resource *surface_resource)
{
   int version = wl_resource_get_version(scaler);
   E_Client *ec;
   struct wl_resource *res;
   E_Comp_Client_Data *cdata;

   if (!(ec = wl_resource_get_user_data(surface_resource))) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (!(cdata = ec->comp_data)) return;

   if (cdata->scaler.viewport)
     {
        wl_resource_post_error(scaler,
                               WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
                               "a viewport for that surface already exists");
        return;
     }

   res = wl_resource_create(client, &wp_viewport_interface, version, id);
   if (res == NULL)
     {
        wl_client_post_no_memory(client);
        return;
     }

   cdata->scaler.viewport = res;
   wl_resource_set_implementation(res, &_e_viewport_interface, ec, _e_viewport_destroy);
}

static const struct wp_viewporter_interface _e_scaler_interface =
{
   _e_scaler_cb_destroy,
   _e_scaler_cb_get_viewport
};

static void
_e_scaler_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   if (!(res = wl_resource_create(client, &wp_viewporter_interface, MIN(version, 2), id)))
     {
        ERR("Could not create scaler resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_scaler_interface, NULL, NULL);
}

Eina_Bool
e_scaler_init(void)
{
   E_Comp_Wl_Data *cdata;

   if (!e_comp) return EINA_FALSE;
   if (!(cdata = e_comp->wl_comp_data)) return EINA_FALSE;
   if (!cdata->wl.disp) return EINA_FALSE;
   /* try to add scaler to wayland globals */
   if (!wl_global_create(cdata->wl.disp, &wp_viewporter_interface, 1,
                         cdata, _e_scaler_cb_bind))
     {
        ERR("Could not add scaler to wayland globals: %m");
        return EINA_FALSE;
     }

   return EINA_TRUE;
}
