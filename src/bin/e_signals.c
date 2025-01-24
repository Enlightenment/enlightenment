/*
 * NOTE TO FreeBSD users. Install libexecinfo from
 * ports/devel/libexecinfo and add -lexecinfo to LDFLAGS
 * to add backtrace support.
 */
#include "e.h"

#ifdef HAVE_WAYLAND
# ifdef USE_MODULE_WL_DRM
#  include <Ecore_Drm2.h>
# endif
#endif

#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif

static volatile Eina_Bool _e_x_composite_shutdown_try = 0;

#ifndef HAVE_WAYLAND_ONLY
static void
_e_x_composite_shutdown(void)
{
//   Ecore_X_Display *dpy;
   Ecore_X_Window root;

   if (_e_x_composite_shutdown_try) return;  /* we failed :-( */
   _e_x_composite_shutdown_try = 1;

//   dpy = ecore_x_display_get();
   root = ecore_x_window_root_first_get();

   /* ignore errors, we really don't care at this point */
   ecore_x_composite_unredirect_subwindows(root, ECORE_X_COMPOSITE_UPDATE_MANUAL);
   _e_x_composite_shutdown_try = 0;
}
#endif

#if 0
#define _e_write_safe(fd, buf) _e_write_safe_int(fd, buf, sizeof(buf))

static void
_e_write_safe_int(int fd, const char *buf, size_t size)
{
   while (size > 0)
     {
        ssize_t done = write(fd, buf, size);
        if (done >= 0)
          {
             buf += done;
             size -= done;
          }
        else
          {
             if ((errno == EAGAIN) || (errno == EINTR))
               continue;
             else
               {
                  perror("write");
                  return;
               }
          }
     }
}

#endif

static void
_e_crash(void)
{
#ifdef HAVE_WAYLAND
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
        return;
#endif
#ifndef HAVE_WAYLAND_ONLY
   _e_x_composite_shutdown();
   ecore_x_pointer_ungrab();
   ecore_x_keyboard_ungrab();
   ecore_x_ungrab();
   ecore_x_sync();
   e_alert_show();
#endif
}

static void
_e_siginfo(int sig, siginfo_t *info)
{
  // dump more info ont he signal we trapped to stdout
  printf("SIG: sig=%i\n", sig);
  printf("SIG: info->signo=");
#define NAME(_x) case _x: printf(# _x "\n"); break
#define NUMX(_v) default: printf("0x%lx\n", (unsigned long)_v); break
#define NUMI(_v) default: printf("%li\n", (long)_v); break
  switch(info->si_signo) {
    NAME(SIGSEGV);
    NAME(SIGILL);
    NAME(SIGFPE);
    NAME(SIGBUS);
    NAME(SIGABRT);
    NUMX(info->si_signo);
  }
  printf("SIG: info->code=");
  switch(info->si_signo) {
   case SIGSEGV:
    switch(info->si_code) {
      NAME(SEGV_MAPERR);
      NAME(SEGV_ACCERR);
#ifdef __linux__
      NAME(SEGV_BNDERR);
#endif
      NAME(SEGV_PKUERR);
      NUMX(info->si_code);
    } break;
   case SIGILL:
    switch (info->si_code) {
      NAME(ILL_ILLOPC);
      NAME(ILL_ILLOPN);
      NAME(ILL_ILLADR);
      NAME(ILL_ILLTRP);
      NAME(ILL_PRVOPC);
      NAME(ILL_PRVREG);
      NAME(ILL_COPROC);
      NAME(ILL_BADSTK);
      NUMX(info->si_code);
    } break;
   case SIGFPE:
    switch (info->si_code) {
      NAME(FPE_INTDIV);
      NAME(FPE_INTOVF);
      NAME(FPE_FLTDIV);
      NAME(FPE_FLTOVF);
      NAME(FPE_FLTUND);
      NAME(FPE_FLTRES);
      NAME(FPE_FLTINV);
      NAME(FPE_FLTSUB);
      NUMX(info->si_code);
    } break;
   case SIGBUS:
    switch (info->si_code) {
      NAME(BUS_ADRALN);
      NAME(BUS_ADRERR);
      NAME(BUS_OBJERR);
#ifdef __linux__
      NAME(BUS_MCEERR_AR);
      NAME(BUS_MCEERR_AO);
#endif
      NUMX(info->si_code);
    } break;
  }
  switch(info->si_signo) {
   case SIGSEGV:
   case SIGILL:
   case SIGFPE:
   case SIGBUS:
# ifdef __linux__
    printf("SIG: info->si_addr=%p\n", info->si_addr);
# else
    printf("SIG: info->si_addr=0x%08lx\n", (unsigned long)info->si_addr);
# endif
   default:
    break;
  }
}

/* a tricky little devil, requires e and it's libs to be built
 * with the -rdynamic flag to GCC for any sort of decent output.
 */
E_API void
e_sigseg_act(int sig, siginfo_t *info, void *data EINA_UNUSED)
{
   _e_siginfo(sig, info);
   _e_crash();
}

E_API void
e_sigill_act(int sig, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
   // In case of a SIGILL in Enlightenment, Enlightenment start will catch the SIGILL and continue,
   // because evas cpu detection use that behaviour. But if we get a SIGILL after that, we end up in
   // this sig handler. So E start remember the SIGILL, and we will commit suicide with a USR1, followed
   // by a SEGV.
   _e_siginfo(sig, info);
   kill(getpid(), SIGUSR1);
   kill(getpid(), SIGSEGV);
   pause();
   /* _e_x_composite_shutdown(); */
   /* ecore_x_pointer_ungrab(); */
   /* ecore_x_keyboard_ungrab(); */
   /* ecore_x_ungrab(); */
   /* ecore_x_sync(); */
   /* e_alert_show(); */
}

E_API void
e_sigfpe_act(int sig, siginfo_t *info, void *data EINA_UNUSED)
{
   _e_siginfo(sig, info);
   _e_crash();
}

E_API void
e_sigbus_act(int sig, siginfo_t *info, void *data EINA_UNUSED)
{
   _e_siginfo(sig, info);
   _e_crash();
}

E_API void
e_sigabrt_act(int sig, siginfo_t *info, void *data EINA_UNUSED)
{
   _e_siginfo(sig, info);
   _e_crash();
}
