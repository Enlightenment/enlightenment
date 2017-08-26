#ifndef PAGER_H
#define PAGER_H

#include "e.h"

EINTERN void *e_modapi_gadget_init(E_Module *m);
EINTERN int   e_modapi_gadget_shutdown(E_Module *m);
EINTERN int   e_modapi_gadget_save(E_Module *m);

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
   unsigned int plain;
   unsigned int permanent_plain;
};

EINTERN Evas_Object    *pager_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN Evas_Object    *config_pager(E_Zone *zone);
EINTERN void           pager_init(void);
EINTERN void           _pager_cb_config_gadget_updated(Eina_Bool style_changed);
EINTERN void           _pager_cb_config_updated(void);

EINTERN extern Config          *pager_config;
EINTERN extern Evas_Object     *cfg_dialog;
EINTERN extern Eina_List       *ginstances, *ghandlers, *phandlers;
/**
 * @addtogroup Optional_Gadgets
 * @{
 *
 * @defgroup Module_Pager Virtual Desktop Pager
 *
 * Shows the grid of virtual desktops and allows changing between
 * them.
 *
 * @}
 */
#endif
