/* Globals a client can use to read or type the user's keys are advertised
 * only to the programs named in a .desktop file of
 * <prefix>/share/enlightenment/data/wayland-privileges or
 * ~/.e/e/wayland-privileges with, for instance:
 *
 *   Exec=fcitx5
 *   X-Enlightenment-Wayland-Privileges=input_method;virtual_keyboard;
 *
 * A client that is refused does not see the global at all, as on a
 * compositor without the protocol, which every input method handles; a
 * protocol error would kill its connection instead.
 *
 * The program is the client's /proc/<pid>/exe, so this cannot tell apart
 * sandboxed clients, which all are the sandbox's binary. */
#define E_COMP_WL
#include "e.h"
#include "input-method-unstable-v2-server-protocol.h"
#include "virtual-keyboard-unstable-v1-server-protocol.h"

static const struct
{
   const struct wl_interface *iface;
   const char *privilege;
} _privileged_globals[] =
{
   { &zwp_input_method_manager_v2_interface, "input_method" },
   { &zwp_virtual_keyboard_manager_v1_interface, "virtual_keyboard" },
};

/* The program the Exec key starts, found in PATH as a shell would, or NULL */
static char *
_e_comp_wl_privileges_exec_resolve(const char *exec)
{
   char prog[PATH_MAX], buf[PATH_MAX];
   const char *path, *end;
   char stop = ' ';
   size_t n = 0;

   while (*exec == ' ') exec++;
   if (*exec == '"') stop = *exec++;
   while (*exec && (*exec != stop) && (n < sizeof(prog) - 1))
     prog[n++] = *exec++;
   prog[n] = 0;
   if (prog[0] == '/') return realpath(prog, NULL);
   if ((!prog[0]) || strchr(prog, '/')) return NULL;

   for (path = getenv("PATH"); path && *path; path = *end ? end + 1 : end)
     {
        for (end = path; *end && (*end != ':'); end++);
        if (snprintf(buf, sizeof(buf), "%.*s/%s", (int)(end - path), path, prog) >=
            (int)sizeof(buf))
          continue;
        if (!access(buf, X_OK)) return realpath(buf, NULL);
     }
   return NULL;
}

static Eina_Bool
_e_comp_wl_privileges_list_has(const char *list, const char *privilege)
{
   size_t len = strlen(privilege);
   const char *p;

   for (p = strstr(list, privilege); p; p = strstr(p + len, privilege))
     {
        if ((p != list) && (p[-1] != ';')) continue;
        if ((p[len] == ';') || (!p[len])) return EINA_TRUE;
     }
   return EINA_FALSE;
}

static Eina_Bool
_e_comp_wl_privileges_file_grants(const char *file, const char *exe, const char *privilege)
{
   Efreet_Ini *ini;
   const char *list, *exec;
   char *prog = NULL;
   Eina_Bool granted;

   ini = efreet_ini_new(file);
   if (!ini) return EINA_FALSE;
   if (efreet_ini_section_set(ini, "Desktop Entry"))
     {
        list = efreet_ini_string_get(ini, "X-Enlightenment-Wayland-Privileges");
        exec = efreet_ini_string_get(ini, "Exec");
        if (list && exec && _e_comp_wl_privileges_list_has(list, privilege))
          prog = _e_comp_wl_privileges_exec_resolve(exec);
     }
   efreet_ini_free(ini);
   granted = prog && (!strcmp(prog, exe));
   free(prog);
   return granted;
}

static Eina_Bool
_e_comp_wl_privileges_dir_grants(const char *dir, const char *exe, const char *privilege)
{
   Eina_Iterator *it;
   const char *file;
   Eina_Bool granted = EINA_FALSE;

   it = eina_file_ls(dir);
   if (!it) return EINA_FALSE;
   EINA_ITERATOR_FOREACH(it, file)
     {
        if ((!granted) && eina_str_has_extension(file, ".desktop"))
          granted = _e_comp_wl_privileges_file_grants(file, exe, privilege);
        eina_stringshare_del(file);
     }
   eina_iterator_free(it);
   return granted;
}

/* The files are read at every check, so a change to them applies to the
 * next client without restarting E. */
static bool
_e_comp_wl_privileges_global_filter(const struct wl_client *client, const struct wl_global *global, void *data EINA_UNUSED)
{
   const struct wl_interface *iface = wl_global_get_interface(global);
   const char *privilege = NULL;
   char dir[PATH_MAX], *exe;
   Eina_Bool granted;
   unsigned int i;
   pid_t pid;

   for (i = 0; i < EINA_C_ARRAY_LENGTH(_privileged_globals); i++)
     if (_privileged_globals[i].iface == iface)
       privilege = _privileged_globals[i].privilege;
   if (!privilege) return true;

   wl_client_get_credentials((struct wl_client *)client, &pid, NULL, NULL);
   snprintf(dir, sizeof(dir), "/proc/%d/exe", (int)pid);
   exe = realpath(dir, NULL);
   if (!exe) return false;

   e_prefix_data_concat_static(dir, "data/wayland-privileges");
   granted = _e_comp_wl_privileges_dir_grants(dir, exe, privilege);
   if (!granted)
     {
        e_user_dir_concat_static(dir, "wayland-privileges");
        granted = _e_comp_wl_privileges_dir_grants(dir, exe, privilege);
     }
   if (!granted)
     INF("%s is not allowed %s, see wayland-privileges", exe, privilege);
   free(exe);
   return granted;
}

EINTERN void
e_comp_wl_privileges_init(void)
{
   wl_display_set_global_filter(e_comp_wl->wl.disp,
                                _e_comp_wl_privileges_global_filter, NULL);
}
