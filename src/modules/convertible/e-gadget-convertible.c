//
// Created by raffaele on 04/05/19.
//
#include "convertible_logging.h"
#include "e-gadget-convertible.h"
#include "e_mod_main.h"

static void
_update_instances(const Instance *current_instance)
{
  Eina_List *l;
  Instance *instance;

  EINA_LIST_FOREACH(instances, l, instance)
    {
      if (current_instance != instance)
        {
          instance->locked_position = current_instance->locked_position;
          if (instance->locked_position == EINA_TRUE)
            edje_object_signal_emit(instance->o_button, "e,lock,rotation,icon", "convertible/tablet");
          else
            edje_object_signal_emit(instance->o_button, "e,unlock,rotation,icon", "convertible/tablet");

          instance->disabled_keyboard = current_instance->disabled_keyboard;
          if (instance->disabled_keyboard == EINA_TRUE)
            edje_object_signal_emit(instance->o_button, "e,disable,keyboard,icon", "convertible/input");
          else
            edje_object_signal_emit(instance->o_button, "e,enable,keyboard,icon", "convertible/input");
        }
    }
}

void
_rotation_signal_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                    const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
  Instance *inst = data;
  if (eina_str_has_prefix(sig, "e,unlock"))
    inst->locked_position = EINA_FALSE;
  if (eina_str_has_prefix(sig, "e,lock"))
    inst->locked_position = EINA_TRUE;
  _update_instances(inst);
}

void
_keyboard_signal_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                    const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
  Instance *inst = data;
  if (eina_str_has_prefix(sig, "e,enable,keyboard"))
    inst->disabled_keyboard = EINA_FALSE;
  if (eina_str_has_prefix(sig, "e,disable,keyboard"))
    inst->disabled_keyboard = EINA_TRUE;
  _update_instances(inst);
}
