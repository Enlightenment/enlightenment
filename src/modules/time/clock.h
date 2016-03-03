#ifndef CLOCK_H
#define CLOCK_H

#include "e.h"

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

typedef struct _Config Config;
typedef struct _Config_Item Config_Item;
typedef struct _Instance Instance;

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


struct _Instance
{
   Evas_Object     *o_clock, *o_table, *o_cal;
   Evas_Object  *popup;

   int              madj;

   char             year[8];
   char             month[64];
   const char      *daynames[7];
   unsigned char    daynums[7][6];
   Eina_Bool        dayweekends[7][6];
   Eina_Bool        dayvalids[7][6];
   Eina_Bool        daytoday[7][6];
   Config_Item     *cfg;
};

EINTERN Evas_Object *config_clock(Config_Item *);
EINTERN void config_timezone_populate(Evas_Object *obj, const char *name);
void clock_instances_redo(void);

EINTERN void time_daynames_clear(Instance *inst);
EINTERN void time_datestring_format(Instance *inst, char *buf, int bufsz);
EINTERN int time_string_format(Instance *inst, char *buf, int bufsz);
EINTERN void time_instance_update(Instance *inst);
EINTERN void time_init(void);
EINTERN void time_shutdown(void);
EINTERN void time_zoneinfo_scan(Evas_Object *obj);

EINTERN Evas_Object *digital_clock_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object *analog_clock_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN void digital_clock_wizard(E_Gadget_Wizard_End_Cb cb, void *data);
EINTERN void analog_clock_wizard(E_Gadget_Wizard_End_Cb cb, void *data);
EINTERN void clock_popup_new(Instance *inst);
EINTERN void time_config_update(Config_Item *ci);

extern Config *time_config;
extern Eina_List *clock_instances;

#endif
