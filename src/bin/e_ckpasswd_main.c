#include "config.h"

#define __USE_MISC
#define _SVID_SOURCE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#if defined(__OpenBSD__)

static int
_check_auth(uid_t uid, const char *guess)
{
   struct passwd *pwent;

   pwent = getpwuid_shadow(uid);
   if (!pwent) return -1;
   if (!pwent->pw_passwd) return -1;

   return crypt_checkpass(guess, pw_ent->pw_passwd);
}





#elif defined(__FreeBSD__)
#include <security/pam_constants.h>

static int
_check_auth(uid_t uid, const char *pw)
{
   struct passwd *pwent = getpwuid(uid);

   if (!pwent) return -1;
   if (!pwent->pw_passwd) return -1;

   if (!strcmp(crypt(pw, pwent->pw_passwd), pwent->pw_passwd)) return 0;
   return -1;
}





#elif defined(HAVE_PAM)
# include <security/pam_appl.h>

typedef struct
{
   const char *user;
   const char *pw;
} Authinfo;

static int
_conv_cb(int num, const struct pam_message **msg, struct pam_response **resp, void *data)
{
   Authinfo *ai = data;
   int replies;
   struct pam_response *reply = NULL;

   reply = malloc(sizeof(struct pam_response) * num);
   if (!reply) return PAM_CONV_ERR;

   for (replies = 0; replies < num; replies++)
     {
        switch (msg[replies]->msg_style)
          {
           case PAM_PROMPT_ECHO_ON:
             reply[replies].resp_retcode = PAM_SUCCESS;
             reply[replies].resp = strdup(ai->user);
             break;
           case PAM_PROMPT_ECHO_OFF:
             reply[replies].resp_retcode = PAM_SUCCESS;
             reply[replies].resp = strdup(ai->pw);
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
_check_auth(uid_t uid, const char *pw)
{
   Authinfo ai;
   struct passwd *pwent;
   const char *user;
   const char *prof;
   const char *host;
   struct stat st;
   pam_handle_t *handle;
   int pamerr;
   struct pam_conv conv;

   pwent = getpwuid(uid);
   if (!pwent) return -1;
   user = pwent->pw_name;
   if (!user) return -1;

   host = "localhost";

   prof = "login";
   if      (!stat("/etc/pam.d/enlightenment", &st)) prof = "enlightenment";
   else if (!stat("/etc/pam.d/xscreensaver", &st))  prof = "xscreensaver";
   else if (!stat("/etc/pam.d/kscreensaver", &st))  prof = "kscreensaver";
   else if (!stat("/etc/pam.d/system-auth", &st))   prof = "system-auth";
   else if (!stat("/etc/pam.d/system", &st))        prof = "system";
   else if (!stat("/etc/pam.d/xdm", &st))           prof = "xdm";
   else if (!stat("/etc/pam.d/gdm", &st))           prof = "gdm";
   else if (!stat("/etc/pam.d/kdm", &st))           prof = "kdm";

   ai.user = user;
   ai.pw = pw;

   conv.conv = _conv_cb;
   conv.appdata_ptr = &ai;
   if (pam_start(prof, user, &conv, &handle) != PAM_SUCCESS) return -1;
   if (pam_set_item(handle, PAM_USER, user) != PAM_SUCCESS) return -1;
   if (pam_set_item(handle, PAM_RHOST, host) != PAM_SUCCESS) return -1;

   pamerr = pam_authenticate(handle, 0);
   pam_end(handle, pamerr);

   if (pamerr != PAM_SUCCESS) return -1;

   return 0;
}





#else

static int
_check_auth(uid_t uid, const char *pw)
{
   return -1;
}





#endif

int
main(int argc, char **argv)
{
   ssize_t rd;
   uid_t id;
   char pw[4096], *p;

   if (argc != 1)
     {
        int i;

        for (i = 1; i < argc; i++)
          fprintf(stderr, "Unknown option %s\n", argv[i]);
        fprintf(stderr,
                "This is an internal tool for Enlightenment\n");
        goto err;
     }

   // get uid who ran this
   id = getuid();

   // read passwd from stdin
   rd = read(0, pw, sizeof(pw) - 1);
   if (rd < 0)
     {
        fprintf(stderr,
                "Error. Can't read passwd on stdin\n");
        goto err;
     }
   pw[rd] = 0;
   for (p = pw; *p; p++)
     {
        if ((*p == '\r') || (*p == '\n'))
          {
             *p = 0;
             break;
          }
     }

   // If we are setuid root then try become root - we can work without though
   // if pam etc. can work without being root
   if (setuid(0) != 0)
     fprintf(stderr,
             "Warning. Can't become user root. If password auth requires root then this will fail\n");
   if (setgid(0) != 0)
     fprintf(stderr,
             "Warning. Can't become group root. If password auth requires root then this will fail\n");
   if (_check_auth(id, pw) == 0) return 0;
err:
   fprintf(stderr,
           "Password auth fail\n");
   return -1;
}
