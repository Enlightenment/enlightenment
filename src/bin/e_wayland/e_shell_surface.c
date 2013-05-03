#include "e.h"

/* local function prototypes */

/* local wayland interfaces */

EAPI E_Shell_Surface *
e_shell_surface_new(E_Surface *surface, unsigned int id)
{
   E_Shell_Surface *ess;

   /* try to allocate space for our shell surface */
   if (!(ess = E_NEW(E_Shell_Surface, 1))) return NULL;

   ess->surface = surface;
   surface->shell_surface = ess;

   ess->active = EINA_TRUE;
   ess->type = E_SHELL_SURFACE_TYPE_NONE;
   ess->ntype = E_SHELL_SURFACE_TYPE_NONE;

   wl_signal_init(&ess->wl.resource.destroy_signal);
   wl_list_init(&ess->wl.link);

   ess->wl.resource.object.id = id;
   ess->wl.resource.object.interface = &wl_shell_surface_interface;
   ess->wl.resource.data = ess;

   return ess;
}
