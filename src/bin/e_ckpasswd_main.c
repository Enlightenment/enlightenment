#include <sys/types.h>

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <security/pam_constants.h>

// Exit codes, per src/modules/lokker/lokker.c:
// 0: success (unlock)
// 1-128: PAM error but also unlock (!!!)
// else: failed.

static char pw[4096];
struct passwd *pwent;

static void
zeropw(void)
{
   /* security - null out passwd string once we are done with it */
   memset(pw, 0, sizeof(pw));
   if (pw[0] || pw[3]) printf("ACK!\n");

   if (pwent == NULL) return;
   if (pwent->pw_passwd == NULL) return;

   /* security - null out passwd string once we are done with it */
   memset(pwent->pw_passwd, 0, strlen(pwent->pw_passwd));
   if (pwent->pw_passwd[0]) printf("ACK!\n");
}

int
main(int argc, char **argv)
{
   ssize_t rd;
   uid_t id;
   int i;

   for (i = 1; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-h")) ||
            (!strcmp(argv[i], "-help")) ||
            (!strcmp(argv[i], "--help")))
          {
             printf("This is an internal tool for Enlightenment.\n"
                    "do not use it.\n");
             exit(129);
          }
     }
   if (argc != 1)
     exit(130);

   id = getuid();

   if (atexit(zeropw)) err(131, "atexit");

   rd = read(0, pw, sizeof(pw) - 1);
   if (rd < 0) err(132, "read");

   if (setuid(0) != 0)
     {
        printf("ERROR: UNABLE TO ASSUME ROOT PRIVILEGES\n");
        exit(133);
     }
   if (setgid(0) != 0)
     {
        printf("ERROR: UNABLE TO ASSUME ROOT GROUP PRIVILEGES\n");
        exit(134);
     }

   pwent = getpwuid(id);
   if (pwent == NULL) return -2;

   if (strcmp(crypt(pw, pwent->pw_passwd), pwent->pw_passwd) == 0)
     return 0;

   return -1;
}
