#ifdef E_TYPEDEFS

typedef struct _E_Region E_Region;

#else
# ifndef E_REGION_H
#  define E_REGION_H

struct _E_Region
{
   struct wl_resource resource;
   Eina_Rectangle *region;
};

EAPI E_Region *e_region_new(unsigned int id);

# endif
#endif
