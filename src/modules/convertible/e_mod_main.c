//
// Created by raffaele on 04/05/19.
//
#include <e.h>
#include <e_module.h>
#include <e_gadcon.h>
#include "convertible_logging.h"
#include "accelerometer-orientation.h"
#include "e-gadget-convertible.h"
#include "e_mod_main.h"
#include "dbus_acceleration.h"
#include "e_mod_config.h"


// The main module reference
E_Module *convertible_module;
Instance *inst;

// Configuration
extern Convertible_Config *convertible_config;
static E_Config_DD *config_edd;

// Logger
int _convertible_log_dom;

/* module setup */
E_API E_Module_Api e_modapi =
 {
        E_MODULE_API_VERSION,
        "convertible"
 };


/* LIST OF INSTANCES */
static Eina_List *instances = NULL;


/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);
/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
        GADCON_CLIENT_CLASS_VERSION,
        "convertible",
        {
                _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
                e_gadcon_site_is_not_toolbar
        },
        E_GADCON_CLIENT_STYLE_PLAIN
};


static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Evas_Object *evas_object;
   E_Gadcon_Client *gcc;
   Instance *instance;

   evas_object = edje_object_add(gc->evas);
   e_theme_edje_object_set(evas_object, "base/theme/modules/convertible",
                           "e/modules/convertible/main");

   instance = E_NEW(Instance, 1);
   instance->accelerometer = inst->accelerometer;
   instance->disabled_keyboard = inst->disabled_keyboard;
   instance->locked_position = inst->locked_position;
   instance->randr2_ids = inst->randr2_ids;
   instance->o_button = evas_object;

   instances = eina_list_append(instances, instance);

   gcc = e_gadcon_client_new(gc, name, id, style, evas_object);
   gcc->data = instance;

    evas_object_size_hint_aspect_set(evas_object, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
    //    evas_object_smart_callback_add(parent, "gadget_site_anchor", _anchor_change, inst);

    // Adding callback for EDJE object
    INF("Adding callback for creation and other events from EDJE");
    edje_object_signal_callback_add(evas_object, "e,lock,rotation", "tablet", _rotation_signal_cb, instance);
    edje_object_signal_callback_add(evas_object, "e,unlock,rotation", "tablet", _rotation_signal_cb, instance);
    edje_object_signal_callback_add(evas_object, "e,enable,keyboard", "keyboard", _keyboard_signal_cb, instance);
    edje_object_signal_callback_add(evas_object, "e,disable,keyboard", "keyboard", _keyboard_signal_cb, instance);
    
    inst->o_button = evas_object;

    return gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   DBG("CONVERTIBLE gadcon shutdown");
   Instance *instance;

   if (!(instance = gcc->data)) return;
   instances = eina_list_remove(instances, instance);
   instance->accelerometer = NULL;

   // Remove callbacks
   DBG("Removing EDJE callbacks");
   edje_object_signal_callback_del(instance->o_button, "lock,rotation", "tablet", _rotation_signal_cb);
   edje_object_signal_callback_del(instance->o_button, "unlock,rotation", "tablet", _rotation_signal_cb);
   edje_object_signal_callback_del(instance->o_button, "enable,keyboard", "keyboard", _keyboard_signal_cb);
   edje_object_signal_callback_del(instance->o_button, "disable,keyboard", "keyboard", _keyboard_signal_cb);

   evas_object_del(instance->o_button);

   E_FREE(instance);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   Evas_Coord mw, mh;
   char buf[PATH_MAX];
   const char *s = "float";

   Instance *current_instance = gcc->data;
   switch (orient)
   {
      case E_GADCON_ORIENT_FLOAT:
         s = "float";
           break;

      case E_GADCON_ORIENT_HORIZ:
         s = "horizontal";
           break;

      case E_GADCON_ORIENT_VERT:
         s = "vertical";
           break;

      case E_GADCON_ORIENT_LEFT:
         s = "left";
           break;

      case E_GADCON_ORIENT_RIGHT:
         s = "right";
           break;

      case E_GADCON_ORIENT_TOP:
         s = "top";
           break;

      case E_GADCON_ORIENT_BOTTOM:
         s = "bottom";
           break;

      case E_GADCON_ORIENT_CORNER_TL:
         s = "top_left";
           break;

      case E_GADCON_ORIENT_CORNER_TR:
         s = "top_right";
           break;

      case E_GADCON_ORIENT_CORNER_BL:
         s = "bottom_left";
           break;

      case E_GADCON_ORIENT_CORNER_BR:
         s = "bottom_right";
           break;

      case E_GADCON_ORIENT_CORNER_LT:
         s = "left_top";
           break;

      case E_GADCON_ORIENT_CORNER_RT:
         s = "right_top";
           break;

      case E_GADCON_ORIENT_CORNER_LB:
         s = "left_bottom";
           break;

      case E_GADCON_ORIENT_CORNER_RB:
         s = "right_bottom";
           break;

      default:
         break;
   }
   snprintf(buf, sizeof(buf), "e,state,orientation,%s", s);
   edje_object_signal_emit(current_instance->o_button, buf, "e");
   edje_object_message_signal_process(current_instance->o_button);

   mw = 0, mh = 0;
   edje_object_size_min_get(current_instance->o_button, &mw, &mh);
   if ((mw < 1) || (mh < 1))
      edje_object_size_min_calc(current_instance->o_button, &mw, &mh);
   if (mw < 4) mw = 4;
   if (mh < 4) mh = 4;
   e_gadcon_client_aspect_set(gcc, mw, mh);
   e_gadcon_client_min_size_set(gcc, mw, mh);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return "Convertible";
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   char buf[PATH_MAX];

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-convertible.edj", convertible_module->dir);
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   static char buf[PATH_MAX];

   snprintf(buf, sizeof(buf), "%s.%d", client_class->name,
             eina_list_count(instances) + 1);
   return buf;
}


/**
 * Prepare to fetch the new value for the DBUS property that has changed
 * */
static void
_cb_properties_changed(void *data, const Eldbus_Message *msg)
{
   Instance *current_instance = data;
   Eldbus_Message_Iter *array, *invalidate;
   char *iface;

   if (!eldbus_message_arguments_get(msg, "sa{sv}as", &iface, &array, &invalidate))
      ERR("Error getting data from properties changed signal.");
   // Given that the property changed, let's get the new value
   Eldbus_Pending *pending_operation = eldbus_proxy_property_get(current_instance->accelerometer->sensor_proxy,
                                                                 "AccelerometerOrientation",
                                                                 on_accelerometer_orientation, current_instance);
   if (!pending_operation)
      ERR("Error: could not get property AccelerometerOrientation");
}

E_API void *
e_modapi_init(E_Module *m)
{
   // Initialise the logger
   _convertible_log_dom = eina_log_domain_register("convertible", EINA_COLOR_LIGHTBLUE);
   convertible_module = m;

   char theme_overlay_path[PATH_MAX];
   snprintf(theme_overlay_path, sizeof(theme_overlay_path), "%s/e-module-convertible.edj", convertible_module->dir);
   elm_theme_extension_add(NULL, theme_overlay_path);

   econvertible_config_init();

   // Config DBus
   DbusAccelerometer *accelerometer = sensor_proxy_init();
   inst = E_NEW(Instance, 1);
   inst->accelerometer = accelerometer;
   inst->locked_position = EINA_FALSE;
   inst->disabled_keyboard = EINA_FALSE;

   // Making sure we rotate the screen to the current orientation coming from the sensor
   inst->accelerometer->pending_orientation = eldbus_proxy_property_get(inst->accelerometer->sensor_proxy,
                                                                 "AccelerometerOrientation",
                                                                 on_accelerometer_orientation, inst);
   if (!inst->accelerometer->pending_orientation)
   {
      ERR("Error: could not get property AccelerometerOrientation");
   }

   // Set the callback for property changed event
   accelerometer->dbus_property_changed_sh = eldbus_proxy_signal_handler_add(accelerometer->sensor_proxy_properties,
                                                                    "PropertiesChanged",
                                                                    _cb_properties_changed, inst);
   if (!accelerometer->dbus_property_changed_sh)
      ERR("Error: could not add the signal handler for PropertiesChanged");

   // Screen related part
   E_Zone *zone;

   // Initialise screen part
   DBG("Looking for the main screen");
   Eina_List *l;
   inst->randr2_ids = NULL;
   EINA_LIST_FOREACH(e_comp->zones, l, zone)
      {
      // Get the screen for the zone
      E_Randr2_Screen *screen = e_randr2_screen_id_find(zone->randr2_id);
      DBG("name randr2 id %s", zone->randr2_id);
      DBG("rot_90 %i", screen->info.can_rot_90);

      // Arbitrarily chosen a condition to check that rotation is enabled
      if (screen->info.can_rot_90 == EINA_TRUE)
         {
         char *randr2_id = strdup(zone->randr2_id);
         if (randr2_id == NULL)
            ERR("Can't copy the screen name");
         else
            inst->randr2_ids = eina_list_append(inst->randr2_ids, randr2_id);
         if (eina_error_get())
            ERR("Memory is low. List allocation failed.");
         }
      }

   if (inst->randr2_ids == NULL)
      ERR("Unable to find rotatable screens");

   DBG("%d screen(s) has been found", eina_list_count(inst->randr2_ids));

   e_gadcon_provider_register(&_gadcon_class);

   INF("Creating menu entries for settings");
   e_configure_registry_category_add("extensions", 90, _("Extensions"), NULL,
                                     "preferences-extensions");
   e_configure_registry_item_add("extensions/convertible", 30, _("Convertible"), NULL,
                                 "preferences-desktop-edge-bindings", e_int_config_convertible_module);

   instances = eina_list_append(instances, inst);

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   INF("Freeing configuration");
   econvertible_config_shutdown();

   e_configure_registry_item_del("extensions/convertible");

   // Shutdown Dbus
   sensor_proxy_shutdown();

   // Remove screen info
   char *element;
   EINA_LIST_FREE(inst->randr2_ids, element)
      free(element);

   free(inst);

   INF("Shutting down the module");
   convertible_module = NULL;
   e_gadcon_provider_unregister(&_gadcon_class);

   // Removing logger
   DBG("Removing the logger");
   eina_log_domain_unregister(_convertible_log_dom);
   _convertible_log_dom = -1;

   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   if (convertible_config)
   {
      e_config_domain_save("module.convertible", config_edd, convertible_config);
   }
   return 1;
}
