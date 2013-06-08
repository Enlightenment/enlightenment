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
static int _e_main_paths_init(void);
static int _e_main_paths_shutdown(void);
static Eina_Bool _e_main_xdg_dirs_check(void);
static int _e_main_screens_init(void);
static int _e_main_screens_shutdown(void);
static Eina_Bool _e_main_cb_idle_before(void *data EINA_UNUSED);
static Eina_Bool _e_main_cb_idle_after(void *data EINA_UNUSED);
static Eina_Bool _e_main_cb_idle_flush(void *data EINA_UNUSED);
static Eina_Bool _e_main_cb_bound(void *data, int type EINA_UNUSED, void *event);

/* local variables */
static Eina_Bool really_know = EINA_FALSE;
static Eina_Bool locked = EINA_FALSE;
static Eina_Bool inloop = EINA_FALSE;
static Eina_Bool bound = EINA_FALSE;
static jmp_buf wl_fatal_buff;
static int _e_main_lvl = 0;
static int (*_e_main_shutdown_func[MAX_LEVEL])(void);
/* static Eina_List *_idle_before_list = NULL; */
static Ecore_Idle_Enterer *_idle_before = NULL;
static Ecore_Idle_Enterer *_idle_after = NULL;
static Ecore_Idle_Enterer *_idle_flush = NULL;
static Ecore_Event_Handler *_hdl_bound = NULL;

/* external variables */
EAPI Eina_Bool e_precache_end = EINA_FALSE;
EAPI Eina_Bool wl_fatal = EINA_FALSE;
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

   _idle_before = ecore_idle_enterer_before_add(_e_main_cb_idle_before, NULL);

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

   TS("E_Config Init");
   if (!e_config_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its config system.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Config Init Done");
   _e_main_shutdown_push(e_config_shutdown);

   TS("E Paths Init");
   if (!_e_main_paths_init())
     {
        e_error_message_show(_("Enlightenment cannot set up paths for finding files.\n"
                               "Perhaps you are out of memory?"));
        _e_main_shutdown(-1);
     }
   TS("E Paths Init Done");
   _e_main_shutdown_push(_e_main_paths_shutdown);

   TS("E_Env Init");
   if (!e_env_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its environment.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Env Init Done");
   _e_main_shutdown_push(e_env_shutdown);

   e_util_env_set("E_ICON_THEME", e_config->icon_theme);
   ecore_exe_run_priority_set(e_config->priority);
   locked |= e_config->desklock_start_locked;

   s = getenv("E_DESKLOCK_LOCKED");
   if ((s) && (!strcmp(s, "locked"))) waslocked = EINA_TRUE;

   if (!_e_main_xdg_dirs_check())
     _e_main_shutdown(-1);

   edje_frametime_set(1.0 / e_config->framerate);

   /* NB: We need to init the e_module subsystem first, then we can init 
    * the "main" compositor subsytem. Reasoning is that the compositor 
    * subsystem will load the specific compositor (x11, drm, etc) and that the 
    * specific compositor will be an e_module */
   TS("E_Module Init");
   if (!e_module_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its module system.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Module Init Done");
   _e_main_shutdown_push(e_module_shutdown);

   TS("E_Compositor Init");
   if (!e_comp_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its compositor system.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Compositor Init Done");
   _e_main_shutdown_push(e_comp_shutdown);

   TS("E_Shell Init");
   if (!e_shell_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its shell system.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Shell Init Done");
   _e_main_shutdown_push(e_shell_shutdown);

   TS("Ecore_Wayland Init");
   if (!ecore_wl_init(NULL))
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Wayland.\n"));
        _e_main_shutdown(-1);
     }
   TS("Ecore_Wayland Init Done");
   _e_main_shutdown_push(ecore_wl_shutdown);

   TS("E_Scale Init");
   if (!e_scale_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its scale system.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Scale Init Done");
   _e_main_shutdown_push(e_scale_shutdown);

   TS("E_Theme Init");
   if (!e_theme_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its theme system.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Theme Init Done");
   _e_main_shutdown_push(e_theme_shutdown);

   /* setup a handler to notify us when ecore_wl has bound the interfaces */
   _hdl_bound = 
     ecore_event_handler_add(ECORE_WL_EVENT_INTERFACES_BOUND, 
                             _e_main_cb_bound, NULL);

   /* while we are not bound, we need to iterate the main loop */
   /* NB: All of this is needed because we cannot create containers until 
    * we are able to create a canvas, and we cannot create a canvas until 
    * we have the interfaces bound */
   while (!bound)
     {
        wl_event_loop_dispatch(_e_comp->wl.loop, 0);
        ecore_main_loop_iterate();
     }

   TS("E_Pointer Init");
   if (!e_pointer_init())
     {
        e_error_message_show(_("Enlightenment cannot set up its pointer system.\n"));
        _e_main_shutdown(-1);
     }
   TS("E_Pointer Init Done");
   _e_main_shutdown_push(e_pointer_shutdown);

   TS("E_Canvas Recache");
   e_canvas_recache();
   TS("E_Canvas Recache Done");

   TS("Screens Init");
   if (!_e_main_screens_init())
     {
        e_error_message_show(_("Enlightenment set up window management for all the screens on your system.\n"
                               "Perhaps another window manager is running?\n"));
        _e_main_shutdown(-1);
     }
   TS("Screens Init Done");
   _e_main_shutdown_push(_e_main_screens_shutdown);

   TS("E_Container Freeze");
   e_container_all_freeze();
   TS("E_Container Freeze Done");

   /* TODO: init other stuff */

   _idle_flush = ecore_idle_enterer_add(_e_main_cb_idle_flush, NULL);

   TS("E_Container Thaw");
   e_container_all_thaw();
   TS("E_Container Thaw Done");

   _idle_after = ecore_idle_enterer_add(_e_main_cb_idle_after, NULL);

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

   /* TODO */
   /* if (!wl_fatal) e_canvas_idle_flush(); */

   e_config_save_flush();
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

EAPI double
e_main_ts(const char *str)
{
   double ret;

   t1 = ecore_time_unix_get();
   printf("ESTART: %1.5f [%1.5f] - %s\n", t1 - t0, t1 - t2, str);
   ret = t1 - t2;
   t2 = t1;
   return ret;
}

/* local functions */
static void 
_e_main_shutdown(int errcode)
{
   int i = 0;

   printf("E17: Begin Shutdown Procedure!\n");

   if (_idle_before) ecore_idle_enterer_del(_idle_before);
   _idle_before = NULL;
   if (_idle_after) ecore_idle_enterer_del(_idle_after);
   _idle_after = NULL;
   if (_idle_flush) ecore_idle_enterer_del(_idle_flush);
   _idle_flush = NULL;

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
   ecore_main_loop_quit();

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

static int
_e_main_paths_init(void)
{
   char buf[PATH_MAX];

   /* setup data paths */
   path_data = e_path_new();
   if (!path_data)
     {
        e_error_message_show("Cannot allocate path for path_data\n");
        return 0;
     }
   e_prefix_data_concat_static(buf, "data");
   e_path_default_path_append(path_data, buf);
   e_path_user_path_set(path_data, &(e_config->path_append_data));

   /* setup image paths */
   path_images = e_path_new();
   if (!path_images)
     {
        e_error_message_show("Cannot allocate path for path_images\n");
        return 0;
     }
   e_user_dir_concat_static(buf, "/images");
   e_path_default_path_append(path_images, buf);
   e_prefix_data_concat_static(buf, "data/images");
   e_path_default_path_append(path_images, buf);
   e_path_user_path_set(path_images, &(e_config->path_append_images));

   /* setup font paths */
   path_fonts = e_path_new();
   if (!path_fonts)
     {
        e_error_message_show("Cannot allocate path for path_fonts\n");
        return 0;
     }
   e_user_dir_concat_static(buf, "/fonts");
   e_path_default_path_append(path_fonts, buf);
   e_prefix_data_concat_static(buf, "data/fonts");
   e_path_default_path_append(path_fonts, buf);
   e_path_user_path_set(path_fonts, &(e_config->path_append_fonts));

   /* setup theme paths */
   path_themes = e_path_new();
   if (!path_themes)
     {
        e_error_message_show("Cannot allocate path for path_themes\n");
        return 0;
     }
   e_user_dir_concat_static(buf, "/themes");
   e_path_default_path_append(path_themes, buf);
   e_prefix_data_concat_static(buf, "data/themes");
   e_path_default_path_append(path_themes, buf);
   e_path_user_path_set(path_themes, &(e_config->path_append_themes));

   /* setup icon paths */
   path_icons = e_path_new();
   if (!path_icons)
     {
        e_error_message_show("Cannot allocate path for path_icons\n");
        return 0;
     }
   e_user_dir_concat_static(buf, "/icons");
   e_path_default_path_append(path_icons, buf);
   e_prefix_data_concat_static(buf, "data/icons");
   e_path_default_path_append(path_icons, buf);
   e_path_user_path_set(path_icons, &(e_config->path_append_icons));

   /* setup module paths */
   path_modules = e_path_new();
   if (!path_modules)
     {
        e_error_message_show("Cannot allocate path for path_modules\n");
        return 0;
     }
   e_user_dir_concat_static(buf, "/modules");
   e_path_default_path_append(path_modules, buf);
   snprintf(buf, sizeof(buf), "%s/enlightenment/modules", e_prefix_lib_get());
   e_path_default_path_append(path_modules, buf);
   /* FIXME: eventually this has to go - moduels should have installers that
    * add appropriate install paths (if not installed to user homedir) to
    * e's module search dirs
    */
   snprintf(buf, sizeof(buf), "%s/enlightenment/modules_extra", e_prefix_lib_get());
   e_path_default_path_append(path_modules, buf);
   e_path_user_path_set(path_modules, &(e_config->path_append_modules));

   /* setup background paths */
   path_backgrounds = e_path_new();
   if (!path_backgrounds)
     {
        e_error_message_show("Cannot allocate path for path_backgrounds\n");
        return 0;
     }
   e_user_dir_concat_static(buf, "/backgrounds");
   e_path_default_path_append(path_backgrounds, buf);
   e_prefix_data_concat_static(buf, "data/backgrounds");
   e_path_default_path_append(path_backgrounds, buf);
   e_path_user_path_set(path_backgrounds, &(e_config->path_append_backgrounds));

   path_messages = e_path_new();
   if (!path_messages)
     {
        e_error_message_show("Cannot allocate path for path_messages\n");
        return 0;
     }
   e_user_dir_concat_static(buf, "/locale");
   e_path_default_path_append(path_messages, buf);
   e_path_default_path_append(path_messages, e_prefix_locale_get());
   e_path_user_path_set(path_messages, &(e_config->path_append_messages));

   return 1;
}

static int
_e_main_paths_shutdown(void)
{
   if (path_data)
     {
        e_object_del(E_OBJECT(path_data));
        path_data = NULL;
     }
   if (path_images)
     {
        e_object_del(E_OBJECT(path_images));
        path_images = NULL;
     }
   if (path_fonts)
     {
        e_object_del(E_OBJECT(path_fonts));
        path_fonts = NULL;
     }
   if (path_themes)
     {
        e_object_del(E_OBJECT(path_themes));
        path_themes = NULL;
     }
   if (path_icons)
     {
        e_object_del(E_OBJECT(path_icons));
        path_icons = NULL;
     }
   if (path_modules)
     {
        e_object_del(E_OBJECT(path_modules));
        path_modules = NULL;
     }
   if (path_backgrounds)
     {
        e_object_del(E_OBJECT(path_backgrounds));
        path_backgrounds = NULL;
     }
   if (path_messages)
     {
        e_object_del(E_OBJECT(path_messages));
        path_messages = NULL;
     }
   return 1;
}

static Eina_Bool 
_e_main_xdg_dirs_check(void)
{
   const char *s, *p;
   char npath[PATH_MAX], buff[PATH_MAX];

   if (!(p = e_prefix_get())) return EINA_FALSE;

   snprintf(npath, sizeof(npath), "%s:%s/share", e_prefix_data_get(), p);
   if ((s = getenv("XDG_DATA_DIRS")))
     {
        if (strncmp(s, npath, strlen(npath)))
          {
             snprintf(buff, sizeof(buff), "%s:%s", npath, s);
             e_util_env_set("XDG_DATA_DIRS", buff);
          }
     }
   else
     {
        snprintf(buff, sizeof(buff), "%s:/usr/local/share:/usr/share", npath);
        e_util_env_set("XDG_DATA_DIRS", buff);
     }

   if ((s = getenv("XDG_RUNTIME_DIR")))
     {
        if (!ecore_file_is_dir(s))
          {
             ERR("XDG_RUNTIME_DIR %s is not a directory", s);
             return EINA_FALSE;
          }
        if ((!ecore_file_can_read(s)) || (!ecore_file_can_write(s)))
          {
             ERR("Cannot read or write to XDG_RUNTIME_DIR: %s", s);
             return EINA_FALSE;
          }
     }
   else
     {
        ERR("XDG_RUNTIME_DIR Not Set. Using '/tmp'");
        e_util_env_set("XDG_RUNTIME_DIR", "/tmp");
     }

   return EINA_TRUE;
}

static int 
_e_main_screens_init(void)
{
   E_Compositor *comp;
   E_Output *output;
   Eina_List *l;
   unsigned int i = 0;

   /* check for valid compositor */
   if (!(comp = e_compositor_get())) return 0;

   TS("\tScreens: manager");
   if (!e_manager_init()) return 0;

   TS("\tScreens: container");
   if (!e_container_init()) return 0;

   TS("\tScreens: zone");
   TS("\tScreens: desk");
   TS("\tScreens: menu");
   /* TODO: exehist */

   EINA_LIST_FOREACH(comp->outputs, l, output)
     {
        E_Manager *man;
        E_Container *con;

        /* try to create a new manager on this output */
        if (!(man = e_manager_new(output, i)))
          {
             e_error_message_show(_("Cannot create manager object\n"));
             return 0;
          }

        /* try to create a new container on this manager */
        if (!(con = e_container_new(man)))
          {
             e_error_message_show(_("Cannot create manager object\n"));
             e_object_del(E_OBJECT(man));
             return 0;
          }

        e_container_show(con);

        i++;
     }

   return 1;
}

static int 
_e_main_screens_shutdown(void)
{
   /* e_menu_shutdown(); */
   /* e_desk_shutdown(); */
   /* e_zone_shutdown(); */
   e_container_shutdown();
   e_manager_shutdown();
   return 1;
}

static Eina_Bool 
_e_main_cb_idle_before(void *data EINA_UNUSED)
{
   /* TODO: finish */
   e_pointer_idler_before();

   edje_thaw();

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_main_cb_idle_after(void *data EINA_UNUSED)
{
   edje_freeze();
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_main_cb_idle_flush(void *data EINA_UNUSED)
{
   eet_clearcache();
   ecore_wl_flush();
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_e_main_cb_bound(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   bound = EINA_TRUE;
   return ECORE_CALLBACK_CANCEL;
}
