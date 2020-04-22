#include "e_system.h"

Eina_Bool alert_backlight_reset = EINA_FALSE;

uid_t uid = -1; // uid of person running me
gid_t gid = -1; // gid of person running me

static int
_conf_allow_deny(const char *cmd, const char *glob)
{
   if (!strcmp(cmd, "allow:"))
     {
        if (!strcmp(glob, "*")) return 1; // allow
     }
   else if (!strcmp(cmd, "deny:"))
     {
        if (!strcmp(glob, "*")) return -1; // deny
     }
   return 0; // unknown
}

static void
_etc_enlightenment_system_conf(void)
{
#define MAXGROUPS 1024
   int gn, i;
   gid_t gl[MAXGROUPS];
   char type[32], usergroup[256], cmd[32], glob[256];
   Eina_Bool in_usergroup;
   FILE *f = fopen("/etc/enlightenment/system.conf", "r");
   if (!f) return;

   gn = getgroups(MAXGROUPS, gl);
   if (gn < 0)
     {
        ERR("User %i member of too many groups\n", uid);
        exit(9);
     }
   if (fscanf(f, "%31s %255s %31s %255s\n", type, usergroup, cmd, glob) == 4)
     {
        if (!strcmp(type, "user:"))
          {
             struct passwd *pw = getpwuid(uid);

             in_usergroup = EINA_FALSE;
             if (pw)
               {
                  if (!fnmatch(usergroup, pw->pw_name, 0))
                    in_usergroup = EINA_TRUE;
               }
             if (in_usergroup)
               {
                  int ok = _conf_allow_deny(cmd, glob);
                  if (ok == 1) goto allow;
                  else if (ok == -1) goto deny;
               }
          }
        else if (!strcmp(type, "group:"))
          {
             struct group *gp;

             in_usergroup = EINA_FALSE;
             gp = getgrgid(gid);
             if (gp)
               {
                  for (i = 0; i < gn; i++)
                    {
                       gp = getgrgid(gl[i]);
                       if (!fnmatch(usergroup, gp->gr_name, 0))
                         {
                            in_usergroup = EINA_TRUE;
                            break;
                         }
                    }
               }
             if (in_usergroup)
               {
                  int ok = _conf_allow_deny(cmd, glob);
                  if (ok == 1) goto allow;
                  else if (ok == -1) goto deny;
               }
          }
     }
allow:
   fclose(f);
   return;
deny:
   fclose(f);
   ERR("Permission denied to use this tool\n");
   exit(11);
}

static void
setuid_setup(void)
{
   struct passwd *pwent;
   static char buf[4096];

   uid = getuid();
   gid = getgid();

   if (setuid(0) != 0)
     {
        fprintf(stderr, "Unable to assume root user privileges\n");
        exit(5);
     }
   if (setgid(0) != 0)
     {
        fprintf(stderr, "Unable to assume root group privileges\n");
        exit(7);
     }

   pwent = getpwuid(getuid());
   if (!pwent)
     {
        fprintf(stderr, "Unable to obtain passwd entry\n");
        exit(1);
     }

   snprintf(buf, sizeof(buf), "HOME=%s", pwent->pw_dir);
   if (putenv(buf) == -1)
     {
        fprintf(stderr, "Unable to set $HOME environment\n");
        exit(1);
     }
   // change CWD to / to avoid path search dlopens finding libs in ./
   chdir("/");

   // die with parent - special as this is setuid
#ifdef HAVE_PRCTL
   prctl(PR_SET_PDEATHSIG, SIGTERM);
#elif defined(HAVE_PROCCTL)
   int sig = SIGTERM;
   procctl(P_PID, 0, PROC_PDEATHSIG_CTL, &sig);
#endif

#ifdef HAVE_UNSETENV
# define NOENV(x) unsetenv(x)
   // pass 1 - just nuke known dangerous env vars brutally if possible via
   // unsetenv(). if you don't have unsetenv... there's pass 2 and 3
   NOENV("IFS");
   NOENV("CDPATH");
   NOENV("LOCALDOMAIN");
   NOENV("RES_OPTIONS");
   NOENV("HOSTALIASES");
   NOENV("NLSPATH");
   NOENV("PATH_LOCALE");
   NOENV("COLORTERM");
   NOENV("LANG");
   NOENV("LANGUAGE");
   NOENV("LINGUAS");
   NOENV("TERM");
   NOENV("LD_PRELOAD");
   NOENV("LD_LIBRARY_PATH");
   NOENV("SHLIB_PATH");
   NOENV("LIBPATH");
   NOENV("AUTHSTATE");
   NOENV("DYLD_*");
   NOENV("KRB_CONF*");
   NOENV("KRBCONFDIR");
   NOENV("KRBTKFILE");
   NOENV("KRB5_CONFIG*");
   NOENV("KRB5_KTNAME");
   NOENV("VAR_ACE");
   NOENV("USR_ACE");
   NOENV("DLC_ACE");
   NOENV("TERMINFO");
   NOENV("TERMINFO_DIRS");
   NOENV("TERMPATH");
   NOENV("TERMCAP");
   NOENV("ENV");
   NOENV("BASH_ENV");
   NOENV("PS4");
   NOENV("GLOBIGNORE");
   NOENV("SHELLOPTS");
   NOENV("JAVA_TOOL_OPTIONS");
   NOENV("PERLIO_DEBUG");
   NOENV("PERLLIB");
   NOENV("PERL5LIB");
   NOENV("PERL5OPT");
   NOENV("PERL5DB");
   NOENV("FPATH");
   NOENV("NULLCMD");
   NOENV("READNULLCMD");
   NOENV("ZDOTDIR");
   NOENV("TMPPREFIX");
   NOENV("PYTHONPATH");
   NOENV("PYTHONHOME");
   NOENV("PYTHONINSPECT");
   NOENV("RUBYLIB");
   NOENV("RUBYOPT");
# ifdef HAVE_ENVIRON
   if (environ)
     {
        Eina_Bool again;
        // go over environment array again and again... safely
        do
          {
             again = EINA_FALSE;
             // walk through and find first entry that we don't like */
             for (i = 0; environ[i]; i++)
               {
                  // if it begins with any of these, it's possibly nasty */
                  if ((!strncmp(environ[i], "LD_",   3)) ||
                      (!strncmp(environ[i], "_RLD_", 5)) ||
                      (!strncmp(environ[i], "LC_",   3)) ||
                      (!strncmp(environ[i], "LDR_",  3)))
                    {
                       // unset it
                       char *tmp, *p;

                       tmp = strdup(environ[i]);
                       if (!tmp) abort();
                       p = strchr(tmp, '=');
                       if (!p) abort();
                       *p = 0;
                       NOENV(tmp);
                       free(tmp);
                       // and mark our do to try again from the start in case
                       // unsetenv changes environ ptr
                       again = EINA_TRUE;
                       break;
                    }
               }
          }
        while (again);
     }
# endif
#endif
   // pass 2 - clear entire environment so it doesn't exist at all. if you
   // can't do this... you're possibly in trouble... but the worst is still
   // fixed in pass 3
#ifdef HAVE_CLEARENV
   clearenv();
#else
# ifdef HAVE_ENVIRON
   environ = NULL;
# endif
#endif
   // pass 3 - set path and ifs to minimal defaults
   putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin");
   putenv("IFS= \t\n");
   _etc_enlightenment_system_conf();
}

// no singleton mode - this is not really a bonus, just painful, so disable
// but keep code aroun for future possible use
/*
static void
_cb_die(void *data EINA_UNUSED, Ecore_Thread *th EINA_UNUSED)
{
   char buf[256];
   int f;

   snprintf(buf, sizeof(buf), "/var/run/enlightenment_sys-%u.lck", uid);
   f = open(buf, O_RDONLY);
   if (f < 0) exit(0);
   exit(0);
}

static void
singleton_setup(void)
{ // only one per uid - kill existing and replace...
   char buf[256];
   int f;
   mode_t um;

   snprintf(buf, sizeof(buf), "/var/run/enlightenment_sys-%u.lck", uid);
   f = open(buf, O_WRONLY | O_NONBLOCK);
   if (f >= 0)
     {
        if (write(f, buf, 1) == 1)
          ERR("Replacing previous enlightenment_system\n");
        close(f);
     }
   unlink(buf);
   um = umask(0);
   mkfifo(buf, S_IRUSR | S_IWUSR);
   umask(um);

   ecore_thread_feedback_run(_cb_die, NULL, NULL, NULL, NULL, EINA_TRUE);
}
*/

static Eina_Bool
_cb_idle_enterer(void *data EINA_UNUSED)
{
   // welcome to unportable code land... :)
   // trim process down as much as possible when going idle
#ifdef HAVE_MALLOC_TRIM
   malloc_trim(0);
#endif
   return ECORE_CALLBACK_RENEW;
}

int
main(int argc EINA_UNUSED, const char **argv EINA_UNUSED)
{
   const char *s;

   // special mode to reset all newly found bl devices to max on
   // discovery because we were run by the e alert crash handler and
   // the user needs to see it...
   s = getenv("E_ALERT_BACKLIGHT_RESET");
   if ((s) && (s[0]  == '1')) alert_backlight_reset = EINA_TRUE;

   setuid_setup();

   ecore_app_no_system_modules();

   eina_init();
   ecore_init();
   ecore_file_init();
#ifdef HAVE_EEZE
   eeze_init();
#endif
   eet_init();

//   singleton_setup();

   e_system_inout_init();
   e_system_backlight_init();
   e_system_ddc_init();
   e_system_storage_init();
   e_system_power_init();
   e_system_rfkill_init();
   e_system_l2ping_init();
   e_system_cpufreq_init();

   ecore_idle_enterer_add(_cb_idle_enterer, NULL);

   ecore_main_loop_begin();

   e_system_cpufreq_shutdown();
   e_system_l2ping_shutdown();
   e_system_rfkill_shutdown();
   e_system_power_shutdown();
   e_system_storage_shutdown();
   e_system_ddc_shutdown();
   e_system_backlight_shutdown();
   e_system_inout_shutdown();

   eet_shutdown();
#ifdef HAVE_EEZE
   eeze_shutdown();
#endif
   ecore_file_shutdown();
   ecore_shutdown();
   eina_shutdown();
   return 0;
}
