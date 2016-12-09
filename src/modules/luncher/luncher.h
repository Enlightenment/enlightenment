#ifndef LUNCHER_H
#define LUNCHER_H

#include "e.h"
#include "config_descriptor.h"

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init     (E_Module *m);
E_API int   e_modapi_shutdown (E_Module *m);
E_API int   e_modapi_save     (E_Module *m);

typedef struct _Instance Instance;
typedef struct _Icon Icon;

struct _Instance
{
   Evas_Object         *o_main;
   Evas_Object         *o_icon_con;
   Evas_Object         *drop_handler;
   Evas_Object         *place_holder;
   E_Order             *order;
   Eina_List           *icons;
   Eina_Hash           *icons_desktop_hash;
   Eina_Hash           *icons_clients_hash;
   Evas_Coord           size;
   Ecore_Job           *resize_job;
   Ecore_Job           *recalc_job;
   E_Comp_Object_Mover *iconify_provider;
   Eina_Bool            bar;
   Eina_Bool            inside;
   Eina_Bool            effect;
   Config_Item         *cfg;
   Icon                *drop_before;
};

struct _Icon
{
   Instance         *inst;
   Evas_Object      *o_layout;
   Evas_Object      *o_icon;
   Evas_Object      *o_overlay;
   Evas_Object      *preview;
   Evas_Object      *preview_box;
   E_Exec_Instance  *exec;
   Efreet_Desktop   *desktop;
   Eina_List        *execs;
   Eina_List        *clients;
   Ecore_Timer      *mouse_in_timer;
   Ecore_Timer      *mouse_out_timer;
   Ecore_Timer      *drag_timer;
   Eina_Stringshare *icon;
   Eina_Stringshare *key;
   Eina_Bool         in_order;
   Eina_Bool         active;
   Eina_Bool         starting;
   struct
   {
      unsigned char start : 1;
      unsigned char dnd : 1;
      int           x, y;
   } drag;
};

EINTERN Evas_Object *config_luncher(E_Zone *zone, Instance *inst, Eina_Bool bar);
EINTERN Evas_Object *bar_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN void         bar_reorder(Instance *inst);
EINTERN void         bar_recalculate(Instance *inst);
EINTERN Evas_Object *grid_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN void         grid_reorder(Instance *inst);
EINTERN void         grid_recalculate(Instance *inst);

extern Config *luncher_config;
extern Eina_List *luncher_instances;
extern E_Module *module;

#endif
