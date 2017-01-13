#define E_COMP_WL
#include "e.h"


#include <uuid.h>
#include "screenshooter-server-protocol.h"
#include "session-recovery-server-protocol.h"
#include "www-server-protocol.h"
#include "xdg-foreign-unstable-v1-server-protocol.h"

/* mutter uses 32, seems reasonable */
#define HANDLE_LEN 32

typedef struct Exported
{
   E_Client *ec;
   struct wl_resource *res;
   char handle[HANDLE_LEN + 1];
   Eina_List *imported;
} Exported;

typedef struct Imported
{
   /* child */
   E_Client *ec;
   struct wl_resource *res;
   Exported *ex;
} Imported;

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
   zwp_e_session_recovery_send_create_uuid(resource, surface, uuid);
   if (ec->sr_remember)
     {
        e_remember_unuse(ec->sr_remember);
        e_remember_del(ec->sr_remember);
     }
   eina_stringshare_replace(&ec->uuid, uuid);
   ec->sr_remember = e_remember_new();
   e_remember_use(ec->sr_remember);
   ec->sr_remember->apply = E_REMEMBER_APPLY_POS | E_REMEMBER_APPLY_SIZE | E_REMEMBER_APPLY_DESKTOP |
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
   rem = e_remember_sr_find(ec);
   if ((!rem) || (rem == ec->sr_remember)) return;
   if (ec->sr_remember)
     {
        e_remember_unuse(ec->sr_remember);
        e_remember_del(ec->sr_remember);
     }
   ec->sr_remember = rem;
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
   if (ec->sr_remember)
     {
        e_remember_unuse(ec->sr_remember);
        e_remember_del(ec->sr_remember);
     }
   ec->sr_remember = e_remember_sr_find(ec);
   if (!ec->sr_remember) return;
   e_remember_use(ec->sr_remember);
   e_remember_apply(ec->sr_remember, ec);
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

   zwp_screenshooter_send_done(resource);
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

///////////////////////////////////////////////////////

static void
_e_comp_wl_zxdg_exported_v1_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void _imported_v1_del(Imported *im);
static void _exported_del(void *data, Evas *e, Evas_Object *obj, void *event_info);

static void
_exported_v1_del(Exported *ex)
{
   while (ex->imported)
     {
        Imported *im = eina_list_data_get(ex->imported);

        zxdg_imported_v1_send_destroyed(im->res);
        _imported_v1_del(im);
     }
   evas_object_event_callback_del(ex->ec->frame, EVAS_CALLBACK_DEL, _exported_del);
   wl_resource_set_user_data(ex->res, NULL);
   eina_hash_del_by_key(e_comp_wl->extensions->zxdg_exporter_v1.surfaces, ex->handle);
   free(ex);
}

static void
_e_zxdg_exported_v1_del(struct wl_resource *resource)
{
   Exported *ex = wl_resource_get_user_data(resource);

   if (ex) _exported_v1_del(ex);
}

static void
_exported_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _exported_v1_del(data);
}

static void
_e_comp_wl_zxdg_exporter_v1_exporter_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct zxdg_exported_v1_interface _e_zxdg_exported_v1_interface =
{
   _e_comp_wl_zxdg_exported_v1_destroy,
};

static void
_e_comp_wl_zxdg_exporter_v1_export(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface)
{
   E_Client *ec = wl_resource_get_user_data(surface);
   Exported *ex;

   if ((!ec) || (!ec->comp_data->is_xdg_surface) || ec->comp_data->cursor)
     {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid role for exported surface");
        return;
     }

   ex = E_NEW(Exported, 1);
   ex->ec = ec;
   ex->res = wl_resource_create(client, &zxdg_exported_v1_interface, wl_resource_get_version(resource), id);
   wl_resource_set_implementation(ex->res, &_e_zxdg_exported_v1_interface, ex, _e_zxdg_exported_v1_del);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_DEL, _exported_del, ex);

   do
     {
        int n;

        for (n = 0; n < HANDLE_LEN; n++)
          {
             /* only printable ascii */
             ex->handle[n] = (rand() % (127 - 32)) + 32;
          }
     } while (eina_hash_find(e_comp_wl->extensions->zxdg_exporter_v1.surfaces, ex->handle));
   eina_hash_add(e_comp_wl->extensions->zxdg_exporter_v1.surfaces, ex->handle, ex);

   zxdg_exported_v1_send_handle(ex->res, ex->handle);
}


static void
_e_comp_wl_zxdg_imported_v1_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void _imported_del(void *data, Evas *e, Evas_Object *obj, void *event_info);

static void
_imported_v1_del(Imported *im)
{
   im->ex->imported = eina_list_remove(im->ex->imported, im);
   if (im->ec)
     {
        evas_object_event_callback_del(im->ec->frame, EVAS_CALLBACK_DEL, _imported_del);
        e_client_parent_set(im->ec, NULL);
     }
   if (im->res) wl_resource_set_user_data(im->res, NULL);
   free(im);
}

static void
_e_zxdg_imported_v1_del(struct wl_resource *resource)
{
   Imported *im = wl_resource_get_user_data(resource);

   if (im) _imported_v1_del(im);
}

static void
_imported_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Imported *im = data;

   im->ec = NULL;
}

static void
_e_comp_wl_zxdg_importer_v1_importer_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_comp_wl_zxdg_imported_v1_set_parent_of(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *surface_resource)
{
   Imported *im = wl_resource_get_user_data(resource);
   E_Client *ec = NULL;

   if (surface_resource) ec = wl_resource_get_user_data(surface_resource);

   if (ec && ((ec->netwm.type != E_WINDOW_TYPE_NORMAL) || (!ec->comp_data->is_xdg_surface)))
     {
        wl_resource_post_error(im->res, WL_DISPLAY_ERROR_INVALID_OBJECT,
          "xdg_imported.set_parent_of called with invalid surface");
        return;
     }

   if (im->ec)
     evas_object_event_callback_del(im->ec->frame, EVAS_CALLBACK_DEL, _imported_del);

   im->ec = ec;

   if (ec)
     {
        evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_DEL, _imported_del, im);
        e_client_parent_set(ec, im->ex->ec);
        ec->parent->modal = ec;
        ec->parent->lock_close = 1;
     }
}

static const struct zxdg_imported_v1_interface _e_zxdg_imported_v1_interface =
{
   _e_comp_wl_zxdg_imported_v1_destroy,
   _e_comp_wl_zxdg_imported_v1_set_parent_of,
};

static void
_e_comp_wl_zxdg_importer_v1_import(struct wl_client *client, struct wl_resource *resource, uint32_t id, const char *handle)
{
   Imported *im;
   Exported *ex;

   im = E_NEW(Imported, 1);
   im->res = wl_resource_create(client, &zxdg_imported_v1_interface, wl_resource_get_version(resource), id);
   wl_resource_set_implementation(im->res, &_e_zxdg_imported_v1_interface, NULL, _e_zxdg_imported_v1_del);

   ex = eina_hash_find(e_comp_wl->extensions->zxdg_exporter_v1.surfaces, handle);
   if ((!ex) || (!ex->ec->netwm.type))
     {
        zxdg_imported_v1_send_destroyed(im->res);
        free(im);
        return;
     }

   im->ex = ex;
   wl_resource_set_user_data(im->res, im);
   ex->imported = eina_list_append(ex->imported, im);
}

/////////////////////////////////////////////////////////

static const struct zwp_e_session_recovery_interface _e_session_recovery_interface =
{
   _e_comp_wl_session_recovery_get_uuid,
   _e_comp_wl_session_recovery_set_uuid,
   _e_comp_wl_session_recovery_destroy_uuid,
};

static const struct zwp_screenshooter_interface _e_screenshooter_interface =
{
   _e_comp_wl_screenshooter_cb_shoot
};

static const struct www_interface _e_www_interface =
{
   _e_comp_wl_www_cb_create
};

static const struct zxdg_exporter_v1_interface _e_zxdg_exporter_v1_interface =
{
   _e_comp_wl_zxdg_exporter_v1_exporter_destroy,
   _e_comp_wl_zxdg_exporter_v1_export,
};

static const struct zxdg_importer_v1_interface _e_zxdg_importer_v1_interface =
{
   _e_comp_wl_zxdg_importer_v1_importer_destroy,
   _e_comp_wl_zxdg_importer_v1_import,
};


#define GLOBAL_BIND_CB(NAME, IFACE, ...) \
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
   wl_resource_set_implementation(res, &_e_##NAME##_interface, NULL, NULL);\
}

GLOBAL_BIND_CB(session_recovery, zwp_e_session_recovery_interface)
GLOBAL_BIND_CB(screenshooter, zwp_screenshooter_interface)
GLOBAL_BIND_CB(www, www_interface)
GLOBAL_BIND_CB(zxdg_exporter_v1, zxdg_exporter_v1_interface)
GLOBAL_BIND_CB(zxdg_importer_v1, zxdg_importer_v1_interface)


#define GLOBAL_CREATE_OR_RETURN(NAME, IFACE, VERSION) \
   do { \
      struct wl_global *global; \
\
      global = wl_global_create(e_comp_wl->wl.disp, &(IFACE), VERSION, \
                                NULL, _e_comp_wl_##NAME##_cb_bind); \
      if (!global) \
        { \
           ERR("Could not add %s to wayland globals", #IFACE); \
           return EINA_FALSE; \
        } \
      e_comp_wl->extensions->NAME.global = global; \
   } while (0)

static Eina_Bool
_dmabuf_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Wl2_Event_Sync_Done *ev;

   ev = event;
   if (ev->display != e_comp_wl->wl.client_disp)
     return ECORE_CALLBACK_PASS_ON;

   /* Proxy not supported yet */
   if (!(e_comp_wl->dmabuf_disable || e_comp_wl->dmabuf_proxy))
     linux_dmabuf_setup(e_comp_wl->wl.disp);

   return ECORE_CALLBACK_PASS_ON;
}

EINTERN Eina_Bool
e_comp_wl_extensions_init(void)
{
   e_comp_wl->extensions = E_NEW(E_Comp_Wl_Extension_Data, 1);

   /* try to add session_recovery to wayland globals */
   GLOBAL_CREATE_OR_RETURN(session_recovery, zwp_e_session_recovery_interface, 1);
   GLOBAL_CREATE_OR_RETURN(screenshooter, zwp_screenshooter_interface, 1);
   GLOBAL_CREATE_OR_RETURN(www, www_interface, 1);
   GLOBAL_CREATE_OR_RETURN(zxdg_exporter_v1, zxdg_exporter_v1_interface, 1);
   e_comp_wl->extensions->zxdg_exporter_v1.surfaces = eina_hash_string_superfast_new(NULL);
   GLOBAL_CREATE_OR_RETURN(zxdg_importer_v1, zxdg_importer_v1_interface, 1);

   ecore_event_handler_add(ECORE_WL2_EVENT_SYNC_DONE, _dmabuf_add, NULL);

   e_client_hook_add(E_CLIENT_HOOK_MOVE_BEGIN, _e_comp_wl_extensions_client_move_begin, NULL);
   e_client_hook_add(E_CLIENT_HOOK_MOVE_END, _e_comp_wl_extensions_client_move_end, NULL);

   return EINA_TRUE;
}
