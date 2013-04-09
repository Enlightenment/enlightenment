#include "e.h"
#include "e_comp_wl.h"
#include "e_mod_main.h"
#include "e_desktop_shell_protocol.h"

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Desktop_Shell" };

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Wayland_Desktop_Shell *shell = NULL;

   /* try to allocate space for the shell structure */
   if (!(shell = E_NEW(E_Wayland_Desktop_Shell, 1)))
     return NULL;

   /* tell the shell what compositor to use */
   shell->compositor = _e_wl_comp;

   /* setup compositor shell interface functions */
   _e_wl_comp->shell_interface.shell = shell;

   return m;

err:
   /* reset compositor shell interface */
   _e_wl_comp->shell_interface.shell = NULL;

   /* free the allocated shell structure */
   E_FREE(shell);

   return NULL;
}

EAPI int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   /* nothing to do here as shell will get the destroy callback from 
    * the compositor and we can cleanup there */
   return 1;
}
