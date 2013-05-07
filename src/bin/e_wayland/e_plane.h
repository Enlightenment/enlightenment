#ifdef E_TYPEDEFS

typedef struct _E_Plane E_Plane;

#else
# ifndef E_PLANE_H
#  define E_PLANE_H

struct _E_Plane
{
   pixman_region32_t damge, clip;
   Evas_Coord x, y;
   struct wl_list link;
};

EAPI void e_plane_init(E_Plane *plane, Evas_Coord x, Evas_Coord y);
EAPI void e_plane_shutdown(E_Plane *plane);

# endif
#endif
