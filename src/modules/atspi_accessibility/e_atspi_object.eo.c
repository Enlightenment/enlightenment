
void _e_atspi_object_object_set(Eo *obj, E_Atspi_Object_Data *pd, Evas_Object *object);

EOAPI EO_VOID_FUNC_BODYV(e_atspi_object_set, EO_FUNC_CALL(object), Evas_Object *object);

Evas_Object * _e_atspi_object_object_get(Eo *obj, E_Atspi_Object_Data *pd);

EOAPI EO_FUNC_BODY(e_atspi_object_get, Evas_Object *, 0);

Eo_Base * _e_atspi_object_eo_base_constructor(Eo *obj, E_Atspi_Object_Data *pd);


static const Eo_Op_Description _e_atspi_object_op_desc[] = {
     EO_OP_FUNC_OVERRIDE(eo_constructor, _e_atspi_object_eo_base_constructor),
     EO_OP_FUNC(e_atspi_object_set, _e_atspi_object_object_set),
     EO_OP_FUNC(e_atspi_object_get, _e_atspi_object_object_get),
};

static const Eo_Class_Description _e_atspi_object_class_desc = {
     EO_VERSION,
     "E_Atspi_Object",
     EO_CLASS_TYPE_REGULAR,
     EO_CLASS_DESCRIPTION_OPS(_e_atspi_object_op_desc),
     NULL,
     sizeof(E_Atspi_Object_Data),
     NULL,
     NULL
};

EO_DEFINE_CLASS(e_atspi_object_class_get, &_e_atspi_object_class_desc, EO_BASE_CLASS, ELM_INTERFACE_ATSPI_ACCESSIBLE_MIXIN, NULL);
EAPI void
e_atspi_object_object_set(E_Atspi_Object *obj, Evas_Object *object)
{
   eo_do((E_Atspi_Object *)obj, e_atspi_object_set(object));
}

EAPI Evas_Object *
e_atspi_object_object_get(const E_Atspi_Object *obj)
{
   Evas_Object * ret;
   eo_do((E_Atspi_Object *)obj, ret = e_atspi_object_get());
   return ret;
}
