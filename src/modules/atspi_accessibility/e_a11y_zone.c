#include "e.h"


static int _zone_a11y_enable(E_Zone *zone)
{
   return 0;
}

static int _zone_a11y_disable(E_Zone *zone)
{
   return 0;
}

/* Initializate atspi-accessibility features of E_Zone objects */
int e_a11y_zones_init(void)
{
   Eina_List *l;
   E_Zone *zone;

   if (!e_comp)
     return -1;

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        _zone_a11y_enable(zone);
     }

   // register on zone add/remove events

   return 0;
}

int e_a11y_zones_shutdown(void)
{
   Eina_List *l;
   E_Zone *zone;

   if (!e_comp)
     return -1;

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        _zone_a11y_disable(zone);
     }

   // unregister zone add/remove events

   return 0;
}

