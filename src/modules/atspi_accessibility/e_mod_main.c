#include "e.h"

E_API void *
e_modapi_init(E_Module *m)
{
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
