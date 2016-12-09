#ifndef IBAR_CONFIG_DESCRIPTOR_H
#define IBAR_CONFIG_DESCRIPTOR_H


#undef _Config
#undef Config
#undef _Config_Item
#undef Config_Item
#undef config_descriptor_init
#undef config_descriptor_shutdown
#undef config_descriptor_get

#define _Config _Ibar_Config
#define Config Ibar_Config
#define _Config_Item _Ibar_Config_Item
#define Config_Item Ibar_Config_Item
#define config_descriptor_init ibar_config_descriptor_init
#define config_descriptor_shutdown ibar_config_descriptor_shutdown
#define config_descriptor_get ibar_config_descriptor_get

typedef struct _Config      Config;
typedef struct _Config_Item Config_Item;

struct _Config
{
   /* saved * loaded config values */
   Eina_List       *items;
   /* just config state */
   E_Module        *module;
   E_Config_Dialog *config_dialog;
   Eina_List       *instances;
   Eina_List       *handlers;
};

struct _Config_Item
{
   const char *id;
   const char *dir;
   int show_label;
   int eap_label;
   int lock_move;
   int dont_add_nonorder;
   unsigned char dont_track_launch;
   unsigned char dont_icon_menu_mouseover;
};

void config_descriptor_init(void);
void config_descriptor_shutdown(void);
E_Config_DD *config_descriptor_get(void);


#endif /* IBAR_CONFIG_DESCRIPTOR_H */
