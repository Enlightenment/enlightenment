#ifndef XKBSWITCH_H
#define XKBSWITCH_H

#include "e.h"

typedef struct _Xkbg
{
   E_Module            *module;
   E_Config_Dialog     *cfd;
   Ecore_Event_Handler *evh;
} Xkbg;

void             _xkbg_update_icon(int);
E_Config_Dialog *_xkbg_cfg_dialog(Evas_Object *, const char *params);

extern Xkbg _xkbg;

EINTERN void *e_modapi_gadget_init(E_Module *m);
EINTERN int e_modapi_gadget_shutdown(E_Module *m EINA_UNUSED);
EINTERN int e_modapi_gadget_save(E_Module *m EINA_UNUSED);
EINTERN Evas_Object *xkbg_gadget_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
EINTERN void xkbg_init(void);
EINTERN void xkbg_shutdown(void);

#endif
