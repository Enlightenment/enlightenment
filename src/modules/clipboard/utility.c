#include "e_mod_main.h"

Eina_Bool
is_empty(const char *str)
{
  EINA_SAFETY_ON_NULL_RETURN_VAL(str, EINA_TRUE);
  while ((isspace((unsigned char)*str)) && (*str++));
  return !*str;
}
