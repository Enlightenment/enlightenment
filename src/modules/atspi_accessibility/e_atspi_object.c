#define ELM_INTERFACE_ATSPI_ACCESSIBLE_PROTECTED
#define EFL_BETA_API_SUPPORT
#define EFL_EO_API_SUPPORT

#include <Elementary.h>
#include "e_atspi_object.eo.h"

struct _E_Atspi_Object_Data
{
   Evas_Object *evas_obj;
};

typedef struct _E_Atspi_Object_Data E_Atspi_Object_Data;

EOLIAN static Eo*
_e_atspi_object_eo_base_constructor(Eo *obj, E_Atspi_Object_Data *_pd EINA_UNUSED)
{
   Eo *parent;
   eo_do_super(obj, E_ATSPI_OBJECT_CLASS, eo_constructor());

   eo_do(obj, parent = eo_parent_get());

   if (!parent || !eo_isa(parent, EVAS_OBJECT_CLASS))
     {
        //CRI("Wrong parent passed to %s constructor, class %s do not implement ELM_INTERFACE_ATSPI_ACCESSIBLE_MIXIN", eo_class_name_get(obj), eo_class_name_get(parent));
        return NULL;
     }

   return obj;
}

static void
_focus_in(void *data, Evas_Object *obj EINA_UNUSED, Evas *evas EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Elm_Interface_Atspi_Accessible *ao = data;
   elm_interface_atspi_accessible_state_changed_signal_emit(ao, ELM_ATSPI_STATE_FOCUSED, EINA_TRUE);
}

static void
_focus_out(void *data, Evas_Object *obj EINA_UNUSED, Evas *evas EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Elm_Interface_Atspi_Accessible *ao = data;
   elm_interface_atspi_accessible_state_changed_signal_emit(ao, ELM_ATSPI_STATE_FOCUSED, EINA_FALSE);
}

static void
_obj_del(void *data, Evas_Object *obj EINA_UNUSED, Evas *evas EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Elm_Interface_Atspi_Accessible *ao = data;
   eo_del(ao);
}

EOLIAN static void
_e_atspi_object_object_set(Eo *obj, E_Atspi_Object_Data *_pd EINA_UNUSED, Evas_Object *evas_obj)
{
   Eina_Bool finalized;
   if (eo_do_ret(obj, finalized, eo_finalized_get()))
     {
        //CRI("This function is only allowed during construction.");
        return;
     }
   if (!evas_obj || !eo_isa(evas_obj, EVAS_OBJECT_CLASS))
     {
        //CRI("Wrong parameter: %s is not EVAS_OBJECT_CLASS", eo_class_name_get(evas_obj));
        return;
     }
   _pd->evas_obj = evas_obj;

   evas_object_event_callback_add(_pd->evas_obj, EVAS_CALLBACK_FOCUS_IN, _focus_in, obj);
   evas_object_event_callback_add(_pd->evas_obj, EVAS_CALLBACK_FOCUS_OUT, _focus_out, obj);
   evas_object_event_callback_add(_pd->evas_obj, EVAS_CALLBACK_DEL, _obj_del, obj);
}

EOLIAN static Evas_Object*
_e_atspi_object_object_get(Eo *obj EINA_UNUSED, E_Atspi_Object_Data *_pd)
{
   return _pd->evas_obj;
}

#include "e_atspi_object.eo.c"
