#include "e.h"
#include "e_comp_wl.h"

/* local structures */
typedef struct _E_XWayland_Server E_XWayland_Server;
struct _E_XWayland_Server
{
   int disp;
};

/* local variables */
static E_XWayland_Server *exs;

/* local functions */
static Eina_Bool 
_lock_create(int disp)
{
   char lock[256], pid[16], *end;
   int fd, size;
   pid_t opid;

   /* assemble lock file name */
   snprintf(lock, sizeof(lock), "/tmp/.X%d-lock", disp);
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

/* module functions */
EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "XWayland" };

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Comp *comp;

   /* try to get the running compositor */
   if (!(comp = e_comp_get(NULL))) return NULL;

   /* make sure it's a wayland compositor */
   if (comp->comp_type != E_PIXMAP_TYPE_WL) return NULL;

   /* alloc space for server struct */
   if (!(exs = calloc(1, sizeof(E_XWayland_Server))))
     return NULL;

   /* default display to zero */
   exs->disp = 0;

lock:
   if (!_lock_create(exs->disp))
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

   /* TODO: bind sockets */

   return m;
}

EAPI int 
e_modapi_shutdown(E_Module *m)
{
   return 1;
}


