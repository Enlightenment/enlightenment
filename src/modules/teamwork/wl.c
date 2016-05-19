#define E_COMP_WL
#include "e_mod_main.h"
#include "wl_teamwork.h"

static struct wl_resource *tw_resource;
static struct wl_global *tw_global;

static void
tw_preload_uri(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED,
               struct wl_resource *surface, const char *uri)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surface);
   if (ec)
     tw_link_detect(ec, uri);
}

static void
tw_activate_uri(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED,
                struct wl_resource *surface, const char *uri, wl_fixed_t x, wl_fixed_t y)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surface);
   if (ec)
     tw_link_show(ec, uri, wl_fixed_to_int(x), wl_fixed_to_int(y));
}

static void
tw_deactivate_uri(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED,
                  struct wl_resource *surface, const char *uri)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surface);
   if (ec)
     tw_link_hide(ec, uri);
}

static void
tw_open_uri(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED,
            struct wl_resource *surface, const char *uri)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surface);
   if (ec)
     tw_link_open(ec, uri);
}

static const struct zwp_teamwork_interface tw_interface =
{
   tw_preload_uri,
   tw_activate_uri,
   tw_deactivate_uri,
   tw_open_uri,
};

static void
wl_tw_cb_unbind(struct wl_resource *resource EINA_UNUSED)
{
   tw_resource = NULL;
}

static void
wl_tw_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version EINA_UNUSED, uint32_t id)
{
   struct wl_resource *res;

   if (!(res = wl_resource_create(client, &zwp_teamwork_interface, E_TW_VERSION, id)))
     {
        ERR("Could not create zwp_teamwork_interface interface");
        wl_client_post_no_memory(client);
        return;
     }

   tw_resource = res;
   wl_resource_set_implementation(res, &tw_interface, NULL, wl_tw_cb_unbind);
}

static void
wl_tw_link_complete(E_Client *ec, const char *uri)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   zwp_teamwork_send_completed_uri(tw_resource, ec->comp_data->surface, uri, 1);
}

static void
wl_tw_link_invalid(E_Client *ec, const char *uri)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   zwp_teamwork_send_completed_uri(tw_resource, ec->comp_data->surface, uri, 0);
}

static void
wl_tw_link_progress(E_Client *ec, const char *uri, uint32_t pct)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   zwp_teamwork_send_fetch_info(tw_resource, ec->comp_data->surface, uri, pct);
}

static void
wl_tw_link_downloading(E_Client *ec, const char *uri)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   zwp_teamwork_send_fetching_uri(tw_resource, ec->comp_data->surface, uri);
}

EINTERN Eina_Bool
wl_tw_init(void)
{
   tw_global = wl_global_create(e_comp_wl->wl.disp, &zwp_teamwork_interface, E_TW_VERSION,
                         NULL, wl_tw_cb_bind);
   if (!tw_global)
     {
        ERR("Could not add zwp_teamwork to wayland globals");
        return EINA_FALSE;
     }
   tw_signal_link_complete[E_PIXMAP_TYPE_WL] = wl_tw_link_complete;
   tw_signal_link_invalid[E_PIXMAP_TYPE_WL] = wl_tw_link_invalid;
   tw_signal_link_progress[E_PIXMAP_TYPE_WL] = wl_tw_link_progress;
   tw_signal_link_downloading[E_PIXMAP_TYPE_WL] = wl_tw_link_downloading;
   return EINA_TRUE;
}

EINTERN void
wl_tw_shutdown(void)
{
   E_FREE_FUNC(tw_resource, wl_resource_destroy);
   E_FREE_FUNC(tw_global, wl_global_destroy);
   tw_signal_link_complete[E_PIXMAP_TYPE_WL] = NULL;
   tw_signal_link_invalid[E_PIXMAP_TYPE_WL] = NULL;
   tw_signal_link_progress[E_PIXMAP_TYPE_WL] = NULL;
   tw_signal_link_downloading[E_PIXMAP_TYPE_WL] = NULL;
}
