#ifndef LUNCHER_CONFIG_DESCRIPTOR_H
#define LUNCHER_CONFIG_DESCRIPTOR_H

#undef _Config
#undef Config
#undef _Config_Item
#undef Config_Item
#undef config_descriptor_init
#undef config_descriptor_shutdown
#undef config_descriptor_get

#define _Config _Luncher_Config
#define Config Luncher_Config
#define _Config_Item _Luncher_Config_Item
#define Config_Item Luncher_Config_Item
#define config_descriptor_init luncher_config_descriptor_init
#define config_descriptor_shutdown luncher_config_descriptor_shutdown
#define config_descriptor_get luncher_config_descriptor_get

typedef struct _Config Config;
typedef struct _Config_Item Config_Item;
struct _Config
{
   Eina_List *items;

   E_Module    *module;
   Evas_Object *config_dialog;
   Evas_Object *slist;
   Evas_Object *list;
   Eina_Bool    bar;
};

struct _Config_Item
{
   int               id;
   Eina_Stringshare *style;
   Eina_Stringshare *dir;
};

void config_descriptor_init(void);
void config_descriptor_shutdown(void);
E_Config_DD *config_descriptor_get(void);

#endif /* LUNCHER_CONFIG_DESCRIPTOR_H */

