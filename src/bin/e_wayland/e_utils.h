#ifdef E_TYPEDEFS
#else
# ifndef E_UTILS_H
#  define E_UTILS_H

#define e_util_dialog_show(title, args...) \
{ \
   char __tmpbuf[PATH_MAX]; \
   \
   snprintf(__tmpbuf, sizeof(__tmpbuf), ##args); \
   e_util_dialog_internal(title, __tmpbuf); \
}

EAPI void e_util_wakeup(void);
EAPI void e_util_env_set(const char *var, const char *val);
EAPI void e_util_dialog_internal(const char *title, const char *txt);
EAPI int e_util_strcmp(const char *s1, const char *s2);
EAPI int e_util_immortal_check(void);
EAPI const char *e_util_filename_escape(const char *filename);
EAPI void e_util_defer_object_del(E_Object *obj);
EAPI Evas_Object *e_util_icon_add(const char *path, Evas *evas);
EAPI Evas_Object *e_util_icon_theme_icon_add(const char *name, unsigned int size, Evas *evas);
EAPI unsigned int e_util_icon_size_normalize(unsigned int desired);
EAPI int e_util_menu_item_theme_icon_set(E_Menu_Item *mi, const char *icon);
EAPI const char *e_util_winid_str_get(Ecore_Wl_Window *win);
EAPI E_Container *e_util_container_number_get(int num);
EAPI E_Zone *e_util_container_zone_number_get(int con, int zone);
EAPI E_Zone *e_util_zone_current_get(E_Manager *man);
EAPI int e_util_glob_match(const char *str, const char *glob);
EAPI int e_util_glob_case_match(const char *str, const char *glob);
EAPI int e_util_edje_icon_set(Evas_Object *obj, const char *name);
EAPI int e_util_icon_theme_set(Evas_Object *obj, const char *icon);
EAPI char *e_util_size_string_get(off_t size);
EAPI char *e_util_file_time_get(time_t ftime);
EAPI void e_util_library_path_strip(void);
EAPI void e_util_library_path_restore(void);
EAPI void e_util_win_auto_resize_fill(E_Win *win);
EAPI Evas_Object *e_util_desktop_icon_add(Efreet_Desktop *desktop, unsigned int size, Evas *evas);
EAPI char *e_util_shell_env_path_eval(const char *path);
EAPI int e_util_edje_collection_exists(const char *file, const char *coll);
EAPI void e_util_desktop_menu_item_icon_add(Efreet_Desktop *desktop, unsigned int size, E_Menu_Item *mi);
EAPI int e_util_container_desk_count_get(E_Container *con);
EAPI int e_util_strcasecmp(const char *s1, const char *s2);
EAPI Efreet_Desktop *e_util_terminal_desktop_get(void);

# endif
#endif
