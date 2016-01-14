#include "e.h"

#include <Elementary.h>

E_API void *
e_modapi_init(E_Module *m)
{
   // Ensure that elm is initialized.
   elm_init(0, NULL);

   // Ensure that atspi mode is on, despite current elm configuration.
   elm_config_atspi_mode_set(EINA_TRUE);

   return NULL;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}

E_API int
e_modapi_shutdown(E_Module *m)
{
   return 1;
}
