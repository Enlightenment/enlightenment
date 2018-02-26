#include "e.h"

E_API int
e_auth_begin(char *passwd)
{
   char buf[PATH_MAX], *p;
   Ecore_Exe *exe = NULL;
   int ret = 0;

   if (strlen(passwd) == 0) goto out;

   snprintf(buf, sizeof(buf), "%s/enlightenment/utils/enlightenment_ckpasswd",
            e_prefix_lib_get());

   exe = ecore_exe_pipe_run(buf, ECORE_EXE_PIPE_WRITE, NULL);
   if (ecore_exe_send(exe, passwd, strlen(passwd)) != EINA_TRUE) goto out;
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
   if (passwd[0] || passwd[3]) fprintf(stderr, "ACK!\n");

   return ret;
}
