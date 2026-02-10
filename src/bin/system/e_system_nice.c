#include "e_system.h"

#include <linux/ioprio.h>

typedef enum
{
  SET,
  ADJ
} Mode;

static void
_pri_adj(int pid, int pri, Mode mode)
{
  Eina_List *files;
  char *file, buf[PATH_MAX];
  int pid2, ppid, oldpri, newpri = 0, ret;
  FILE *f;
  int fd;
  struct stat st;

  oldpri = getpriority(PRIO_PROCESS, pid);
  if      (mode == SET)  newpri = pri;
  else if (mode == ADJ)  newpri = oldpri + pri;
  if      (newpri >  19) newpri =  19; // lowest pri
  else if (newpri < -20) newpri = -20; // highest pri
  ret = setpriority(PRIO_PROCESS, pid, newpri);
// shouldn't need to do this as default ionice class is "none" (0), and
// this inherits io priority FROM nice level
//  ioprio_set(IOPRIO_WHO_PROCESS, pid,
//             IOPRIO_PRIO_VALUE(2, 5));
  files = ecore_file_ls("/proc");
  EINA_LIST_FREE(files, file)
    {
      if (isdigit(file[0]))
        {
          snprintf(buf, sizeof(buf), "/proc/%s/stat", file);
          fd = open(buf, O_RDONLY);
          if (fd >= 0)
            {
              if (fstat(fd, &st) == 0)
                {
                  if (st.st_uid == uid)
                    {
                      f = fdopen(fd, "r");
                      if (f)
                        {
                          pid2 = ppid = -1;
                          ret = fscanf(f, "%i %*s %*s %i %*s", &pid2, &ppid);
                          fclose(f);
                          close(fd);
                          fd = -1;
                          if ((ret == 2) && (ppid == pid))
                             _pri_adj(pid2, pri, mode);
                        }
                    }
                }
            }
          if (fd >= 0) close(fd);
          free(file);
        }
    }
}
static void
_cb_nice_set(void *data EINA_UNUSED, const char *params)
{
  // PID set|adj NICE_VAL_OR_ADJUST
  char buf[64] = "";
  int pid = -1, adj = 0;

  if (sscanf(params, "%i %63s %i", &pid, buf, &adj) != 3) return;
  if      (!strcmp(buf, "set")) _pri_adj(pid, adj, SET);
  else if (!strcmp(buf, "adj")) _pri_adj(pid, adj, ADJ);
}

void
e_system_nice_init(void)
{
  e_system_inout_command_register("nice-set", _cb_nice_set, NULL);
}

void
e_system_nice_shutdown(void)
{
   // only shutdown things we really have to - no need to free mem etc.
}
