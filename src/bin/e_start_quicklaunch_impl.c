#include <Ecore_Wl2.h>

extern int e_pid;
static Ecore_Event_Handler *ql_exe_data;
static Ecore_Event_Handler *ql_exe_del;

static Eina_Inlist *instances;

typedef struct Instance
{
   EINA_INLIST;
   char **argv;
   char *cwd;
   int argc;
   int last;
   Ecore_Exe_Flags flags;
   Ecore_Exe *exe;
} Instance;

typedef struct Exe
{
   int pid;
   struct wl_resource *res;
} Exe;

__attribute__((visibility("hidden"))) void post_fork();

static const struct quicklaunch_exe_interface ql_exe_instance_interface =
{
};

static void
set_cwd(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *cwd)
{
   Instance *inst = wl_resource_get_user_data(resource);
   inst->cwd = eina_strdup(cwd);
}

static void
set_flags(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t flags)
{
   Instance *inst = wl_resource_get_user_data(resource);
   inst->flags = flags;
}

static void
ql_instance_run(struct wl_client *client, struct wl_resource *resource, uint32_t id, const char *env, uint32_t envc, struct wl_array *envoffsets)
{
   struct wl_resource *res;
   Instance *inst = wl_resource_get_user_data(resource);
   Exe *exe;

   res = wl_resource_create(client, &quicklaunch_exe_interface, 1, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }
   exe = calloc(1, sizeof(Exe));
   wl_resource_set_implementation(res, &ql_exe_instance_interface, exe, ql_exe_instance_destroy);
   if (!efl_quicklaunch_prepare(inst->argc, inst->argv, inst->cwd))
     if (!elm_quicklaunch_prepare(inst->argc, inst->argv, inst->cwd))
       {
          wl_resource_destroy(res);
          return;
       }

   exe->pid = elm_quicklaunch_fork(e_argc, e_args, e_cwd, post_fork, NULL);
   if (exe->pid == -1)
     {
        wl_resource_destroy(res);
        return;
     }
   elm_quicklaunch_cleanup();
}

static void
ql_instance_destroy(struct wl_client *client, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct quicklaunch_instance_interface ql_instance_interface =
{
   ql_instance_set_flags,
   ql_instance_run,
   ql_instance_destroy,
};

static void
ql_instance_destroy(struct wl_resource *res)
{
   Instance *inst = wl_resource_get_user_data(res);
   char **argv;
   
   for (argv = inst->argv; *argv; argv++)
     free(*argv);
   free(inst->argv);
   ecore_exe_free(inst->exe);
   instances = eina_inlist_remove(instances, EINA_INLIST_GET(inst));
   free(inst);
}

static void
ql_instance_create(struct wl_client *client, struct wl_resource *resource, uint32_t id, const char *cmd)
{
   Instance *inst;
   Eina_Strbuf *buf;
   struct wl_resource *res;

   res = wl_resource_create(client, &quicklaunch_instance_interface, 1, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }
   inst = calloc(1, sizeof(Instance));
   inst->argc = -1;
   instances = eina_inlist_append(instances, EINA_INLIST_GET(inst));
   buf = eina_strbuf_new();
   eina_strbuf_append_printf(buf, "e_ql_helper %s", cmd)
   inst->exe = ecore_exe_pipe_run(eina_strbuf_string_get(buf), ECORE_EXE_USE_SH | ECORE_EXE_NOT_LEADER | ECORE_EXE_PIPE_READ_LINE_BUFFERED, inst);
   wl_resource_set_implementation(res, &ql_instance_interface, inst, ql_instance_destroy);
   eina_strbuf_free(buf);
}

static const struct quicklaunch_interface ql_interface =
{
   ql_instance_create
};

static void
ql_impl_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;
   pid_t pid;

   wl_client_get_credentials(client, &pid, NULL, NULL);
   if (pid != e_pid)
     {
        wl_client_post_no_memory(client);
        return;
     }
   res = wl_resource_create(client, &quicklaunch_interface, version, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &ql_interface, NULL, NULL);
}

static Eina_Bool
ql_data(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Exe_Event_Data *ev)
{
   Instance *inst;

   EINA_INLIST_FOREACH(instances, inst)
     {
        int i;

        if (inst->exe != ev->exe) continue;
        for (i = 0; ev->lines[i].line; i++)
          {
            if ((!i) && (inst->argc == -1))
              {
                 inst->argc = strtol(ev->lines[i].line, NULL, 10);
                 inst->argv = calloc(argc + 1, sizeof(char*));
              }
            else
              inst->argv[inst->last++] = strdup(ev->lines[i].line);
          }
        break;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
ql_del(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Exe_Event_Del *ev)
{
   EINA_INLIST_FOREACH(instances, inst)
     {
        if (inst->exe != ev->exe) continue;
        ecore_exe_free(inst->exe);
        inst->exe = NULL;
        break;
     }
   return ECORE_CALLBACK_RENEW;
}

__attribute__((visibility("hidden"))) void
e_start_ql_postfork(void)
{
   ecore_event_handler_del(ql_exe_data);
   ecore_event_handler_del(ql_exe_del);
}

__attribute__((visibility("hidden"))) void
e_start_ql_impl_init(Ecore_Wl2_Display *disp)
{
   struct wl_display *d = ecore_wl2_display_get(disp);
   
   wl_global_create(d, &quicklaunch_interface, 1, NULL, ql_impl_bind);
   ql_exe_data = ecore_event_handler_add(ECORE_EXE_EVENT_DATA, (Ecore_Event_Handler_Cb)ql_data, NULL);
   ql_exe_del = ecore_event_handler_add(ECORE_EXE_EVENT_DEL, (Ecore_Event_Handler_Cb)ql_del, NULL);
}
