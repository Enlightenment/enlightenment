#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "e.h"

EINTERN void *e_modapi_gadget_init(E_Module *m EINA_UNUSED);
EINTERN int e_modapi_gadget_shutdown(E_Module *m EINA_UNUSED);
EINTERN int e_modapi_gadget_save(E_Module *m EINA_UNUSED);

EINTERN Evas_Object *backlight_gadget_create(Evas_Object *parent, int *id EINA_UNUSED, E_Gadget_Site_Orient orient);
EINTERN void backlight_init(void);
EINTERN void backlight_shutdown(void);

extern E_Module *gm;

#endif
