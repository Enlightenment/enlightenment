#ifdef E_TYPEDEFS

typedef enum 
{
   E_BG_TRANSITION_NONE,
   E_BG_TRANSITION_START,
   E_BG_TRANSITION_DESK,
   E_BG_TRANSITION_CHANGE
} E_Bg_Transition;

#else
# ifndef E_BG_H
#  define E_BG_H

extern EAPI int E_EVENT_BG_UPDATE;

EINTERN int e_bg_init(void);
EINTERN int e_bg_shutdown(void);

EAPI void e_bg_zone_update(E_Zone *zone, E_Bg_Transition transition);
EAPI const char *e_bg_file_get(int con_num, int zone_num, int desk_x, int desk_y);
EAPI const E_Config_Desktop_Background *e_bg_config_get(int con_num, int zone_num, int desk_x, int desk_y);
EAPI void e_bg_default_set(const char *file);
EAPI void e_bg_add(int con, int zone, int dx, int dy, const char *file);
EAPI void e_bg_del(int con, int zone, int dx, int dy);
EAPI void e_bg_update(void);

# endif
#endif
