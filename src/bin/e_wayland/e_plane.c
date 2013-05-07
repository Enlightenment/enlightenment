#include "e.h"

EAPI void 
e_plane_init(E_Plane *plane, Evas_Coord x, Evas_Coord y)
{
   plane->x = x;
   plane->y = y;
   pixman_region32_init(&plane->damage);
   pixman_region32_init(&plane->clip);
}

EAPI void 
e_plane_shutdown(E_Plane *plane)
{
   pixman_region32_fini(&plane->damage);
   pixman_region32_fini(&plane->clip);
}
