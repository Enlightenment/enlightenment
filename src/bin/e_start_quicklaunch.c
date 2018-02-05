#include "config.h"

#include <Elementary.h>
#include <sys/prctl.h>
# ifdef E_API
#  undef E_API
# endif
# ifdef WIN32
#  ifdef BUILDING_DLL
#   define E_API __declspec(dllexport)
#  else
#   define E_API __declspec(dllimport)
#  endif
# else
#  ifdef __GNUC__
#   if __GNUC__ >= 4
/* BROKEN in gcc 4 on amd64 */
#    if 0
#     pragma GCC visibility push(hidden)
#    endif
#    define E_API __attribute__ ((visibility("default")))
#   else
#    define E_API
#   endif
#  else
#   define E_API
#  endif
# endif

static char e_cwd[PATH_MAX];
static char **e_args;
static int e_argc;
static Eina_Bool e_usr1;
static Ecore_Event_Handler *exit_handler;
static Ecore_Event_Handler *signal_handler;

static void env_set(const char *var, const char *val);
E_API int    prefix_determine(char *argv0);

static int e_pid = -1;
static Ecore_Exe *crash_exe;
static Ecore_Exe *bt_exe;

static void
env_set(const char *var, const char *val)
{
   if (val)
     {
#ifdef HAVE_SETENV
        setenv(var, val, 1);
#else
        char *buf;
        size_t size = strlen(var) + 1 + strlen(val) + 1;

        buf = alloca(size);
        sprintf(buf, "%s=%s", var, val);
        if (getenv(var)) putenv(buf);
        else putenv(strdup(buf));
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

/* local subsystem globals */
static Eina_Prefix *pfx = NULL;

/* externally accessible functions */
E_API int
prefix_determine(char *argv0)
{
   pfx = eina_prefix_new(argv0, prefix_determine,
                         "E", "enlightenment", "AUTHORS",
                         PACKAGE_BIN_DIR, PACKAGE_LIB_DIR,
                         PACKAGE_DATA_DIR, LOCALE_DIR);
   if (!pfx) return 0;
   return 1;
}

static int
find_valgrind(char *path, size_t path_len)
{
   const char *env = getenv("PATH");

   while (env)
     {
        const char *p = strchr(env, ':');
        ssize_t p_len;

        if (p) p_len = p - env;
        else p_len = strlen(env);
        if (p_len <= 0) goto next;
        else if (p_len + sizeof("/valgrind") >= path_len)
          goto next;
        memcpy(path, env, p_len);
        memcpy(path + p_len, "/valgrind", sizeof("/valgrind"));
        if (access(path, X_OK | R_OK) == 0) return 1;
next:
        if (p) env = p + 1;
        else break;
     }
   path[0] = '\0';
   return 0;
}

/* maximum number of arguments added above */
#define VALGRIND_MAX_ARGS 11
/* bitmask with all supported bits set */
#define VALGRIND_MODE_ALL 15

static int
valgrind_append(char **dst, int valgrind_gdbserver, int valgrind_mode, int valgrind_tool, char *valgrind_path, const char *valgrind_log)
{
   int i = 0;

   if (valgrind_tool)
     {
        dst[i++] = valgrind_path;
        switch (valgrind_tool)
          {
           case 1: dst[i++] = "--tool=massif"; break;

           case 2: dst[i++] = "--tool=callgrind"; break;
          }
        return i;
     }
   if (valgrind_gdbserver) dst[i++] = "--db-attach=yes";
   if (!valgrind_mode) return 0;
   dst[i++] = valgrind_path;
   dst[i++] = "--num-callers=40";
   dst[i++] = "--track-origins=yes";
   dst[i++] = "--malloc-fill=13"; /* invalid pointer, make it crash */
   if (valgrind_log)
     {
        static char logparam[PATH_MAX + sizeof("--log-file=")];

        snprintf(logparam, sizeof(logparam), "--log-file=%s", valgrind_log);
        dst[i++] = logparam;
     }
   if (valgrind_mode & 2) dst[i++] = "--trace-children=yes";
   if (valgrind_mode & 4)
     {
        dst[i++] = "--leak-check=full";
        dst[i++] = "--leak-resolution=high";
        dst[i++] = "--track-fds=yes";
     }
   if (valgrind_mode & 8) dst[i++] = "--show-reachable=yes";
   return i;
}

static void
copy_args(char **dst, char **src, size_t count)
{
   for (; count > 0; count--, dst++, src++)
     *dst = *src;
}

static void
_env_path_prepend(const char *env, Eina_Strbuf *buf)
{
   char *p;
   const char *p2;
   int len = 0, len2 = 0;

   p = getenv(env);
   p2 = eina_strbuf_string_get(buf);
   len2 = eina_strbuf_length_get(buf);
   if (p)
     {
        len = strlen(p);
        // path already there at the start. don't prepend. :)
        if ((!strcmp(p, p2)) ||
            ((len > len2) &&
             (!strncmp(p, p2, len2)) &&
             (p[len2] == ':')))
          return;
     }
   if (p)
     {
        if (p[len - 1] != ':')
          eina_strbuf_append_printf(buf, ":%s", p);
        else
          eina_strbuf_append(buf, p);
     }
   env_set(env, eina_strbuf_string_get(buf));
}

static void
_env_path_append(const char *env, Eina_Strbuf *buf)
{
   char *p;
   const char *p2;
   int len = 0, len2 = 0;

   p = getenv(env);
   p2 = eina_strbuf_string_get(buf);
   len2 = eina_strbuf_length_get(buf);
   if (p)
     {
        len = strlen(p);
        // path already there at the end. don't append. :)
        if ((!strcmp(p, p2)) ||
            ((len > len2) &&
             (!strcmp((p + len - len2), p2)) &&
             (p[len - len2 - 1] == ':')))
          return;
     }
   if (p)
     {
      if (len && (p[len - 1] != ':'))
        eina_strbuf_prepend_printf(buf, "%s:", p);
      else if (len)
        eina_strbuf_prepend(buf, p);
     }
   env_set(env, eina_strbuf_string_get(buf));
}

static void
_print_usage(const char *hstr)
{
   printf("Options:\n"
          "\t-valgrind[=MODE]\n"
          "\t\tRun enlightenment from inside valgrind, mode is OR of:\n"
          "\t\t   1 = plain valgrind to catch crashes (default)\n"
          "\t\t   2 = trace children (thumbnailer, efm slaves, ...)\n"
          "\t\t   4 = check leak\n"
          "\t\t   8 = show reachable after processes finish.\n"
          "\t\t all = all of above\n"
          "\t-massif\n"
          "\t\tRun enlightenment from inside massif valgrind tool.\n"
          "\t-callgrind\n"
          "\t\tRun enlightenment from inside callgrind valgrind tool.\n"
          "\t-valgrind-log-file=<FILENAME>\n"
          "\t\tSave valgrind log to file, see valgrind's --log-file for details.\n"
          "\n"
          "Please run:\n"
          "\tenlightenment %s\n"
          "for more options.\n",
          hstr);
   exit(0);
}

static Eina_Bool
_sig_continue(int sig)
{
   return (sig != SIGSEGV &&
           sig != SIGFPE &&
//         sig != SIGBUS &&
           sig != SIGABRT);
}

static void
post_fork()
{
   ecore_event_handler_del(exit_handler);
   ecore_event_handler_del(signal_handler);
   e_pid = -1;
   e_args = NULL;
   e_argc = -1;
   prctl(PR_SET_NAME, "enlightenment", NULL, NULL, NULL);
   prctl(PR_SET_PDEATHSIG, SIGKILL);
}

static void
_e_start_child()
{
   Eina_Bool first = e_pid == -1;
   // Try new form before trying old form
   if (!efl_quicklaunch_prepare(e_argc, e_args, e_cwd))
     if (!elm_quicklaunch_prepare(e_argc, e_args, e_cwd))
       {
          fprintf(stderr, "could not dlopen enlightenment binary\n");
          exit(-1);
       }

   e_pid = elm_quicklaunch_fork(e_argc, e_args, e_cwd, post_fork, NULL);
   elm_quicklaunch_cleanup();
   if (e_pid == -1) exit(-1);
   if (first) env_set("E_RESTART", "1");
}

static Eina_Bool
do_backtrace(void *buf)
{
   bt_exe = ecore_exe_run(buf, NULL);
   free(buf);
   return EINA_FALSE;
}

static void
_e_call_gdb(int child, const char *home)
{
   char buf[4096];
   /* call e_sys gdb */
   snprintf(buf, sizeof(buf),
            "coredumpctl --no-pager -q dump %d > /dev/null 2> '%s/.e-crashdump.%d.txt'",
            child, home, child);
   ecore_timer_loop_add(0.25, do_backtrace, strdup(buf));
   fprintf(stderr, "called coredumpctl dump with '%s'\n", buf);
}

static void
_e_call_alert(int child, int sig, int exit_gdb, const char *home)
{
   char path[PATH_MAX] = {0}, buf[4096];

   if (home)
     snprintf(path, sizeof(path),
              " '%s/.e-crashdump.%d.txt'",
              home, child);
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_alert %i %i %i%s",
            eina_prefix_lib_get(pfx),
            (e_usr1 && (sig == SIGSEGV)) ? SIGILL : sig,
            child,
            exit_gdb, path);
   crash_exe = ecore_exe_run(buf, NULL);
}

static int
path_contains(const char *path)
{
   char *realp, *realp2, *env2 = NULL, *p, *p2;
   char buf[PATH_MAX], buf2[PATH_MAX];
   const char *env;
   ssize_t p_len;
   int ret = 0;

   if (!path) return ret;
   realp = realpath(path, buf);
   if (!realp) realp = (char *)path;

   env = getenv("PATH");
   if (!env) goto done;
   env2 = strdup(env);
   if (!env2) goto done;

   p = env2;
   while (p)
     {
        p2 = strchr(p, ':');

        if (p2) p_len = p2 - p;
        else p_len = strlen(p);

        if (p_len <= 0) goto next;
        if (p2) *p2 = 0;
        realp2 = realpath(p, buf2);
        if (realp2)
          {
             if (!strcmp(realp, realp2)) goto ok;
          }
        else
          {
             if (!strcmp(realp, p)) goto ok;
          }
next:
        if (p2) p = p2 + 1;
        else break;
     }
   // failed to find
   goto done;
ok:
   ret = 1;
done:
   free(env2);
   return ret;
}

static Eina_Bool
e_exited(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Exe_Event_Del *ev)
{
   if (ev->pid == e_pid)
     {
        if (ev->signalled)
          {
             if (!_sig_continue(ev->exit_signal))
               {
                  char *home = getenv("HOME");
                  /* And call gdb if available */
                  if (home)
                    _e_call_gdb(e_pid, home);
                  _e_call_alert(e_pid, ev->exit_signal, 0, home);
                  e_pid = -1;
               }
          }
        else if (ev->exited) _exit(0);
     }
   else if (crash_exe && (ev->pid == ecore_exe_pid_get(crash_exe)))
     {
        crash_exe = NULL;
        if (!bt_exe)
          {
             if (ev->exited && (ev->exit_code == 1))
               _e_start_child();
             else
               _exit(1);
          }
     }
   else if (bt_exe && (ev->pid == ecore_exe_pid_get(bt_exe)))
     {
        if (ev->exit_code) //failed, probably not ready yet
          ecore_timer_loop_add(0.25, do_backtrace, strdup(ecore_exe_cmd_get(bt_exe)));
        bt_exe = NULL;
        if ((!crash_exe) && (e_pid == -1)) _e_start_child();
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
e_signal(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Signal_User *ev)
{
   if (ev->data.si_pid != e_pid) return ECORE_CALLBACK_RENEW;
   if (ev->data.si_signo == SIGUSR1) e_usr1 = 1;
   return ECORE_CALLBACK_RENEW;
}

int
main(int argc, char **argv)
{
   int i, valgrind_mode = 0;
   int valgrind_tool = 0;
   int valgrind_gdbserver = 0;
   char *home;
   char valgrind_path[PATH_MAX] = "";
   const char *valgrind_log = NULL;
   const char *bindir;
   Eina_Bool really_know = EINA_FALSE;
   Eina_Strbuf *buf;

   unsetenv("NOTIFY_SOCKET");

   elm_init(argc, argv);
   prefix_determine(argv[0]);

   env_set("E_START", argv[0]);

   for (i = 1; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-h")) || (!strcmp(argv[i], "-help")) ||
            (!strcmp(argv[i], "--help")))
          {
             _print_usage(argv[i]);
          }
        if (!strcmp(argv[i], "-valgrind-gdb"))
          valgrind_gdbserver = 1;
        else if (!strcmp(argv[i], "-massif"))
          valgrind_tool = 1;
        else if (!strcmp(argv[i], "-callgrind"))
          valgrind_tool = 2;
        else if (!strcmp(argv[i], "-display"))
          {
             i++;
             env_set("DISPLAY", argv[i]);
          }
        else if (!strncmp(argv[i], "-valgrind", sizeof("-valgrind") - 1))
          {
             const char *val = argv[i] + sizeof("-valgrind") - 1;
             switch (*val)
               {
                case '\0':
                  valgrind_mode = 1;
                  break;
                case '=':
                  val++;
                  if (!strcmp(val, "all")) valgrind_mode = VALGRIND_MODE_ALL;
                  else valgrind_mode = atoi(val);
                  break;
                case '-':
                  val++;
                  if (!strncmp(val, "log-file=", sizeof("log-file=") - 1))
                    {
                       valgrind_log = val + sizeof("log-file=") - 1;
                       if (*valgrind_log == '\0') valgrind_log = NULL;
                    }
                  break;
                default:
                  printf("Unknown valgrind option: %s\n", argv[i]);
                  break;
               }
          }
        else if (!strcmp(argv[i], "-i-really-know-what-i-am-doing-and-accept"
                                  "-full-responsibility-for-it"))
          {
             really_know = EINA_TRUE;
          }
     }

   buf = eina_strbuf_new();
   bindir = eina_prefix_bin_get(pfx);
   if (!path_contains(bindir))
     {
        eina_strbuf_append(buf, bindir);
        if (really_know) _env_path_append("PATH", buf);
        else _env_path_prepend("PATH", buf);
     }

   if ((valgrind_mode || valgrind_tool) &&
       !find_valgrind(valgrind_path, sizeof(valgrind_path)))
     {
        printf("E - valgrind required but no binary found! Ignoring request.\n");
        valgrind_mode = 0;
     }

   printf("E - PID=%i, valgrind=%d", getpid(), valgrind_mode);
   if (valgrind_mode)
     {
        printf(" valgrind-command='%s'", valgrind_path);
        if (valgrind_log)
          printf(" valgrind-log-file='%s'", valgrind_log);
     }
   putchar('\n');

   home = getenv("HOME");
   eina_strbuf_reset(buf);
   if (home)
     {
        const char *tmps = getenv("XDG_DATA_HOME");

        if (tmps)
          eina_strbuf_append_printf(buf, "%s/Applications/.bin", tmps);
        else
          eina_strbuf_append_printf(buf, "%s/Applications/.bin", home);

        if (really_know)
          _env_path_append("PATH", buf);
        else
          _env_path_prepend("PATH", buf);
     }
   eina_strbuf_reset(buf);

   /* run e directly now */
   eina_strbuf_append_printf(buf, "%s/enlightenment", eina_prefix_bin_get(pfx));

   e_args = alloca((argc + 2 + VALGRIND_MAX_ARGS) * sizeof(char *));
   i = valgrind_append(e_args, valgrind_gdbserver, valgrind_mode, valgrind_tool,
                       valgrind_path, valgrind_log);
   e_args[i++] = eina_strbuf_string_steal(buf);
   copy_args(e_args + i, argv + 1, argc - 1);
   e_args[i + argc - 1] = NULL;
   e_argc = i + argc - 1;
   eina_strbuf_free(buf);

   if (valgrind_tool || valgrind_mode)
     really_know = EINA_TRUE;

   getcwd(e_cwd, sizeof(e_cwd) - 1);
   ecore_job_add(_e_start_child, NULL);
   exit_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL, (Ecore_Event_Handler_Cb)e_exited, NULL);
   signal_handler = ecore_event_handler_add(ECORE_EVENT_SIGNAL_USER, (Ecore_Event_Handler_Cb)e_signal, NULL);
   ecore_main_loop_begin();

   return 0;
}
