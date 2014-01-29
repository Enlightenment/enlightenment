#ifdef E_TYPEDEFS

typedef void (*E_Iconify_Provider_Cb) (void *data, E_Client *ec, Eina_Bool iconify, int *x, int *y, int *w, int *h);

#else

#ifndef E_ICONIFY_H
#define E_ICONIFY_H

EAPI void e_iconify_provider_add(int pri, E_Iconify_Provider_Cb provider, const void *data);
EAPI void e_iconify_provider_del(int pri, E_Iconify_Provider_Cb provider, const void *data);
EAPI void e_iconify_provider_obj_message(E_Client *ec, Eina_Bool iconify, Evas_Object *edje_target);

#endif
#endif
