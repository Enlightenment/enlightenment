#include "e.h"

/* TODO List:
 *
 * * add module types/classes
 * * add list of exclusions that a module can't work with Api
 *
 */

/* local subsystem functions */
static void      _e_module_free(E_Module *m);
static void      _e_module_dialog_disable_create(const char *title, const char *body, E_Module *m);
static void      _e_module_cb_dialog_disable(void *data, E_Dialog *dia);
static void      _e_module_event_update_free(void *data, void *event);
static Eina_Bool _e_module_cb_idler(void *data);
static int       _e_module_sort_priority(const void *d1, const void *d2);
static void      _e_module_whitelist_check(void);
static Eina_Bool _e_module_desktop_list_cb(const Eina_Hash *hash EINA_UNUSED, const void *key, void *data, void *fdata);

/* local subsystem globals */
static Eina_List *_e_modules = NULL;
static Eina_Hash *_e_modules_hash = NULL;
static Ecore_Idle_Enterer *_e_module_idler = NULL;
static Eina_List *_e_modules_delayed = NULL;
static Eina_Bool _e_modules_initting = EINA_FALSE;
static Eina_Bool _e_modules_init_end = EINA_FALSE;

static Eina_List *_e_module_path_monitors = NULL;
static Eina_List *_e_module_path_lists = NULL;
static Eina_List *handlers = NULL;
static Eina_Hash *_e_module_path_hash = NULL;

E_API int E_EVENT_MODULE_UPDATE = 0;
E_API int E_EVENT_MODULE_INIT_END = 0;

static Eina_Stringshare *mod_src_path = NULL;

static Eina_Bool
_module_filter_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED, const Eina_File_Direct_Info *info)
{
   struct stat st;

   if (lstat(info->path, &st)) return EINA_FALSE;
   return (info->path[info->name_start] != '.');
}

static void
_module_main_cb(void *d, Eio_File *ls EINA_UNUSED, const Eina_File_Direct_Info *info)
{
   Eina_Stringshare *s;

   s = eina_hash_set(_e_module_path_hash, info->path + info->name_start, eina_stringshare_add(info->path));
   if (!s) return;
   if (!d)
     {
        if (!strstr(s, e_user_dir_get()))
          INF("REPLACING DUPLICATE MODULE PATH: %s -> %s", s, info->path);
        else
          {
             INF("NOT REPLACING DUPLICATE MODULE PATH: %s -X> %s", s, info->path);
             s = eina_hash_set(_e_module_path_hash, info->path + info->name_start, s);
          }
     }
   eina_stringshare_del(s);
}

static void
_module_done_cb(void *d EINA_UNUSED, Eio_File *ls)
{
   _e_module_path_lists = eina_list_remove(_e_module_path_lists, ls);
   if (_e_module_path_lists) return;
   if (_e_modules_initting) e_module_all_load();
   else if (!_e_modules_init_end)
     {
        ecore_event_add(E_EVENT_MODULE_INIT_END, NULL, NULL, NULL);
        _e_modules_init_end = EINA_TRUE;
     }
}

static void
_module_error_cb(void *d EINA_UNUSED, Eio_File *ls, int error EINA_UNUSED)
{
   _e_module_path_lists = eina_list_remove(_e_module_path_lists, ls);
   if (_e_module_path_lists) return;
   if (_e_modules_initting) e_module_all_load();
}

static Eina_Bool
_module_monitor_dir_create(void *d, int type EINA_UNUSED, Eio_Monitor_Event *ev)
{
   Eina_Stringshare *s;
   const char *path;

   path = ecore_file_file_get(ev->filename);
   s = eina_hash_set(_e_module_path_hash, path, eina_stringshare_ref(ev->filename));
   if (!s) return ECORE_CALLBACK_RENEW;
   if ((!d) && (s != ev->filename))
     {
        if (!strstr(s, e_user_dir_get()))
          INF("REPLACING DUPLICATE MODULE PATH: %s -> %s", s, ev->filename);
        else
          {
             INF("NOT REPLACING DUPLICATE MODULE PATH: %s -X> %s", s, ev->filename);
             s = eina_hash_set(_e_module_path_hash, path, s);
          }
     }
   eina_stringshare_del(s);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_module_monitor_dir_del(void *d EINA_UNUSED, int type EINA_UNUSED, Eio_Monitor_Event *ev)
{
   Eina_Stringshare *s;
   const char *path;

   path = ecore_file_file_get(ev->filename);
   s = eina_hash_find(_e_module_path_hash, path);
   if (s == ev->filename)
     eina_hash_del_by_key(_e_module_path_hash, path);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_module_monitor_error(void *d EINA_UNUSED, int type EINA_UNUSED, Eio_Monitor_Error *ev)
{
   _e_module_path_monitors = eina_list_remove(_e_module_path_monitors, ev->monitor);
   eio_monitor_del(ev->monitor);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_module_is_nosave(const char *name)
{
   const char *blacklist[] =
   {
      "comp",
      "conf_comp",
      "xwayland",
   };
   unsigned int i;

   for (i = 0; i < EINA_C_ARRAY_LENGTH(blacklist); i++)
     if (eina_streq(name, blacklist[i])) return EINA_TRUE;
   return !strncmp(name, "wl_", 3); //block wl_* modules from being saved
}

static Eina_Bool
_module_is_important(const char *name)
{
   const char *list[] =
   {
      "xwayland",
      "wl_desktop_shell",
      "wl_drm",
      "wl_fb",
      "wl_text_input",
      "wl_weekeyboard",
      "wl_wl",
      "wl_x11",
   };
   unsigned int i;

   for (i = 0; i < EINA_C_ARRAY_LENGTH(list); i++)
     if (eina_streq(name, list[i])) return EINA_TRUE;
   return EINA_FALSE;
}

/* externally accessible functions */
EINTERN int
e_module_init(void)
{
   Eina_List *module_paths;
   Eina_List *next_path;
   E_Path_Dir *epd;
   Eio_Monitor *mon;
   Eio_File *ls;

   if (_e_modules_hash) return 1;
   E_EVENT_MODULE_UPDATE = ecore_event_type_new();
   E_EVENT_MODULE_INIT_END = ecore_event_type_new();
   _e_module_path_hash = eina_hash_string_superfast_new((Eina_Free_Cb)eina_stringshare_del);
   _e_modules_hash = eina_hash_string_superfast_new(NULL);

   if (!mod_src_path)
     mod_src_path = eina_stringshare_add(getenv("E_MODULE_SRC_PATH"));

   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_DIRECTORY_CREATED, _module_monitor_dir_create, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_DIRECTORY_DELETED, _module_monitor_dir_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_ERROR, _module_monitor_error, NULL);

   if (mod_src_path)
     {
        if (ecore_file_is_dir(mod_src_path))
          {
             mon = eio_monitor_stringshared_add(mod_src_path);
             ls = eio_file_direct_ls(mod_src_path, _module_filter_cb, _module_main_cb, _module_done_cb, _module_error_cb, NULL);
             _e_module_path_monitors = eina_list_append(_e_module_path_monitors, mon);
             _e_module_path_lists = eina_list_append(_e_module_path_lists, ls);
             return 1;
          }
     }
   module_paths = e_path_dir_list_get(path_modules);
   EINA_LIST_FOREACH(module_paths, next_path, epd)
     {
        if (ecore_file_is_dir(epd->dir))
          {
             void *data = NULL;

             mon = eio_monitor_stringshared_add(epd->dir);
             data = (intptr_t*)(long)!!strstr(epd->dir, e_user_dir_get());
             ls = eio_file_direct_ls(epd->dir, _module_filter_cb, _module_main_cb, _module_done_cb, _module_error_cb, data);
             _e_module_path_monitors = eina_list_append(_e_module_path_monitors, mon);
             _e_module_path_lists = eina_list_append(_e_module_path_lists, ls);
          }
     }
   e_path_dir_list_free(module_paths);

   return 1;
}

EINTERN int
e_module_shutdown(void)
{
   E_Module *m;

#ifdef HAVE_VALGRIND
   /* do a leak check now before we dlclose() all those plugins, cause
    * that means we won't get a decent backtrace to leaks in there
    */
   VALGRIND_DO_LEAK_CHECK
#endif

   /* do not use EINA_LIST_FREE! e_object_del modifies list */
   if (x_fatal)
     e_module_save_all();
   else
     {
        while (_e_modules)
          {
             m = _e_modules->data;
             if ((m) && (m->enabled) && !(m->error))
               {
                  if (m->func.save) m->func.save(m);
                  if (m->func.shutdown) m->func.shutdown(m);
                  m->enabled = 0;
               }
             e_object_del(E_OBJECT(m));
          }
     }

   E_FREE_FUNC(_e_module_path_hash, eina_hash_free);
   E_FREE_FUNC(_e_modules_hash, eina_hash_free);
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FREE_LIST(_e_module_path_monitors, eio_monitor_del);
   E_FREE_LIST(_e_module_path_lists, eio_file_cancel);

   return 1;
}

E_API void
e_module_all_load(void)
{
   Eina_List *l, *ll;
   E_Config_Module *em;
   char buf[128];

   _e_modules_initting = EINA_TRUE;
   if (_e_module_path_lists) return;

   e_config->modules =
     eina_list_sort(e_config->modules, 0, _e_module_sort_priority);

   EINA_LIST_FOREACH_SAFE(e_config->modules, l, ll, em)
     {
        if ((!em) || (!em->name)) continue;

        if (_module_is_nosave(em->name))
          {
             eina_stringshare_del(em->name);
             e_config->modules = eina_list_remove_list(e_config->modules, l);
             free(em);
             continue;
          }
        if ((em->delayed) && (em->enabled) && (!e_config->no_module_delay))
          {
             if (!_e_module_idler)
               _e_module_idler = ecore_idle_enterer_add(_e_module_cb_idler, NULL);
             _e_modules_delayed =
               eina_list_append(_e_modules_delayed,
                                eina_stringshare_add(em->name));
          }
        else if (em->enabled)
          {
             E_Module *m;

             if (eina_hash_find(_e_modules_hash, em->name)) continue;

             e_util_env_set("E_MODULE_LOAD", em->name);
             snprintf(buf, sizeof(buf), _("Loading Module: %s"), em->name);
             e_init_status_set(buf);

             m = e_module_new(em->name);
             if (m) e_module_enable(m);
          }
     }

   if (!_e_modules_delayed)
     {
        ecore_event_add(E_EVENT_MODULE_INIT_END, NULL, NULL, NULL);
        _e_modules_init_end = EINA_TRUE;
        _e_modules_initting = EINA_FALSE;
        _e_module_whitelist_check();
     }

   unsetenv("E_MODULE_LOAD");
}

E_API Eina_Bool
e_module_loading_get(void)
{
   return !_e_modules_init_end;
}

E_API E_Module *
e_module_new(const char *name)
{
   E_Module *m;
   char buf[PATH_MAX];
   char body[4096], title[1024];
   const char *modpath = NULL;
   char *s;
   Eina_List *l, *ll;
   E_Config_Module *em;
   int in_list = 0;

   if (!name) return NULL;
   m = E_OBJECT_ALLOC(E_Module, E_MODULE_TYPE, _e_module_free);
   if (name[0] != '/')
     {
        Eina_Stringshare *path = NULL;

        if (!mod_src_path)
          mod_src_path = eina_stringshare_add(getenv("E_MODULE_SRC_PATH"));
        if (mod_src_path)
          {
             snprintf(buf, sizeof(buf), "%s/%s/.libs/module.so", mod_src_path, name);
             modpath = eina_stringshare_add(buf);
          }
        if (!modpath)
          path = eina_hash_find(_e_module_path_hash, name);
        if (path)
          {
             snprintf(buf, sizeof(buf), "%s/%s/module.so", path, MODULE_ARCH);
             modpath = eina_stringshare_add(buf);
          }
        else if (!modpath)
          {
             snprintf(buf, sizeof(buf), "%s/%s/module.so", name, MODULE_ARCH);
             modpath = e_path_find(path_modules, buf);
          }
     }
   else if (eina_str_has_extension(name, ".so"))
     modpath = eina_stringshare_add(name);
   if (!modpath)
     {
        snprintf(body, sizeof(body),
                 _("There was an error loading the module named: %s<br>"
                   "No module named %s could be found in the<br>"
                   "module search directories.<br>"), name, buf);
        _e_module_dialog_disable_create(_("Error loading Module"), body, m);
        m->error = 1;
        goto init_done;
     }
   m->handle = dlopen(modpath, (RTLD_NOW | RTLD_GLOBAL));
   if (!m->handle)
     {
        snprintf(body, sizeof(body),
                 _("There was an error loading the module named: %s<br>"
                   "The full path to this module is:<br>"
                   "%s<br>"
                   "The error reported was:<br>"
                   "%s<br>"), name, buf, dlerror());
        _e_module_dialog_disable_create(_("Error loading Module"), body, m);
        m->error = 1;
        goto init_done;
     }
   m->api = dlsym(m->handle, "e_modapi");
   m->func.init = dlsym(m->handle, "e_modapi_init");
   m->func.shutdown = dlsym(m->handle, "e_modapi_shutdown");
   m->func.save = dlsym(m->handle, "e_modapi_save");

   if ((!m->func.init) || (!m->api))
     {
        snprintf(body, sizeof(body),
                 _("There was an error loading the module named: %s<br>"
                   "The full path to this module is:<br>"
                   "%s<br>"
                   "The error reported was:<br>"
                   "%s<br>"),
                 name, buf, _("Module does not contain all needed functions"));
        _e_module_dialog_disable_create(_("Error loading Module"), body, m);
        m->api = NULL;
        m->func.init = NULL;
        m->func.shutdown = NULL;
        m->func.save = NULL;

        dlclose(m->handle);
        m->handle = NULL;
        m->error = 1;
        goto init_done;
     }
   if (m->api->version != E_MODULE_API_VERSION)
     {
        snprintf(body, sizeof(body),
                 _("Module API Error<br>Error initializing Module: %s<br>"
                   "It requires a module API version of: %i.<br>"
                   "The module API advertized by Enlightenment is: %i.<br>"),
                 _(m->api->name), m->api->version, E_MODULE_API_VERSION);

        snprintf(title, sizeof(title), _("Enlightenment %s Module"),
                 _(m->api->name));

        _e_module_dialog_disable_create(title, body, m);
        m->api = NULL;
        m->func.init = NULL;
        m->func.shutdown = NULL;
        m->func.save = NULL;
        dlclose(m->handle);
        m->handle = NULL;
        m->error = 1;
        goto init_done;
     }

init_done:

   _e_modules = eina_list_append(_e_modules, m);
   if (!_e_modules_hash)
     {
        /* wayland module preloading */
        if (!e_module_init())
          CRI("WTFFFFF");
     }
   eina_hash_add(_e_modules_hash, name, m);
   m->name = eina_stringshare_add(name);
   if (modpath)
     {
        s = ecore_file_dir_get(modpath);
        if (s)
          {
             char *s2;

             s2 = ecore_file_dir_get(s);
             free(s);
             if (s2)
               {
                  m->dir = eina_stringshare_add(s2);
                  free(s2);
               }
          }
     }
   EINA_LIST_FOREACH_SAFE(e_config->modules, l, ll, em)
     {
        if (!em) continue;
        if (em->name == m->name)
          {
             if (in_list)
               {
                  /* duplicate module config */
                  e_config->modules = eina_list_remove_list(e_config->modules, l);
                  eina_stringshare_del(em->name);
                  free(em);
               }
             else
               in_list = 1;
          }
     }
   if (!in_list)
     {
        E_Config_Module *module;

        module = E_NEW(E_Config_Module, 1);
        module->name = eina_stringshare_add(m->name);
        module->enabled = 0;
        e_config->modules = eina_list_append(e_config->modules, module);
        e_config_save_queue();
     }
   if (modpath) eina_stringshare_del(modpath);
   return m;
}

E_API int
e_module_save(E_Module *m)
{
   E_OBJECT_CHECK_RETURN(m, 0);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MODULE_TYPE, 0);
   if ((!m->enabled) || (m->error)) return 0;
   return m->func.save ? m->func.save(m) : 1;
}

E_API const char *
e_module_dir_get(E_Module *m)
{
   E_OBJECT_CHECK_RETURN(m, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MODULE_TYPE, 0);
   return m->dir;
}

E_API int
e_module_enable(E_Module *m)
{
   Eina_List *l;
   E_Event_Module_Update *ev;
   E_Config_Module *em;

   E_OBJECT_CHECK_RETURN(m, 0);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MODULE_TYPE, 0);
   if ((m->enabled) || (m->error)) return 0;
   m->data = m->func.init(m);
   if (m->data)
     {
        m->enabled = 1;
        EINA_LIST_FOREACH(e_config->modules, l, em)
          {
             if (!em) continue;
             if (!e_util_strcmp(em->name, m->name))
               {
                  em->enabled = 1;
                  e_config_save_queue();

                  ev = E_NEW(E_Event_Module_Update, 1);
                  ev->name = eina_stringshare_ref(em->name);
                  ev->enabled = 1;
                  ecore_event_add(E_EVENT_MODULE_UPDATE, ev,
                                  _e_module_event_update_free, NULL);
                  break;
               }
          }
        if (_e_modules_hash && (!_e_modules_initting))
          _e_module_whitelist_check();
        return 1;
     }
   return 0;
}

E_API int
e_module_disable(E_Module *m)
{
   E_Event_Module_Update *ev;
   Eina_List *l;
   E_Config_Module *em;
   int ret;

   E_OBJECT_CHECK_RETURN(m, 0);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MODULE_TYPE, 0);
   if ((!m->enabled) || (m->error)) return 0;
   if ((!stopping) && _module_is_important(m->name)) return 0;
   ret = m->func.shutdown ? m->func.shutdown(m) : 1;
   m->data = NULL;
   m->enabled = 0;
   EINA_LIST_FOREACH(e_config->modules, l, em)
     {
        if (!em) continue;
        if (!e_util_strcmp(em->name, m->name))
          {
             em->enabled = 0;
             e_config_save_queue();

             ev = E_NEW(E_Event_Module_Update, 1);
             ev->name = eina_stringshare_ref(em->name);
             ev->enabled = 0;
             ecore_event_add(E_EVENT_MODULE_UPDATE, ev,
                             _e_module_event_update_free, NULL);
             break;
          }
     }
   return ret;
}

E_API int
e_module_enabled_get(E_Module *m)
{
   E_OBJECT_CHECK_RETURN(m, 0);
   E_OBJECT_TYPE_CHECK_RETURN(m, E_MODULE_TYPE, 0);
   return m->enabled;
}

E_API int
e_module_save_all(void)
{
   Eina_List *l;
   E_Module *m;
   int ret = 1;

   EINA_LIST_FOREACH(_e_modules, l, m)
     {
        e_object_ref(E_OBJECT(m));
        if ((m->enabled) && (!m->error))
          {
             if (m->func.save && (!m->func.save(m))) ret = 0;
          }
        e_object_unref(E_OBJECT(m));
     }
   return ret;
}

E_API E_Module *
e_module_find(const char *name)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(name, NULL);
   return eina_hash_find(_e_modules_hash, name);
}

E_API Eina_List *
e_module_list(void)
{
   return _e_modules;
}

E_API void
e_module_dialog_show(E_Module *m, const char *title, const char *body)
{
   E_Dialog *dia;
   char buf[PATH_MAX];
   const char *icon = NULL;

   dia = e_dialog_new(NULL,
                      "E", "_module_dialog");
   if (!dia) return;

   e_dialog_title_set(dia, title);
   if (m)
     {
        Efreet_Desktop *desktop;

        snprintf(buf, sizeof(buf), "%s/module.desktop", e_module_dir_get(m));

        desktop = efreet_desktop_new(buf);
        if ((desktop) && (desktop->icon))
          {
             icon = efreet_icon_path_find(e_config->icon_theme, desktop->icon, 64);
             if (!icon)
               {
                  snprintf(buf, sizeof(buf), "%s/%s.edj",
                           e_module_dir_get(m), desktop->icon);
                  dia->icon_object = e_util_icon_add(buf, evas_object_evas_get(dia->win));
               }
             else
               dia->icon_object = e_util_icon_add(icon, evas_object_evas_get(dia->win));
             evas_object_size_hint_min_set(dia->icon_object, 64, 64);
             edje_object_part_swallow(dia->bg_object, "e.swallow.icon", dia->icon_object);
             evas_object_show(dia->icon_object);
          }
        if (desktop) efreet_desktop_free(desktop);
     }
   else
     e_dialog_icon_set(dia, "preferences-plugin", 64);

   e_dialog_text_set(dia, body);
   e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
   e_dialog_button_focus_num(dia, 0);
   elm_win_center(dia->win, 1, 1);
   e_dialog_show(dia);
   if (!m) return;
   e_win_client_icon_set(dia->win, icon);
}

E_API void
e_module_delayed_set(E_Module *m, int delayed)
{
   Eina_List *l;
   E_Config_Module *em;

   EINA_LIST_FOREACH(e_config->modules, l, em)
     {
        if (!em) continue;
        if (!e_util_strcmp(m->name, em->name))
          {
             if (em->delayed != delayed)
               {
                  em->delayed = delayed;
                  e_config_save_queue();
               }
             break;
          }
     }
}

E_API void
e_module_priority_set(E_Module *m, int priority)
{
   /* Set the loading order for a module.
      More priority means load earlier */
   Eina_List *l;
   E_Config_Module *em;

   EINA_LIST_FOREACH(e_config->modules, l, em)
     {
        if (!em) continue;
        if (!e_util_strcmp(m->name, em->name))
          {
             if (em->priority != priority)
               {
                  em->priority = priority;
                  e_config_save_queue();
               }
             break;
          }
     }
}

E_API Eina_List *
e_module_desktop_list(void)
{
   Eina_List *l = NULL;

   eina_hash_foreach(_e_module_path_hash, _e_module_desktop_list_cb, &l);
   return l;
}

E_API void
e_module_desktop_free(E_Module_Desktop *md)
{
   if (!md) return;
   eina_stringshare_del(md->dir);
   efreet_desktop_free(md->desktop);
   free(md);
}

/* local subsystem functions */

static void
_e_module_free(E_Module *m)
{
   E_Config_Module *em;
   Eina_List *l;

   EINA_LIST_FOREACH(e_config->modules, l, em)
     {
        if (!em) continue;
        if (!e_util_strcmp(em->name, m->name))
          {
             e_config->modules = eina_list_remove(e_config->modules, em);
             if (em->name) eina_stringshare_del(em->name);
             E_FREE(em);
             break;
          }
     }

   if ((m->enabled) && (!m->error))
     {
        if (m->func.save) m->func.save(m);
        if (m->func.shutdown) m->func.shutdown(m);
     }
   if (m->name) eina_stringshare_del(m->name);
   if (m->dir) eina_stringshare_del(m->dir);
//   if (m->handle) dlclose(m->handle); DONT dlclose! causes problems with deferred callbacks for free etc. - when their code goes away!
   _e_modules = eina_list_remove(_e_modules, m);
   free(m);
}

static Eina_Bool
_e_module_desktop_list_cb(const Eina_Hash *hash EINA_UNUSED, const void *key EINA_UNUSED, void *data, void *fdata)
{
   char buf[PATH_MAX];
   Eina_List **l = fdata;
   Efreet_Desktop *desktop;
   E_Module_Desktop *md;

   snprintf(buf, sizeof(buf), "%s/module.desktop", (char*)data);
   desktop = efreet_desktop_new(buf);
   if (desktop)
     {
        md = E_NEW(E_Module_Desktop, 1);
        md->desktop = desktop;
        md->dir = eina_stringshare_ref(data);
        *l = eina_list_append(*l, md);
     }
   return EINA_TRUE;
}

typedef struct Disable_Dialog
{
   char *title;
   char *body;
   E_Module *m;
} Disable_Dialog;

static void
_e_module_dialog_disable_show(const char *title, const char *body, E_Module *m)
{
   E_Dialog *dia;
   char buf[4096];

   printf("MODULE ERR:\n%s\n", body);

   dia = e_dialog_new(NULL, "E", "_module_unload_dialog");

   snprintf(buf, sizeof(buf), "%s<br>%s", body,
            _("What action should be taken with this module?<br>"));

   e_dialog_title_set(dia, title);
   e_dialog_icon_set(dia, "enlightenment", 64);
   e_dialog_text_set(dia, buf);
   e_dialog_button_add(dia, _("Unload"), NULL, _e_module_cb_dialog_disable, m);
   e_dialog_button_add(dia, _("Keep"), NULL, NULL, NULL);
   elm_win_center(dia->win, 1, 1);
   e_win_no_remember_set(dia->win, 1);
   e_dialog_show(dia);
}

static Eina_Bool
_e_module_dialog_disable_timer(Disable_Dialog *dd)
{
   _e_module_dialog_disable_show(dd->title, dd->body, dd->m);
   free(dd->title);
   free(dd->body);
   free(dd);
   return EINA_FALSE;
}

static void
_e_module_dialog_disable_create(const char *title, const char *body, E_Module *m)
{
   Disable_Dialog *dd;

   dd = E_NEW(Disable_Dialog, 1);
   dd->title = strdup(title);
   dd->body = strdup(body);
   dd->m = m;
   ecore_timer_add(1.5, (Ecore_Task_Cb)_e_module_dialog_disable_timer, dd);
}

static void
_e_module_cb_dialog_disable(void *data, E_Dialog *dia)
{
   E_Module *m;

   m = data;
   e_module_disable(m);
   e_object_del(E_OBJECT(m));
   e_object_del(E_OBJECT(dia));
   e_config_save_queue();
}

static Eina_Bool
_e_module_cb_idler(void *data EINA_UNUSED)
{
   while (_e_modules_delayed)
     {
        const char *name;
        E_Module *m;

        name = eina_list_data_get(_e_modules_delayed);
        _e_modules_delayed =
          eina_list_remove_list(_e_modules_delayed, _e_modules_delayed);
        if (eina_hash_find(_e_modules_hash, name))
          {
             eina_stringshare_del(name);
             break;
          }
        m = NULL;
        if (name) m = e_module_new(name);
        if (m)
          {
#ifndef E_RELEASE_BUILD
             char buf[1024];
             snprintf(buf, sizeof(buf), "DELAYED MODULE LOAD: %s", name);
             e_main_ts(buf);
#endif
             e_module_enable(m);
          }
        eina_stringshare_del(name);
        break;
     }
   if (_e_modules_delayed)
     {
        e_util_wakeup();
        return ECORE_CALLBACK_RENEW;
     }

   ecore_event_add(E_EVENT_MODULE_INIT_END, NULL, NULL, NULL);
   _e_modules_init_end = EINA_TRUE;
   _e_modules_initting = EINA_FALSE;
   _e_module_whitelist_check();

   _e_module_idler = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static int
_e_module_sort_priority(const void *d1, const void *d2)
{
   const E_Config_Module *m1, *m2;

   m1 = d1;
   m2 = d2;
   return m2->priority - m1->priority;
}

static void
_e_module_event_update_free(void *data EINA_UNUSED, void *event)
{
   E_Event_Module_Update *ev;

   if (!(ev = event)) return;
   eina_stringshare_del(ev->name);
   E_FREE(ev);
}

static void
_cleanup_cb(void *data, E_Dialog *dialog)
{
   Eina_List *badl = data;
   const char *s;

   e_object_del(E_OBJECT(dialog));
   EINA_LIST_FREE(badl, s)
     eina_stringshare_del(s);
}

static void
_ignore_cb(void *data, E_Dialog *dialog)
{
   const char *s;

   e_object_del(E_OBJECT(dialog));

   EINA_LIST_FREE(e_config->bad_modules, s)
     eina_stringshare_del(s);
   e_config->bad_modules = data;
   e_config_save_queue();
}

static Eina_Bool
_e_module_whitelist_dialog_timer(void *badl)
{
   E_Dialog *dia;
   Eina_Strbuf *sbuf;
   Eina_List *l;
   const char *s;

   dia = e_dialog_new(NULL,
                      "E", "_module_whitelist_dialog");
   sbuf = eina_strbuf_new();
   eina_strbuf_append
     (sbuf, _("The following modules are not standard ones for<br>"
              "Enlightenment and may cause bugs and crashes.<br>"
              "Please remove them before reporting any bugs.<br>"
              "<br>"
              "The module list is as follows:<br>"
              "<br>"));
   EINA_LIST_FOREACH(badl, l, s)
     {
        eina_strbuf_append(sbuf, s);
        eina_strbuf_append(sbuf, "<br>");
     }

   e_dialog_title_set(dia, _("Unstable module tainting"));
   e_dialog_icon_set(dia, "enlightenment", 64);
   e_dialog_text_set(dia, eina_strbuf_string_get(sbuf));
   e_dialog_button_add(dia, _("OK"), NULL, _cleanup_cb, badl);
   e_dialog_button_add(dia, _("I know"), NULL, _ignore_cb, badl);
   elm_win_center(dia->win, 1, 1);
   e_dialog_show(dia);
   eina_strbuf_free(sbuf);
   return EINA_FALSE;
}

static void
_e_module_whitelist_check(void)
{
   Eina_List *l, *badl = NULL;
   E_Module *m;
   unsigned int known = 0;
   int i;
   const char *s;
   const char *goodmods[] =
   {
      "backlight",
      "battery",
      "bluez4",
      "clock",
      "conf",
      "conf_applications",
      "conf_comp",
      "conf_dialogs",
      "conf_display",
      "conf_interaction",
      "conf_intl",
      "conf_bindings",
      "conf_menus",
      "conf_paths",
      "conf_performance",
      "conf_randr",
      "conf_shelves",
      "conf_theme",
      "conf_window_manipulation",
      "conf_window_remembers",
      "connman",
      "cpufreq",
      "everything",
      "fileman",
      "fileman_opinfo",
      "gadman",
      "ibar",
      "ibox",
      "layout",
      "lokker",
      "mixer",
      "msgbus",
      "notification",
      "ofono",
      "pager",
      "pager_plain",
      "quickaccess",
      "shot",
      "start",
      "syscon",
      "systray",
      "tasks",
      "teamwork",
      "temperature",
      "tiling",
      "time",
      "winlist",
      "wireless",
      "wizard",
      "wl_desktop_shell",
      "wl_x11",
      "wl_wl",
      "wl_drm",
      "wl_shell",
      "wl_desktop_shell",
      "xkbswitch",
      "music-control",
      "appmenu",
      "packagekit",
      "policy_mobile",
      "geolocation",
      "xwayland",
      NULL   // end marker
   };

   EINA_LIST_FOREACH(_e_modules, l, m)
     {
        Eina_Bool ok;

        if (!m->name) continue;
        ok = EINA_FALSE;
        for (i = 0; goodmods[i]; i++)
          {
             if (!strcmp(m->name, goodmods[i]))
               {
                  ok = EINA_TRUE;
                  break;
               }
          }
        if (!ok) badl = eina_list_append(badl, eina_stringshare_add(m->name));
     }

   EINA_LIST_FOREACH(badl, l, s)
     {
        const char *tmp;
        Eina_List *ll;
        Eina_Bool found = EINA_FALSE;

        EINA_LIST_FOREACH(e_config->bad_modules, ll, tmp)
          {
             if (!strcmp(s, tmp))
               {
                  found = EINA_TRUE;
                  break;
               }
          }

        if (!found) break;
        known++;
     }

#ifndef HAVE_WAYLAND_ONLY
   {
      const char *state;

      state = badl ? "YES" : "NO";

      if (e_comp->comp_type == E_PIXMAP_TYPE_X)
        {
           Ecore_X_Atom _x_tainted;
           unsigned int _e_tainted = badl ? 1 : 0;

           _x_tainted = ecore_x_atom_get("_E_TAINTED");
           ecore_x_window_prop_card32_set(ecore_x_window_root_first_get(),
                                          _x_tainted, &_e_tainted, 1);
        }

      e_env_set("E_TAINTED", state);
   }
#endif

   if (eina_list_count(badl) != known)
     ecore_timer_add(1.5, _e_module_whitelist_dialog_timer, badl);
   else
     {
        EINA_LIST_FREE(badl, s)
          eina_stringshare_del(s);
     }
}

