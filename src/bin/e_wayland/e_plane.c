#include "e.h"

EAPI void 
e_plane_init(E_Plane *plane, Evas_Coord x, Evas_Coord y)
{
   plane->x = x;
   plane->y = y;
   plane->damage = eina_rectangle_new(x, y, 0, 0);
   plane->clip = eina_rectangle_new(x, y, 0, 0);
}

EAPI void 
e_plane_shutdown(E_Plane *plane)
{
   eina_rectangle_free(plane->damage);
   eina_rectangle_free(plane->clip);
}
