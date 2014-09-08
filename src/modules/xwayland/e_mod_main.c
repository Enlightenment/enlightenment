#define E_COMP_WL
#include "e.h"
#include <sys/socket.h>
#include <sys/un.h>

/* local structures */
typedef struct _E_XWayland_Server E_XWayland_Server;
struct _E_XWayland_Server
{
   int disp;
   int abs_fd, unx_fd, wm_fd;
   char lock[256];

   struct wl_display *wl_disp;
   struct wl_event_loop *loop;
   struct wl_client *client;

   Ecore_Fd_Handler *abs_hdlr, *unx_hdlr;
   Ecore_Event_Handler *sig_hdlr;

   struct 
     {
        pid_t pid;
        /* cleanup_func func; */
     } process;
};

/* local variables */
static E_XWayland_Server *exs;

/* local functions */
static Eina_Bool 
_lock_create(char *lock)
{
   char pid[16], *end;
   int fd, size;
   pid_t opid;

   fd = open(lock, (O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL), 0444);
   if ((fd < 0) && (errno == EEXIST))
     {
        fd = open(lock, (O_CLOEXEC | O_RDONLY));
        if ((fd < 0) || (read(fd, pid, 11) != 11))
          {
             ERR("Could not read XWayland lock file: %m");
             if (fd >= 0) close(fd);
             errno = EEXIST;
             return EINA_FALSE;
          }

        opid = strtol(pid, &end, 0);
        if ((end != (pid + 10)))
          {
             if (fd >= 0) close(fd);
             errno = EEXIST;
             return EINA_FALSE;
          }

        if ((kill(opid, 0) < 0) && (errno == ESRCH))
          {
             /* close stale lock file */
             if (fd >= 0) close(fd);

             if (unlink(lock))
               errno = EEXIST;
             else
               errno = EAGAIN;

             return EINA_FALSE;
          }
        close(fd);
        errno = EEXIST;
        return EINA_FALSE;
     }
   else if (fd < 0)
     {
        ERR("Could not create XWayland lock file: %m");
        return EINA_FALSE;
     }

   /* use the pid of the wayland compositor */
   size = snprintf(pid, sizeof(pid), "%10d\n", getpid());
   if (write(fd, pid, size) != size)
     {
        unlink(lock);
        close(fd);
        return EINA_FALSE;
     }

   close(fd);

   return EINA_TRUE;
}

static int 
_abstract_socket_bind(int disp)
{
   struct sockaddr_un addr;
   socklen_t size, nsize;
   int fd;

   /* try to create a local socket */
   if ((fd = socket(PF_LOCAL, (SOCK_STREAM | SOCK_CLOEXEC), 0)) < 0) 
     return -1;

   addr.sun_family = AF_LOCAL;
   nsize = snprintf(addr.sun_path, sizeof(addr.sun_path), 
                    "%c/tmp/.X11-unix/X%d", 0, disp);
   size = offsetof(struct sockaddr_un, sun_path) + nsize;

   /* try to bind to the socket */
   if (bind(fd, (struct sockaddr *)&addr, size) < 0)
     {
        ERR("Failed to bind to abstract socket %s: %m", addr.sun_path + 1);
        goto err;
     }

   /* try to listen on the bound socket */
   if (listen(fd, 1) < 0) 
     {
        ERR("Failed to listen to abstract fd: %d", fd);
        goto err;
     }

   return fd;

err:
   close(fd);
   return -1;
}

static int 
_unix_socket_bind(int disp)
{
   struct sockaddr_un addr;
   socklen_t size, nsize;
   int fd;

   /* try to create a local socket */
   if ((fd = socket(PF_LOCAL, (SOCK_STREAM | SOCK_CLOEXEC), 0)) < 0) 
     return -1;

   addr.sun_family = AF_LOCAL;
   nsize = snprintf(addr.sun_path, sizeof(addr.sun_path), 
                    "/tmp/.X11-unix/X%d", disp) + 1;
   size = offsetof(struct sockaddr_un, sun_path) + nsize;

   unlink(addr.sun_path);

   /* try to bind to the socket */
   if (bind(fd, (struct sockaddr *)&addr, size) < 0)
     {
        ERR("Failed to bind to abstract socket %s: %m", addr.sun_path + 1);
        goto err;
     }

   /* try to listen on the bound socket */
   if (listen(fd, 1) < 0) 
     {
        ERR("Failed to listen to unix fd: %d", fd);
        goto err;
     }

   return fd;

err:
   close(fd);
   return -1;
}

static Eina_Bool 
_cb_xserver_event(void *data EINA_UNUSED, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   int socks[2], wms[2], fd;
   char disp[8], s[8], xserver[PATH_MAX];
   char abs_fd[8], unx_fd[8], wm_fd[8];

   if (socketpair(AF_UNIX, (SOCK_STREAM | SOCK_CLOEXEC), 0, socks) < 0)
     {
        ERR("XServer Socketpair failed: %m");
        return ECORE_CALLBACK_RENEW;
     }

   if (socketpair(AF_UNIX, (SOCK_STREAM | SOCK_CLOEXEC), 0, wms) < 0)
     {
        ERR("Window Manager Socketpair failed: %m");
        return ECORE_CALLBACK_RENEW;
     }

   exs->process.pid = fork();
   switch (exs->process.pid)
     {
      case 0:
        /* dup will unset CLOEXEC on the client as cloexec closes both ends */
        if ((fd = dup(socks[1]) < 0)) goto fail;
        snprintf(s, sizeof(s), "%d", fd);
        setenv("WAYLAND_SOCKET", s, 1);

        if ((fd = dup(exs->abs_fd)) < 0) goto fail;
        snprintf(abs_fd, sizeof(abs_fd), "%d", fd);

        if ((fd = dup(exs->unx_fd)) < 0) goto fail;
        snprintf(unx_fd, sizeof(unx_fd), "%d", fd);

        if ((fd = dup(wms[1])) < 0) goto fail;
        snprintf(wm_fd, sizeof(wm_fd), "%d", fd);

        /* ignore usr1 and have X send it to the parent process */
        signal(SIGUSR1, SIG_IGN);

        snprintf(disp, sizeof(disp), ":%d", exs->disp);

        snprintf(xserver, sizeof(xserver), "%s", XWAYLAND_BIN);
        DBG("\tLaunching XWayland: %s: %s", xserver, disp);
        if (execl(xserver, xserver, disp, "-rootless", "-listen", abs_fd,
                  "-listen", unx_fd, "-wm", wm_fd, "-terminate", "-shm", 
                  NULL) < 0)
          {
             ERR("Failed to exec XWayland: %m");
          }

fail:
        _exit(EXIT_FAILURE);

      default:
        close(socks[1]);
        exs->client = wl_client_create(exs->wl_disp, socks[0]);

        close(wms[1]);
        exs->wm_fd = wms[0];

        /* TODO: watch process ?? */

        if (exs->abs_hdlr)
          ecore_main_fd_handler_del(exs->abs_hdlr);
        if (exs->unx_hdlr)
          ecore_main_fd_handler_del(exs->unx_hdlr);
        break;
      case -1:
        ERR("Failed to fork: %m");
        break;
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool 
_cb_signal_event(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Signal_User *ev;

   ev = event;
   if (ev->number != 1) return ECORE_CALLBACK_RENEW;

   /* NB: SIGUSR1 comes from XWayland Server when it has finished 
    * initialized. */

   DBG("XWayland Finished Init");

   /* TODO: create "window manager" process */

   /* TODO: NB: 
    * 
    * Weston creates a smaller window manager process here.
    * We Maybe able to just do e_comp_x_init, but will have to test that */

   return ECORE_CALLBACK_CANCEL;
}

/* module functions */
EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "XWayland" };

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Comp *comp;
   char disp[8];

   DBG("LOAD XWAYLAND MODULE");

   /* try to get the running compositor */
   if (!(comp = e_comp_get(NULL))) return NULL;

   /* make sure it's a wayland compositor */
   if (comp->comp_type != E_PIXMAP_TYPE_WL) return NULL;

   /* alloc space for server struct */
   if (!(exs = calloc(1, sizeof(E_XWayland_Server))))
     return NULL;

   /* record wayland display */
   exs->wl_disp = comp->wl_comp_data->wl.disp;

   /* default display to zero */
   exs->disp = 0;

lock:
   /* assemble lock file name */
   snprintf(exs->lock, sizeof(exs->lock), "/tmp/.X%d-lock", exs->disp);

   /* try to create the xserver lock file */
   if (!_lock_create(exs->lock))
     {
        if (errno == EAGAIN)
          goto lock;
        else if (errno == EEXIST)
          {
             exs->disp++;
             goto lock;
          }
        else
          {
             free(exs);
             return NULL;
          }
     }

   /* try to bind abstract socket */
   exs->abs_fd = _abstract_socket_bind(exs->disp);
   if ((exs->abs_fd < 0) && (errno == EADDRINUSE))
     {
        exs->disp++;
        unlink(exs->lock);
        goto lock;
     }

   /* try to bind unix socket */
   if ((exs->unx_fd = _unix_socket_bind(exs->disp)) < 0)
     {
        unlink(exs->lock);
        close(exs->abs_fd);
        free(exs);
        return NULL;
     }

   /* assemble x11 display name and set it */
   snprintf(disp, sizeof(disp), ":%d", exs->disp);
   DBG("XWayland Listening on display: %s", disp);
   setenv("DISPLAY", disp, 1);

   /* setup ecore_fd handlers for abstract and unix socket fds */
   exs->abs_hdlr = 
     ecore_main_fd_handler_add(exs->abs_fd, ECORE_FD_READ, 
                               _cb_xserver_event, NULL, NULL, NULL);
   exs->unx_hdlr = 
     ecore_main_fd_handler_add(exs->unx_fd, ECORE_FD_READ, 
                               _cb_xserver_event, NULL, NULL, NULL);

   /* setup listener for SIGUSR1 */
   exs->sig_hdlr = 
     ecore_event_handler_add(ECORE_EVENT_SIGNAL_USER, _cb_signal_event, exs);

   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   char path[256];

   if (!exs) return 1;

   unlink(exs->lock);

   snprintf(path, sizeof(path), "/tmp/.X11-unix/X%d", exs->disp);
   unlink(path);

   if (exs->abs_hdlr) ecore_main_fd_handler_del(exs->abs_hdlr);
   if (exs->unx_hdlr) ecore_main_fd_handler_del(exs->unx_hdlr);

   close(exs->abs_fd);
   close(exs->unx_fd);

   if (exs->sig_hdlr) ecore_event_handler_del(exs->sig_hdlr);

   free(exs);

   return 1;
}
