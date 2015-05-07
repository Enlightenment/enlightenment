#include "e.h"

#ifdef HAVE_PAM
# include <security/pam_appl.h>
# include <pwd.h>


typedef struct E_Auth
{
   struct
   {
      struct pam_conv conv;
      pam_handle_t   *handle;
   } pam;

   char user[4096];
   char passwd[4096];
} E_Auth;

static pid_t _e_auth_child_pid = -1;

static char *
_auth_auth_get_current_user(void)
{
   char *user;
   struct passwd *pwent = NULL;

   pwent = getpwuid(getuid());
   if (!pwent) return NULL;
   user = strdup(pwent->pw_name);
   return user;
}

static int
_auth_auth_pam_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
   int replies = 0;
   E_Auth *da = (E_Auth *)appdata_ptr;
   struct pam_response *reply = NULL;

   reply = (struct pam_response *)malloc(sizeof(struct pam_response) * num_msg);

   if (!reply) return PAM_CONV_ERR;

   for (replies = 0; replies < num_msg; replies++)
     {
        switch (msg[replies]->msg_style)
          {
           case PAM_PROMPT_ECHO_ON:
             reply[replies].resp_retcode = PAM_SUCCESS;
             reply[replies].resp = strdup(da->user);
             break;

           case PAM_PROMPT_ECHO_OFF:
             reply[replies].resp_retcode = PAM_SUCCESS;
             reply[replies].resp = strdup(da->passwd);
             break;

           case PAM_ERROR_MSG:
           case PAM_TEXT_INFO:
             reply[replies].resp_retcode = PAM_SUCCESS;
             reply[replies].resp = NULL;
             break;

           default:
             free(reply);
             return PAM_CONV_ERR;
          }
     }
   *resp = reply;
   return PAM_SUCCESS;
}

static int
_auth_pam_init(E_Auth *da)
{
   int pamerr;
   const char *pam_prof;
   char *current_host;
   char *current_user;

   if (!da) return -1;

   da->pam.conv.conv = _auth_auth_pam_conv;
   da->pam.conv.appdata_ptr = da;
   da->pam.handle = NULL;

   /* try other pam profiles - and system-auth (login for fbsd users) is a fallback */
   pam_prof = "login";
   if (ecore_file_exists("/etc/pam.d/enlightenment"))
     pam_prof = "enlightenment";
   else if (ecore_file_exists("/etc/pam.d/xscreensaver"))
     pam_prof = "xscreensaver";
   else if (ecore_file_exists("/etc/pam.d/kscreensaver"))
     pam_prof = "kscreensaver";
   else if (ecore_file_exists("/etc/pam.d/system-auth"))
     pam_prof = "system-auth";
   else if (ecore_file_exists("/etc/pam.d/system"))
     pam_prof = "system";
   else if (ecore_file_exists("/etc/pam.d/xdm"))
     pam_prof = "xdm";
   else if (ecore_file_exists("/etc/pam.d/gdm"))
     pam_prof = "gdm";
   else if (ecore_file_exists("/etc/pam.d/kdm"))
     pam_prof = "kdm";

   if ((pamerr = pam_start(pam_prof, da->user, &(da->pam.conv),
                           &(da->pam.handle))) != PAM_SUCCESS)
     return pamerr;

   current_user = _auth_auth_get_current_user();

   if ((pamerr = pam_set_item(da->pam.handle, PAM_USER, current_user)) != PAM_SUCCESS)
     {
        free(current_user);
        return pamerr;
     }

   current_host = e_auth_hostname_get();
   if ((pamerr = pam_set_item(da->pam.handle, PAM_RHOST, current_host)) != PAM_SUCCESS)
     {
        free(current_user);
        free(current_host);
        return pamerr;
     }

   free(current_user);
   free(current_host);
   return 0;
}

#endif

E_API int
#ifdef HAVE_PAM
e_auth_begin(char *passwd)
{
   /* child */
   int pamerr;
   E_Auth da;
   char *current_user, *p;
   struct sigaction action;

   _e_auth_child_pid = fork();
   if (_e_auth_child_pid > 0) return _e_auth_child_pid;
   if (_e_auth_child_pid < 0) return -1;

   action.sa_handler = SIG_DFL;
   action.sa_flags = SA_ONSTACK | SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
   sigemptyset(&action.sa_mask);
   sigaction(SIGSEGV, &action, NULL);
   sigaction(SIGILL, &action, NULL);
   sigaction(SIGFPE, &action, NULL);
   sigaction(SIGBUS, &action, NULL);
   sigaction(SIGABRT, &action, NULL);

   current_user = _auth_auth_get_current_user();
   eina_strlcpy(da.user, current_user, sizeof(da.user));
   eina_strlcpy(da.passwd, passwd, sizeof(da.passwd));
   /* security - null out passwd string once we are done with it */
   for (p = passwd; *p; p++)
     *p = 0;
   da.pam.handle = NULL;
   da.pam.conv.conv = NULL;
   da.pam.conv.appdata_ptr = NULL;

   pamerr = _auth_pam_init(&da);
   if (pamerr != PAM_SUCCESS)
     {
        free(current_user);
        exit(1);
     }
   pamerr = pam_authenticate(da.pam.handle, 0);
   pam_end(da.pam.handle, pamerr);
   /* security - null out passwd string once we are done with it */
   memset(da.passwd, 0, sizeof(da.passwd));
   /* break compiler optimization */
   if (da.passwd[0] || da.passwd[3])
     fprintf(stderr, "ACK!\n");
   if (pamerr == PAM_SUCCESS)
     {
        free(current_user);
        exit(0);
     }
   free(current_user);
   exit(-1);

   return 0;
}
#else
e_auth_begin(char *passwd EINA_UNUSED)
{
   return 0;
}
#endif

E_API char *
e_auth_hostname_get(void)
{
   return strdup("localhost");
}
