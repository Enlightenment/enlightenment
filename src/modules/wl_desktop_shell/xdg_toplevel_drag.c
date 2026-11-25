/* xdg_toplevel_drag_v1 carries a window with a drag, as a browser does
 * when a tab is torn off. It is here because attach takes an
 * xdg_toplevel, which this module defines; the drag and the window it
 * carries are in e_comp_wl_data.c. */
#define E_COMP_WL
#include "e.h"
#include "e_mod_main.h"
#include "xdg-toplevel-drag-v1-server-protocol.h"

typedef struct Toplevel_Drag
{
   E_Comp_Wl_Data_Source *source;
   struct wl_listener source_destroy;
} Toplevel_Drag;

static void
_e_xdg_toplevel_drag_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   Toplevel_Drag *td = wl_resource_get_user_data(resource);

   if ((td->source) && (e_comp_wl->drag_source == td->source))
     {
        wl_resource_post_error(resource, XDG_TOPLEVEL_DRAG_V1_ERROR_ONGOING_DRAG,
                               "the drag is still running");
        return;
     }
   wl_resource_destroy(resource);
}

static void
_e_xdg_toplevel_drag_cb_attach(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *toplevel, int32_t x_offset, int32_t y_offset)
{
   Toplevel_Drag *td = wl_resource_get_user_data(resource);
   E_Client *ec = wl_resource_get_user_data(toplevel), *attached;

   if ((!td->source) || (!ec) || (e_object_is_del(E_OBJECT(ec)))) return;
   attached = td->source->toplevel;
   if ((attached) && (attached != ec) && (!e_object_is_del(E_OBJECT(attached))) &&
       (attached->comp_data->mapped))
     {
        wl_resource_post_error(resource, XDG_TOPLEVEL_DRAG_V1_ERROR_TOPLEVEL_ATTACHED,
                               "a mapped toplevel is already attached");
        return;
     }
   e_comp_wl_data_source_toplevel_set(td->source, ec, x_offset, y_offset);
}

static const struct xdg_toplevel_drag_v1_interface _e_xdg_toplevel_drag_interface =
{
   _e_xdg_toplevel_drag_cb_destroy,
   _e_xdg_toplevel_drag_cb_attach,
};

static void
_e_xdg_toplevel_drag_cb_source_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   Toplevel_Drag *td = container_of(listener, Toplevel_Drag, source_destroy);

   wl_list_remove(&td->source_destroy.link);
   td->source = NULL;
}

static void
_e_xdg_toplevel_drag_cb_resource_destroy(struct wl_resource *resource)
{
   Toplevel_Drag *td = wl_resource_get_user_data(resource);

   if (td->source)
     {
        wl_list_remove(&td->source_destroy.link);
        e_comp_wl_data_source_toplevel_set(td->source, NULL, 0, 0);
     }
   free(td);
}

static void
_e_xdg_toplevel_drag_manager_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

/* The source must not have been used yet: the protocol wants this before
 * start_drag, and the source can never become a selection. */
static void
_e_xdg_toplevel_drag_manager_cb_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *source_resource)
{
   E_Comp_Wl_Data_Source *source = wl_resource_get_user_data(source_resource);
   struct wl_resource *res;
   Toplevel_Drag *td;

   if ((e_comp_wl->drag_source == source) || (e_comp_wl->selection.data_source == source))
     {
        wl_resource_post_error(resource, XDG_TOPLEVEL_DRAG_MANAGER_V1_ERROR_INVALID_SOURCE,
                               "the source is already in use");
        return;
     }

   td = E_NEW(Toplevel_Drag, 1);
   if (!td)
     {
        wl_resource_post_no_memory(resource);
        return;
     }
   res = wl_resource_create(client, &xdg_toplevel_drag_v1_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        free(td);
        wl_resource_post_no_memory(resource);
        return;
     }
   td->source = source;
   td->source_destroy.notify = _e_xdg_toplevel_drag_cb_source_destroy;
   wl_signal_add(&source->destroy_signal, &td->source_destroy);
   wl_resource_set_implementation(res, &_e_xdg_toplevel_drag_interface, td,
                                  _e_xdg_toplevel_drag_cb_resource_destroy);
   source->dnd_only = EINA_TRUE;
}

static const struct xdg_toplevel_drag_manager_v1_interface _e_xdg_toplevel_drag_manager_interface =
{
   _e_xdg_toplevel_drag_manager_cb_destroy,
   _e_xdg_toplevel_drag_manager_cb_get,
};

static void
_e_xdg_toplevel_drag_manager_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &xdg_toplevel_drag_manager_v1_interface, version, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }
   wl_resource_set_implementation(res, &_e_xdg_toplevel_drag_manager_interface, NULL, NULL);
}

EINTERN Eina_Bool
e_xdg_toplevel_drag_init(void)
{
   return !!wl_global_create(e_comp_wl->wl.disp, &xdg_toplevel_drag_manager_v1_interface,
                             1, NULL, _e_xdg_toplevel_drag_manager_cb_bind);
}
