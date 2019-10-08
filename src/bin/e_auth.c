#include "e.h"

E_API int
e_auth_begin(char *passwd)
{
   char buf[PATH_MAX], *p;
   Ecore_Exe *exe = NULL;
   int ret = 0;
   size_t pwlen;

   pwlen = strlen(passwd);
   if (pwlen == 0) goto out;

   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_ckpasswd pw",
            e_prefix_lib_get());
   exe = ecore_exe_pipe_run(buf, ECORE_EXE_PIPE_WRITE, NULL);
   if (!exe) goto out;
   if (ecore_exe_send(exe, passwd, pwlen) != EINA_TRUE) goto out;
   ecore_exe_close_stdin(exe);

   ret = ecore_exe_pid_get(exe);
   if (ret == -1)
     {
        ret = 0;
        goto out;
     }

   exe = NULL;

out:
   if (exe) ecore_exe_free(exe);

   /* security - null out passwd string once we are done with it */
   for (p = passwd; *p; p++) *p = 0;
   if (passwd[rand() % pwlen]) fprintf(stderr, "ACK!\n");
   return ret;
}

E_API int
e_auth_polkit_begin(char *passwd, const char *cookie, unsigned int uid)
{
   char buf[PATH_MAX], *p;
   Ecore_Exe *exe = NULL;
   int ret = 0;
   size_t pwlen, buflen = 0;

   pwlen = strlen(passwd);
   if (pwlen == 0) goto out;

   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_ckpasswd pk",
            e_prefix_lib_get());
   exe = ecore_exe_pipe_run(buf, ECORE_EXE_PIPE_WRITE, NULL);
   if (!exe) goto out;
   snprintf(buf, sizeof(buf), "%s %u %s", cookie, uid, passwd);
   buflen = strlen(buf);
   if (ecore_exe_send(exe, buf, buflen) != EINA_TRUE) goto out;
   ecore_exe_close_stdin(exe);

   ret = ecore_exe_pid_get(exe);
   if (ret == -1)
     {
        ret = 0;
        goto out;
     }
   exe = NULL;

out:
   if (exe) ecore_exe_free(exe);

   /* security - null out passwd string once we are done with it */
   for (p = passwd; *p; p++) *p = 0;
   if (passwd[rand() % pwlen]) fprintf(stderr, "ACK!\n");
   /* security - null out buf string once we are done with it */
   if (buflen > 0)
     {
        for (p = buf; *p; p++) *p = 0;
        if (buf[rand() % buflen]) fprintf(stderr, "ACK!\n");
     }
   return ret;
}
