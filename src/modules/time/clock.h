#ifndef CLOCK_H
#define CLOCK_H

#include "e.h"
#include "config_descriptor.h"

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

typedef struct _Instance Instance;


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

EINTERN Evas_Object *config_clock(Config_Item *, E_Zone*);
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
