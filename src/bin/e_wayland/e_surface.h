#ifdef E_TYPEDEFS

typedef struct _E_Surface E_Surface;

#else
# ifndef E_SURFACE_H
#  define E_SURFACE_H

struct _E_Surface
{
   struct 
     {
        struct wl_surface surface;
        struct wl_list link;
     } wl;
};

EAPI E_Surface *e_surface_new(unsigned int id);

# endif
#endif
