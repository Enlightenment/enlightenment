#include "e.h"

EINTERN int e_log_dom = -1;

EINTERN int
e_log_init(void)
{
   e_log_dom = eina_log_domain_register("e", EINA_COLOR_WHITE);
   return e_log_dom != -1;
}

EINTERN int
e_log_shutdown(void)
{
   eina_log_domain_unregister(e_log_dom);
   e_log_dom = -1;
   return 0;
}

