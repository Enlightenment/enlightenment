#ifdef E_TYPEDEFS
#else
# ifndef E_THEME_H
#  define E_THEME_H

EINTERN int e_theme_init(void);
EINTERN int e_theme_shutdown(void);

EAPI Eina_Bool e_theme_edje_object_set(Evas_Object *obj, const char *category, const char *group);
EAPI void e_theme_file_set(const char *category, const char *file);
EAPI const char *e_theme_edje_file_get(const char *category, const char *group);
EAPI const char *e_theme_edje_icon_fallback_file_get(const char *group);

# endif
#endif
