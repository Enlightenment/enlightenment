#ifndef TIME_CONFIG_DESCRIPTOR_H
#define TIME_CONFIG_DESCRIPTOR_H

#undef _Config
#undef Config
#undef _Config_Item
#undef Config_Item
#undef config_descriptor_init
#undef config_descriptor_shutdown
#undef config_descriptor_get


#define _Config _Time_Config
#define Config Time_Config
#define _Config_Item _Time_Config_Item
#define Config_Item Time_Config_Item
#define config_descriptor_init time_config_descriptor_init
#define config_descriptor_shutdown time_config_descriptor_shutdown
#define config_descriptor_get time_config_descriptor_get

typedef struct _Config Config;
typedef struct _Config_Item Config_Item;

typedef enum
{
   CLOCK_DATE_DISPLAY_NONE,
   CLOCK_DATE_DISPLAY_FULL,
   CLOCK_DATE_DISPLAY_NUMERIC,
   CLOCK_DATE_DISPLAY_DATE_ONLY,
   CLOCK_DATE_DISPLAY_ISO8601,
   CLOCK_DATE_DISPLAY_CUSTOM,
} Clock_Date_Display;

struct _Config
{
  Eina_List *items;

  E_Module *module;
  Evas_Object *config_dialog;
};

struct _Config_Item
{
  int id;
  struct {
      int start, len; // 0->6 0 == sun, 6 == sat, number of days
   } weekend;
   struct {
      int start; // 0->6 0 == sun, 6 == sat
   } week;
   Eina_Bool  digital_clock;
   Eina_Bool  digital_24h;
   Eina_Bool  show_seconds;
   Clock_Date_Display show_date;
   Eina_Bool  advanced;
   Eina_Stringshare *timezone;
   Eina_Stringshare *time_str[2];
   Eina_Stringshare *colorclass[2];
};

void config_descriptor_init(void);
void config_descriptor_shutdown(void);
E_Config_DD *config_descriptor_get(void);

#endif /* TIME_CONFIG_DESCRIPTOR_H */
