#include "e.h"
#ifdef HAVE_ECORE_IMF
# include <Ecore_IMF.h>
#endif

#define MAX_LEVEL 64

#define TS_DO
#ifdef TS_DO
# define TS(x)                                                    \
  {                                                               \
     t1 = ecore_time_unix_get();                                  \
     printf("ESTART: %1.5f [%1.5f] - %s\n", t1 - t0, t1 - t2, x); \
     t2 = t1;                                                     \
  }
static double t0, t1, t2;
#else
# define TS(x)
#endif

#ifdef HAVE_ELEMENTARY
#include <Elementary.h>
#endif

#ifdef HAVE_EMOTION
#include <Emotion.h>
#endif

/* externally accessible functions */
int
main(int argc, char **argv)
{
   Eina_Bool nostartup = EINA_FALSE;
   Eina_Bool safe_mode = EINA_FALSE;
   Eina_Bool after_restart = EINA_FALSE;
   Eina_Bool waslocked = EINA_FALSE;
   double t = 0.0, tstart = 0.0;
   char *s = NULL, buff[32];
   struct sigaction action;
#ifdef TS_DO
   t0 = t1 = t2 = ecore_time_unix_get();
#endif

   TS("Begin Startup");

   /* trap deadly bug signals and allow some form of sane recovery */
   /* or ability to gdb attach and debug at this point - better than your */
   /* wm/desktop vanishing and not knowing what happened */
   if (!getenv("NOTIFY_SOCKET"))
     {
        /* TODO: signals */
     }

   t = ecore_time_unix_get();
   s = getenv("E_START_TIME");
   if ((s) && (!getenv("E_RESTART_OK")))
     {
        tstart = atof(s);
        if ((t - tstart) < 5.0) safe_mode = EINA_TRUE;
     }
   tstart = t;
   snprintf(buff, sizeof(buff), "%1.1f", tstart);
   e_util_env_set("E_START_TIME", buff);

   if (getenv("E_START_MTRACK"))
     e_util_env_set("MTRACK", NULL);

   return 0;
}
