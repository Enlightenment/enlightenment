#include "e.h"

#define MAX_OUTPUT_CHARACTERS 5000

/* externally accessible functions */
EINTERN int
e_exec_init(void)
{
   return 1;
}

EINTERN int
e_exec_shutdown(void)
{
   return 1;
}

EAPI void
e_exec_executor_set(E_Exec_Instance *(*func)(void *data, E_Zone * zone, Efreet_Desktop * desktop, const char *exec, Eina_List *files, const char *launch_method), const void *data)
{

}

EAPI E_Exec_Instance *
e_exec(E_Zone *zone, Efreet_Desktop *desktop, const char *exec, Eina_List *files, const char *launch_method)
{
   return NULL;
}

EAPI E_Exec_Instance *
e_exec_startup_id_pid_instance_find(int id, pid_t pid)
{
   return NULL;
}

EAPI Efreet_Desktop *
e_exec_startup_id_pid_find(int id, pid_t pid)
{
   return NULL;
}

EAPI E_Exec_Instance *
e_exec_startup_desktop_instance_find(Efreet_Desktop *desktop)
{
   return NULL;
}

EAPI void
e_exec_instance_found(E_Exec_Instance *inst)
{

}

EAPI void
e_exec_instance_watcher_add(E_Exec_Instance *inst, void (*func)(void *data, E_Exec_Instance *inst, E_Exec_Watch_Type type), const void *data)
{

}

EAPI void
e_exec_instance_watcher_del(E_Exec_Instance *inst, void (*func)(void *data, E_Exec_Instance *inst, E_Exec_Watch_Type type), const void *data)
{

}
