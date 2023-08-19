//
// Created by raffaele on 01/05/19.
//
//#include <Ecore_X.h>
#include "convertible_logging.h"
#include "accelerometer-orientation.h"
#include "dbus_acceleration.h"
#include "e_mod_main.h"
#include "input_rotation.h"

static DbusAccelerometer* accelerometer_dbus;


/**
 * Helper function to extract ta boolean property from the message
 * @param msg The message coming from the get property invocation
 * @param variant
 * @param boolean_property_value The boolean property pointer where the value should be stored, if read
 * @return
 */
static Eina_Bool
_access_bool_property(const Eldbus_Message *msg, Eldbus_Message_Iter **variant, Eina_Bool *boolean_property_value)
{
    char *type;
    Eina_Bool res = EINA_TRUE;

    if (!eldbus_message_arguments_get(msg, "v", variant))
    {
        WARN("Error getting arguments.");
        res = EINA_FALSE;
    }
    type = eldbus_message_iter_signature_get((*variant));
    if (type == NULL)
    {
        WARN("Unable to get the type.");
        res = EINA_FALSE;
        return res;
    }

    if (type[1])
    {
        WARN("It is a complex type, not handle yet.");
        res = EINA_FALSE;
    }
    if (type[0] != 'b')
    {
        WARN("Expected type is int.");
        res = EINA_FALSE;
    }
    if (!eldbus_message_iter_arguments_get((*variant), "b", boolean_property_value))
    {
        WARN("error in eldbus_message_iter_arguments_get()");
        res = EINA_FALSE;
    }
    free(type);
    return res;
}

/**
 * Callback definition to handle the execution of the ReleaseAccelerometer() method of DBUS
 * interface net.hadess.SensorProxy
 * @param data not used
 * @param msg The message
 * @param pending
 */
static void
_on_accelerometer_released(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
    const char *errname, *errmsg;

    INF("Going to release the accelerometer_dbus");
    if (eldbus_message_error_get(msg, &errname, &errmsg))
    {
        ERR("Error: %s %s", errname, errmsg);
        return;
    }
    INF("Accelerometer released");
}


/**
 * Callback definition to handle the execution of the ClaimAccelerometer() method of DBUS
 * interface net.hadess.SensorProxy
 * @param data not used
 * @param msg The message
 * @param pending
 */
static void
_on_accelerometer_claimed(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
    const char *errname, *errmsg;

    INF("Going to claim the accelerometer_dbus");
    if (eldbus_message_error_get(msg, &errname, &errmsg))
    {
        ERR("Error: %s %s", errname, errmsg);
        return;
    }
    INF("Accelerometer claimed");
}

/**
 * Callback definition to handle the request of the hasAccelerometer property of DBUS interface net.hadess.SensorProxy
 * @param data DbusAccelerometer
 * @param msg The message
 * @param pending
 */
static void
_on_has_accelerometer(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
    const char *errname, *errmsg;
    Eina_Bool has_accelerometer = EINA_FALSE;
    Eldbus_Message_Iter *variant = NULL;

    if (eldbus_message_error_get(msg, &errname, &errmsg))
    {
        ERR("Error: %s %s", errname, errmsg);
    }

    _access_bool_property(msg, &variant, &has_accelerometer);
    DbusAccelerometer *accelerometer = data;
    accelerometer->has_accelerometer = has_accelerometer;
    DBG("Has Accelerometer: %d", accelerometer->has_accelerometer);
}


DbusAccelerometer*
sensor_proxy_init(void)
{
   // Initialise DBUS component
   if (accelerometer_dbus)
   {
      INF("We already have a struct filled");
      return accelerometer_dbus;
   }
   accelerometer_dbus  = calloc(1, sizeof(DbusAccelerometer));
   EINA_SAFETY_ON_NULL_RETURN_VAL(accelerometer_dbus, NULL);

   // The next line is probably redundant
   accelerometer_dbus->orientation = UNDEFINED;

   INF("Getting dbus interfaces");
   accelerometer_dbus->sensor_proxy = get_dbus_interface(EFL_DBUS_ACC_IFACE);
   accelerometer_dbus->sensor_proxy_properties = get_dbus_interface(ELDBUS_FDO_INTERFACE_PROPERTIES);
   if (accelerometer_dbus->sensor_proxy == NULL)
   {
      ERR("Unable to get the proxy for interface %s", EFL_DBUS_ACC_IFACE);
      return accelerometer_dbus;
   }

   accelerometer_dbus->pending_has_orientation = eldbus_proxy_property_get(accelerometer_dbus->sensor_proxy,
                                                                            "HasAccelerometer", _on_has_accelerometer,
                                                                            accelerometer_dbus);
   if (!accelerometer_dbus->pending_has_orientation)
   {
      ERR("Error: could not get property HasAccelerometer");
   }

   // Claim the accelerometer_dbus
   accelerometer_dbus->pending_acc_claim = eldbus_proxy_call(accelerometer_dbus->sensor_proxy, "ClaimAccelerometer",
                                                              _on_accelerometer_claimed, accelerometer_dbus, -1, "");

   if (!accelerometer_dbus->pending_acc_claim)
   {
      ERR("Error: could not call ClaimAccelerometer");
   }

   return accelerometer_dbus;
}

void
sensor_proxy_shutdown(void)
{
   INF("Removing signal handler dbus_property_changed_sh");
   eldbus_signal_handler_del(accelerometer_dbus->dbus_property_changed_sh);

   // TODO Should to this and wait for the release before continuing
   INF("Freeing convertible resources");
   accelerometer_dbus->pending_acc_crelease = eldbus_proxy_call(accelerometer_dbus->sensor_proxy, "ReleaseAccelerometer", _on_accelerometer_released, accelerometer_dbus, -1, "");
   if (accelerometer_dbus)
   {
      e_object_del(E_OBJECT(accelerometer_dbus));
      free(accelerometer_dbus);
      accelerometer_dbus = NULL;
   }

   DBG("Shutting down ELDBUS");
   eldbus_shutdown();
}

int
_convertible_rotation_get(const enum screen_rotation orientation);

int
_is_device_a_touch_pointer(int dev_counter, int num_properties, char **iterator);

/**
 * Helper to get the interface
 * */
static Eldbus_Proxy *
get_dbus_interface(const char *IFACE)
{
   DBG("Working on interface: %s", IFACE);
   Eldbus_Connection *conn;
   Eldbus_Object *obj;
   Eldbus_Proxy *sensor_proxy;

   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   if (!conn)
   {
      ERR("Error: could not get system bus");
      return NULL;
   }
   obj = eldbus_object_get(conn, EFL_DBUS_ACC_BUS, EFL_DBUS_ACC_PATH);
   if (!obj)
   {
      ERR("Error: could not get object");
      return NULL;
   }
   sensor_proxy = eldbus_proxy_get(obj, IFACE);
   if (!sensor_proxy)
   {
      ERR("Error: could not get proxy for interface %s", IFACE);
      return NULL;
   }

   return sensor_proxy;
}

/**
 * Helper function to extract ta string property from the message
 * @param msg The message coming from the get property invocation
 * @param variant
 * @return Enum specifying the orientation. UNDEFINED by default
 */
enum screen_rotation
_access_string_property(const Eldbus_Message *msg, Eldbus_Message_Iter **variant)
{
   enum screen_rotation rotation = UNDEFINED;
   char *type = NULL;

   if (!eldbus_message_arguments_get(msg, "v", variant))
   {
      WARN("Error getting arguments.");
   }
   type = eldbus_message_iter_signature_get((*variant));
   if (type == NULL)
   {
      WARN("Unable to get the type.");
      return rotation;
   }

   type = eldbus_message_iter_signature_get((*variant));
   if (type[1])
   {
      WARN("It is a complex type, not handle yet.");
   }
   if (type[0] != 's')
   {
      WARN("Expected type is string(s).");
   }
   const char **string_property_value = calloc(PATH_MAX, sizeof(char));
   EINA_SAFETY_ON_NULL_RETURN_VAL(string_property_value, EINA_FALSE);
   if (!eldbus_message_iter_arguments_get((*variant), "s", string_property_value))
   {
      WARN("error in eldbus_message_iter_arguments_get()");
   }

   if (!strcmp(ACCELEROMETER_ORIENTATION_RIGHT, *string_property_value))
      rotation = RIGHT_UP;
   if (!strcmp(ACCELEROMETER_ORIENTATION_LEFT, *string_property_value))
      rotation = LEFT_UP;
   if (!strcmp(ACCELEROMETER_ORIENTATION_BOTTOM, *string_property_value))
      rotation = FLIPPED;
   if (!strcmp(ACCELEROMETER_ORIENTATION_NORMAL, *string_property_value))
      rotation = NORMAL;

   free(type);
   free(string_property_value);
   return rotation;
}

void
on_accelerometer_orientation(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   INF("New orientation received");
   Instance *inst = (Instance *) data;

   if (inst->locked_position == EINA_TRUE)
   {
      WARN("Locked position. Ignoring rotation");
      return;
   }


   const char *errname, *errmsg;
   enum screen_rotation orientation;
   Eldbus_Message_Iter *variant = NULL;

   if (eldbus_message_error_get(msg, &errname, &errmsg))
   {
      ERR("Error: %s %s", errname, errmsg);
      return;
   }

   orientation = _access_string_property(msg, &variant);
   if (orientation == UNDEFINED)
   {
      INF("Failed to retrieve the orientation from dbus message");
      return;
   }

   inst->accelerometer->orientation = orientation;
   DBG("Current Orientation: %d", inst->accelerometer->orientation);

   if (inst->randr2_ids == NULL)
      ERR("Screen not set.");
   else
   {
      Eina_List *l;
      char *randr_id = NULL;
      EINA_LIST_FOREACH(inst->randr2_ids, l, randr_id)
      {
         _fetch_and_rotate_screen(randr_id, orientation);
      }
   }
}

int
_convertible_rotation_get(const enum screen_rotation orientation)
{
   switch (orientation)
   {
      case NORMAL: return 0;
      case LEFT_UP: return 90;
      case FLIPPED: return 180;
      case RIGHT_UP: return 270;

      default: return 0;
   }
}

const float *
_get_matrix_rotation_transformation(int rotation)
{
   const float *transformation;
   switch (rotation) {
      case 90:
         transformation = MATRIX_ROTATION_90;
         break;
      case 180:
         transformation = MATRIX_ROTATION_180;
         break;
      case 270:
         transformation = MATRIX_ROTATION_270;
         break;
      default:
         transformation = MATRIX_ROTATION_IDENTITY;
      }
   return transformation;
}

int
_fetch_X_device_input_number(void)
{
   // I should get the touchscreen associated with the screen probably by looking at the classes of the input devices
   // I need to submit my patch to add getters for other XIDeviceInfo fields, like raster mentioned in his commit.
   const char *dev_name;
   char **property_name;
   int dev_num = ecore_x_input_device_num_get();
   int dev_number = -1;

   for (int dev_counter = 0; dev_counter < dev_num; dev_counter++)
   {
      dev_name = ecore_x_input_device_name_get(dev_counter);
      // Less horrible hack that relies on the presence of a property containing the work Calibration
       DBG("Found device with name %s", dev_name);
       int num_properties;
       property_name = ecore_x_input_device_properties_list(dev_counter, &num_properties);
       DBG("Found %d properties", num_properties);
       char **iterator = property_name;
       int is_correct_device = _is_device_a_touch_pointer(dev_counter, num_properties, iterator);
       if (is_correct_device == EINA_FALSE)
          continue;
       iterator = property_name;
       for (int i = 0; i < num_properties; i++)
       {
          if (!strcmp(*iterator, CTM_name))
          {
	     dev_number = dev_counter;
             DBG("Setting device: %d", dev_number);
          }
          iterator++;
       }
   }

   return dev_number;
}

int
_is_device_a_touch_pointer(int dev_counter, int num_properties, char **iterator)
{
   // Looking for a device with either a libinput property for calibration or the old evdev Axlis labels property.
   int is_correct_device = EINA_FALSE;
   for (int i = 0; i < num_properties; i++)
   {
      if (strstr(*iterator, "libinput Calibration Matrix"))
         is_correct_device = EINA_TRUE;
      if (strstr(*iterator, "Axis Labels"))
      {
         int num_ret, unit_size_ret;
         Ecore_X_Atom format_ret;
         char *result = ecore_x_input_device_property_get(dev_counter, *iterator, &num_ret, &format_ret, &unit_size_ret);
         if (result) {
            // TODO Shall check for the value "Abs MT Position"
         }
         DBG("Looks like I found a device with calibration capabilities");
         is_correct_device = EINA_TRUE;
      }
      iterator++;
   }
   return is_correct_device;
}

/**
 * Fetch a screen from its ID and rotate it according to the rotation parameter
 * @param randr_id The randr2 id
 * @param rotation The expected rotation
 */
void
_fetch_and_rotate_screen(const char* randr_id, enum screen_rotation orientation)
{
   DBG("Working on screen %s", randr_id);
   E_Randr2_Screen *rotatable_screen = e_randr2_screen_id_find(randr_id);
   E_Config_Randr2_Screen *screen_randr_cfg = e_randr2_config_screen_find(rotatable_screen, e_randr2_cfg);
   int rotation = _convertible_rotation_get(orientation);
   DBG("Screen %s is going to be rotated to %d", randr_id, rotation);

   if (rotation == screen_randr_cfg->rotation)
   {
      WARN("Screen %s is already rotated to %d degrees", randr_id, rotation);
   } else
   {
      screen_randr_cfg->rotation = rotation;
      e_randr2_config_apply();
      DBG("Screen %s rotated to %d", randr_id, rotation);

      int x_dev_num = _fetch_X_device_input_number();
      if (x_dev_num == -1)
      {
         ERR("Unable to find a pointer device with coordinate transformation capabilities");
         return;
      }
      DBG("Rotating input number %d", x_dev_num);

      int num_ret, unit_size_ret;
      Ecore_X_Atom format_ret;
      char *result = NULL;
      TransformationMatrix *matrix = calloc(1, sizeof(TransformationMatrix));
      EINA_SAFETY_ON_NULL_RETURN_VAL(matrix, NULL);
      result = ecore_x_input_device_property_get(x_dev_num, CTM_name, &num_ret, &format_ret, &unit_size_ret);
      if (result)
      {

         DBG("Device with coordinates transformation matrix");
         // format_ret of 116 -> ECORE_X_ATOM_FLOAT
         // num_ret of 9 -> 9 (float) to read
         // unit_size_ret of 32 -> each float is 32 bits
         memcpy(matrix->values, result, sizeof(matrix->values));
         const float * rotation_matrix_2d = _get_matrix_rotation_transformation(rotation);
         memcpy(matrix->values, rotation_matrix_2d, 6 * sizeof(*rotation_matrix_2d));
         ecore_x_input_device_property_set(x_dev_num, CTM_name, matrix->values, num_ret, format_ret, unit_size_ret);

         DBG("Input device %d rotated to %d", x_dev_num, rotation);
      } else {
         ERR("Unable to fetch coordinates transformation matrix for device %d", x_dev_num);
      }
      free(matrix);
   }
}
