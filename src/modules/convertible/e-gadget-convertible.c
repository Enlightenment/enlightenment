//
// Created by raffaele on 04/05/19.
//
#include "convertible_logging.h"
#include "e-gadget-convertible.h"
#include "e_mod_main.h"

/* LIST OF INSTANCES */
static Eina_List *instances = NULL;

void _update_instances(const Instance *current_instance)
{
    Eina_List *l;
    Instance *instance = NULL;
    EINA_LIST_FOREACH(instances, l, instance)
    {
        if (current_instance != instance)
        {
            instance->locked_position = current_instance->locked_position;
            if (instance->locked_position == EINA_TRUE)
                edje_object_signal_emit(instance->o_button, "lock,rotation,icon", "convertible/tablet");
            else
                edje_object_signal_emit(instance->o_button, "unlock,rotation,icon", "convertible/tablet");
        }
    }
}

void _rotation_signal_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED,
                         const char *src EINA_UNUSED)
{
   DBG("Rotation: Signal %s received from %s", sig, src);
   Instance *inst = data;
   if (eina_str_has_prefix(sig, "unlock"))
      inst->locked_position = EINA_FALSE;
   if (eina_str_has_prefix(sig, "lock"))
      inst->locked_position = EINA_TRUE;
   _update_instances(inst);
}

void _keyboard_signal_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED,
                         const char *src EINA_UNUSED)
{
   DBG("Keyboard: Signal %s received from %s", sig, src);
}


/**
 * Callback for gadget creation
 * */
static void
_gadget_created(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   DBG("Inside gadget created");
   //    do_orient(inst, e_gadget_site_orient_get(obj), e_gadget_site_anchor_get(obj));
   evas_object_smart_callback_del_full(obj, "gadget_created", _gadget_created, NULL);
}


void update_instances(Eina_List *new_instances) {
   instances = new_instances;
}
