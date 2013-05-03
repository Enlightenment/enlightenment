#include "e.h"

EINTERN int 
e_shell_init(void)
{
   E_Module *mod = NULL;
   const char *modname;

   /* NB: Basically, this function needs to load the appropriate 
    * shell module (desktop, tablet, etc) */

   /* FIXME: Get this from config */
   modname = "wl_desktop";

   if (!(mod = e_module_find(modname)))
     mod = e_module_new(modname);

   if (mod) 
     {
        if ((e_module_enable(mod)))
          return 1;
     }

   return 0;
}

EINTERN int 
e_shell_shutdown(void)
{
   E_Module *mod = NULL;
   const char *modname;

   /* NB: This function needs to unload the shell module */

   /* FIXME: Get this from config */
   modname = "wl_desktop";

   if ((mod = e_module_find(modname)))
     e_module_disable(mod);

   return 1;
}
