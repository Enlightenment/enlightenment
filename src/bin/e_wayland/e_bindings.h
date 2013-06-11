#ifdef E_TYPEDEFS

typedef enum _E_Binding_Context
{
   E_BINDING_CONTEXT_NONE,
   E_BINDING_CONTEXT_UNKNOWN,
   E_BINDING_CONTEXT_BORDER,
   E_BINDING_CONTEXT_ZONE,
   E_BINDING_CONTEXT_CONTAINER,
   E_BINDING_CONTEXT_MANAGER,
   E_BINDING_CONTEXT_MENU,
   E_BINDING_CONTEXT_WINLIST,
   E_BINDING_CONTEXT_POPUP,
   E_BINDING_CONTEXT_ANY
} E_Binding_Context;

#else
# ifndef E_BINDINGS_H
#  define E_BINDINGS_H

EAPI E_Action *e_bindings_acpi_event_handle(E_Binding_Context ctxt __UNUSED__, E_Object *obj __UNUSED__, E_Event_Acpi *ev __UNUSED__);
EAPI E_Action *e_bindings_key_down_event_find(E_Binding_Context ctxt __UNUSED__, Ecore_Event_Key *ev);

# endif
#endif
