#ifdef E_TYPEDEFS

typedef struct _E_Pixmap E_Pixmap;

typedef enum
{
   E_PIXMAP_TYPE_X,
   E_PIXMAP_TYPE_WL,
   E_PIXMAP_TYPE_NONE,
} E_Pixmap_Type;

#else
# ifndef E_PIXMAP_H
# define E_PIXMAP_H

EAPI int e_pixmap_free(E_Pixmap *cp);
EAPI E_Pixmap *e_pixmap_ref(E_Pixmap *cp);
EAPI E_Pixmap *e_pixmap_new(E_Pixmap_Type type, ...);
EAPI E_Pixmap_Type e_pixmap_type_get(const E_Pixmap *cp);
EAPI void *e_pixmap_resource_get(E_Pixmap *cp);
EAPI void e_pixmap_resource_set(E_Pixmap *cp, void *resource);
EAPI void e_pixmap_parent_window_set(E_Pixmap *cp, Ecore_Window win);
EAPI void e_pixmap_visual_cmap_set(E_Pixmap *cp, void *visual, unsigned int cmap);
EAPI unsigned int e_pixmap_failures_get(const E_Pixmap *cp);
EAPI void *e_pixmap_visual_get(const E_Pixmap *cp);
EAPI Eina_Bool e_pixmap_dirty_get(E_Pixmap *cp);
EAPI void e_pixmap_clear(E_Pixmap *cp);
EAPI void e_pixmap_usable_set(E_Pixmap *cp, Eina_Bool set);
EAPI Eina_Bool e_pixmap_usable_get(const E_Pixmap *cp);
EAPI void e_pixmap_dirty(E_Pixmap *cp);
EAPI Eina_Bool e_pixmap_refresh(E_Pixmap *cp);
EAPI Eina_Bool e_pixmap_size_changed(E_Pixmap *cp, int w, int h);
EAPI Eina_Bool e_pixmap_size_get(E_Pixmap *cp, int *w, int *h);
EAPI void e_pixmap_client_set(E_Pixmap *cp, E_Client *ec);
EAPI E_Client *e_pixmap_client_get(E_Pixmap *cp);
EAPI E_Pixmap *e_pixmap_find(E_Pixmap_Type type, ...);
EAPI E_Client *e_pixmap_find_client(E_Pixmap_Type type, ...);
EAPI uint64_t e_pixmap_window_get(E_Pixmap *cp);
EAPI Ecore_Window e_pixmap_parent_window_get(E_Pixmap *cp);
EAPI Eina_Bool e_pixmap_native_surface_init(E_Pixmap *cp, Evas_Native_Surface *ns);
EAPI void e_pixmap_image_clear(E_Pixmap *cp, Eina_Bool cache);
EAPI Eina_Bool e_pixmap_image_refresh(E_Pixmap *cp);
EAPI Eina_Bool e_pixmap_image_exists(const E_Pixmap *cp);
EAPI Eina_Bool e_pixmap_image_is_argb(const E_Pixmap *cp);
EAPI void *e_pixmap_image_data_get(E_Pixmap *cp);
EAPI Eina_Bool e_pixmap_image_data_argb_convert(E_Pixmap *cp, void *pix, void *ipix, Eina_Rectangle *r, int stride);
EAPI Eina_Bool e_pixmap_image_draw(E_Pixmap *cp, const Eina_Rectangle *r);

EAPI void e_pixmap_image_opaque_set(E_Pixmap *cp, int x, int y, int w, int h);
EAPI void e_pixmap_image_opaque_get(E_Pixmap *cp, int *x, int *y, int *w, int *h);

static inline Eina_Bool
e_pixmap_is_x(const E_Pixmap *cp)
{
   return cp && e_pixmap_type_get(cp) == E_PIXMAP_TYPE_X;
}

# endif

#endif
