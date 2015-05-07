#ifndef E_MOD_CONFIG_H
#define E_MOD_CONFIG_H

#define IL_CONFIG_MIN 3
#define IL_CONFIG_MAJ 1

typedef struct _Il_Kbd_Config Il_Kbd_Config;

struct _Il_Kbd_Config 
{
   int version;

   int use_internal;
   const char *dict, *run_keyboard;

   // Not User Configurable. Placeholders
   const char *mod_dir;
   int zoom_level;
   int slide_dim;
   double hold_timer;
   double scale_height;
   int layout;

   E_Config_Dialog *cfd;
};

E_API int il_kbd_config_init(E_Module *m);
E_API int il_kbd_config_shutdown(void);
E_API int il_kbd_config_save(void);

E_API void il_kbd_config_show(E_Comp *comp, const char *params);

extern E_API Il_Kbd_Config *il_kbd_cfg;

#endif
