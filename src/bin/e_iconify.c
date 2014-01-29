#include "e.h"

typedef struct _E_Iconify_Provider E_Iconify_Provider;

struct _E_Iconify_Provider
{
   E_Iconify_Provider_Cb func;
   const void *data;
   int pri;
};

static Eina_List *_e_iconify_providers = NULL;

static E_Iconify_Provider *
_e_iconify_provider_get(void)
{
   Eina_List *l;
   E_Iconify_Provider *prov, *prov_found = NULL;
   int pri_found = -9999;

   EINA_LIST_FOREACH(_e_iconify_providers, l, prov)
     {
        if (prov->pri >= pri_found)
          {
             pri_found = prov->pri;
             prov_found = prov;
          }
     }
   return prov_found;
}

EAPI void
e_iconify_provider_add(int pri, E_Iconify_Provider_Cb provider, const void *data)
{
   E_Iconify_Provider *prov;
   
   prov = E_NEW(E_Iconify_Provider, 1);
   if (!prov) return;
   _e_iconify_providers = eina_list_append(_e_iconify_providers, prov);
   prov->func = provider;
   prov->data = data;
   prov->pri = pri;
}

EAPI void
e_iconify_provider_del(int pri, E_Iconify_Provider_Cb provider, const void *data)
{
   Eina_List *l;
   E_Iconify_Provider *prov;
   
   EINA_LIST_FOREACH(_e_iconify_providers, l, prov)
     {
        if ((prov->func == provider) &&
            (prov->data == data) &&
            (prov->pri == pri))
          {
             _e_iconify_providers =
               eina_list_remove_list(_e_iconify_providers, l);
             free(prov);
             return;
          }
     }
}

EAPI void
e_iconify_provider_obj_message(E_Client *ec, Eina_Bool iconify, Evas_Object *edje_target)
{
   E_Iconify_Provider *prov;
   Evas_Coord ox, oy, ow, oh;
   Edje_Message_Int_Set *msg;
   int x, y, w, h;
   
   prov = _e_iconify_provider_get();
   if (!prov) return;
   evas_object_geometry_get(edje_target, &ox, &oy, &ow, &oh);
   x = ox + (ow / 2);
   y = oy + (oh / 2);
   w = 1;
   h = 1;
   prov->func((void *)(prov->data), ec, iconify, &x, &y, &w, &h);
   msg = alloca(sizeof(Edje_Message_Int_Set) + ((4 - 1) * sizeof(int)));
   msg->count = 4;
   msg->val[0] = x - ox;
   msg->val[1] = y - oy;
   msg->val[2] = w;
   msg->val[3] = h;
   edje_object_message_send(edje_target, EDJE_MESSAGE_INT_SET, 10, msg);
}
