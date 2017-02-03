#define E_COMP_WL
#include "e.h"


#include <uuid.h>
#include "screenshooter-server-protocol.h"
#include "session-recovery-server-protocol.h"
#include "www-server-protocol.h"
#include "xdg-foreign-unstable-v1-server-protocol.h"
#include "relative-pointer-unstable-v1-server-protocol.h"
#include "pointer-constraints-unstable-v1-server-protocol.h"

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

typedef struct Constraint
{
   E_Client *ec;
   struct wl_resource *res;
   struct wl_resource *seat;
   struct wl_resource *surface;
   Eina_Tiler *region;
   Eina_Tiler *pending;
   Evas_Point *pending_xy;
   Evas_Point *pointer_xy;
   Eina_Bool lock : 1; // if not lock, confine
   Eina_Bool persistent : 1;
   Eina_Bool active : 1;
} Constraint;

static Eina_List *active_constraints;

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

static void
_e_comp_wl_zwp_relative_pointer_manager_v1_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_relative_pointer_destroy(struct wl_resource *resource)
{
   e_comp_wl->extensions->zwp_relative_pointer_manager_v1.resources =
     eina_list_remove(e_comp_wl->extensions->zwp_relative_pointer_manager_v1.resources, resource);
}

static void
_e_comp_wl_zwp_relative_pointer_v1_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}


static const struct zwp_relative_pointer_v1_interface _e_zwp_relative_pointer_v1_interface =
{
   _e_comp_wl_zwp_relative_pointer_v1_destroy,
};

static void
_e_comp_wl_zwp_relative_pointer_manager_v1_get_relative_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *pointer)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &zwp_relative_pointer_v1_interface, wl_resource_get_version(resource), id);
   wl_resource_set_implementation(res, &_e_zwp_relative_pointer_v1_interface, pointer, _relative_pointer_destroy);
   e_comp_wl->extensions->zwp_relative_pointer_manager_v1.resources =
     eina_list_append(e_comp_wl->extensions->zwp_relative_pointer_manager_v1.resources, res);
}

/////////////////////////////////////////////////////////

static void
_e_comp_wl_zwp_pointer_constraints_v1_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_constraint_destroy(struct wl_resource *resource)
{
   Eina_Hash *constraints;
   Constraint *c = wl_resource_get_user_data(resource);

   constraints = eina_hash_find(e_comp_wl->extensions->zwp_pointer_constraints_v1.constraints, &c->seat);
   if (constraints)
     eina_hash_del_by_key(constraints, &c->surface);
   if (c->active)
     {
        active_constraints = eina_list_remove(active_constraints, c);
        if (c->lock && c->pointer_xy)
          ecore_evas_pointer_warp(e_comp->ee, c->ec->client.x + c->pointer_xy->x, c->ec->client.y + c->pointer_xy->y);
     }
   if (c->ec)
     {
        if (c->ec->comp_data->constraints)
          c->ec->comp_data->constraints = eina_list_remove(c->ec->comp_data->constraints, c);
     }
   eina_tiler_free(c->pending);
   eina_tiler_free(c->region);
   free(c->pointer_xy);
   free(c->pending_xy);
   free(c);
}

static void
_constraint_set_pending(Constraint *c)
{
   if (c->ec->comp_data->constraints)
     c->ec->comp_data->constraints = eina_list_remove(c->ec->comp_data->constraints, c);
   c->ec->comp_data->constraints = eina_list_append(c->ec->comp_data->constraints, c);
}

static void
_constraint_set_region(struct wl_resource *resource, struct wl_resource *region)
{
   Constraint *c = wl_resource_get_user_data(resource);
   Eina_Tiler *r = NULL;

   if (region) r = wl_resource_get_user_data(region);
   else E_FREE_FUNC(c->pending, eina_tiler_free);

   if (c->pending)
     eina_tiler_clear(c->pending);
   else
     {
        c->pending = eina_tiler_new(65535, 65535);
        eina_tiler_tile_size_set(c->pending, 1, 1);
     }
   if (r)
     eina_tiler_union(c->pending, r);
   _constraint_set_pending(c);
}

static void
_e_comp_wl_locked_pointer_v1_set_region(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region)
{
   _constraint_set_region(resource, region);
}

static void
_e_comp_wl_locked_pointer_v1_set_cursor_position_hint(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
   Constraint *c = wl_resource_get_user_data(resource);

   if (!c->pending_xy)
     c->pending_xy = E_NEW(Evas_Point, 1);
   c->pending_xy->x = wl_fixed_to_int(surface_x);
   c->pending_xy->y = wl_fixed_to_int(surface_y);
   _constraint_set_pending(c);
}

static const struct zwp_locked_pointer_v1_interface _e_comp_wl_locked_pointer_v1_interface =
{
   _e_comp_wl_zwp_pointer_constraints_v1_destroy,
   _e_comp_wl_locked_pointer_v1_set_cursor_position_hint,
   _e_comp_wl_locked_pointer_v1_set_region,
};


static void
_e_comp_wl_confined_pointer_v1_set_region(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region)
{
   _constraint_set_region(resource, region);
}

static const struct zwp_confined_pointer_v1_interface _e_comp_wl_confined_pointer_v1_interface =
{
   _e_comp_wl_zwp_pointer_constraints_v1_destroy,
   _e_comp_wl_confined_pointer_v1_set_region,
};

static Constraint *
do_constraint(const struct wl_interface *interface, const void *impl, struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface, struct wl_resource *pointer, struct wl_resource *region, uint32_t lifetime)
{
   struct wl_resource *res, *seat;
   Eina_Hash *constraints;
   Constraint *c;

   seat = wl_resource_get_user_data(pointer);
   constraints = eina_hash_find(e_comp_wl->extensions->zwp_pointer_constraints_v1.constraints, &seat);
   if (constraints)
     {
        c = eina_hash_find(constraints, &surface);
        if (c)
          {
             wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
               "constraint already exists for requested seat+surface");
             return NULL;
          }
     }
   else
     {
        constraints = eina_hash_pointer_new(NULL);
        eina_hash_add(e_comp_wl->extensions->zwp_pointer_constraints_v1.constraints, &seat, constraints);
     }
   c = E_NEW(Constraint, 1);
   c->seat = seat;
   c->surface = surface;
   c->persistent = lifetime == ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT;
   c->ec = wl_resource_get_user_data(surface);
   c->res = res = wl_resource_create(client, interface, wl_resource_get_version(resource), id);
   wl_resource_set_implementation(res, impl, pointer, _constraint_destroy);
   wl_resource_set_user_data(res, c);
   _constraint_set_region(res, region);
   return c;
}

static void
_e_comp_wl_zwp_pointer_constraints_v1_lock_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface, struct wl_resource *pointer, struct wl_resource *region, uint32_t lifetime)
{
   Constraint *c;

   c = do_constraint(&zwp_locked_pointer_v1_interface, &_e_comp_wl_locked_pointer_v1_interface,
     client, resource, id, surface, pointer, region, lifetime);
   if (c)
     c->lock = 1;
}

static void
_e_comp_wl_zwp_pointer_constraints_v1_confine_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface, struct wl_resource *pointer, struct wl_resource *region, uint32_t lifetime)
{
   do_constraint(&zwp_confined_pointer_v1_interface, &_e_comp_wl_confined_pointer_v1_interface,
     client, resource, id, surface, pointer, region, lifetime);
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

static const struct zwp_relative_pointer_manager_v1_interface _e_zwp_relative_pointer_manager_v1_interface =
{
   _e_comp_wl_zwp_relative_pointer_manager_v1_destroy,
   _e_comp_wl_zwp_relative_pointer_manager_v1_get_relative_pointer,
};

static const struct zwp_pointer_constraints_v1_interface _e_zwp_pointer_constraints_v1_interface =
{
   _e_comp_wl_zwp_pointer_constraints_v1_destroy,
   _e_comp_wl_zwp_pointer_constraints_v1_lock_pointer,
   _e_comp_wl_zwp_pointer_constraints_v1_confine_pointer,
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
GLOBAL_BIND_CB(zwp_relative_pointer_manager_v1, zwp_relative_pointer_manager_v1_interface)
GLOBAL_BIND_CB(zwp_pointer_constraints_v1, zwp_pointer_constraints_v1_interface)


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
   GLOBAL_CREATE_OR_RETURN(zwp_relative_pointer_manager_v1, zwp_relative_pointer_manager_v1_interface, 1);
   GLOBAL_CREATE_OR_RETURN(zwp_pointer_constraints_v1, zwp_pointer_constraints_v1_interface, 1);
   e_comp_wl->extensions->zwp_pointer_constraints_v1.constraints = eina_hash_pointer_new(NULL);

   ecore_event_handler_add(ECORE_WL2_EVENT_SYNC_DONE, _dmabuf_add, NULL);

   e_client_hook_add(E_CLIENT_HOOK_MOVE_BEGIN, _e_comp_wl_extensions_client_move_begin, NULL);
   e_client_hook_add(E_CLIENT_HOOK_MOVE_END, _e_comp_wl_extensions_client_move_end, NULL);

   return EINA_TRUE;
}

E_API void
e_comp_wl_extension_relative_motion_event(uint64_t time_usec, double dx, double dy, double dx_unaccel, double dy_unaccel)
{
   Eina_List *l;
   struct wl_resource *res;
   E_Client *focused;
   struct wl_client *wc;
   uint32_t hi, lo;

   focused = e_client_focused_get();
   if ((!focused) || e_object_is_del(E_OBJECT(focused))) return;

   wc = wl_resource_get_client(focused->comp_data->surface);

   hi = time_usec >> 32;
   lo = (uint32_t)time_usec;

   EINA_LIST_FOREACH(e_comp_wl->extensions->zwp_relative_pointer_manager_v1.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        zwp_relative_pointer_v1_send_relative_motion(res, hi, lo, wl_fixed_from_double(dx),
          wl_fixed_from_double(dy), wl_fixed_from_double(dx_unaccel), wl_fixed_from_double(dy_unaccel));
     }
}

E_API void
e_comp_wl_extension_pointer_constraints_commit(E_Client *ec)
{
   Eina_List *l;
   Constraint *c;

   EINA_LIST_FOREACH(ec->comp_data->constraints, l, c)
     {
        if (c->pending)
          {
             eina_tiler_free(c->region);
             c->region = c->pending;
             c->pending = NULL;
          }
        if (c->pending_xy)
          {
             if (c->pointer_xy)
               free(c->pointer_xy);
             c->pointer_xy = c->pending_xy;
             c->pending_xy = NULL;
          }
     }
}

static Eina_Bool
_inside_tiler(Eina_Tiler *r, Eina_Bool active, int x, int y, int px, int py, int *ax, int *ay)
{
   Eina_Iterator *it;
   Eina_Rectangle *rect;
   Eina_Bool ret = EINA_FALSE;
   Eina_Rectangle prect, arect;
   Eina_Bool cur = EINA_FALSE, prev = EINA_FALSE;

   if (!r) return EINA_TRUE;
   it = eina_tiler_iterator_new(r);
   if (!it) return EINA_TRUE;

   EINA_ITERATOR_FOREACH(it, rect)
     {
        Eina_Bool found = EINA_FALSE;
        if (active && eina_rectangle_coords_inside(rect, px, py))
          {
             found = prev = EINA_TRUE;
             prect = *rect;
             if (cur) break;
          }
        if (eina_rectangle_coords_inside(rect, x, y))
          {
             if (active)
               {
                  cur = EINA_TRUE;
                  arect = *rect;
                  if (found) ret = EINA_TRUE;
                  if (prev) break;
               }
             else
               ret = EINA_TRUE;
             if (!active) break;
          }
     }
   eina_iterator_free(it);
   if ((!ret) && cur && prev)
     {
        int dx, dy;

        dx = abs(x - px);
        dy = abs(y - py);
        if ((!dx) || (!dy))
          {
             /* line motion along a single axis: check for adjacent confine rects */
             if (dx)
               ret = (arect.x + arect.w == prect.x) || (prect.x + prect.w == arect.x);
             else
               ret = (arect.y + arect.h == prect.y) || (prect.y + prect.h == arect.y);
          }
        else
          {
             /* check for completely contiguous regions over entire motion vector
              * use rect of vector points and check for overlap
              */
             Eina_Tiler *a;
             int w, h;
             unsigned int size, usize = 0;

             eina_tiler_area_size_get(r, &w, &h);
             a = eina_tiler_new(w, h);
             eina_tiler_tile_size_set(a, 1, 1);
             eina_tiler_rect_add(a, &arect);
             eina_tiler_rect_add(a, &prect);
             eina_tiler_rect_del(a, &(Eina_Rectangle){MIN(x, px), MIN(y, py), dx, dy});
             size = (arect.w * arect.h) + (prect.w * prect.h);
             it = eina_tiler_iterator_new(a);
             EINA_ITERATOR_FOREACH(it, rect)
               usize += rect->w * rect->h;
             ret = size - dx * dy == usize;
             eina_iterator_free(it);
             eina_tiler_free(a);
          }
     }
   if (prev && (!ret))
     {
        if (!eina_rectangle_xcoord_inside(&prect, x))
          {
             if (x < prect.x)
               *ax = prect.x;
             else
               *ax = prect.x + prect.w - 1;
             if (*ax == px)
               {
                  if (eina_rectangle_ycoord_inside(&prect, y))
                    *ay = y;
                  else
                    {
                       _inside_tiler(r, active, *ax, y, px, py, ax, ay);
                    }
               }
             else
               *ay = lround(((y - py) / (double)(x - px)) * (*ax - x)) + y;
          }
        else
          {
             if (y < prect.y)
               *ay = prect.y;
             else
               *ay = prect.y + prect.h - 1;
             if (*ay == py)
               *ax = x;
             else
               *ax = lround(((double)(x - px) / (y - py)) * (*ay - y)) + x;
          }
     }

   return ret;
}

E_API Eina_Bool
e_comp_wl_extension_pointer_constraints_update(E_Client *ec, int x, int y)
{
   Eina_List *l, *ll;
   Constraint *c;
   Eina_Bool inside;
   int px, py;

   inside = e_comp_object_coords_inside_input_area(ec->frame, x, y);
   evas_pointer_canvas_xy_get(e_comp->evas, &px, &py);

   /* if constraint is active, check prev canvas coords and lock if */

   EINA_LIST_FOREACH_SAFE(ec->comp_data->constraints, l, ll, c)
     {
        int ax = px - ec->client.x, ay = py - ec->client.y;
        Eina_Bool inside_region = _inside_tiler(c->region, c->active, x - ec->client.x, y - ec->client.y,
          px - ec->client.x, py - ec->client.y, &ax, &ay);
        if ((!c->active) && inside && inside_region)
          {
             c->active = 1;
             active_constraints = eina_list_append(active_constraints, c);
             if (c->lock)
               zwp_locked_pointer_v1_send_locked(c->res);
             else
               zwp_confined_pointer_v1_send_confined(c->res);
          }
        if (!c->active) continue;
        if (c->lock || ((!inside) || (!inside_region)))
          {
             ecore_evas_pointer_warp(e_comp->ee, ax + ec->client.x, ay + ec->client.y);
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

E_API void
e_comp_wl_extension_pointer_unconstrain(E_Client *ec)
{
   Constraint *c;

   if (ec)
     {
        /* deleting client */
        EINA_LIST_FREE(ec->comp_data->constraints, c)
          {
             c->active = EINA_FALSE;
             if (c->lock)
               zwp_locked_pointer_v1_send_unlocked(c->res);
             else
               zwp_confined_pointer_v1_send_unconfined(c->res);
             active_constraints = eina_list_remove(active_constraints, c);
             c->ec = NULL;
          }
     }
   EINA_LIST_FREE(active_constraints, c)
     {
        c->active = EINA_FALSE;
        if (c->lock)
          zwp_locked_pointer_v1_send_unlocked(c->res);
        else
          zwp_confined_pointer_v1_send_unconfined(c->res);
     }
}
