#include "e.h"

/*
 * NOTE TO FreeBSD users. Install libexecinfo from
 * ports/devel/libexecinfo and add -lexecinfo to LDFLAGS
 * to add backtrace support.
 */

#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif

EINTERN void 
e_sigseg_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
   /* ecore_wl_input_ungrab(); */
   /* ecore_wl_sync(); */
}

EINTERN void 
e_sigill_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
   // In case of a sigill in Enlightenment, Enlightenment start will catch the sigill and continue,
   // because evas cpu detection use that behaviour. But if we get a SIGILL after that, we endup in
   // this sig handler. So E start remember the SIGILL, and we will commit succide with a USR1, followed
   // by a SEGV.
   kill(getpid(), SIGUSR1);
   kill(getpid(), SIGSEGV);
   pause();
}

EINTERN void 
e_sigfpe_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{

}

EINTERN void 
e_sigbus_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{

}

EINTERN void 
e_sigabrt_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{

}
