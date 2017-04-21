#include "e.h"

#ifdef HAVE_WAYLAND
# include "e_comp_wl.h"
# ifndef EGL_TEXTURE_FORMAT
#  define EGL_TEXTURE_FORMAT		0x3080
# endif
# ifndef EGL_TEXTURE_RGBA
#  define EGL_TEXTURE_RGBA		0x305E
# endif
# ifndef DRM_FORMAT_ARGB8888
#  define DRM_FORMAT_ARGB8888           0x34325241
#  define DRM_FORMAT_XRGB8888           0x34325258
# endif
#endif
#ifndef HAVE_WAYLAND_ONLY
# include "e_comp_x.h"
#endif

#include <sys/mman.h>

static Eina_Hash *pixmaps[2] = {NULL};
static Eina_Hash *aliases[2] = {NULL};

struct _E_Pixmap
{
   unsigned int refcount;
   unsigned int failures;

   E_Client *client;
   E_Pixmap_Type type;

   int64_t win;
   Ecore_Window parent;

   int w, h;

#ifndef HAVE_WAYLAND_ONLY
   void *image;
   void *visual;
   int ibpp, ibpl;
   unsigned int cmap;
   uint32_t pixmap;
   Eina_List *images_cache;
#endif

#ifdef HAVE_WAYLAND
   E_Comp_Wl_Buffer *buffer;
   E_Comp_Wl_Buffer *native_buffer;
   E_Comp_Wl_Buffer *held_buffer;
   struct wl_listener buffer_destroy_listener;
   struct wl_listener held_buffer_destroy_listener;
   void *data;
   Eina_Rectangle opaque;
   Eina_List *free_buffers;
#endif

   Eina_Bool usable : 1;
   Eina_Bool dirty : 1;
   Eina_Bool image_argb : 1;
};

#ifdef HAVE_WAYLAND

double wayland_time_base;

static void
_e_pixmap_cb_deferred_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Wl_Buffer *buffer;

   buffer = container_of(listener, E_Comp_Wl_Buffer, deferred_destroy_listener);
   buffer->discarding_pixmap->free_buffers = eina_list_remove(buffer->discarding_pixmap->free_buffers, buffer);
   buffer->discarding_pixmap = NULL;
}

static void 
_e_pixmap_cb_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Pixmap *cp;

   cp = container_of(listener, E_Pixmap, buffer_destroy_listener);
   cp->buffer = NULL;
   cp->buffer_destroy_listener.notify = NULL;
}

static void
_e_pixmap_cb_held_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Pixmap *cp;

   cp = container_of(listener, E_Pixmap, held_buffer_destroy_listener);
   cp->held_buffer = NULL;
   cp->held_buffer_destroy_listener.notify = NULL;
}
#endif

static void
_e_pixmap_clear(E_Pixmap *cp, Eina_Bool cache)
{
   cp->w = cp->h = 0;
   cp->image_argb = EINA_FALSE;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        if (cp->pixmap)
          {
             ecore_x_pixmap_free(cp->pixmap);
             cp->pixmap = 0;
             ecore_x_e_comp_pixmap_set(cp->parent ?: (Ecore_X_Window)cp->win, 0);
             e_pixmap_image_clear(cp, cache);
          }
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        e_pixmap_image_clear(cp, cache);
#endif
        break;
      default: 
        break;
     }
}

#ifndef HAVE_WAYLAND_ONLY
static void
_e_pixmap_image_clear_x(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eina_List *list = data;
   E_FREE_LIST(list, ecore_x_image_free);
}
#endif

#ifdef HAVE_WAYLAND
static void
_e_pixmap_wayland_buffer_release(E_Pixmap *cp, E_Comp_Wl_Buffer *buffer)
{
   if (!buffer) return;

   if (e_comp->rendering)
     {
        if (buffer->discarding_pixmap) return;

        buffer->discarding_pixmap = cp;
        buffer->deferred_destroy_listener.notify = _e_pixmap_cb_deferred_buffer_destroy;
        wl_signal_add(&buffer->destroy_signal, &buffer->deferred_destroy_listener);
        cp->free_buffers = eina_list_append(cp->free_buffers, buffer);
        return;
     }

   buffer->busy--;
   if (buffer->busy) return;

   wl_resource_queue_event(buffer->resource, WL_BUFFER_RELEASE);
   wl_shm_pool_unref(buffer->pool);
   buffer->pool = NULL;
}

static void
_e_pixmap_wl_buffers_free(E_Pixmap *cp)
{
   E_Comp_Wl_Buffer *b;

   EINA_LIST_FREE(cp->free_buffers, b)
     {
        wl_list_remove(&b->deferred_destroy_listener.link);
        b->deferred_destroy_listener.notify = NULL;
        _e_pixmap_wayland_buffer_release(cp, b);
        b->discarding_pixmap = NULL;
     }
}

static void
_e_pixmap_wayland_image_clear(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);

   if (!cp->held_buffer) return;
   if (!cp->held_buffer->pool) return;

   _e_pixmap_wayland_buffer_release(cp, cp->held_buffer);
   if (cp->held_buffer_destroy_listener.notify)
     {
        wl_list_remove(&cp->held_buffer_destroy_listener.link);
        cp->held_buffer_destroy_listener.notify = NULL;
     }

   cp->data = NULL;
   cp->held_buffer = NULL;
}
#endif

static void
_e_pixmap_free(E_Pixmap *cp)
{
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        if (!cp->images_cache) break;
        if (cp->client)
          evas_object_event_callback_add(cp->client->frame, EVAS_CALLBACK_FREE, _e_pixmap_image_clear_x, cp->images_cache);
        else
          {
             void *i;

             EINA_LIST_FREE(cp->images_cache, i)
               ecore_job_add((Ecore_Cb)ecore_x_image_free, i);
          }
        cp->images_cache = NULL;
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        _e_pixmap_wayland_image_clear(cp);
        if (cp->buffer_destroy_listener.notify)
          {
             wl_list_remove(&cp->buffer_destroy_listener.link);
             cp->buffer_destroy_listener.notify = NULL;
          }
#endif
        break;
      default:
        break;
     }
   _e_pixmap_clear(cp, 1);
   free(cp);
}

static E_Pixmap *
_e_pixmap_new(E_Pixmap_Type type)
{
   E_Pixmap *cp;

   cp = E_NEW(E_Pixmap, 1);
   cp->type = type;
   cp->w = cp->h = 0;
   cp->refcount = 1;
   cp->dirty = 1;
   return cp;
}

static E_Pixmap *
_e_pixmap_find(E_Pixmap_Type type, va_list *l)
{
#ifndef HAVE_WAYLAND_ONLY
   Ecore_X_Window xwin;
#endif
#ifdef HAVE_WAYLAND
   int64_t id;
#endif
   E_Pixmap *cp;
   
   if (!pixmaps[type]) return NULL;
   switch (type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        xwin = va_arg(*l, uint32_t);
        cp = eina_hash_find(aliases[type], &xwin);
        if (!cp) cp = eina_hash_find(pixmaps[type], &xwin);
        return cp;
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        id = va_arg(*l, int64_t);
        cp = eina_hash_find(aliases[type], &id);
        if (!cp) cp = eina_hash_find(pixmaps[type], &id);
        return cp;
#endif
        break;
      default: break;
     }
   return NULL;
}

E_API int
e_pixmap_free(E_Pixmap *cp)
{
   if (!cp) return 0;
   if (--cp->refcount) return cp->refcount;
   e_pixmap_image_clear(cp, EINA_FALSE);
   eina_hash_del_by_key(pixmaps[cp->type], &cp->win);
   return 0;
}

E_API E_Pixmap *
e_pixmap_ref(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);
   cp->refcount++;
   return cp;
}

E_API E_Pixmap *
e_pixmap_new(E_Pixmap_Type type, ...)
{
   E_Pixmap *cp = NULL;
   va_list l;
#ifndef HAVE_WAYLAND_ONLY
   Ecore_X_Window xwin;
#endif
#ifdef HAVE_WAYLAND
   int64_t id;
#endif

   EINA_SAFETY_ON_TRUE_RETURN_VAL((type != E_PIXMAP_TYPE_WL) && (type != E_PIXMAP_TYPE_X), NULL);
   va_start(l, type);
   switch (type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        xwin = va_arg(l, uint32_t);
        if (pixmaps[type])
          {
             cp = eina_hash_find(pixmaps[type], &xwin);
             if (cp)
               {
                  cp->refcount++;
                  break;
               }
          }
        else
          pixmaps[type] = eina_hash_int32_new((Eina_Free_Cb)_e_pixmap_free);
        cp = _e_pixmap_new(type);
        cp->win = xwin;
        eina_hash_add(pixmaps[type], &xwin, cp);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        id = va_arg(l, int64_t);
        if (pixmaps[type])
          {
             cp = eina_hash_find(pixmaps[type], &id);
             if (cp)
               {
                  cp->refcount++;
                  break;
               }
          }
        else
          {
             pixmaps[type] = eina_hash_int64_new((Eina_Free_Cb)_e_pixmap_free);
             wayland_time_base = ecore_loop_time_get();
          }
        cp = _e_pixmap_new(type);
        cp->win = id;
        eina_hash_add(pixmaps[type], &id, cp);
#endif
        break;
      default: break;
     }
   va_end(l);
   return cp;
}

E_API E_Pixmap_Type
e_pixmap_type_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 9999);
   return cp->type;
}

E_API void
e_pixmap_parent_window_set(E_Pixmap *cp, Ecore_Window win)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   if (cp->parent == win) return;

   e_pixmap_usable_set(cp, 0);
   e_pixmap_clear(cp);

   cp->parent = win;
}

E_API void
e_pixmap_visual_cmap_set(E_Pixmap *cp, void *visual, unsigned int cmap)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   if (cp->type != E_PIXMAP_TYPE_X) return;
#ifndef HAVE_WAYLAND_ONLY
   cp->visual = visual;
   cp->cmap = cmap;
#else
   (void) visual;
   (void) cmap;
#endif
}

E_API void *
e_pixmap_visual_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);
   if (cp->type != E_PIXMAP_TYPE_X) return NULL;
#ifndef HAVE_WAYLAND_ONLY
   return cp->visual;
#endif
   return NULL;
}

E_API uint32_t
e_pixmap_pixmap_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 0);
#ifndef HAVE_WAYLAND_ONLY
   if (e_pixmap_is_x(cp)) return cp->pixmap;
#endif
   return 0;
}

E_API void
e_pixmap_usable_set(E_Pixmap *cp, Eina_Bool set)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   cp->usable = !!set;
}

E_API Eina_Bool
e_pixmap_usable_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   return cp->usable;
}

E_API Eina_Bool
e_pixmap_dirty_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   return cp->dirty;
}

E_API void
e_pixmap_clear(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   _e_pixmap_clear(cp, 0);
   cp->dirty = EINA_TRUE;
}

E_API void
e_pixmap_dirty(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   cp->dirty = 1;
}

E_API Eina_Bool
e_pixmap_refresh(E_Pixmap *cp)
{
   Eina_Bool success = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);

   if (!cp->dirty) return EINA_TRUE;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        {
           uint32_t pixmap;
           int pw, ph;
           E_Comp_X_Client_Data *cd = NULL;

           if (!cp->usable)
             {
                cp->failures++;
                return EINA_FALSE;
             }
           pixmap = ecore_x_composite_name_window_pixmap_get(cp->parent ?: (Ecore_X_Window)cp->win);
           if (cp->client)
             {
                cd = (E_Comp_X_Client_Data*)cp->client->comp_data;
             }
           success = !!pixmap;
           if (!success) break;
           if (cd && cd->pw && cd->ph)
             {
                pw = cd->pw;
                ph = cd->ph;
             }
           else
             ecore_x_pixmap_geometry_get(pixmap, NULL, NULL, &pw, &ph);
           success = (pw > 0) && (ph > 0);
           if (success)
             {
                if ((pw != cp->w) || (ph != cp->h))
                  {
                     ecore_x_pixmap_free(cp->pixmap);
                     cp->pixmap = pixmap;
                     cp->w = pw, cp->h = ph;
                     ecore_x_e_comp_pixmap_set(cp->parent ?: (Ecore_X_Window)cp->win, cp->pixmap);
                     e_pixmap_image_clear(cp, 0);
                  }
                else
                  ecore_x_pixmap_free(pixmap);
             }
           else
             ecore_x_pixmap_free(pixmap);
        }
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        {
           E_Comp_Wl_Buffer *buffer = cp->buffer;
           struct wl_shm_buffer *shm_buffer;
           int format;

           cp->w = cp->h = 0;
           cp->image_argb = EINA_FALSE;

           if (!buffer) return EINA_FALSE;

           shm_buffer = buffer->shm_buffer;
           cp->w = buffer->w;
           cp->h = buffer->h;

           if (shm_buffer)
             format = wl_shm_buffer_get_format(shm_buffer);
           else if (buffer->dmabuf_buffer)
             format = buffer->dmabuf_buffer->attributes.format;
           else
             e_comp_wl->wl.glapi->evasglQueryWaylandBuffer(e_comp_wl->wl.gl, buffer->resource, EGL_TEXTURE_FORMAT, &format);

           switch (format)
             {
              case DRM_FORMAT_ARGB8888:
              case WL_SHM_FORMAT_ARGB8888:
              case EGL_TEXTURE_RGBA:
                cp->image_argb = EINA_TRUE;
                break;
              default:
                cp->image_argb = EINA_FALSE;
                break;
             }

           success = ((cp->w > 0) && (cp->h > 0));
        }
#endif
        break;
      default:
        break;
     }

   if (success)
     {
        cp->dirty = 0;
        cp->failures = 0;
     }
   else
     cp->failures++;
   return success;
}

E_API Eina_Bool
e_pixmap_size_changed(E_Pixmap *cp, int w, int h)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   if (cp->dirty) return EINA_TRUE;
   return (w != cp->w) || (h != cp->h);
}

E_API Eina_Bool
e_pixmap_size_get(E_Pixmap *cp, int *w, int *h)
{
   if (w) *w = 0;
   if (h) *h = 0;
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   if (w) *w = cp->w;
   if (h) *h = cp->h;
   return (cp->w > 0) && (cp->h > 0);
}

E_API unsigned int
e_pixmap_failures_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 0);
   return cp->failures;
}

E_API void
e_pixmap_client_set(E_Pixmap *cp, E_Client *ec)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   if (cp->client && ec) CRI("ACK!");
   cp->client = ec;
}

E_API E_Client *
e_pixmap_client_get(E_Pixmap *cp)
{
   if (!cp) return NULL;
   return cp->client;
}

E_API E_Pixmap *
e_pixmap_find(E_Pixmap_Type type, ...)
{
   va_list l;
   E_Pixmap *cp;

   va_start(l, type);
   cp = _e_pixmap_find(type, &l);
   va_end(l);
   return cp;
}

E_API E_Client *
e_pixmap_find_client(E_Pixmap_Type type, ...)
{
   va_list l;
   E_Pixmap *cp;

   va_start(l, type);
   cp = _e_pixmap_find(type, &l);
   va_end(l);
   return (!cp) ? NULL : cp->client;
}

E_API int64_t
e_pixmap_window_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 0);
   return cp->win;
}

E_API void *
e_pixmap_resource_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);
   if (cp->type != E_PIXMAP_TYPE_WL)
     {
        CRI("ACK");
        return NULL;
     }
#ifdef HAVE_WAYLAND
   return cp->buffer;
#endif
   return NULL;
}

E_API void
e_pixmap_resource_set(E_Pixmap *cp, void *resource)
{
   if ((!cp) || (cp->type != E_PIXMAP_TYPE_WL)) return;
#ifdef HAVE_WAYLAND
   if (cp->buffer == resource) return;

   if (cp->buffer)
     {
        cp->buffer->busy--;
        if (!cp->buffer->busy) wl_resource_queue_event(cp->buffer->resource, WL_BUFFER_RELEASE);
     }
   if (cp->buffer_destroy_listener.notify)
     {
        wl_list_remove(&cp->buffer_destroy_listener.link);
        cp->buffer_destroy_listener.notify = NULL;
     }
   cp->buffer = resource;
   if (!cp->buffer) return;
   cp->buffer_destroy_listener.notify = _e_pixmap_cb_buffer_destroy;
   wl_signal_add(&cp->buffer->destroy_signal,
                 &cp->buffer_destroy_listener);

   cp->buffer->busy++;
#else
   (void)resource;
#endif
}

E_API Ecore_Window
e_pixmap_parent_window_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 0);
   return cp->parent;
}

E_API Eina_Bool
e_pixmap_is_pixels(E_Pixmap *cp)
{
   switch (cp->type)
     {
        case E_PIXMAP_TYPE_X:
          return EINA_FALSE;
#ifdef HAVE_WAYLAND
        case E_PIXMAP_TYPE_WL:
          if (!cp->buffer) return EINA_TRUE;
          if (cp->buffer->shm_buffer) return EINA_TRUE;
          return EINA_FALSE;
#endif
        default:
          return EINA_TRUE;
     }
}

E_API Eina_Bool
e_pixmap_native_surface_init(E_Pixmap *cp, Evas_Native_Surface *ns)
{
   Eina_Bool ret = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ns, EINA_FALSE);

   ns->version = EVAS_NATIVE_SURFACE_VERSION;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        ns->type = EVAS_NATIVE_SURFACE_X11;
        ns->data.x11.visual = cp->visual;
        ns->data.x11.pixmap = cp->pixmap;
        ret = EINA_TRUE;
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        if (!cp->buffer) return EINA_FALSE;
        if (cp->buffer->dmabuf_buffer)
          {
             ns->type = EVAS_NATIVE_SURFACE_WL_DMABUF;
             ns->version = EVAS_NATIVE_SURFACE_VERSION;

             ns->data.wl_dmabuf.attr = &cp->buffer->dmabuf_buffer->attributes;
             ns->data.wl_dmabuf.resource = cp->buffer->resource;
             cp->native_buffer = cp->buffer;
             ret = EINA_TRUE;
          }
        else if (!cp->buffer->shm_buffer)
          {
             ns->type = EVAS_NATIVE_SURFACE_WL;
             ns->version = EVAS_NATIVE_SURFACE_VERSION;
             ns->data.wl.legacy_buffer = cp->buffer->resource;
             ret = EINA_TRUE;
          }
        else ret = EINA_FALSE;
#endif
        break;
      default:
        break;
     }

   return ret;
}

E_API void
e_pixmap_image_clear(E_Pixmap *cp, Eina_Bool cache)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);

   if (!cache)
     {
#ifndef HAVE_WAYLAND_ONLY
        if (e_pixmap_is_x(cp))
          if (!cp->image) return;
#endif
#ifdef HAVE_WAYLAND
        if (cp->type == E_PIXMAP_TYPE_WL)
          if (!cp->buffer) return;
#endif
     }

   cp->failures = 0;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        if (cache)
          {
             void *i;

             EINA_LIST_FREE(cp->images_cache, i)
               ecore_job_add((Ecore_Cb)ecore_x_image_free, i);
          }
        else
          {
             cp->images_cache = eina_list_append(cp->images_cache, cp->image);
             cp->image = NULL;
          }
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        _e_pixmap_wl_buffers_free(cp);
        if (cache)
          {
             E_Comp_Wl_Client_Data *cd;
             struct wl_resource *cb;
             Eina_List *free_list;

             if ((!cp->client) || (!cp->client->comp_data)) return;
             cd = (E_Comp_Wl_Client_Data *)cp->client->comp_data;

             /* The destroy callback will remove items from the frame list
              * so we move the list to a temporary before walking it here
              */
             free_list = cd->frames;
             cd->frames = NULL;
             EINA_LIST_FREE(free_list, cb)
               {
                  double t = ecore_loop_time_get() - wayland_time_base;
                  wl_callback_send_done(cb, t * 1000);
                  wl_resource_destroy(cb);
               }
          }
#endif
        break;
      default:
        break;
     }
}

E_API Eina_Bool
e_pixmap_image_refresh(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);

#ifndef HAVE_WAYLAND_ONLY
   if (cp->image) return EINA_TRUE;
#endif

   if (cp->dirty)
     {
        CRI("BUG! PIXMAP %p IS DIRTY!", cp);
        return EINA_FALSE;
     }

   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        if (cp->image) return EINA_TRUE;
        if ((!cp->visual) || (!cp->client->depth)) return EINA_FALSE;
        cp->image = ecore_x_image_new(cp->w, cp->h, cp->visual, cp->client->depth);
        if (cp->image)
          cp->image_argb = ecore_x_image_is_argb32_get(cp->image);
        return !!cp->image;
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        {
           if (cp->held_buffer == cp->buffer) return EINA_TRUE;

           if (cp->held_buffer) _e_pixmap_wayland_image_clear(cp);

           if (!cp->buffer->shm_buffer) return EINA_TRUE;

           cp->held_buffer = cp->buffer;
           if (!cp->held_buffer) return EINA_TRUE;

           cp->held_buffer->pool = wl_shm_buffer_ref_pool(cp->held_buffer->shm_buffer);
           cp->held_buffer->busy++;
           cp->data = wl_shm_buffer_get_data(cp->buffer->shm_buffer);

           cp->held_buffer_destroy_listener.notify = _e_pixmap_cb_held_buffer_destroy;
           wl_signal_add(&cp->held_buffer->destroy_signal,
                         &cp->held_buffer_destroy_listener);
           return EINA_TRUE;
        }
#endif
        break;
      default:
        break;
     }

   return EINA_FALSE;
}

E_API Eina_Bool
e_pixmap_image_exists(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);

#ifndef HAVE_WAYLAND_ONLY
   if (cp->type == E_PIXMAP_TYPE_X)
     return !!cp->image;
#endif
#ifdef HAVE_WAYLAND
   return (!!cp->data) ||
     (cp->buffer && ((e_comp->gl && (!cp->buffer->shm_buffer)) || cp->buffer->dmabuf_buffer));
#endif

   return EINA_FALSE;
}

E_API Eina_Bool
e_pixmap_image_is_argb(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);

   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        return cp->image && cp->image_argb;
#endif
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        if (cp->usable)
          return cp->image_argb;
        /* only cursors can be override in wayland */
        if (cp->client->override) return EINA_TRUE;
#endif
        default: break;
     }
   return EINA_FALSE;
}

E_API void *
e_pixmap_image_data_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);

   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        if (cp->image)
          return ecore_x_image_data_get(cp->image, &cp->ibpl, NULL, &cp->ibpp);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        return cp->data;
#endif
        break;
      default:
        break;
     }
   return NULL;
}

E_API Eina_Bool
e_pixmap_image_data_argb_convert(E_Pixmap *cp, void *pix, void *ipix, Eina_Rectangle *r, int stride)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);

   if (cp->image_argb) return EINA_TRUE;

   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
        if (cp->image_argb) return EINA_TRUE;
#ifndef HAVE_WAYLAND_ONLY
        return ecore_x_image_to_argb_convert(ipix, cp->ibpp, cp->ibpl,
                                             cp->cmap, cp->visual,
                                             r->x, r->y, r->w, r->h,
                                             pix, stride, r->x, r->y);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
        if (cp->image_argb) return EINA_TRUE;
        return EINA_FALSE;
      default:
        break;
     }
   return EINA_FALSE;
}

E_API Eina_Bool
e_pixmap_image_draw(E_Pixmap *cp, const Eina_Rectangle *r)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);

   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        if ((!cp->image) || (!cp->pixmap)) return EINA_FALSE;
        return ecore_x_image_get(cp->image, cp->pixmap, r->x, r->y, r->x, r->y, r->w, r->h);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
#endif
        (void) r;
        return EINA_TRUE;
      default:
        break;
     }
   return EINA_FALSE;
}

E_API void
e_pixmap_image_opaque_set(E_Pixmap *cp, int x, int y, int w, int h)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
#ifdef HAVE_WAYLAND
   EINA_RECTANGLE_SET(&cp->opaque, x, y, w, h);
#else
   (void)x;
   (void)y;
   (void)w;
   (void)h;
#endif
}

E_API void
e_pixmap_image_opaque_get(E_Pixmap *cp, int *x, int *y, int *w, int *h)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
#ifdef HAVE_WAYLAND
   if (x) *x = cp->opaque.x;
   if (y) *y = cp->opaque.y;
   if (w) *w = cp->opaque.w;
   if (h) *h = cp->opaque.h;
#else
   if (x) *x = 0;
   if (y) *y = 0;
   if (w) *w = 0;
   if (h) *h = 0;
#endif
}

E_API void
e_pixmap_alias(E_Pixmap *cp, E_Pixmap_Type type, ...)
{
   va_list l;
#ifndef HAVE_WAYLAND_ONLY
   Ecore_X_Window xwin;
#endif
#ifdef HAVE_WAYLAND
   int64_t id;
#endif

   va_start(l, type);
   switch (type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        xwin = va_arg(l, uint32_t);
        if (!aliases[type])
          aliases[type] = eina_hash_int32_new(NULL);
        eina_hash_set(aliases[type], &xwin, cp);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND
        id = va_arg(l, int64_t);
        if (!aliases[type])
          aliases[type] = eina_hash_int64_new(NULL);
        eina_hash_set(aliases[type], &id, cp);
#endif
        break;
      default: break;
     }
   va_end(l);
}

#ifdef HAVE_WAYLAND
E_API Eina_Bool
e_pixmap_dmabuf_test(struct linux_dmabuf_buffer *dmabuf)
{
   Evas_Native_Surface ns;
   Evas_Object *test;
   int size;
   void *data;

   if (e_comp->gl)
     {
        Eina_Bool ret;
        ns.type = EVAS_NATIVE_SURFACE_WL_DMABUF;
        ns.version = EVAS_NATIVE_SURFACE_VERSION;
        ns.data.wl_dmabuf.attr = &dmabuf->attributes;
        ns.data.wl_dmabuf.resource = NULL;
        test = evas_object_image_add(e_comp->evas);
        evas_object_image_native_surface_set(test, &ns);
        ret = evas_object_image_load_error_get(test) == EVAS_LOAD_ERROR_NONE;
        evas_object_del(test);
        if (!ns.data.wl_dmabuf.attr) return EINA_FALSE;
        return ret;
     }

   /* TODO: Software rendering for multi-plane formats */
   if (dmabuf->attributes.n_planes != 1) return EINA_FALSE;
   if (dmabuf->attributes.format != DRM_FORMAT_ARGB8888 &&
       dmabuf->attributes.format != DRM_FORMAT_XRGB8888) return EINA_FALSE;

   /* This is only legit for ARGB8888 */
   size = dmabuf->attributes.height * dmabuf->attributes.stride[0];
   data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf->attributes.fd[0], 0);
   if (data == MAP_FAILED) return EINA_FALSE;
   munmap(data, size);

   return EINA_TRUE;
}
#endif
