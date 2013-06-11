#include "e.h"

/* local function prototypes */
static void _e_desk_cb_free(E_Desk *desk);

EINTERN int 
e_desk_init(void)
{
   return 1;
}

EINTERN int 
e_desk_shutdown(void)
{
   return 1;
}

EAPI E_Desk *
e_desk_new(E_Zone *zone, int x, int y)
{
   E_Desk *desk;

   E_OBJECT_CHECK_RETURN(zone, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, NULL);

   desk = E_OBJECT_ALLOC(E_Desk, E_DESK_TYPE, _e_desk_cb_free);
   if (!desk)
     {
        printf("FAILED TO ALLOCATE DESK\n");
        return NULL;
     }

   printf("Created New Desktop: %d %d\n", x, y);

   desk->zone = zone;
   desk->x = x;
   desk->y = y;

   return desk;
}

EAPI void 
e_desk_show(E_Desk *desk)
{
   E_OBJECT_CHECK(desk);
   E_OBJECT_TYPE_CHECK(desk, E_DESK_TYPE);

   if (desk->visible) return;

   desk->visible = EINA_TRUE;
   e_bg_zone_update(desk->zone, E_BG_TRANSITION_START);
}

EAPI void 
e_desk_hide(E_Desk *desk)
{
   E_OBJECT_CHECK(desk);
   E_OBJECT_TYPE_CHECK(desk, E_DESK_TYPE);

   if (!desk->visible) return;

   desk->visible = EINA_FALSE;
}

EAPI E_Desk *
e_desk_current_get(E_Zone *zone)
{
   E_OBJECT_CHECK_RETURN(zone, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, NULL);

   return e_desk_at_xy_get(zone, zone->desk_x_current, zone->desk_y_current);
}

EAPI E_Desk *
e_desk_at_xy_get(E_Zone *zone, int x, int  y)
{
   E_OBJECT_CHECK_RETURN(zone, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, NULL);

   if ((x >= zone->desk_x_count) || (y >= zone->desk_y_count))
     return NULL;
   else if ((x < 0) || (y < 0))
     return NULL;

   return zone->desks[x + (y * zone->desk_x_count)];
}

/* local functions */
static void 
_e_desk_cb_free(E_Desk *desk)
{
   free(desk);
}
