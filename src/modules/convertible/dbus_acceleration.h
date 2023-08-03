#include <Ecore.h>
#include <Elementary.h>
#include <e.h>

#ifndef EFL_DBUS_ACCELERATION
#define EFL_DBUS_ACCELERATION

#define EFL_DBUS_ACC_BUS "net.hadess.SensorProxy"
#define EFL_DBUS_ACC_PATH "/net/hadess/SensorProxy"
#define EFL_DBUS_ACC_IFACE "net.hadess.SensorProxy"

// This enum represents the 4 states of screen rotation plus UNDEFINED
enum screen_rotation {UNDEFINED, NORMAL, RIGHT_UP, FLIPPED, LEFT_UP};

typedef struct _DbusAccelerometer DbusAccelerometer;

struct _DbusAccelerometer
{
   Eina_Bool has_accelerometer;
   enum screen_rotation orientation;
   Eldbus_Proxy *sensor_proxy, *sensor_proxy_properties;
   Eldbus_Pending *pending_has_orientation, *pending_orientation, *pending_acc_claim, *pending_acc_crelease;
   Eldbus_Signal_Handler *dbus_property_changed_sh;
};

/**
 * Fetch the DBUS interfaces and fill the DbusAccelerometer struct
 * */
DbusAccelerometer* sensor_proxy_init(void);


void
sensor_proxy_shutdown(void);

/**
 * Helper to get the interface
 * */
Eldbus_Proxy *
get_dbus_interface(const char *IFACE);

/**
 * Callback definition to handle the request of the hasAccelerometer property of DBUS interface net.hadess.SensorProxy
 * @param data DbusAccelerometer
 * @param msg The message
 * @param pending
 */
void
on_has_accelerometer(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED);

/**
 * Callback definition to handle the request of the accelerometer property of DBUS interface net.hadess.SensorProxy
 * @param data DbusAccelerometer
 * @param msg The message
 * @param pending
 */
void
on_accelerometer_orientation(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED);

/**
 * Callback definition to handle the execution of the ClaimAccelerometer() method of DBUS
 * interface net.hadess.SensorProxy
 * @param data not used
 * @param msg The message
 * @param pending
 */
void
on_accelerometer_claimed(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED);

/**
 * Callback definition to handle the execution of the ReleaseAccelerometer() method of DBUS
 * interface net.hadess.SensorProxy
 * @param data not used
 * @param msg The message
 * @param pending
 */
void
on_accelerometer_released(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED);
#endif
