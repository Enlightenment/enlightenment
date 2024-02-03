//
// Created by raffaele on 04/05/19.
//
#include <Ecore.h>
#include <Elementary.h>
#include "e_mod_main.h"

#ifndef E_GADGET_CONVERTIBLE_E_GADGET_CONVERTIBLE_H
#define E_GADGET_CONVERTIBLE_E_GADGET_CONVERTIBLE_H

/* LIST OF INSTANCES */
extern Eina_List *instances;

/* gadcon callback for actions */
void
_rotation_signal_cb(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED);

void
_keyboard_signal_cb(void *data EINA_UNUSED, Evas_Object *obj, const char *sig EINA_UNUSED, const char *src EINA_UNUSED);

/* end gadcon callback for actions */


#endif //E_GADGET_CONVERTIBLE_E_GADGET_CONVERTIBLE_H
