#include "e.h"

#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
# include "e_comp_wl.h"
#endif
#ifndef HAVE_WAYLAND_ONLY
# include "e_comp_x.h"
#endif

static Eina_Hash *pixmaps[2] = {NULL};

struct _E_Pixmap
{
   unsigned int refcount;
   unsigned int failures;

   E_Client *client;
   E_Pixmap_Type type;

   uint64_t win;
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

#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
   E_Comp_Wl_Buffer_Ref buffer_ref;
   struct wl_listener buffer_destroy_listener;
   void *data;
   Eina_Rectangle opaque;
#endif

   Eina_Bool usable : 1;
   Eina_Bool dirty : 1;
   Eina_Bool image_argb : 1;
};

#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
static void 
_e_pixmap_cb_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Pixmap *cp;

   cp = container_of(listener, E_Pixmap, buffer_destroy_listener);
   cp->data = NULL;
   cp->buffer_destroy_listener.notify = NULL;
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
             ecore_x_e_comp_pixmap_set(cp->parent ?: cp->win, 0);
             e_pixmap_image_clear(cp, cache);
          }
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        if (cp->buffer_destroy_listener.notify)
          {
             wl_list_remove(&cp->buffer_destroy_listener.link);
             cp->buffer_destroy_listener.notify = NULL;
          }

        e_comp_wl_buffer_reference(&cp->buffer_ref, NULL);

        (void)cache;
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
   E_FREE_LIST(data, ecore_x_image_free);
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
   uint64_t id;
#endif
   
   if (!pixmaps[type]) return NULL;
   switch (type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        xwin = va_arg(*l, uint32_t);
        return eina_hash_find(pixmaps[type], &xwin);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        id = va_arg(*l, uint64_t);
        return eina_hash_find(pixmaps[type], &id);
#endif
        break;
      default: break;
     }
   return NULL;
}

EAPI int
e_pixmap_free(E_Pixmap *cp)
{
   if (!cp) return 0;
   if (--cp->refcount) return cp->refcount;
   e_pixmap_image_clear(cp, EINA_FALSE);
   if (cp->parent) eina_hash_set(pixmaps[cp->type], &cp->parent, NULL);
   eina_hash_del_by_key(pixmaps[cp->type], &cp->win);
   return 0;
}

EAPI E_Pixmap *
e_pixmap_ref(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);
   cp->refcount++;
   return cp;
}

EAPI E_Pixmap *
e_pixmap_new(E_Pixmap_Type type, ...)
{
   E_Pixmap *cp = NULL;
   va_list l;
#ifndef HAVE_WAYLAND_ONLY
   Ecore_X_Window xwin;
#endif
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
   uint64_t id;
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        id = va_arg(l, uint64_t);
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
          pixmaps[type] = eina_hash_int64_new((Eina_Free_Cb)_e_pixmap_free);
        cp = _e_pixmap_new(type);
        cp->win = id;
        eina_hash_add(pixmaps[type], &id, cp);
#endif
        break;
     }
   va_end(l);
   return cp;
}

EAPI E_Pixmap_Type
e_pixmap_type_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 9999);
   return cp->type;
}

EAPI void
e_pixmap_parent_window_set(E_Pixmap *cp, Ecore_Window win)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   if (cp->parent == win) return;

   e_pixmap_usable_set(cp, 0);
   e_pixmap_clear(cp);

   if (cp->parent)
     eina_hash_set(pixmaps[cp->type], &cp->parent, NULL);
   cp->parent = win;
   if (win) eina_hash_add(pixmaps[cp->type], &win, cp);
}

EAPI void
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

EAPI void *
e_pixmap_visual_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);
   if (cp->type != E_PIXMAP_TYPE_X) return NULL;
#ifndef HAVE_WAYLAND_ONLY
   return cp->visual;
#endif
   return NULL;
}

EAPI void
e_pixmap_usable_set(E_Pixmap *cp, Eina_Bool set)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   cp->usable = !!set;
}

EAPI Eina_Bool
e_pixmap_usable_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   return cp->usable;
}

EAPI Eina_Bool
e_pixmap_dirty_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   return cp->dirty;
}

EAPI void
e_pixmap_clear(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   _e_pixmap_clear(cp, 0);
   cp->dirty = EINA_TRUE;
}

EAPI void
e_pixmap_dirty(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   cp->dirty = 1;
}

EAPI Eina_Bool
e_pixmap_refresh(E_Pixmap *cp)
{
   Eina_Bool success = EINA_FALSE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);

   if (!cp->usable)
     {
        cp->failures++;
        return EINA_FALSE;
     }
   if (!cp->dirty) return EINA_TRUE;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef HAVE_WAYLAND_ONLY
        {
           uint32_t pixmap;
           int pw, ph;
           E_Comp_X_Client_Data *cd = NULL;

           pixmap = ecore_x_composite_name_window_pixmap_get(cp->parent ?: cp->win);
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
                     ecore_x_e_comp_pixmap_set(cp->parent ?: cp->win, cp->pixmap);
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        success = ((cp->w > 0) && (cp->h > 0));
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

EAPI Eina_Bool
e_pixmap_size_changed(E_Pixmap *cp, int w, int h)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   if (cp->dirty) return EINA_TRUE;
   return (w != cp->w) || (h != cp->h);
}

EAPI Eina_Bool
e_pixmap_size_get(E_Pixmap *cp, int *w, int *h)
{
   if (w) *w = 0;
   if (h) *h = 0;
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   if (w) *w = cp->w;
   if (h) *h = cp->h;
   return (cp->w > 0) && (cp->h > 0);
}

EAPI unsigned int
e_pixmap_failures_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 0);
   return cp->failures;
}

EAPI void
e_pixmap_client_set(E_Pixmap *cp, E_Client *ec)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
   cp->client = ec;
}

EAPI E_Client *
e_pixmap_client_get(E_Pixmap *cp)
{
   if (!cp) return NULL;
   return cp->client;
}

EAPI E_Pixmap *
e_pixmap_find(E_Pixmap_Type type, ...)
{
   va_list l;
   E_Pixmap *cp;

   va_start(l, type);
   cp = _e_pixmap_find(type, &l);
   va_end(l);
   return cp;
}

EAPI E_Client *
e_pixmap_find_client(E_Pixmap_Type type, ...)
{
   va_list l;
   E_Pixmap *cp;

   va_start(l, type);
   cp = _e_pixmap_find(type, &l);
   va_end(l);
   return (!cp) ? NULL : cp->client;
}

EAPI uint64_t
e_pixmap_window_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 0);
   return cp->win;
}

EAPI void *
e_pixmap_resource_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);
   if (cp->type != E_PIXMAP_TYPE_WL)
     {
        CRI("ACK");
        return NULL;
     }
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
   return cp->buffer_ref.buffer;
#endif
   return NULL;
}

EAPI void
e_pixmap_resource_set(E_Pixmap *cp, void *resource)
{
   if ((!cp) || (cp->type != E_PIXMAP_TYPE_WL)) return;
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
     {
        E_Comp_Wl_Buffer *buffer;
        struct wl_shm_buffer *shm_buffer;

        buffer = (E_Comp_Wl_Buffer *)resource;
        e_comp_wl_buffer_reference(&cp->buffer_ref, buffer);

        if (cp->buffer_destroy_listener.notify)
          {
             wl_list_remove(&cp->buffer_destroy_listener.link);
             cp->buffer_destroy_listener.notify = NULL;
          }

        cp->w = cp->h = 0;
        cp->image_argb = EINA_FALSE;

        if (!buffer) return;

        if (!(shm_buffer = wl_shm_buffer_get(buffer->resource)))
          {
             WRN("Cannot get shm buffer from buffer resource");
             e_comp_wl_buffer_reference(&cp->buffer_ref, NULL);
             return;
          }

        buffer->shm_buffer = shm_buffer;
        cp->w = buffer->w;
        cp->h = buffer->h;
        /* buffer->w = cp->w = wl_shm_buffer_get_width(shm_buffer); */
        /* buffer->h = cp->h = wl_shm_buffer_get_height(shm_buffer); */

        switch (wl_shm_buffer_get_format(shm_buffer))
          {
           case WL_SHM_FORMAT_ARGB8888:
             cp->image_argb = EINA_TRUE;
             break;
           default:
             cp->image_argb = EINA_FALSE;
             break;
          }

        cp->data = wl_shm_buffer_get_data(shm_buffer);

        cp->buffer_destroy_listener.notify = _e_pixmap_cb_buffer_destroy;
        wl_signal_add(&buffer->destroy_signal, &cp->buffer_destroy_listener);
     }
#else
   (void)resource;
#endif
}

EAPI Ecore_Window
e_pixmap_parent_window_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 0);
   return cp->parent;
}

EAPI Eina_Bool
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        ns->type = EVAS_NATIVE_SURFACE_OPENGL;
        ns->version = EVAS_NATIVE_SURFACE_VERSION;
        ns->data.opengl.texture_id = 0;
        ns->data.opengl.framebuffer_id = 0;
        ns->data.opengl.x = 0;
        ns->data.opengl.y = 0;
        ns->data.opengl.w = cp->w;
        ns->data.opengl.h = cp->h;
        ret = EINA_TRUE;
#endif
        break;
      default:
        break;
     }

   return ret;
}

EAPI void
e_pixmap_image_clear(E_Pixmap *cp, Eina_Bool cache)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);

   if (!cache)
     {
#ifndef HAVE_WAYLAND_ONLY
        if (!cp->image) return;
#endif
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        if (!cp->buffer_ref.buffer) return;
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        if (cache)
          {
             E_Comp_Wl_Client_Data *cd;
             struct wl_resource *cb;

             if ((!cp->client) || (!cp->client->comp_data)) return;
             cd = (E_Comp_Wl_Client_Data *)cp->client->comp_data;
             EINA_LIST_FREE(cd->frames, cb)
               {
                  wl_callback_send_done(cb, ecore_time_unix_get());
                  wl_resource_destroy(cb);
               }
          }
#endif
        break;
      default:
        break;
     }
}

EAPI Eina_Bool
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        /* FIXME */
        return EINA_TRUE;
#endif
        break;
      default:
        break;
     }

   return EINA_FALSE;
}

EAPI Eina_Bool
e_pixmap_image_exists(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);

#ifndef HAVE_WAYLAND_ONLY
   if (cp->type == E_PIXMAP_TYPE_X)
     return !!cp->image;
#endif
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
   return (cp->buffer_ref.buffer != NULL);
#endif

   return EINA_FALSE;
}

EAPI Eina_Bool
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        return ((cp->buffer_ref.buffer != NULL) && (cp->image_argb));
#endif
        default: break;
     }
   return EINA_FALSE;
}

EAPI void *
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        return cp->data;
#endif
        break;
      default:
        break;
     }
   return NULL;
}

EAPI Eina_Bool
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
        if (cp->buffer_ref.buffer)
          {
             struct wl_shm_buffer *shm_buffer;
             uint32_t format;
             int i, x, y;
             unsigned int *src, *dst;

             shm_buffer = wl_shm_buffer_get(cp->buffer_ref.buffer->resource);
             if (!shm_buffer) return EINA_FALSE;

             format = wl_shm_buffer_get_format(shm_buffer);
             if (format == WL_SHM_FORMAT_XRGB8888)
               {
                  dst = (unsigned int *)pix;
                  src = (unsigned int *)ipix;

                  for (y = 0; y < r->h; y++)
                    {
                       i = (r->y + y) * stride / 4 + r->x;
                       for (x = 0; x < r->w; x++)
                         dst[i+x] = 0xff000000 | src[i+x];
                    }
                  pix = (void *)dst;
               }

             return EINA_TRUE;
          }
#endif
        break;
      default:
        break;
     }
   return EINA_FALSE;
}

EAPI Eina_Bool
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
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
#endif
        (void) r;
        return EINA_TRUE;
      default:
        break;
     }
   return EINA_FALSE;
}

EAPI void 
e_pixmap_image_draw_done(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);

#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
   struct wl_shm_buffer *shm_buffer;

   shm_buffer = wl_shm_buffer_get(cp->buffer_ref.buffer->resource);
   if (!shm_buffer) return;

   wl_shm_buffer_end_access(shm_buffer);
#endif
}

EAPI void
e_pixmap_image_opaque_set(E_Pixmap *cp, int x, int y, int w, int h)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
   EINA_RECTANGLE_SET(&cp->opaque, x, y, w, h);
#else
   (void)x;
   (void)y;
   (void)w;
   (void)h;
#endif
}

EAPI void
e_pixmap_image_opaque_get(E_Pixmap *cp, int *x, int *y, int *w, int *h)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);
#if defined(HAVE_WAYLAND_CLIENTS) || defined(HAVE_WAYLAND_ONLY)
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
