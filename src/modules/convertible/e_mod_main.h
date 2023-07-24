//
// Created by raffaele on 04/05/19.
//
#include <e.h>
#include "dbus_acceleration.h"


#ifndef E_GADGET_CONVERTIBLE_CONVERTIBLE_H
#define E_GADGET_CONVERTIBLE_CONVERTIBLE_H

typedef struct _Instance Instance;

struct _Instance
{
    Evas_Object           *o_button;
    DbusAccelerometer     *accelerometer;
    // I hate to put DBUS related stuff in this struct. Unfortunately, I did not (quickly) find a better way of
    // handling signals across multiple instances sharing one DbusAccelerometer struct.
    Eina_List             *randr2_ids;

    Eina_Bool             locked_position;
    Eina_Bool             disabled_keyboard;
};

#endif //E_GADGET_CONVERTIBLE_CONVERTIBLE_H
