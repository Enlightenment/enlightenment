#include "e.h"

/* local function prototypes */

/* local wayland interfaces */

EAPI E_Shell_Surface *
e_shell_surface_new(unsigned int id)
{
   E_Shell_Surface *ess;

   /* try to allocate space for our shell surface */
   if (!(ess = E_NEW(E_Shell_Surface, 1))) return NULL;

   ess->wl.resource.object.id = id;
   ess->wl.resource.object.interface = &wl_shell_surface_interface;
   ess->wl.resource.data = ess;

   return ess;
}
