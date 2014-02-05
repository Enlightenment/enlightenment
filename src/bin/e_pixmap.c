#include "e.h"

#ifdef HAVE_WAYLAND_CLIENTS
# include "e_comp_wl.h"
#endif
#ifndef WAYLAND_ONLY
# include "e_comp_x.h"
#endif

static Eina_Hash *pixmaps[2] = {NULL};

struct _E_Pixmap
{
   unsigned int refcount;
   E_Pixmap_Type type;
   uint64_t win;
   void *visual;
   void *image;
   Eina_List *images_cache;
   unsigned int cmap;
   int ibpp, ibpl;
   Ecore_Window parent;
#ifdef HAVE_WAYLAND_CLIENTS
   struct wl_resource *resource;
#endif
#ifndef WAYLAND_ONLY
   uint32_t pixmap;
#endif
   int w, h;
   E_Client *client;
   unsigned int failures;
   Eina_Bool usable : 1;
   Eina_Bool dirty : 1;
   Eina_Bool image_argb : 1;
#ifdef HAVE_WAYLAND_CLIENTS
   Eina_Bool copy_image : 1;
#endif
};

static void
_e_pixmap_clear(E_Pixmap *cp, Eina_Bool cache)
{
   cp->w = cp->h = 0;
   cp->image_argb = EINA_FALSE;
   switch (cp->type)
     {
#ifndef WAYLAND_ONLY
      case E_PIXMAP_TYPE_X:
        if (cp->pixmap)
          {
             ecore_x_pixmap_free(cp->pixmap);
             cp->pixmap = 0;
             ecore_x_e_comp_pixmap_set(cp->parent ?: cp->win, 0);
             e_pixmap_image_clear(cp, cache);
          }
        break;
#endif
#ifdef HAVE_WAYLAND_CLIENTS
      case E_PIXMAP_TYPE_WL:
        e_pixmap_image_clear(cp, cache);
        break;
#endif
      default: break;
     }
}

#ifndef WAYLAND_ONLY
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
#ifndef WAYLAND_ONLY
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
#ifdef HAVE_WAYLAND_CLIENTS
        if (!cp->copy_image) break;
        /* maybe delay image free here... */
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
#ifndef WAYLAND_ONLY
   Ecore_X_Window xwin;
#endif
#ifdef HAVE_WAYLAND_CLIENTS
   uint64_t id;
#endif
   
   if (!pixmaps[type]) return NULL;
   switch (type)
     {
#ifndef WAYLAND_ONLY
      case E_PIXMAP_TYPE_X:
        xwin = va_arg(*l, uint32_t);
        return eina_hash_find(pixmaps[type], &xwin);
#endif
#ifdef HAVE_WAYLAND_CLIENTS
      case E_PIXMAP_TYPE_WL:
        id = va_arg(*l, uint64_t);
        return eina_hash_find(pixmaps[type], &id);
#endif
      default: break;
     }
   return NULL;
}

EAPI int
e_pixmap_free(E_Pixmap *cp)
{
   if (!cp) return 0;
   if (--cp->refcount) return cp->refcount;
   e_pixmap_image_clear(cp, 0);
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef WAYLAND_ONLY
        if (cp->parent) eina_hash_set(pixmaps[cp->type], &cp->parent, NULL);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND_CLIENTS
        if (cp->parent) eina_hash_set(pixmaps[cp->type], &cp->parent, NULL);
#endif
        break;
     }
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
#ifndef WAYLAND_ONLY
   Ecore_X_Window xwin;
#endif
#ifdef HAVE_WAYLAND_CLIENTS
   uint64_t id;
#endif

   EINA_SAFETY_ON_TRUE_RETURN_VAL((type != E_PIXMAP_TYPE_WL) && (type != E_PIXMAP_TYPE_X), NULL);
   va_start(l, type);
   switch (type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef WAYLAND_ONLY
        xwin = va_arg(l, uint32_t);
        if (pixmaps[type])
          {
             cp = eina_hash_find(pixmaps[type], &xwin);
             if (cp)
               {
                  cp->refcount++;
                  return cp;
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
#ifdef HAVE_WAYLAND_CLIENTS
        id = va_arg(l, uint64_t);
        if (pixmaps[type])
          {
             cp = eina_hash_find(pixmaps[type], &id);
             if (cp)
               {
                  cp->refcount++;
                  return cp;
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
   cp->visual = visual;
   cp->cmap = cmap;
}

EAPI void *
e_pixmap_visual_get(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);
   return cp->visual;
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
   e_pixmap_dirty(cp);
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
   int pw, ph;

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
#ifndef WAYLAND_ONLY
        {
           uint32_t pixmap;

           pixmap = ecore_x_composite_name_window_pixmap_get(cp->parent ?: cp->win);
           if (cp->client)
             e_comp_object_native_surface_set(cp->client->frame, 0);
           success = !!pixmap;
           if (!success) break;
           if (ecore_x_present_exists() && cp->client->comp_data->pw && cp->client->comp_data->ph &&
               (!e_client_util_resizing_get(cp->client))) //PRESENT is unreliable during resizes
             {
                pw = cp->client->comp_data->pw;
                ph = cp->client->comp_data->ph;
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
#ifdef HAVE_WAYLAND_CLIENTS
      {
         E_Wayland_Surface *ews;
         E_Wayland_Buffer *buff;
         struct wl_shm_buffer *shm_buffer;

         ews = (E_Wayland_Surface*)cp->parent;
         buff = ews->buffer_reference.buffer;

         cp->resource = buff->wl.resource;
         shm_buffer = wl_shm_buffer_get(cp->resource);
         cp->w = wl_shm_buffer_get_width(shm_buffer);
         cp->h = wl_shm_buffer_get_height(shm_buffer);
         success = (cp->w > 0) && (cp->h > 0);
      }
#endif
         break;
      default: break;
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
   return cp->failures;
}

EAPI void
e_pixmap_client_set(E_Pixmap *cp, E_Client *ec)
{
   cp->client = ec;
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
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, 0);
   if (cp->type != E_PIXMAP_TYPE_WL)
     CRI("ACK!");
#ifdef HAVE_WAYLAND_CLIENTS
   return cp->resource;
#else
   return NULL;
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
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ns, EINA_FALSE);

   ns->version = EVAS_NATIVE_SURFACE_VERSION;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef WAYLAND_ONLY
        ns->type = EVAS_NATIVE_SURFACE_X11;
        ns->data.x11.visual = cp->visual;
        ns->data.x11.pixmap = cp->pixmap;
#endif
        break;
      case E_PIXMAP_TYPE_WL:
        return EINA_FALSE;
#ifdef HAVE_WAYLAND_CLIENTS
#warning FIXME WL NATIVE SURFACES!
        ns->type = EVAS_NATIVE_SURFACE_OPENGL;
        ns->version = EVAS_NATIVE_SURFACE_VERSION;
        ns->data.opengl.texture_id = 0;
        ns->data.opengl.framebuffer_id = 0;
        ns->data.opengl.x = 0;
        ns->data.opengl.y = 0;
        ns->data.opengl.w = cp->w;
        ns->data.opengl.h = cp->h;
#endif
        break;
      default:
        break;
     }
   return EINA_TRUE;
}

EAPI void
e_pixmap_image_clear(E_Pixmap *cp, Eina_Bool cache)
{
   EINA_SAFETY_ON_NULL_RETURN(cp);

   if ((!cache) && (!cp->image)) return;
   cp->failures = 0;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef WAYLAND_ONLY
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
      case E_PIXMAP_TYPE_WL: //lel wayland
#ifdef HAVE_WAYLAND_CLIENTS
        if (cache)
          {
             if (cp->copy_image)
               {
                  //INF("IMG FREE");
                  E_FREE(cp->image);
               }
             else
               cp->image = NULL;
             cp->copy_image = 0;
          }
        else if (!cp->copy_image)
          {
             void *copy;
             size_t size;

             size = cp->w * cp->h * sizeof(int);
             copy = malloc(size);
             memcpy(copy, cp->image, size);
             cp->image = copy;
             cp->copy_image = 1;
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

   if (cp->image) return EINA_TRUE;
   if (cp->dirty)
     {
        CRI("BUG! PIXMAP %p IS DIRTY!", cp);
        return EINA_FALSE;
     }
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef WAYLAND_ONLY
        if ((!cp->visual) || (!cp->client->depth)) return EINA_FALSE;
        cp->image = ecore_x_image_new(cp->w, cp->h, cp->visual, cp->client->depth);
        if (cp->image)
          cp->image_argb = ecore_x_image_is_argb32_get(cp->image);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND_CLIENTS
      {
         struct wl_shm_buffer *shm_buffer;

         cp->image_argb = EINA_TRUE; //for now...
         //size = cp->w * cp->h * sizeof(int);
         shm_buffer = wl_shm_buffer_get(cp->resource);
         if (cp->w != wl_shm_buffer_get_width(shm_buffer))
           CRI("ACK!");
         if (cp->h != wl_shm_buffer_get_height(shm_buffer))
           CRI("ACK!");
         cp->image = wl_shm_buffer_get_data(shm_buffer);
      }
#endif
         break;
      default:
        break;
     }
   return !!cp->image;
}

EAPI Eina_Bool
e_pixmap_image_exists(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   return !!cp->image;
}

EAPI Eina_Bool
e_pixmap_image_is_argb(const E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   return cp->image && cp->image_argb;
}

EAPI void *
e_pixmap_image_data_get(E_Pixmap *cp)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);
   if (!cp->image) return NULL;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
#ifndef WAYLAND_ONLY
        return ecore_x_image_data_get(cp->image, &cp->ibpl, NULL, &cp->ibpp);
#endif
        break;
      case E_PIXMAP_TYPE_WL:
#ifdef HAVE_WAYLAND_CLIENTS
        cp->copy_image = 0;
        return cp->image;
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
   if (!cp->image) return EINA_FALSE;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
        if (cp->image_argb) return EINA_FALSE;
        return ecore_x_image_to_argb_convert(ipix, cp->ibpp, cp->ibpl,
                                             cp->cmap, cp->visual,
                                             r->x, r->y, r->w, r->h,
                                             pix, stride, r->x, r->y);
      case E_PIXMAP_TYPE_WL:
        return EINA_TRUE; //already guaranteed to be argb
      default:
        break;
     }
   return EINA_FALSE;
}

EAPI Eina_Bool
e_pixmap_image_draw(E_Pixmap *cp, const Eina_Rectangle *r)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, EINA_FALSE);
   if (!cp->image) return EINA_FALSE;
   switch (cp->type)
     {
      case E_PIXMAP_TYPE_X:
        if (!cp->pixmap) return EINA_FALSE;
        return ecore_x_image_get(cp->image, cp->pixmap, r->x, r->y, r->x, r->y, r->w, r->h);
      case E_PIXMAP_TYPE_WL:
        return EINA_TRUE; //this call is a NOP
      default:
        break;
     }
   return EINA_FALSE;
}
