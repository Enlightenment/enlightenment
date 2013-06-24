#ifdef E_TYPEDEFS

typedef struct _E_Region E_Region;

#else
# ifndef E_REGION_H
#  define E_REGION_H

struct _E_Region
{
   struct wl_resource *resource;
   pixman_region32_t region;
};

EAPI E_Region *e_region_new(struct wl_client *client, unsigned int id);

# endif
#endif
