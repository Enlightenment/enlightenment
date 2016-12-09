#ifndef CLOCK_CONFIG_DESCRIPTOR_H
#define CLOCK_CONFIG_DESCRIPTOR_H

#undef _Config
#undef Config
#undef _Config_Item
#undef Config_Item
#undef config_descriptor_init
#undef config_descriptor_shutdown
#undef config_descriptor_get

#define _Config _Clock_Config
#define Config Clock_Config
#define _Config_Item _Clock_Config_Item
#define Config_Item Clock_Config_Item
#define config_descriptor_init clock_config_descriptor_init
#define config_descriptor_shutdown clock_config_descriptor_shutdown
#define config_descriptor_get clock_config_descriptor_get

typedef struct _Config Config;
typedef struct _Config_Item Config_Item;

struct _Config
{
  Eina_List *items;

  E_Module *module;
  E_Config_Dialog *config_dialog;
};

struct _Config_Item
{
  const char *id;
  struct {
      int start, len; // 0->6 0 == sun, 6 == sat, number of days
   } weekend;
   struct {
      int start; // 0->6 0 == sun, 6 == sat
   } week;
   int digital_clock;
   int digital_24h;
   int show_seconds;
   int show_date;
   Eina_Bool changed;
};

void config_descriptor_init(void);
void config_descriptor_shutdown(void);
E_Config_DD *config_descriptor_get(void);

#endif /* CLOCK_CONFIG_DESCRIPTOR_H */
