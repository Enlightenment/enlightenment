#define E_COMP_WL
#include "e.h"


#include <uuid.h>
#include "e_comp_wl_screenshooter_server.h"
#include "session-recovery.h"
#include "www-protocol.h"

static void
_e_comp_wl_extensions_client_move_begin(void *d EINA_UNUSED, E_Client *ec)
{
   if (e_client_has_xwindow(ec) || e_object_is_del(E_OBJECT(ec))) return;

   if (ec->comp_data->www.surface)
     www_surface_send_start_drag(ec->comp_data->www.surface);
}

static void
_e_comp_wl_extensions_client_move_end(void *d EINA_UNUSED, E_Client *ec)
{
   if (e_client_has_xwindow(ec) || e_object_is_del(E_OBJECT(ec))) return;

   if (ec->comp_data->www.surface)
     www_surface_send_end_drag(ec->comp_data->www.surface);
}

static void
_e_comp_wl_session_recovery_get_uuid(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *surface)
{
   E_Client *ec;
   uuid_t u;
   char uuid[37];

   ec = wl_resource_get_user_data(surface);
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (ec->internal || ec->uuid) return;
   uuid_generate(u);
   uuid_unparse_lower(u, uuid);
   eina_stringshare_replace(&ec->uuid, uuid);
   zwp_e_session_recovery_send_create_uuid(resource, surface, uuid);
   if (ec->remember)
     e_remember_unuse(ec->remember);
   ec->remember = e_remember_new();
   e_remember_use(ec->remember);
   ec->remember->apply = E_REMEMBER_APPLY_POS | E_REMEMBER_APPLY_DESKTOP |
                         E_REMEMBER_APPLY_LAYER | E_REMEMBER_APPLY_ZONE | E_REMEMBER_APPLY_UUID;
   e_remember_update(ec);
}

static void
_e_comp_wl_session_recovery_set_uuid(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *surface, const char *uuid)
{
   E_Client *ec;
   E_Remember *rem;

   ec = wl_resource_get_user_data(surface);
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (ec->internal || ec->uuid) return; //FIXME: error
   eina_stringshare_replace(&ec->uuid, uuid);
   rem = e_remember_find_usable(ec);
   if ((!rem) || (rem == ec->remember)) return;
   if (ec->remember)
     e_remember_unuse(ec->remember);
   ec->remember = rem;
   e_remember_use(rem);
   e_remember_apply(rem, ec);
   ec->re_manage = 1;
}

static void
_e_comp_wl_session_recovery_destroy_uuid(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *surface, const char *uuid)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surface);
   if (!eina_streq(ec->uuid, uuid)) return; //FIXME: error
   eina_stringshare_replace(&ec->uuid, NULL);
   e_remember_unuse(ec->remember);
   e_remember_del(ec->remember);
   ec->remember = e_remember_find_usable(ec);
   if (!ec->remember) return;
   e_remember_use(ec->remember);
   e_remember_apply(ec->remember, ec);
}

static void
_e_comp_wl_screenshooter_cb_shoot(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource, struct wl_resource *buffer_resource)
{
   E_Comp_Wl_Output *output;
   E_Comp_Wl_Buffer *buffer;
   struct wl_shm_buffer *shm_buffer;
   int stride;
   void *pixels, *d;

   output = wl_resource_get_user_data(output_resource);
   buffer = e_comp_wl_buffer_get(buffer_resource);

   if (!buffer)
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   if ((buffer->w < output->w) || (buffer->h < output->h))
     {
        ERR("Buffer size less than output");
        /* send done with bad buffer error */
        return;
     }

   stride = buffer->w * sizeof(int);

   pixels = malloc(stride * buffer->h);
   if (!pixels)
     {
        /* send done with bad buffer error */
        ERR("Could not allocate space for destination");
        return;
     }

   if (e_comp_wl->extensions->screenshooter.read_pixels)
     e_comp_wl->extensions->screenshooter.read_pixels(output, pixels);

   shm_buffer = wl_shm_buffer_get(buffer->resource);
   if (!shm_buffer)
     {
        ERR("Could not get shm_buffer from resource");
        return;
     }

   stride = wl_shm_buffer_get_stride(shm_buffer);
   d = wl_shm_buffer_get_data(shm_buffer);
   if (!d)
     {
        ERR("Could not get buffer data");
        return;
     }

   wl_shm_buffer_begin_access(shm_buffer);
   memcpy(d, pixels, buffer->h * stride);
   wl_shm_buffer_end_access(shm_buffer);
   free(pixels);

   screenshooter_send_done(resource);
}

static void
_e_comp_wl_www_surface_del(struct wl_resource *res)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(res);
   if (!e_object_is_del(E_OBJECT(ec)))
     ec->comp_data->www.surface = NULL;
   ec->maximize_anims_disabled = 0;
   e_object_unref(E_OBJECT(ec));
}

static void
_e_comp_wl_www_surface_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct www_surface_interface _e_www_surface_interface =
{
   _e_comp_wl_www_surface_destroy,
};

static void
_e_comp_wl_www_cb_create(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface)
{
    struct wl_resource *ww;
    E_Client *ec;

    ec = wl_resource_get_user_data(surface);
    if (ec->comp_data->www.surface)
      {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Surface already created www_surface");
        return;
      }
    ww = wl_resource_create(client, &www_surface_interface, 1, id);
    if (!ww)
      {
         ERR("Could not create www_surface!");
         wl_client_post_no_memory(client);
         return;
      }
    wl_resource_set_implementation(ww, &_e_www_surface_interface, NULL, _e_comp_wl_www_surface_del);
    wl_resource_set_user_data(ww, ec);
    ec->comp_data->www.surface = ww;
    ec->comp_data->www.x = ec->x;
    ec->comp_data->www.y = ec->y;
    ec->maximize_anims_disabled = 1;
    e_object_ref(E_OBJECT(ec));
}

static const struct zwp_e_session_recovery_interface _e_session_recovery_interface =
{
   _e_comp_wl_session_recovery_get_uuid,
   _e_comp_wl_session_recovery_set_uuid,
   _e_comp_wl_session_recovery_destroy_uuid,
};

static const struct screenshooter_interface _e_screenshooter_interface =
{
   _e_comp_wl_screenshooter_cb_shoot
};

static const struct www_interface _e_www_interface =
{
   _e_comp_wl_www_cb_create
};


#define GLOBAL_BIND_CB(NAME, IFACE, ...) \
static void \
_e_comp_wl_##NAME##_cb_unbind(struct wl_resource *resource EINA_UNUSED) \
{ \
   e_comp_wl->extensions->NAME.global = NULL; \
} \
static void \
_e_comp_wl_##NAME##_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version EINA_UNUSED, uint32_t id) \
{ \
   struct wl_resource *res; \
\
   if (!(res = wl_resource_create(client, &(IFACE), 1, id))) \
     { \
        ERR("Could not create %s interface", #NAME);\
        wl_client_post_no_memory(client);\
        return;\
     }\
\
   e_comp_wl->extensions->NAME.global = res; \
   wl_resource_set_implementation(res, &_e_##NAME##_interface, NULL, _e_comp_wl_##NAME##_cb_unbind);\
}

GLOBAL_BIND_CB(session_recovery, zwp_e_session_recovery_interface)
GLOBAL_BIND_CB(screenshooter, screenshooter_interface)
GLOBAL_BIND_CB(www, www_interface)


#define GLOBAL_CREATE_OR_RETURN(NAME, IFACE) \
   do { \
      if (!wl_global_create(e_comp_wl->wl.disp, &(IFACE), 1, \
                            NULL, _e_comp_wl_##NAME##_cb_bind)) \
        { \
           ERR("Could not add %s to wayland globals", #IFACE); \
           return EINA_FALSE; \
        } \
   } while (0)


EINTERN Eina_Bool
e_comp_wl_extensions_init(void)
{
   /* try to add session_recovery to wayland globals */
   GLOBAL_CREATE_OR_RETURN(session_recovery, zwp_e_session_recovery_interface);
   GLOBAL_CREATE_OR_RETURN(screenshooter, screenshooter_interface);
   GLOBAL_CREATE_OR_RETURN(www, www_interface);

   e_client_hook_add(E_CLIENT_HOOK_MOVE_BEGIN, _e_comp_wl_extensions_client_move_begin, NULL);
   e_client_hook_add(E_CLIENT_HOOK_MOVE_END, _e_comp_wl_extensions_client_move_end, NULL);

   e_comp_wl->extensions = E_NEW(E_Comp_Wl_Extension_Data, 1);
   return EINA_TRUE;
}
