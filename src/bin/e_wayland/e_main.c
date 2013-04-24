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

/* local function prototypes */
static void _e_main_shutdown(int errcode);
static void _e_main_shutdown_push(int (*func)(void));

/* local variables */
static int _e_main_lvl = 0;
static int (*_e_main_shutdown_func[MAX_LEVEL])(void);

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
        TS("Signal Trap");
        action.sa_sigaction = e_sigseg_act;
	action.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGSEGV, &action, NULL);

	action.sa_sigaction = e_sigill_act;
	action.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGILL, &action, NULL);

	action.sa_sigaction = e_sigfpe_act;
	action.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGFPE, &action, NULL);

	action.sa_sigaction = e_sigbus_act;
	action.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGBUS, &action, NULL);

	action.sa_sigaction = e_sigabrt_act;
	action.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGABRT, &action, NULL);
	TS("Signal Trap Done");
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

/* local functions */
static void 
_e_main_shutdown(int errcode)
{
   int i = 0;

   printf("E17: Begin Shutdown Procedure!\n");

   /* if (_idle_before) ecore_idle_enterer_del(_idle_before); */
   /* _idle_before = NULL; */
   /* if (_idle_after) ecore_idle_enterer_del(_idle_after); */
   /* _idle_after = NULL; */
   /* if (_idle_flush) ecore_idle_enterer_del(_idle_flush); */
   /* _idle_flush = NULL; */

   for (i = (_e_main_lvl - 1); i >= 0; i--)
     (*_e_main_shutdown_func[i])();
   if (errcode < 0) exit(errcode);
}

static void 
_e_main_shutdown_push(int (*func)(void))
{
   _e_main_lvl++;
   if (_e_main_lvl > MAX_LEVEL)
     {
        _e_main_lvl--;
        e_error_message_show("WARNING: too many init levels. MAX = %i\n",
                             MAX_LEVEL);
        return;
     }
   _e_main_shutdown_func[_e_main_lvl - 1] = func;
}
