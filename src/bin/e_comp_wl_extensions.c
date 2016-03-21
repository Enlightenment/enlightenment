#define E_COMP_WL
#include "e.h"

#include "e_comp_wl_screenshooter_server.h"
#include "session-recovery-server-protocol.h"

static void
_e_comp_wl_sr_cb_provide_uuid(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, const char *uuid)
{
   DBG("Provide UUID callback called for UUID: %s", uuid);
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

static const struct zwp_e_session_recovery_interface _e_session_recovery_interface =
{
   _e_comp_wl_sr_cb_provide_uuid,
};

static const struct screenshooter_interface _e_screenshooter_interface =
{
   _e_comp_wl_screenshooter_cb_shoot
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

   e_comp_wl->extensions = E_NEW(E_Comp_Wl_Extension_Data, 1);
   return EINA_TRUE;
}
