#ifndef PAGER_CONFIG_DESCRIPTOR_H
#define PAGER_CONFIG_DESCRIPTOR_H


#undef _Config
#undef Config
#undef config_descriptor_init
#undef config_descriptor_shutdown
#undef config_descriptor_get

#define _Config _Pager_Config
#define Config Pager_Config
#define config_descriptor_init pager_config_descriptor_init
#define config_descriptor_shutdown pager_config_descriptor_shutdown
#define config_descriptor_get pager_config_descriptor_get

typedef struct _Config Config;
struct _Config
{
   unsigned int popup;
   double       popup_speed;
   unsigned int popup_urgent;
   unsigned int popup_urgent_stick;
   unsigned int popup_urgent_focus;
   double       popup_urgent_speed;
   unsigned int show_desk_names;
   int          popup_act_height;
   int          popup_height;
   unsigned int drag_resist;
   unsigned int btn_drag;
   unsigned int btn_noplace;
   unsigned int btn_desk;
   unsigned int flip_desk;
};

void config_descriptor_init(void);
void config_descriptor_shutdown(void);
E_Config_DD *config_descriptor_get(void);


#endif /* PAGER_CONFIG_DESCRIPTOR_H */
