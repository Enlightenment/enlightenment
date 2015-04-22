/*
 * NOTE TO FreeBSD users. Install libexecinfo from
 * ports/devel/libexecinfo and add -lexecinfo to LDFLAGS
 * to add backtrace support.
 */
#include "e.h"

#ifdef HAVE_WAYLAND_ONLY
#include <Ecore_Drm.h>
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

/* a tricky little devil, requires e and it's libs to be built
 * with the -rdynamic flag to GCC for any sort of decent output.
 */
EAPI void
e_sigseg_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
#ifdef HAVE_WAYLAND_ONLY
   const Eina_List *list, *l, *ll;
   Ecore_Drm_Device *dev;

   list = ecore_drm_devices_get();
   EINA_LIST_FOREACH_SAFE(list, l, ll, dev)
     {
        ecore_drm_inputs_destroy(dev);
        ecore_drm_sprites_destroy(dev);
        ecore_drm_device_close(dev);
        ecore_drm_launcher_disconnect(dev);
        ecore_drm_device_free(dev);
     }

   ecore_drm_shutdown();
#else
   _e_x_composite_shutdown();
   ecore_x_pointer_ungrab();
   ecore_x_keyboard_ungrab();
   ecore_x_ungrab();
   ecore_x_sync();
   e_alert_show();
#endif
}

EAPI void
e_sigill_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
   // In case of a sigill in Enlightenment, Enlightenment start will catch the sigill and continue,
   // because evas cpu detection use that behaviour. But if we get a SIGILL after that, we endup in
   // this sig handler. So E start remember the SIGILL, and we will commit succide with a USR1, followed
   // by a SEGV.
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

EAPI void
e_sigfpe_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
#ifdef HAVE_WAYLAND_ONLY
   const Eina_List *list, *l, *ll;
   Ecore_Drm_Device *dev;

   list = ecore_drm_devices_get();
   EINA_LIST_FOREACH_SAFE(list, l, ll, dev)
     {
        ecore_drm_inputs_destroy(dev);
        ecore_drm_sprites_destroy(dev);
        ecore_drm_device_close(dev);
        ecore_drm_launcher_disconnect(dev);
        ecore_drm_device_free(dev);
     }

   ecore_drm_shutdown();
#else
   _e_x_composite_shutdown();
   ecore_x_pointer_ungrab();
   ecore_x_keyboard_ungrab();
   ecore_x_ungrab();
   ecore_x_sync();
   e_alert_show();
#endif
}

EAPI void
e_sigbus_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
#ifdef HAVE_WAYLAND_ONLY
   const Eina_List *list, *l, *ll;
   Ecore_Drm_Device *dev;

   list = ecore_drm_devices_get();
   EINA_LIST_FOREACH_SAFE(list, l, ll, dev)
     {
        ecore_drm_inputs_destroy(dev);
        ecore_drm_sprites_destroy(dev);
        ecore_drm_device_close(dev);
        ecore_drm_launcher_disconnect(dev);
        ecore_drm_device_free(dev);
     }

   ecore_drm_shutdown();
#else
   _e_x_composite_shutdown();
   ecore_x_pointer_ungrab();
   ecore_x_keyboard_ungrab();
   ecore_x_ungrab();
   ecore_x_sync();
   e_alert_show();
#endif
}

EAPI void
e_sigabrt_act(int x EINA_UNUSED, siginfo_t *info EINA_UNUSED, void *data EINA_UNUSED)
{
#ifdef HAVE_WAYLAND_ONLY
   const Eina_List *list, *l, *ll;
   Ecore_Drm_Device *dev;

   list = ecore_drm_devices_get();
   EINA_LIST_FOREACH_SAFE(list, l, ll, dev)
     {
        ecore_drm_inputs_destroy(dev);
        ecore_drm_sprites_destroy(dev);
        ecore_drm_device_close(dev);
        ecore_drm_launcher_disconnect(dev);
        ecore_drm_device_free(dev);
     }

   ecore_drm_shutdown();
#else
   _e_x_composite_shutdown();
   ecore_x_pointer_ungrab();
   ecore_x_keyboard_ungrab();
   ecore_x_ungrab();
   ecore_x_sync();
   e_alert_show();
#endif
}
