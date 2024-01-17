#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#ifdef HAVE_PRCTL
# include <sys/prctl.h>
#elif defined(HAVE_PROCCTL)
# include <sys/procctl.h>
#endif

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

#include <fcntl.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#include <signal.h>
#include <errno.h>

# if !defined (__FreeBSD__) && !defined (__OpenBSD__)
#  ifdef HAVE_MALLOC_H
#   include <malloc.h>
#  endif
# endif

#include <Eina.h>

#define myasprintf(__b, __fmt, args...) do { \
   char __bb[sizeof(__fmt) + 1]; \
   int __cnt = snprintf(__bb, sizeof(__bb), __fmt, ##args); \
   if (__cnt >= 0) { \
      *(__b) = alloca(__cnt + 1); \
      snprintf(*(__b), __cnt + 1, __fmt, ##args); \
   } \
} while (0)

#ifdef E_API
# undef E_API
#endif
#ifdef WIN32
# ifdef BUILDING_DLL
#  define E_API __declspec(dllexport)
# else
#  define E_API __declspec(dllimport)
# endif
#else
# ifdef __GNUC__
#  if __GNUC__ >= 4
/* BROKEN in gcc 4 on amd64 */
#   if 0
#    pragma GCC visibility push(hidden)
#   endif
#   define E_API __attribute__ ((visibility("default")))
#  else
#   define E_API
#  endif
# else
#  define E_API
# endif
#endif

static Eina_Bool stop_ptrace = EINA_FALSE;

static void  env_set(const char *var, const char *val);
E_API int    prefix_determine(char *argv0);

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

        // yes - this does leak. we know. intentional if we set the same
        // env multiple times because we likely won't and alloca is wrong
        // as we lose the env content on the stack. but becomes part of
        // the environment so it has to stay around after putenv is called
        buf = malloc(size);
        sprintf(buf, "%s=%s", var, val);
        putenv(buf);
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

static void
copy_args(char **dst, char **src, size_t count)
{
   for (; count > 0; count--, dst++, src++) *dst = *src;
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
        // path already there at the start. don't prepend. :)
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
             if (p[0] != ':') strcat(s, ":");
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
        // path already there at the end. don't append. :)
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
             if (len > 0)
               {
                  if (s[len - 1] != ':') strcat(s, ":");
               }
          }
        strcat(s, p2);
        env_set(env, s);
        free(s);
     }
}

static void
_sigusr1(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
   struct sigaction action;

   // release ptrace
   stop_ptrace = EINA_TRUE;

   action.sa_sigaction = _sigusr1;
   action.sa_flags = SA_RESETHAND;
   sigemptyset(&action.sa_mask);
   sigaction(SIGUSR1, &action, NULL);
}

static void
_sigusr2(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
   struct sigaction action;

   putenv("E_DESKLOCK_LOCKED=locked");

   action.sa_sigaction = _sigusr2;
   action.sa_flags = SA_RESETHAND;
   sigemptyset(&action.sa_mask);
   sigaction(SIGUSR2, &action, NULL);
}

static void
_sighup(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
   struct sigaction action;

   putenv("E_DESKLOCK_LOCKED=no");

   action.sa_sigaction = _sighup;
   action.sa_flags = SA_RESETHAND;
   sigemptyset(&action.sa_mask);
   sigaction(SIGHUP, &action, NULL);
}

static void
_print_usage(const char *hstr)
{
   printf("Please run:\n"
          "\tenlightenment %s\n"
          "\tfor more options.\n",
          hstr);
   exit(0);
}

static Eina_Bool
_sig_continue(siginfo_t sig)
{
   return ((sig.si_signo != SIGSEGV) &&
           (sig.si_signo != SIGFPE) &&
//         (sig.si_signo != SIGBUS) &&
           (sig.si_signo != SIGABRT));
}

static void
_sig_remember(siginfo_t sig, Eina_Bool *susr1, Eina_Bool *sill)
{
   if (sig.si_signo == SIGUSR1)
     {
        if (*sill) *susr1 = EINA_TRUE;
     }
   else *sill = (sig.si_signo == SIGILL);
}

static int
_e_ptrace_attach(int child, int *status, Eina_Bool really_know)
{
   int result = 0;

#ifdef HAVE_SYS_PTRACE_H
   if (really_know) return waitpid(child, status, 0);

   ptrace(PT_ATTACH, child, NULL, 0);
   result = waitpid(child, status, 0);

   if (!stop_ptrace && WIFSTOPPED(*status)) ptrace(PT_CONTINUE, child, NULL, 0);
#else
   (void)child;
   (void)really_know;
   (void)status;
#endif

   return result;
}

static void
_e_ptrace_detach(int child, int back, Eina_Bool really_know)
{
#ifdef HAVE_SYS_PTRACE_H
   if (!really_know) ptrace(PT_DETACH, child, NULL, back);
#else
   (void)child;
   (void)back;
   (void)really_know;
#endif
}

static void
_e_ptrace_traceme(Eina_Bool really_know)
{
#ifdef HAVE_SYS_PTRACE_H
   if (!really_know) ptrace(PT_TRACE_ME, 0, NULL, 0);
#else
   (void)really_know;
#endif
}

static int
_e_ptrace_getsiginfo(int child, siginfo_t *sig, Eina_Bool really_know)
{
   memset(sig, 0, sizeof(siginfo_t));
#ifdef HAVE_SYS_PTRACE_H
   if (!really_know) return ptrace(PT_GETSIGINFO, child, NULL, sig);
#else
   (void)child;
   (void)sig;
   (void)really_know;
#endif
   return 0;
}

static void
_e_ptrace_continue(int child, int back, Eina_Bool really_know)
{
#ifdef HAVE_SYS_PTRACE_H
   if (!really_know) ptrace(PT_CONTINUE, child, NULL, back);
#else
   (void)child;
   (void)back;
   (void)really_know;
#endif
}

static void
_e_start_stdout_err_redir(const char *home)
{
   int logfd;
   char *logf = NULL, *logf_old = NULL;

   // rename old olg file
   myasprintf(&logf, "%s/.e-log.log", home);
   myasprintf(&logf_old, "%s/.e-log.log.old", home);
   rename(logf, logf_old);
   // open new log file and move stdout/err to it
   logfd = open(logf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
   printf("Enlightenment: See logs in: %s\n", logf);
   if (logfd < 0) return;
   dup2(logfd, 1); // stdout to file
   dup2(logfd, 2); // stderr to file
}

static int
_e_start_child(const char *home, char **args, Eina_Bool really_know)
{
   // have e process die with parent enlightenment_start
#ifdef HAVE_PRCTL
   prctl(PR_SET_PDEATHSIG, SIGTERM);
#elif defined(HAVE_PROCCTL)
   int sig = SIGTERM;
   procctl(P_PID, 0, PROC_PDEATHSIG_CTL, &sig);
#endif
   _e_start_stdout_err_redir(home);
   _e_ptrace_traceme(really_know);
   execv(args[0], args);
   // We failed, 0 means normal exit from E with no restart or crash so
   // let's exit
   return 0;
}

static Eina_Bool
_e_ptrace_kernel_check(const char *home)
{
#ifdef __linux__
   // Check if patch to prevent ptrace to another process is present
   // in the kernel
   Eina_Bool ret = EINA_FALSE;
   int fd = open("/proc/sys/kernel/yama/ptrace_scope", O_RDONLY);
   if (fd != -1)
     {
        char c;
        ret = read(fd, &c, (sizeof(c)) == sizeof(c)) && (c != '0');
        close(fd);
     }
   if (ret)
     {
        char *buf = NULL;
        FILE *f;

        myasprintf(&buf, "%s/.e-crashdump.txt", home);
        f = fopen(buf, "a");
        if (f)
          {
             fprintf(f,
                     "ERROR: /proc/sys/kernel/yama/ptrace_scope is 1 disallowing remote\n"
                     "attachment to a process. This means a gdb backtrace cannot be logged.\n"
                     "To fix this, as root please do:\n"
                     "  echo 0 > /proc/sys/kernel/yama/ptrace_scope\n");
             fclose(f);
          }
     }
   return ret;
#else
   return EINA_FALSE;
#endif
}

static int
_e_call_gdb(int child, const char *home, char **backtrace_str)
{
   int r = 0;
   char *buf = NULL;
   /* call e_sys gdb */
   myasprintf(&buf,
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

   fprintf(stderr, "called gdb with '%s' = %i\n", buf, WEXITSTATUS(r));
   myasprintf(&buf, "%s/.e-crashdump.txt", home);
   *backtrace_str = strdup(buf);
   return WEXITSTATUS(r);
}

static int
_e_call_alert(int child, siginfo_t sig, int exit_gdb, const char *backtrace_str,
              Eina_Bool susr1)
{
   char *buf = NULL;

   myasprintf(&buf,
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

int
main(int argc, char **argv)
{
   int i, ret = -1;
   char *buf = NULL, *buf2 = NULL, *buf3 = NULL, **args, *home;
   const char *bindir;
   Eina_Bool really_know = EINA_FALSE;
   struct sigaction action;
   pid_t child = -1;
   Eina_Bool restart = EINA_TRUE;
   unsigned int provided_eina_version, required_eina_version;

   unsetenv("NOTIFY_SOCKET");

   /* Setup USR1 to detach from the child process and let it get gdb by advanced users */
   action.sa_sigaction = _sigusr1;
   action.sa_flags = SA_RESETHAND;
   sigemptyset(&action.sa_mask);
   sigaction(SIGUSR1, &action, NULL);

   /* Setup USR2 to go into "lock down" mode */
   action.sa_sigaction = _sigusr2;
   action.sa_flags = SA_RESETHAND;
   sigemptyset(&action.sa_mask);
   sigaction(SIGUSR2, &action, NULL);

   /* Setup HUP to go exit "lock down" mode */
   action.sa_sigaction = _sighup;
   action.sa_flags = SA_RESETHAND;
   sigemptyset(&action.sa_mask);
   sigaction(SIGHUP, &action, NULL);

   eina_init();

   /* check eina version ... this should be the whole efl version */
   /* check for sanity here in case someone has done something very silly */
   provided_eina_version =
     (eina_version->major * 1000 * 1000) +
     (eina_version->minor * 1000       ) +
     (eina_version->micro);
   required_eina_version =
     (MIN_EFL_VERSION_MAJ * 1000 * 1000) +
     (MIN_EFL_VERSION_MIN * 1000) +
     (MIN_EFL_VERSION_MIC);
   printf("Enlightenment: EFL Version Check: %u >= %u\n",
          provided_eina_version, required_eina_version);
   if (provided_eina_version < required_eina_version)
     {
        char *logf = NULL, *logf_old = NULL;
        FILE *fps[2];
        FILE *outf;

        home = getenv("HOME");
        // rename old olg file
        if (!home)
          {
             myasprintf(&logf, ".e-log.log");
             myasprintf(&logf_old, ".e-log.log.old");
          }
        else
          {
             myasprintf(&logf, "%s/.e-log.log", home);
             myasprintf(&logf_old, "%s/.e-log.log.old", home);
          }
        rename(logf, logf_old);
        outf = fopen(logf, "w");
        fps[0] = stderr;
        fps[1] = outf;
        for (i = 0; i < 2; i++)
          {
             if (fps[i])
               fprintf(fps[i],
                       "ERROR: EFL version provided is %i.%i.%i\n"
                       "Enlightenment requires a minimum of %i.%i.%i\n"
                       "Abort\n",
                       eina_version->major,
                       eina_version->minor,
                       eina_version->micro,
                       MIN_EFL_VERSION_MAJ,
                       MIN_EFL_VERSION_MIN,
                       MIN_EFL_VERSION_MIC);
          }
        if (outf) fclose(outf);
        exit(42); // exit 42 for this as life the universe and everything...
     }

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
   putenv("E_START_MANAGER=1");

   if ((!getenv("DISPLAY")) && (!getenv("E_WL_FORCE")))
     {
        printf("***************************************************************\n");
        printf("*  You are probably starting Enlightenment in wayland DRM/KMS *\n");
        printf("***************************************************************\n");
        printf("                                                               \n");
        printf("  Wayland support is experimental. It may work for you.        \n");
        printf("  It may not. If you wish you help out then please do, but     \n");
        printf("  for a proper experience use Enlightenment in X11.            \n");
        printf("                                                               \n");
        printf("  If you do not want this pause and message, please set        \n");
        printf("  the following environment variable:                          \n");
        printf("                                                               \n");
        printf("    E_WL_FORCE=drm                                             \n");
        printf("                                                               \n");
        printf("  Example:                                                     \n");
        printf("                                                               \n");
        printf("    E_WL_FORCE=drm enlightenment_start                         \n");
        printf("                                                               \n");
        printf("  If you wish to set software or GL rendering too then also:   \n");
        printf("                                                               \n");
        printf("    E_WL_FORCE=drm E_COMP_ENGINE=gl enlightenment_start        \n");
        printf("    E_WL_FORCE=drm E_COMP_ENGINE=sw enlightenment_start        \n");
        printf("                                                               \n");
        sleep(10);
     }
   for (i = 1; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-h")) || (!strcmp(argv[i], "-help")) ||
            (!strcmp(argv[i], "--help")))
          {
             _print_usage(argv[i]);
          }
        else if (!strcmp(argv[i], "-display"))
          {
             i++;
             env_set("DISPLAY", argv[i]);
          }
        else if (!strcmp(argv[i], "-i-really-know-what-i-am-doing-and-accept"
                                  "-full-responsibility-for-it"))
          {
             really_know = EINA_TRUE;
          }
     }

   bindir = eina_prefix_bin_get(pfx);
   if (!path_contains(bindir))
     {
        if (really_know) _env_path_append("PATH", bindir);
        else _env_path_prepend("PATH", bindir);
     }
   myasprintf(&buf2,
              "E_ALERT_FONT_DIR=%s/data/fonts",
              eina_prefix_data_get(pfx));
   putenv(buf2);
   myasprintf(&buf3,
              "E_ALERT_SYSTEM_BIN=%s/enlightenment/utils/enlightenment_system",
              eina_prefix_lib_get(pfx));
   putenv(buf3);

   home = getenv("HOME");
   if (home)
     {
        const char *tmps = getenv("XDG_DATA_HOME");

        if (tmps)        myasprintf(&buf, "%s/Applications/.bin", tmps);
        else             myasprintf(&buf, "%s/Applications/.bin", home);
        if (really_know) _env_path_append("PATH", buf);
        else             _env_path_prepend("PATH", buf);
     }

   /* run e directly now */
   myasprintf(&buf, "%s/enlightenment", eina_prefix_bin_get(pfx));

   args = alloca((argc + 1) * sizeof(char *));
   printf("Enlightenment: Command: %s\n", buf);
   args[0] = buf;
   copy_args(&args[1], argv + 1, argc - 1);
   args[argc] = NULL;

   /* Now looping until */
   while (restart)
     {
        pid_t result;
        int status, back;
        Eina_Bool done = EINA_FALSE;
        Eina_Bool remember_sigill = EINA_FALSE;
        Eina_Bool remember_sigusr1 = EINA_FALSE;

        stop_ptrace = EINA_FALSE;

        child = fork();

        if (child < 0)
          {
             ret = -1;
             break;
          }
        else if (child == 0)
          { // we are in the child fork - so exec
             ret = _e_start_child(home, args, really_know);
             break;
          }

        putenv("E_RESTART_OK=");
        /* in the parent - ptrace attach and continue */
        putenv("E_RESTART=1");
        _e_ptrace_attach(child, &status, really_know);

        /* now loop until done */
not_done:
#ifdef HAVE_MALLOC_TRIM
        malloc_trim(0);
#endif
        result = waitpid(child, &status, WNOHANG);
        /* Wait for E */
        if (!result) result = waitpid(-1, &status, 0);

        if (result == child)
          {
             if (WIFSTOPPED(status) && (!stop_ptrace))
               {
                  char *backtrace_str = NULL;

                  siginfo_t sig;
                  int r = _e_ptrace_getsiginfo(child, &sig,
                                               really_know);

                  back = ((r == 0) && (sig.si_signo != SIGTRAP))
                          ? sig.si_signo : 0;

                  _sig_remember(sig, &remember_sigusr1, &remember_sigill);

                  if ((r != 0) || (_sig_continue(sig)))
                    {
                       _e_ptrace_continue(child, back, really_know);
                       goto not_done;
                    }
                  _e_ptrace_detach(child, back, really_know);
                  usleep(200000);

                  /* And call gdb if available */
                  if (home && !_e_ptrace_kernel_check(home))
                    r = _e_call_gdb(child, home, &backtrace_str);
                  else
                    r = 0;

                  /* kill e */
                  if (!getenv("DISPLAY"))
                    {
                       kill(child, SIGKILL);
                       usleep(500000);
                    }

                  /* call e_alert */
                  r = _e_call_alert(child, sig, r, backtrace_str,
                                    remember_sigusr1);
                  free(backtrace_str);

                  if (getenv("DISPLAY")) kill(child, SIGKILL);
                  if (WEXITSTATUS(r) == 101) restart = EINA_FALSE;
                  done = EINA_TRUE;
               }
             else if (!WIFEXITED(status) || (stop_ptrace))
               {
                  restart = EINA_TRUE;
                  done = EINA_TRUE;
               }
             else if (WEXITSTATUS(status) == 121)
               {
                  putenv("E_RESTART_OK=1");
                  restart = EINA_TRUE;
                  done = EINA_TRUE;
               }
             else if (WEXITSTATUS(status) == 111)
               {
                  putenv("E_RESTART_OK=1");
                  restart = EINA_TRUE;
                  done = EINA_TRUE;
               }
             else if (WEXITSTATUS(status) == 101)
               {
                  printf("Explicit error exit from enlightenment\n");
                  restart = EINA_FALSE;
                  done = EINA_TRUE;
               }
             else if (WEXITSTATUS(status) == 0)
               {
                  restart = EINA_FALSE;
                  done = EINA_TRUE;
               }
             else
               {
                  printf("Invalid exit from enlightenment: code=%i\n", WEXITSTATUS(status));
                  restart = EINA_TRUE;
                  done = EINA_TRUE;
               }
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
        if (!done) goto not_done;
     }
   return ret;
}

