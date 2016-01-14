#ifndef _E_ATSPI_OBJECT_EO_H_
#define _E_ATSPI_OBJECT_EO_H_

#ifndef _E_ATSPI_OBJECT_EO_CLASS_TYPE
#define _E_ATSPI_OBJECT_EO_CLASS_TYPE

typedef Eo E_Atspi_Object;

#endif

#ifndef _E_ATSPI_OBJECT_EO_TYPES
#define _E_ATSPI_OBJECT_EO_TYPES


#endif
#define E_ATSPI_OBJECT_CLASS e_atspi_object_class_get()

EAPI const Eo_Class *e_atspi_object_class_get(void) EINA_CONST;

EOAPI void e_atspi_object_set(Evas_Object *object);

EOAPI Evas_Object *e_atspi_object_get(void);


#endif
