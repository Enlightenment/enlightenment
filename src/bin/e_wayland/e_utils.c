#include "e.h"

/* local function prototypes */
static Eina_Bool _e_util_cb_delayed_del(void *data);
static Eina_Bool _e_util_wakeup_cb(void *data);
static int _e_util_menu_item_fdo_icon_set(E_Menu_Item *mi, const char *icon);
static int _e_util_menu_item_edje_icon_set(E_Menu_Item *mi, const char *name, Eina_Bool fallback);
static Evas_Object *_e_util_icon_add(const char *path, Evas *evas, int size);
static int _e_util_icon_fdo_set(Evas_Object *obj, const char *icon);
static int _e_util_icon_theme_set(Evas_Object *obj, const char *icon, Eina_Bool fallback);
static Efreet_Desktop *_e_util_default_terminal_get(const char *defaults_list);

/* local variables */
static Ecore_Timer *_e_util_dummy_timer = NULL;
static char *prev_ld_library_path = NULL;
static char *prev_path = NULL;

/* external variables */
EAPI E_Path *path_data = NULL;
EAPI E_Path *path_images = NULL;
EAPI E_Path *path_fonts = NULL;
EAPI E_Path *path_themes = NULL;
EAPI E_Path *path_icons = NULL;
EAPI E_Path *path_modules = NULL;
EAPI E_Path *path_backgrounds = NULL;
EAPI E_Path *path_messages = NULL;

EAPI void 
e_util_wakeup(void)
{
   if (_e_util_dummy_timer) return;
   _e_util_dummy_timer = ecore_timer_add(0.0, _e_util_wakeup_cb, NULL);
}

EAPI void 
e_util_env_set(const char *var, const char *val)
{
   if (val)
     {
#ifdef HAVE_SETENV
	setenv(var, val, 1);
#else
	char buf[8192];

	snprintf(buf, sizeof(buf), "%s=%s", var, val);
	if (getenv(var))
	  putenv(buf);
	else
	  putenv(strdup(buf));
#endif
     }
   else
     {
#ifdef HAVE_UNSETENV
	unsetenv(var);
#else
	if (getenv(var)) putenv(var);
#endif
     }
}

EAPI void 
e_util_dialog_internal(const char *title, const char *txt)
{
   E_Dialog *dia;

   dia = 
     e_dialog_new(e_container_current_get(e_manager_current_get()), 
                  "E", "_error_dialog");
   if (!dia) return;
   e_dialog_title_set(dia, title);
   e_dialog_text_set(dia, txt);
   e_dialog_icon_set(dia, "dialog-error", 64);
   e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
   e_dialog_button_focus_num(dia, 0);
   e_win_centered_set(dia->win, EINA_TRUE);
   e_dialog_show(dia);
}

EAPI int 
e_util_strcmp(const char *s1, const char *s2)
{
   if ((s1) && (s2)) return strcmp(s1, s2);
   return 0x7fffffff;
}

EAPI int 
e_util_immortal_check(void)
{
   return 0;
}

EAPI const char *
e_util_filename_escape(const char *filename)
{
   const char *p;
   char *q;
   static char buf[PATH_MAX];

   if (!filename) return NULL;
   p = filename;
   q = buf;
   while (*p)
     {
	if ((q - buf) > 4090) return NULL;
	if (
	    (*p == ' ') || (*p == '\t') || (*p == '\n') ||
	    (*p == '\\') || (*p == '\'') || (*p == '\"') ||
	    (*p == ';') || (*p == '!') || (*p == '#') ||
	    (*p == '$') || (*p == '%') || (*p == '&') ||
	    (*p == '*') || (*p == '(') || (*p == ')') ||
	    (*p == '[') || (*p == ']') || (*p == '{') ||
	    (*p == '}') || (*p == '|') || (*p == '<') ||
	    (*p == '>') || (*p == '?')
	    )
	  {
	     *q = '\\';
	     q++;
	  }
	*q = *p;
	q++;
	p++;
     }
   *q = 0;
   return buf;
}

EAPI void 
e_util_defer_object_del(E_Object *obj)
{
   if (stopping)
     e_object_del(obj);
   else
     ecore_idle_enterer_before_add(_e_util_cb_delayed_del, obj);
}

EAPI Evas_Object *
e_util_icon_add(const char *path, Evas *evas)
{
   Evas_Object *o = NULL;
   const char *ext;
   int size = 64;

   if (!path) return NULL;
   if (!ecore_file_exists(path)) return NULL;

   o = e_icon_add(evas);
   e_icon_scale_size_set(o, size); 
   e_icon_preload_set(o, 1);
   ext = strrchr(path, '.');
   if (ext)
     {
	if (!strcmp(ext, ".edj"))
	  e_icon_file_edje_set(o, path, "icon");
	else
	  e_icon_file_set(o, path);
     }
   else
     e_icon_file_set(o, path);
   e_icon_fill_inside_set(o, 1);

   return o;
}

EAPI Evas_Object *
e_util_icon_theme_icon_add(const char *name, unsigned int size, Evas *evas)
{
   if (!name) return NULL;
   if (name[0] == '/') return _e_util_icon_add(name, evas, size);
   else
     {
	Evas_Object *obj;
	const char *path;

	path = efreet_icon_path_find(e_config->icon_theme, name, size);
	if (path)
	  {
	     obj = _e_util_icon_add(path, evas, size);
	     return obj;
	  }
     }
   return NULL;
}

EAPI unsigned int
e_util_icon_size_normalize(unsigned int desired)
{
   const unsigned int *itr, known_sizes[] =
     {
        16, 22, 24, 32, 36, 48, 64, 72, 96, 128, 192, 256, -1
     };

   for (itr = known_sizes; *itr > 0; itr++)
     if (*itr >= desired)
       return *itr;

   return 256; /* largest know size? */
}

EAPI int
e_util_menu_item_theme_icon_set(E_Menu_Item *mi, const char *icon)
{
   if (e_config->icon_theme_overrides)
     {
	if (_e_util_menu_item_fdo_icon_set(mi, icon))
	  return 1;
	if (_e_util_menu_item_edje_icon_set(mi, icon, EINA_FALSE))
	  return 1;
	return _e_util_menu_item_edje_icon_set(mi, icon, EINA_TRUE);
     }
   else
     {
	if (_e_util_menu_item_edje_icon_set(mi, icon, EINA_FALSE))
	  return 1;
	if (_e_util_menu_item_fdo_icon_set(mi, icon))
	  return 1;
	return _e_util_menu_item_edje_icon_set(mi, icon, EINA_TRUE);
     }
}

EAPI const char *
e_util_winid_str_get(Ecore_Wl_Window *win)
{
   const char *vals = "qWeRtYuIoP5-$&<~";
   static char id[9];
   unsigned int val;

   val = (unsigned int)win->id;
   id[0] = vals[(val >> 28) & 0xf];
   id[1] = vals[(val >> 24) & 0xf];
   id[2] = vals[(val >> 20) & 0xf];
   id[3] = vals[(val >> 16) & 0xf];
   id[4] = vals[(val >> 12) & 0xf];
   id[5] = vals[(val >> 8) & 0xf];
   id[6] = vals[(val >> 4) & 0xf];
   id[7] = vals[(val) & 0xf];
   id[8] = 0;

   return id;
}

EAPI E_Container *
e_util_container_number_get(int num)
{
   Eina_List *l;
   E_Manager *man;

   EINA_LIST_FOREACH(e_manager_list(), l, man)
     {
        E_Container *con;

        if ((con = e_container_number_get(man, num)))
          return con;
     }

   return NULL;
}

EAPI E_Zone *
e_util_container_zone_number_get(int con, int zone)
{
   E_Container *c;

   if (!(c = e_util_container_number_get(con)))
     return NULL;

   return e_container_zone_number_get(c, zone);
}

EAPI E_Zone *
e_util_zone_current_get(E_Manager *man)
{
   E_Container *con;

   E_OBJECT_CHECK_RETURN(man, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(man, E_MANAGER_TYPE, NULL);

   if ((con = e_container_current_get(man)))
     {
	E_Zone *zone;

	zone = e_zone_current_get(con);
	return zone;
     }
   return NULL;
}

EAPI int
e_util_glob_match(const char *str, const char *glob)
{
   if ((!str) || (!glob)) return 0;
   if (glob[0] == 0)
     {
	if (str[0] == 0) return 1;
	return 0;
     }
   if (!strcmp(glob, "*")) return 1;
   if (!fnmatch(glob, str, 0)) return 1;
   return 0;
}

EAPI int 
e_util_glob_case_match(const char *str, const char *glob)
{
   const char *p;
   char *tstr, *tglob, *tp;

   if (glob[0] == 0)
     {
	if (str[0] == 0) return 1;
	return 0;
     }
   if (!strcmp(glob, "*")) return 1;
   tstr = alloca(strlen(str) + 1);
   for (tp = tstr, p = str; *p != 0; p++, tp++) *tp = tolower(*p);
   *tp = 0;
   tglob = alloca(strlen(glob) + 1);
   for (tp = tglob, p = glob; *p != 0; p++, tp++) *tp = tolower(*p);
   *tp = 0;
   if (!fnmatch(tglob, tstr, 0)) return 1;
   return 0;
}

EAPI int 
e_util_edje_icon_set(Evas_Object *obj, const char *name)
{
   const char *file;
   char buf[PATH_MAX];

   if ((!name) || (!name[0])) return 0;
   snprintf(buf, sizeof(buf), "e/icons/%s", name);
   file = e_theme_edje_file_get("base/theme/icons", buf);
   if (file[0])
     {
	edje_object_file_set(obj, file, buf);
	return 1;
     }
   return 0;
}

EAPI int 
e_util_icon_theme_set(Evas_Object *obj, const char *icon)
{
   if (e_config->icon_theme_overrides)
     {
	if (_e_util_icon_fdo_set(obj, icon))
	  return 1;
	if (_e_util_icon_theme_set(obj, icon, EINA_FALSE))
	  return 1;
	return _e_util_icon_theme_set(obj, icon, EINA_TRUE);
     }
   else
     {
	if (_e_util_icon_theme_set(obj, icon, EINA_FALSE))
	  return 1;
	if (_e_util_icon_fdo_set(obj, icon))
	  return 1;
	return _e_util_icon_theme_set(obj, icon, EINA_TRUE);
     }
}

EAPI char *
e_util_size_string_get(off_t size)
{
   double dsize;
   char buf[256];

   dsize = (double)size;
   if (dsize < 1024.0) snprintf(buf, sizeof(buf), _("%'.0f Bytes"), dsize);
   else
     {
	dsize /= 1024.0;
	if (dsize < 1024) snprintf(buf, sizeof(buf), _("%'.0f KB"), dsize);
	else
	  {
	     dsize /= 1024.0;
	     if (dsize < 1024) snprintf(buf, sizeof(buf), _("%'.0f MB"), dsize);
	     else
	       {
		  dsize /= 1024.0;
		  snprintf(buf, sizeof(buf), _("%'.1f GB"), dsize);
	       }
	  }
     }
   return strdup(buf);
}

EAPI char *
e_util_file_time_get(time_t ftime)
{
   time_t diff;
   time_t ltime;
   char buf[256];
   char *s = NULL;

   ltime = time(NULL);
   diff = ltime - ftime;
   buf[0] = 0;
   if (ftime > ltime)
     snprintf(buf, sizeof(buf), _("In the Future"));
   else
     {
	if (diff <= 60)
	  snprintf(buf, sizeof(buf), _("In the last Minute"));
	else if (diff >= 31526000)
	  snprintf(buf, sizeof(buf), _("%li Years ago"), (diff / 31526000));
	else if (diff >= 2592000)
	  snprintf(buf, sizeof(buf), _("%li Months ago"), (diff / 2592000));
	else if (diff >= 604800)
	  snprintf(buf, sizeof(buf), _("%li Weeks ago"), (diff / 604800));
	else if (diff >= 86400)
	  snprintf(buf, sizeof(buf), _("%li Days ago"), (diff / 86400));
	else if (diff >= 3600)
	  snprintf(buf, sizeof(buf), _("%li Hours ago"), (diff / 3600));
	else if (diff > 60)
	  snprintf(buf, sizeof(buf), _("%li Minutes ago"), (diff / 60));
     }

   if (buf[0])
     s = strdup(buf);
   else
     s = strdup(_("Unknown"));
   return s;
}

EAPI void 
e_util_library_path_strip(void)
{
   char *p, *p2;

   p = getenv("LD_LIBRARY_PATH");
   E_FREE(prev_ld_library_path);
   if (p)
     {
	prev_ld_library_path = strdup(p);
	p2 = strchr(p, ':');
	if (p2) p2++;
	e_util_env_set("LD_LIBRARY_PATH", p2);
     }
   p = getenv("PATH");
   E_FREE(prev_path);
   if (p)
     {
	prev_path = strdup(p);
	p2 = strchr(p, ':');
	if (p2) p2++;
	e_util_env_set("PATH", p2);
     }
}

EAPI void 
e_util_library_path_restore(void)
{
   if (prev_ld_library_path)
     {
        e_util_env_set("LD_LIBRARY_PATH", prev_ld_library_path);
        E_FREE(prev_ld_library_path);
     }
   if (prev_path)
     {
        e_util_env_set("PATH", prev_path);
        E_FREE(prev_path);
     }
}

EAPI void 
e_util_win_auto_resize_fill(E_Win *win)
{

}

EAPI Evas_Object *
e_util_desktop_icon_add(Efreet_Desktop *desktop, unsigned int size, Evas *evas)
{
   if ((!desktop) || (!desktop->icon)) return NULL;
   return e_util_icon_theme_icon_add(desktop->icon, size, evas);
}

EAPI char *
e_util_shell_env_path_eval(const char *path)
{
   /* evaluate things like:
    * $HOME/bling -> /home/user/bling
    * $HOME/bin/$HOSTNAME/blah -> /home/user/bin/localhost/blah
    * etc. etc.
    */
   const char *p, *v2, *v1 = NULL;
   char buf[PATH_MAX], *pd, *s, *vp;
   char *v = NULL;
   int esc = 0, invar = 0;

   for (p = path, pd = buf; (pd < (buf + sizeof(buf) - 1)); p++)
     {
	if (invar)
	  {
	     if (!((isalnum(*p)) || (*p == '_')))
	       {
		  v2 = p;
		  invar = 0;
		  if ((v2 - v1) > 1)
		    {
		       s = alloca(v2 - v1);
		       strncpy(s, v1 + 1, v2 - v1 - 1);
		       s[v2 - v1 - 1] = 0;
		       if (strncmp(s, "XDG", 3))
			 v = getenv(s);
		       else
			 {
			    if (!strcmp(s, "XDG_CONFIG_HOME"))
			      v = (char *)efreet_config_home_get();
			    else if (!strcmp(s, "XDG_CACHE_HOME"))
			      v = (char *)efreet_cache_home_get();
			    else if (!strcmp(s, "XDG_DATA_HOME"))
			      v = (char *)efreet_data_home_get();
			 }

		       if (v)
			 {
			    vp = v;
			    while ((*vp) && (pd < (buf + sizeof(buf) - 1)))
			      {
				 *pd = *vp;
				 vp++;
				 pd++;
			      }
			 }
		    }
		  if (pd < (buf + sizeof(buf) - 1))
		    {
		       *pd = *p;
		       pd++;
		    }
	       }
	  }
	else
	  {
	     if (esc)
	       {
		  *pd = *p;
		  pd++;
	       }
	     else
	       {
		  if (*p == '\\') esc = 1;
		  else if (*p == '$')
		    {
		       invar = 1;
		       v1 = p;
		    }
		  else
		    {
		       *pd = *p;
		       pd++;
		    }
	       }
	  }
	if (*p == 0) break;
     }
   *pd = 0;
   return strdup(buf);
}

EAPI int
e_util_edje_collection_exists(const char *file, const char *coll)
{
   Eina_List *clist, *l;
   char *str;

   clist = edje_file_collection_list(file);
   EINA_LIST_FOREACH(clist, l, str)
     {
	if (!strcmp(coll, str))
	  {
	     edje_file_collection_list_free(clist);
	     return 1;
	  }
     }
   edje_file_collection_list_free(clist);
   return 0;
}

EAPI void
e_util_desktop_menu_item_icon_add(Efreet_Desktop *desktop, unsigned int size, E_Menu_Item *mi)
{
   const char *path = NULL;

   if ((!desktop) || (!desktop->icon)) return;

   if (desktop->icon[0] == '/') path = desktop->icon;
   else path = efreet_icon_path_find(e_config->icon_theme, desktop->icon, size);

   if (path)
     {
	const char *ext;

	ext = strrchr(path, '.');
	if (ext)
	  {
	     if (strcmp(ext, ".edj") == 0)
	       e_menu_item_icon_edje_set(mi, path, "icon");
	     else
	       e_menu_item_icon_file_set(mi, path);
	  }
	else
	  e_menu_item_icon_file_set(mi, path);
     }
}

EAPI int 
e_util_container_desk_count_get(E_Container *con)
{
   Eina_List *zl;
   E_Zone *zone;
   int count = 0;

   E_OBJECT_CHECK_RETURN(con, 0);
   E_OBJECT_TYPE_CHECK_RETURN(con, E_CONTAINER_TYPE, 0);

   EINA_LIST_FOREACH(con->zones, zl, zone)
     {
        int x, y;
        int cx = 0, cy = 0;

        e_zone_desk_count_get(zone, &cx, &cy);
        for (x = 0; x < cx; x++)
          {
             for (y = 0; y < cy; y++)
               count += 1;
          }
     }
   return count;
}

EAPI int
e_util_strcasecmp(const char *s1, const char *s2)
{
   if ((!s1) && (!s2)) return 0;
   if (!s1) return -1;
   if (!s2) return 1;
   return strcasecmp(s1, s2);
}

EAPI Efreet_Desktop *
e_util_terminal_desktop_get(void)
{
   const char *terms[] =
   {
      "terminology.desktop",
      "xterm.desktop",
      "rxvt.desktop",
      "gnome-terminal.desktop",
      "konsole.desktop",
      NULL
   };
   const char *s;
   char buf[PATH_MAX];
   Efreet_Desktop *tdesktop = NULL, *td;
   Eina_List *l;
   int i;

   s = efreet_data_home_get();
   if (s)
     {
        snprintf(buf, sizeof(buf), "%s/applications/defaults.list", s);
        tdesktop = _e_util_default_terminal_get(buf);
     }
   if (tdesktop) return tdesktop;
   EINA_LIST_FOREACH(efreet_data_dirs_get(), l, s)
     {
        snprintf(buf, sizeof(buf), "%s/applications/defaults.list", s);
        tdesktop = _e_util_default_terminal_get(buf);
        if (tdesktop) return tdesktop;
     }

   for (i = 0; terms[i]; i++)
     {
        tdesktop = efreet_util_desktop_file_id_find(terms[i]);
        if (tdesktop) return tdesktop;
     }
   if (!tdesktop)
     {
        l = efreet_util_desktop_category_list("TerminalEmulator");
        if (l)
          {
             // just take first one since above list doesn't work.
             tdesktop = l->data;
             EINA_LIST_FREE(l, td)
               {
                  // free/unref the desktosp we are not going to use
                  if (td != tdesktop) efreet_desktop_free(td);
               }
          }
     }
   return tdesktop;
}

/* local functions */
static Eina_Bool
_e_util_cb_delayed_del(void *data)
{
   e_object_del(E_OBJECT(data));
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_util_wakeup_cb(void *data __UNUSED__)
{
   _e_util_dummy_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static int
_e_util_menu_item_fdo_icon_set(E_Menu_Item *mi, const char *icon)
{
   const char *path = NULL;
   unsigned int size;

   if ((!icon) || (!icon[0])) return 0;
   size = e_util_icon_size_normalize(24 * e_scale);
   path = efreet_icon_path_find(e_config->icon_theme, icon, size);
   if (!path) return 0;
   e_menu_item_icon_file_set(mi, path);
   return 1;
}

static int
_e_util_menu_item_edje_icon_set(E_Menu_Item *mi, const char *name, Eina_Bool fallback)
{
   const char *file;
   char buf[PATH_MAX];

   if ((!name) || (!name[0])) return 0;

   if ((!fallback) && (name[0]=='/') && ecore_file_exists(name))
     {
	e_menu_item_icon_edje_set(mi, name, "icon");
	return 1;
     }
   snprintf(buf, sizeof(buf), "e/icons/%s", name);

   if (fallback)
     file = e_theme_edje_icon_fallback_file_get(buf);
   else
     file = e_theme_edje_file_get("base/theme/icons", buf);

   if (file[0])
     {
	e_menu_item_icon_edje_set(mi, file, buf);
	return 1;
     }
   return 0;
}

static Evas_Object *
_e_util_icon_add(const char *path, Evas *evas, int size)
{
   Evas_Object *o = NULL;
   const char *ext;

   if (!path) return NULL;
   if (!ecore_file_exists(path)) return NULL;

   o = e_icon_add(evas);
   e_icon_scale_size_set(o, size); 
   e_icon_preload_set(o, 1);
   ext = strrchr(path, '.');
   if (ext)
     {
	if (!strcmp(ext, ".edj"))
	  e_icon_file_edje_set(o, path, "icon");
	else
	  e_icon_file_set(o, path);
     }
   else
     e_icon_file_set(o, path);
   e_icon_fill_inside_set(o, 1);

   return o;
}

static int
_e_util_icon_fdo_set(Evas_Object *obj, const char *icon)
{
   const char *path = NULL;
   unsigned int size;
   
   if ((!icon) || (!icon[0])) return 0;
   size = e_icon_scale_size_get(obj);
   if (size < 16) size = 16;
   size = e_util_icon_size_normalize(size * e_scale);

   path = efreet_icon_path_find(e_config->icon_theme, icon, size);
   if (!path) return 0;

   e_icon_file_set(obj, path);
   return 1;
}

static int
_e_util_icon_theme_set(Evas_Object *obj, const char *icon, Eina_Bool fallback)
{
   const char *file;
   char buf[PATH_MAX];

   if ((!icon) || (!icon[0])) return 0;
   snprintf(buf, sizeof(buf), "e/icons/%s", icon);

   if (fallback)
     file = e_theme_edje_icon_fallback_file_get(buf);
   else
     file = e_theme_edje_file_get("base/theme/icons", buf);

   if (file[0])
     {
	e_icon_file_edje_set(obj, file, buf);
	return 1;
     }

   return 0;
}

static Efreet_Desktop *
_e_util_default_terminal_get(const char *defaults_list)
{
   Efreet_Desktop *tdesktop = NULL;
   Efreet_Ini *ini;
   const char *s;

   ini = efreet_ini_new(defaults_list);
   if ((ini) && (ini->data) &&
       (efreet_ini_section_set(ini, "Default Applications")) &&
       (ini->section))
     {
        s = efreet_ini_string_get(ini, "x-scheme-handler/terminal");
        if (s) tdesktop = efreet_util_desktop_file_id_find(s);
     }
   if (ini) efreet_ini_free(ini);
   return tdesktop;
}
