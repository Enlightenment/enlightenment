#include "e.h"

EAPI E_Path *path_data = NULL;
EAPI E_Path *path_images = NULL;
EAPI E_Path *path_fonts = NULL;
EAPI E_Path *path_themes = NULL;
EAPI E_Path *path_icons = NULL;
EAPI E_Path *path_modules = NULL;
EAPI E_Path *path_backgrounds = NULL;
EAPI E_Path *path_messages = NULL;

EAPI void 
e_util_env_set(const char *var, const char *val)
{
   if (val)
     {
#ifdef HAVE_SETENV
        setenv(var, val, 1);
#else
        char buf[8192];

        snprintf(buf, sizeof(buf), "%s=%s", var, val);
        if (getenv(var))
          putenv(buf);
        else
          putenv(strdup(buf));
#endif
     }
   else
     {
#ifdef HAVE_UNSETENV
        unsetenv(var);
#else
        if (getenv(var)) putenv(var);
#endif
     }
}

EAPI int
e_util_strcmp(const char *s1, const char *s2)
{
   if ((s1) && (s2))
     {
        if (s1 == s2) return 0;
        return strcmp(s1, s2);
     }
   return 0x7fffffff;
}
