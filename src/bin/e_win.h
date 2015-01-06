#ifndef E_TYPEDEFS
#ifndef E_WIN_H
#define E_WIN_H

EINTERN int    e_win_init               (void);
EINTERN int    e_win_shutdown           (void);


EAPI E_Client *e_win_client_get(Evas_Object *obj);
EAPI Ecore_Evas *e_win_ee_get(Evas_Object *obj);
EAPI E_Pointer *e_win_pointer_get(Evas_Object *obj);
EAPI Eina_Bool e_win_centered_get(Evas_Object *obj);

EAPI void e_win_client_icon_set(Evas_Object *win, const char *icon);
EAPI void e_win_client_icon_key_set(Evas_Object *win, const char *key);

EAPI void e_win_placed_set(Evas_Object *win, Eina_Bool placed);
EAPI void e_win_no_remember_set(Evas_Object *win, Eina_Bool no_rem);
EAPI void e_win_no_reopen_set(Evas_Object *win, Eina_Bool no_reopen);

static inline Evas_Object *
e_win_evas_object_win_get(Evas_Object *obj)
{
   return ecore_evas_data_get(e_win_ee_get(obj), "elm_win");
}

static inline Evas_Object *
e_win_evas_win_get(Evas *e)
{
   return ecore_evas_data_get(ecore_evas_ecore_evas_get(e), "elm_win");
}

static inline Eina_Bool
e_obj_is_win(const void *obj)
{
   const char *type = evas_object_type_get(obj);
   return type && !strcmp(type, "elm_win");
}

EAPI Evas_Object *e_elm_win_add(Evas_Object *parent, const char *name, Elm_Win_Type type);

#define elm_win_add(X, Y, Z) \
   e_elm_win_add((X), (Y), (Z))

#endif
#endif
