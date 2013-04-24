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
static void _e_main_parse_arguments(int argc, char **argv);
static Eina_Bool _e_main_cb_signal_exit(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *ev EINA_UNUSED);
static Eina_Bool _e_main_cb_signal_hup(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *ev EINA_UNUSED);
static Eina_Bool _e_main_cb_signal_user(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *ev);
static int _e_main_dirs_init(void);
static int _e_main_dirs_shutdown(void);

/* local variables */
static Eina_Bool really_know = EINA_FALSE;
static Eina_Bool locked = EINA_FALSE;
static Eina_Bool inloop = EINA_FALSE;
static jmp_buf wl_fatal_buff;
static int _e_main_lvl = 0;
static int (*_e_main_shutdown_func[MAX_LEVEL])(void);

/* external variables */
EAPI Eina_Bool e_precache_end = EINA_FALSE;
EAPI Eina_Bool x_fatal = EINA_FALSE;
EAPI Eina_Bool good = EINA_FALSE;
EAPI Eina_Bool evil = EINA_FALSE;
EAPI Eina_Bool starting = EINA_TRUE;
EAPI Eina_Bool stopping = EINA_FALSE;
EAPI Eina_Bool restart = EINA_FALSE;
EAPI Eina_Bool e_nopause = EINA_FALSE;

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

   TS("Eina Init");
   if (!eina_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Eina!\n"));
        _e_main_shutdown(-1);
     }
   _e_main_shutdown_push(eina_shutdown);
   TS("Eina Init Done");

   TS("E Log Init");
   if (!e_log_init())
     {
        e_error_message_show(_("Enlightenment could not create a logging domain!\n"));
        _e_main_shutdown(-1);
     }
   _e_main_shutdown_push(e_log_shutdown);
   TS("E Log Init Done");

   TS("Determine Prefix");
   if (!e_prefix_determine(argv[0]))
     {
        fprintf(stderr,
                "ERROR: Enlightenment cannot determine it's installed\n"
                "       prefix from the system or argv[0].\n"
                "       This is because it is not on Linux AND has been\n"
                "       executed strangely. This is unusual.\n");
     }
   TS("Determine Prefix Done");

   /* for debugging by redirecting stdout of e to a log file to tail */
   setvbuf(stdout, NULL, _IONBF, 0);

   TS("Environment Variables");
   if (getenv("E_RESTART")) after_restart = EINA_TRUE;
   if (getenv("DESKTOP_STARTUP_ID"))
     e_util_env_set("DESKTOP_STARTUP_ID", NULL);
   e_util_env_set("E_RESTART_OK", NULL);
   e_util_env_set("PANTS", "ON");
   e_util_env_set("DESKTOP", "Enlightenment-0.17.0");
   TS("Environment Variables Done");

   TS("Parse Arguments");
   _e_main_parse_arguments(argc, argv);
   TS("Parse Arguments Done");

   /*** Initialize Core EFL Libraries We Need ***/

   TS("Eet Init");
   if (!eet_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Eet!\n"));
        _e_main_shutdown(-1);
     }
   TS("Eet Init Done");
   _e_main_shutdown_push(eet_shutdown);

   TS("Ecore Init");
   if (!ecore_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore!\n"));
        _e_main_shutdown(-1);
     }
   TS("Ecore Init Done");
   _e_main_shutdown_push(ecore_shutdown);

   TS("Eio Init");
   if (!eio_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize EIO!\n"));
        _e_main_shutdown(-1);
     }
   TS("Eio Init Done");
   _e_main_shutdown_push(eio_shutdown);

   ecore_app_args_set(argc, (const char **)argv);

   TS("Ecore Event Handlers");
   if (!ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT,
                                _e_main_cb_signal_exit, NULL))
     {
        e_error_message_show(_("Enlightenment cannot set up an exit signal handler.\n"
                               "Perhaps you are out of memory?"));
        _e_main_shutdown(-1);
     }
   if (!ecore_event_handler_add(ECORE_EVENT_SIGNAL_HUP,
                                _e_main_cb_signal_hup, NULL))
     {
        e_error_message_show(_("Enlightenment cannot set up a HUP signal handler.\n"
                               "Perhaps you are out of memory?"));
        _e_main_shutdown(-1);
     }
   if (!ecore_event_handler_add(ECORE_EVENT_SIGNAL_USER,
                                _e_main_cb_signal_user, NULL))
     {
        e_error_message_show(_("Enlightenment cannot set up a USER signal handler.\n"
                               "Perhaps you are out of memory?"));
        _e_main_shutdown(-1);
     }
   TS("Ecore Event Handlers Done");

   TS("Ecore_File Init");
   if (!ecore_file_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_File!\n"));
        _e_main_shutdown(-1);
     }
   TS("Ecore_File Init Done");
   _e_main_shutdown_push(ecore_file_shutdown);

   TS("Ecore_Con Init");
   if (!ecore_con_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Con!\n"));
        _e_main_shutdown(-1);
     }
   TS("Ecore_Con Init Done");
   _e_main_shutdown_push(ecore_con_shutdown);

   TS("Ecore_Ipc Init");
   if (!ecore_ipc_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Ipc!\n"));
        _e_main_shutdown(-1);
     }
   TS("Ecore_Ipc Init Done");
   _e_main_shutdown_push(ecore_ipc_shutdown);

   /* TODO: idler_before */

#ifdef HAVE_ECORE_IMF
   TS("Ecore_IMF Init");
   if (!ecore_imf_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_IMF!\n"));
        _e_main_shutdown(-1);
     }
   TS("Ecore_IMF Init Done");
   _e_main_shutdown_push(ecore_imf_shutdown);
#endif

   TS("Ecore_Evas Init");
   if (!ecore_evas_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Evas!\n"));
        _e_main_shutdown(-1);
     }
   TS("Ecore_Evas Init Done");
//   _e_main_shutdown_push(ecore_evas_shutdown);

   TS("Ecore_Evas Engine Check");
   if (!ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_EGL))
     {
        if (!ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_SHM))
          {
             e_error_message_show(_("Enlightenment found ecore_evas doesn't support the Wayland Shm\n"
                                    "rendering in Evas. Please check your installation of Evas and\n"
                                    "Ecore and check they support the Wayland Shm rendering engine."));
             _e_main_shutdown(-1);
          }
     }
   /* if (!ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_SOFTWARE_BUFFER)) */
   /*   { */
   /*      e_error_message_show(_("Enlightenment found ecore_evas doesn't support the Software Buffer\n" */
   /*                             "rendering in Evas. Please check your installation of Evas and\n" */
   /*                             "Ecore and check they support the Software Buffer rendering engine.")); */
   /*      _e_main_shutdown(-1); */
   /*   } */
   TS("Ecore_Evas Engine Check Done");

   TS("Efreet Init");
   if (!efreet_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize the FDO desktop system.\n"
                               "Perhaps you lack permissions on ~/.cache/efreet or are\n"
                               "out of memory or disk space?"));
        _e_main_shutdown(-1);
     }
   TS("Efreet Init Done");
   _e_main_shutdown_push(efreet_shutdown);

   TS("Edje Init");
   if (!edje_init())
     {
        e_error_message_show(_("Enlightenment cannot initialize Edje!\n"));
        _e_main_shutdown(-1);
     }
   TS("Edje Init Done");
   _e_main_shutdown_push(edje_shutdown);

   edje_freeze();

   /*** Initialize E Subsystems We Need ***/

   /* TS("E Intl Init"); */
   /* if (!e_intl_init()) */
   /*   { */
   /*      e_error_message_show(_("Enlightenment cannot initialize E_Intl!\n")); */
   /*      _e_main_shutdown(-1); */
   /*   } */
   /* TS("E Intl Init Done"); */
   /* _e_main_shutdown_push(e_intl_shutdown); */

   TS("E Directories Init");
   /* setup directories we will be using for configurations storage etc. */
   if (!_e_main_dirs_init())
     {
        e_error_message_show(_("Enlightenment cannot create directories in your home directory.\n"
                               "Perhaps you have no home directory or the disk is full?"));
        _e_main_shutdown(-1);
     }
   TS("E Directories Init Done");
   _e_main_shutdown_push(_e_main_dirs_shutdown);

   TS("E_Filereg Init");
   if (!e_filereg_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its file registry system.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Filereg Init Done");
   _e_main_shutdown_push(e_filereg_shutdown);



   /*** Main Loop ***/

   starting = EINA_FALSE;
   inloop = EINA_TRUE;

   e_util_env_set("E_RESTART", "1");

   /* TODO: set callback for fatal wayland errors */
   TS("MAIN LOOP AT LAST");
   if (!setjmp(wl_fatal_buff))
     ecore_main_loop_begin();
   else
     CRI("FATAL: Connection gone. Abbreviated Shutdown\n");

   inloop = EINA_FALSE;
   stopping = EINA_TRUE;

   _e_main_shutdown(0);

   if (restart)
     {
        e_util_env_set("E_RESTART_OK", "1");
        if (getenv("E_START_MTRACK"))
          e_util_env_set("MTRACK", "track");
        ecore_app_restart();
     }

   e_prefix_shutdown();

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

static void
_e_main_parse_arguments(int argc, char **argv)
{
   /* char *s = NULL; */
   int i = 0;

   /* handle some command-line parameters */
   for (i = 1; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-display")) && (i < (argc - 1)))
          {
             i++;
             e_util_env_set("DISPLAY", argv[i]);
          }
        else if ((!strcmp(argv[i], "-fake-xinerama-screen")) && (i < (argc - 1)))
          {
             /* int x, y, w, h; */

             i++;
             /* if (sscanf(argv[i], "%ix%i+%i+%i", &w, &h, &x, &y) == 4) */
             /*   e_xinerama_fake_screen_add(x, y, w, h); */
          }
        else if (!strcmp(argv[i], "-good"))
          {
             good = EINA_TRUE;
             evil = EINA_FALSE;
             printf("LA LA LA\n");
          }
        else if (!strcmp(argv[i], "-evil"))
          {
             good = EINA_FALSE;
             evil = EINA_TRUE;
             printf("MUHAHAHAHHAHAHAHAHA\n");
          }
        else if (!strcmp(argv[i], "-psychotic"))
          {
             good = EINA_TRUE;
             evil = EINA_TRUE;
             printf("MUHAHALALALALALALALA\n");
          }
        else if ((!strcmp(argv[i], "-profile")) && (i < (argc - 1)))
          {
             i++;
             if (!getenv("E_CONF_PROFILE"))
               e_util_env_set("E_CONF_PROFILE", argv[i]);
          }
        else if (!strcmp(argv[i], "-i-really-know-what-i-am-doing-and-accept-full-responsibility-for-it"))
          really_know = EINA_TRUE;
        else if (!strcmp(argv[i], "-locked"))
          locked = EINA_TRUE;
        else if (!strcmp(argv[i], "-nopause"))
          e_nopause = EINA_TRUE;
        else if ((!strcmp(argv[i], "-h")) ||
                 (!strcmp(argv[i], "-help")) ||
                 (!strcmp(argv[i], "--help")))
          {
             printf
               (_(
                 "Options:\n"
                 "\t-display DISPLAY\n"
                 "\t\tConnect to display named DISPLAY.\n"
                 "\t\tEG: -display :1.0\n"
                 "\t-fake-xinerama-screen WxH+X+Y\n"
                 "\t\tAdd a FAKE xinerama screen (instead of the real ones)\n"
                 "\t\tgiven the geometry. Add as many as you like. They all\n"
                 "\t\treplace the real xinerama screens, if any. This can\n"
                 "\t\tbe used to simulate xinerama.\n"
                 "\t\tEG: -fake-xinerama-screen 800x600+0+0 -fake-xinerama-screen 800x600+800+0\n"
                 "\t-profile CONF_PROFILE\n"
                 "\t\tUse the configuration profile CONF_PROFILE instead of the user selected default or just \"default\".\n"
                 "\t-good\n"
                 "\t\tBe good.\n"
                 "\t-evil\n"
                 "\t\tBe evil.\n"
                 "\t-psychotic\n"
                 "\t\tBe psychotic.\n"
                 "\t-locked\n"
                 "\t\tStart with desklock on, so password will be asked.\n"
                 "\t-i-really-know-what-i-am-doing-and-accept-full-responsibility-for-it\n"
                 "\t\tIf you need this help, you don't need this option.\n"
                 )
               );
             _e_main_shutdown(-1);
          }
     }

   /* fix up DISPLAY to be :N.0 if no .screen is in it */
   /* s = getenv("DISPLAY"); */
   /* if (s) */
   /*   { */
   /*      char *p, buff[PATH_MAX]; */

   /*      if (!(p = strrchr(s, ':'))) */
   /*        { */
   /*           snprintf(buff, sizeof(buff), "%s:0.0", s); */
   /*           e_util_env_set("DISPLAY", buff); */
   /*        } */
   /*      else */
   /*        { */
   /*           if (!(p = strrchr(p, '.'))) */
   /*             { */
   /*                snprintf(buff, sizeof(buff), "%s.0", s); */
   /*                e_util_env_set("DISPLAY", buff); */
   /*             } */
   /*        } */
   /*   } */

   /* we want to have been launched by enlightenment_start. there is a very */
   /* good reason we want to have been launched this way, thus check */
   if (!getenv("E_START"))
     {
        e_error_message_show(_("You are executing enlightenment directly. This is\n"
                               "bad. Please do not execute the \"enlightenment\"\n"
                               "binary. Use the \"enlightenment_start\" launcher. It\n"
                               "will handle setting up environment variables, paths,\n"
                               "and launching any other required services etc.\n"
                               "before enlightenment itself begins running.\n"));
        _e_main_shutdown(-1);
     }
}

static Eina_Bool
_e_main_cb_signal_exit(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *ev EINA_UNUSED)
{
   /* called on ctrl-c, kill (pid) (also SIGINT, SIGTERM and SIGQIT) */
   /* e_sys_action_do(E_SYS_EXIT, NULL); */
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_main_cb_signal_hup(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *ev EINA_UNUSED)
{
   /* e_sys_action_do(E_SYS_RESTART, NULL); */
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_main_cb_signal_user(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *ev)
{
   Ecore_Event_Signal_User *e;

   e = ev;
   if (e->number == 1)
     {
//        E_Action *a = e_action_find("configuration");
//        if ((a) && (a->func.go)) a->func.go(NULL, NULL);
     }
   else if (e->number == 2)
     {
        // comp module has its own handler for this for enabling/disabling fps debug
     }

   return ECORE_CALLBACK_RENEW;
}

static int
_e_main_dirs_init(void)
{
   const char *base;
   const char *dirs[] =
   {
      "images",
      "fonts",
      "themes",
      "icons",
      "backgrounds",
      "applications",
      "applications/menu",
      "applications/menu/favorite",
      "applications/menu/all",
      "applications/bar",
      "applications/bar/default",
      "applications/startup",
      "applications/restart",
      "applications/trash",
      "applications/desk-lock",
      "applications/desk-unlock",
      "modules",
      "config",
      "locale",
      "input_methods",
      NULL
   };

   base = e_user_dir_get();
   if (ecore_file_mksubdirs(base, dirs) != sizeof(dirs) / sizeof(dirs[0]) - 1)
     {
        e_error_message_show("Could not create one of the required "
                             "subdirectories of '%s'\n", base);
        return 0;
     }

   return 1;
}

static int
_e_main_dirs_shutdown(void)
{
   return 1;
}
