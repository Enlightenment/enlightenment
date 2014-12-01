#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>

/* the ptrace interface used here is really linux specific -
 * FreeBSD, NetBSD and Mac OS X use slightly different ptrace API that should
 * still be feasible to use (but not compatible), OpenBSD uses a really old
 * version of the FreeBSD API that lacks required functionality we need, and
 * Solaris doesn't have ptrace at all
 */
#ifdef HAVE_SYS_PTRACE_H
# ifdef __linux__
#  include <sys/ptrace.h>
# else
#  undef HAVE_SYS_PTRACE_H
# endif
#endif

#include <limits.h>
#include <fcntl.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#include <signal.h>
#include <errno.h>

#include <Eina.h>
#include <Evas.h>

# define E_CSERVE

static Eina_Bool stop_ptrace = EINA_FALSE;

static void env_set(const char *var, const char *val);
EAPI int    prefix_determine(char *argv0);

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
EAPI int
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
_env_path_prepend(const char *env, const char *path)
{
   char *p, *s;
   const char *p2;
   int len = 0, len2 = 0;

   if (!path) return;

   p = getenv(env);
   p2 = path;
   len2 = strlen(p2);
   if (p)
     {
        len = strlen(p);
        // path already there at the start. dont prepend. :)
        if ((!strcmp(p, p2)) ||
            ((len > len2) &&
             (!strncmp(p, p2, len2)) &&
             (p[len2] == ':')))
          return;
     }
   s = malloc(len + 1 + len2 + 1);
   if (s)
     {
        s[0] = 0;
        strcat(s, p2);
        if (p)
          {
             strcat(s, ":");
             strcat(s, p);
          }
        env_set(env, s);
        free(s);
     }
}

static void
_env_path_append(const char *env, const char *path)
{
   char *p, *s;
   const char *p2;
   int len = 0, len2 = 0;

   if (!path) return;

   p = getenv(env);
   p2 = path;
   len2 = strlen(p2);
   if (p)
     {
        len = strlen(p);
        // path already there at the end. dont append. :)
        if ((!strcmp(p, p2)) ||
            ((len > len2) &&
             (!strcmp((p + len - len2), p2)) &&
             (p[len - len2 - 1] == ':')))
          return;
     }
   s = malloc(len + 1 + len2 + 1);
   if (s)
     {
        s[0] = 0;
        if (p)
          {
             strcat(s, p);
             strcat(s, ":");
          }
        strcat(s, p2);
        env_set(env, s);
        free(s);
     }
}

static void
_sigusr1(int x __UNUSED__, siginfo_t *info __UNUSED__, void *data __UNUSED__)
{
   struct sigaction action;

   /* release ptrace */
   stop_ptrace = EINA_TRUE;

   action.sa_sigaction = _sigusr1;
   action.sa_flags = SA_RESETHAND;
   sigemptyset(&action.sa_mask);
   sigaction(SIGUSR1, &action, NULL);
}

#ifdef E_CSERVE
static pid_t
_cserve2_start()
{
   pid_t cs_child;
   cs_child = fork();
   if (cs_child == 0)
     {
        char *cs_args[2] = { NULL, NULL };

        cs_args[0] = (char *)evas_cserve_path_get();
        execv(cs_args[0], cs_args);
        exit(-1);
     }
   else if (cs_child > 0)
     {
        putenv("EVAS_CSERVE2=1");
     }
   else
     {
        unsetenv("EVAS_CSERVE2");
     }
   return cs_child;
}
#endif

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
_sig_continue(siginfo_t sig)
{
   return (sig.si_signo != SIGSEGV &&
           sig.si_signo != SIGFPE &&
//         sig.si_signo != SIGBUS &&
           sig.si_signo != SIGABRT);
}

static void
_sig_remember(siginfo_t sig, Eina_Bool *susr1, Eina_Bool *sill)
{
   if (sig.si_signo == SIGUSR1)
     {
        if (*sill) *susr1 = EINA_TRUE;
     }
   else
     *sill = (sig.si_signo == SIGILL);
}

static int
_e_ptrace_attach(int child, int *status, Eina_Bool really_know)
{
#ifdef HAVE_SYS_PTRACE_H
   int result = 0;

   if (really_know)
     return waitpid(child, status, 0);

   ptrace(PT_ATTACH, child, NULL, 0);
   result = waitpid(child, status, 0);

   if (!stop_ptrace && WIFSTOPPED(*status))
     ptrace(PT_CONTINUE, child, NULL, 0);
#else
   (void)child;
   (void)really_know;
   (void)status;
   return 0;
#endif
}

static void
_e_ptrace_detach(int child, int back, Eina_Bool really_know)
{
#ifdef HAVE_SYS_PTRACE_H
   if (!really_know)
     ptrace(PT_DETACH, child, NULL, back);
#else
   (void)child;
   (void)back;
   (void)really_know);
#endif
}

static void
_e_ptrace_traceme(Eina_Bool really_know)
{
#ifdef HAVE_SYS_PTRACE_H
   if (!really_know)
     ptrace(PT_TRACE_ME, 0, NULL, 0);
#else
   (void)really_know;
#endif
}

static int
_e_ptrace_getsiginfo(int child, siginfo_t *sig, Eina_Bool really_know)
{
   memset(sig, 0, sizeof(siginfo_t));
#ifdef HAVE_SYS_PTRACE_H
   if (!really_know)
     return ptrace(PT_GETSIGINFO, child, NULL, sig);
#else
   (void)really_know;
#endif
   return 0;
}

static void
_e_ptrace_continue(int child, int back, Eina_Bool really_know)
{
#ifdef HAVE_SYS_PTRACE_H
   if (!really_know)
     ptrace(PT_CONTINUE, child, NULL, back);
#else
   (void)child;
   (void)back;
   (void)really_know);
#endif
}

static int
_e_start_child(char **args, Eina_Bool really_know)
{
   _e_ptrace_traceme(really_know);
    execv(args[0], args);
    /* We failed, 0 mean normal exit from E with no restart or crash so let exit */
    return 0;
}

static Eina_Bool
_e_ptrace_kernel_check()
{
#ifdef __linux__
   /* Check if patch to prevent ptrace to another process is present in the kernel. */
   Eina_Bool ret = EINA_FALSE;
   int fd = open("/proc/sys/kernel/yama/ptrace_scope", O_RDONLY);
   if (fd != -1)
    {
       char c;
       ret = (read(fd, &c, sizeof (c)) == sizeof (c) && c != '0');
    }
    close(fd);
    return ret;
#else
   return EINA_FALSE;
#endif
}

static int
_e_call_gdb(int child, const char *home, char **backtrace_str)
{
   int r = 0;
   char buf[4096];
   /* call e_sys gdb */
   snprintf(buf, sizeof(buf),
            "gdb "
            "--pid=%i "
            "-batch "
            "-ex 'set logging file %s/.e-crashdump.txt' "
            "-ex 'set logging on' "
            "-ex 'thread apply all backtrace full' "
            "-ex detach > /dev/null 2>&1 < /dev/zero",
            child,
            home);
   r = system(buf);

   fprintf(stderr, "called gdb with '%s' = %i\n",
           buf, WEXITSTATUS(r));

   snprintf(buf, 4096,
            "%s/.e-crashdump.txt",
            home);

   *backtrace_str = strdup(buf);
   return WEXITSTATUS(r);
}

static int
_e_call_alert(int child, siginfo_t sig, int exit_gdb, const char *backtrace_str,
              Eina_Bool susr1)
{
   char buf[4096];
   snprintf(buf, sizeof(buf),
            backtrace_str ?
            "%s/enlightenment/utils/enlightenment_alert %i %i %i '%s'" :
            "%s/enlightenment/utils/enlightenment_alert %i %i %i",
            eina_prefix_lib_get(pfx),
            (sig.si_signo == SIGSEGV && susr1) ? SIGILL : sig.si_signo,
            child,
            exit_gdb,
            backtrace_str);
   return system(buf);
}

int
main(int argc, char **argv)
{
   int i, valgrind_mode = 0;
   int valgrind_tool = 0;
   int valgrind_gdbserver = 0;
   char buf[16384], **args, *home;
   char valgrind_path[PATH_MAX] = "";
   const char *valgrind_log = NULL;
   Eina_Bool really_know = EINA_FALSE;
   struct sigaction action;
   pid_t child = -1;
#ifdef E_CSERVE
   pid_t cs_child = -1;
   Eina_Bool cs_use = EINA_FALSE;
#endif
   Eina_Bool restart = EINA_TRUE;

   unsetenv("NOTIFY_SOCKET");

   /* Setup USR1 to detach from the child process and let it get gdb by advanced users */
   action.sa_sigaction = _sigusr1;
   action.sa_flags = SA_RESETHAND;
   sigemptyset(&action.sa_mask);
   sigaction(SIGUSR1, &action, NULL);

   eina_init();

   /* reexcute myself with dbus-launch if dbus-launch is not running yet */
   if ((!getenv("DBUS_SESSION_BUS_ADDRESS")) &&
       (!getenv("DBUS_LAUNCHD_SESSION_BUS_SOCKET")))
     {
        char **dbus_argv;

        dbus_argv = alloca((argc + 3) * sizeof (char *));
        dbus_argv[0] = "dbus-launch";
        dbus_argv[1] = "--exit-with-session";
        copy_args(dbus_argv + 2, argv, argc);
        dbus_argv[2 + argc] = NULL;
        execvp("dbus-launch", dbus_argv);
     }

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

   if (really_know)
     _env_path_append("PATH", eina_prefix_bin_get(pfx));
   else
     _env_path_prepend("PATH", eina_prefix_bin_get(pfx));

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

   /* mtrack memory tracker support */
   home = getenv("HOME");
   if (home)
     {
        FILE *f;
        const char *tmps;

        /* if you have ~/.e-mtrack, then the tracker will be enabled
         * using the content of this file as the path to the mtrack.so
         * shared object that is the mtrack preload */
        snprintf(buf, sizeof(buf), "%s/.e-mtrack", home);
        f = fopen(buf, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f))
               {
                  int len = strlen(buf);
                  if ((len > 1) && (buf[len - 1] == '\n'))
                    {
                       buf[len - 1] = 0;
                       len--;
                    }
                  env_set("LD_PRELOAD", buf);
                  env_set("MTRACK", "track");
                  env_set("E_START_MTRACK", "track");
                  snprintf(buf, sizeof(buf), "%s/.e-mtrack.log", home);
                  env_set("MTRACK_TRACE_FILE", buf);
               }
             fclose(f);
          }

        tmps = getenv("XDG_DATA_HOME");
        if (tmps)
          snprintf(buf, sizeof(buf), "%s/Applications/.bin", tmps);
        else
          snprintf(buf, sizeof(buf), "%s/Applications/.bin", home);

        if (really_know)
          _env_path_append("PATH", buf);
        else
          _env_path_prepend("PATH", buf);
     }

   /* run e directly now */
   snprintf(buf, sizeof(buf), "%s/enlightenment", eina_prefix_bin_get(pfx));

   args = alloca((argc + 2 + VALGRIND_MAX_ARGS) * sizeof(char *));
   i = valgrind_append(args, valgrind_gdbserver, valgrind_mode, valgrind_tool,
                       valgrind_path, valgrind_log);
   args[i++] = buf;
   copy_args(args + i, argv + 1, argc - 1);
   args[i + argc - 1] = NULL;

   if (valgrind_tool || valgrind_mode)
     really_know = EINA_TRUE;

   /* not run at the moment !! */
#ifdef E_CSERVE
   if (getenv("E_CSERVE"))
     {
        cs_use = EINA_TRUE;
        cs_child = _cserve2_start();
     }
#endif

   /* Now looping until */
   while (restart)
     {
        pid_t result;
        int status;
        Eina_Bool done = EINA_FALSE;
        Eina_Bool remember_sigill = EINA_FALSE;
        Eina_Bool remember_sigusr1 = EINA_FALSE;

        stop_ptrace = EINA_FALSE;

        child = fork();

        if (child < 0)
          return -1;
        else if (child == 0)
          return _e_start_child(args, really_know);

        /* in the parent - ptrace attach and continue */
        env_set("E_RESTART", "1");
        _e_ptrace_attach(child, &status, really_know);

        /* now loop until done */
not_done:
        result = waitpid(child, &status, WNOHANG);
        /* Wait for evas_cserve2 and E */
        if (!result)
          result = waitpid(-1, &status, 0);

        if (result == child)
          {
             if (WIFSTOPPED(status) && !stop_ptrace)
               {
                  char buffer[4096];
                  char *backtrace_str = NULL;

                  siginfo_t sig;
                  int r = _e_ptrace_getsiginfo(child, &sig,
                                               really_know);

                  int back = (r == 0 && sig.si_signo != SIGTRAP)
                              ? sig.si_signo : 0;

                  _sig_remember(sig, &remember_sigusr1, &remember_sigill);

                  if (r != 0 || _sig_continue(sig))
                    {
                       _e_ptrace_continue(child, back, really_know);
                       goto not_done;
                    }
                  _e_ptrace_detach(child, back, really_know);

                  /* And call gdb if available */
                  if (home && !_e_ptrace_kernel_check())
                    r = _e_call_gdb(child, home, &backtrace_str);
                  else
                    r = 0;

                  /* call e_alert */
                  r = _e_call_alert(child, sig, r, backtrace_str,
                                    remember_sigusr1);
                  free(backtrace_str);

                  /* kill e */
                  kill(child, SIGKILL);

                  if (WEXITSTATUS(r) != 1)
                    restart = EINA_FALSE;
               }
             else if (!WIFEXITED(status) || stop_ptrace)
               done = EINA_TRUE;
          }
        else if (result == -1)
          {
             if (errno != EINTR)
               {
                  done = EINA_TRUE;
                  restart = EINA_FALSE;
               }
             else if (stop_ptrace)
               {
                  kill(child, SIGSTOP);
                  usleep(200000);
                  _e_ptrace_detach(child, 0, really_know);
               }
          }
#ifdef E_CSERVE
        else if (cs_use && (result == cs_child))
          {
             if (WIFSIGNALED(status))
               {
                  printf("E - cserve2 terminated with signal %d\n",
                         WTERMSIG(status));
                  cs_child = _cserve2_start();
               }
             else if (WIFEXITED(status))
               {
                  printf("E - cserve2 exited with code %d\n",
                         WEXITSTATUS(status));
                  cs_child = -1;
               }
          }
#endif
        if (!done)
          goto not_done;
     }

#ifdef E_CSERVE
   if (cs_child > 0)
     {
        pid_t result;
        int status;

        alarm(2);
        kill(cs_child, SIGINT);
        result = waitpid(cs_child, &status, 0);
        if (result != cs_child)
          {
             printf("E - cserve2 did not shutdown in 2 seconds, killing!\n");
             kill(cs_child, SIGKILL);
          }
     }
#endif

   return -1;
}

