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

   wl_list_init(&ess->wl.link);

   return ess;
}

EAPI void 
e_shell_surface_destroy(E_Shell_Surface *ess)
{
   if (!ess) return;

   wl_signal_emit(&ess->signals.destroy, ess);

   /* TODO: handle popup */

   wl_list_remove(&ess->wl.surface_destroy.link);
   ess->surface->configure = NULL;

   if (ess->ping_timer) 
     {
        if (ess->ping_timer->source)
          wl_event_source_remove(ess->ping_timer->source);

        E_FREE(ess->ping_timer);
     }

   free(ess->title);
   free(ess->clas);

   wl_list_remove(&ess->wl.link);

   E_FREE(ess);
}
