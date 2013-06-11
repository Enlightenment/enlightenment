#include "e.h"

EAPI E_Action *
e_bindings_acpi_event_handle(E_Binding_Context ctxt __UNUSED__, E_Object *obj __UNUSED__, E_Event_Acpi *ev __UNUSED__)
{
   return NULL;
}

EAPI E_Action *
e_bindings_key_down_event_find(E_Binding_Context ctxt __UNUSED__, Ecore_Event_Key *ev)
{
   return NULL;
}
