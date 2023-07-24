#include <Ecore.h>
#include <Elementary.h>
#include <e.h>

#ifndef EFL_DBUS_ACCELERATION
#define EFL_DBUS_ACCELERATION

#define EFL_DBUS_ACC_BUS "net.hadess.SensorProxy"
#define EFL_DBUS_ACC_PATH "/net/hadess/SensorProxy"
#define EFL_DBUS_ACC_IFACE "net.hadess.SensorProxy"

// This enum represents the 4 states of screen rotation plus undefined
enum screen_rotation {undefined, normal, right_up, flipped, left_up};

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
DbusAccelerometer* sensor_proxy_init();


void sensor_proxy_shutdown();

/**
 * Helper to get the interface
 * */
Eldbus_Proxy *get_dbus_interface(const char *IFACE);

/**
 * Helper function to extract ta string property from the message
 * @param msg The message coming from the get property invocation
 * @param variant
 * @param result 1 if result is ok, 0 if it failed
 * @return Enum specifying the orientation
 */
enum screen_rotation
access_string_property(const Eldbus_Message *msg, Eldbus_Message_Iter **variant, Eina_Bool* result);

/**
 * Helper function to extract ta boolean property from the message
 * @param msg The message coming from the get property invocation
 * @param variant
 * @param boolean_property_value The boolean property pointer where the value should be stored, if read
 * @return
 */
Eina_Bool
access_bool_property(const Eldbus_Message *msg, Eldbus_Message_Iter **variant, Eina_Bool *boolean_property_value);

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


/**
 * Fetch a screen from its ID and rotate it according to the rotation parameter
 * @param randr_id The randr2 id
 * @param rotation The expected rotation
 */
void _fetch_and_rotate_screen(const char* randr_id, enum screen_rotation orientation);
#endif
