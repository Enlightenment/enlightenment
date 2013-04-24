#ifdef E_TYPEDEFS
#else
# ifndef E_UTILS_H
#  define E_UTILS_H

EAPI void e_util_env_set(const char *var, const char *val);
EAPI int e_util_strcmp(const char *s1, const char *s2);
EAPI int e_util_strcasecmp(const char *s1, const char *s2);

# endif
#endif
