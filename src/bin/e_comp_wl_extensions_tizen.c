#define E_COMP_WL
#include "e.h"

#include "efl-aux-hints-server-protocol.h"


enum _E_Policy_Hint_Type
{
   E_POLICY_HINT_MSG_USE = 0,
};

static const char *hint_names[] =
{
   "wm.policy.win.msg.use",
};
static Eina_List *aux_hints;

static void
_e_policy_wl_allowed_aux_hint_send(E_Client *ec, int id)
{
   Eina_List *l;
   struct wl_client *wc;
   struct wl_resource *res;

   wc = wl_resource_get_client(ec->comp_data->surface);
   EINA_LIST_FOREACH(aux_hints, l, res)
     if (wl_resource_get_client(res) == wc) break;
   if (!res) return;

   efl_aux_hints_send_allowed_aux_hint(
     res,
      ec->comp_data->surface,
      id);
}

static void
_e_policy_wl_aux_hint_apply(E_Client *ec)
{
   E_Comp_Wl_Aux_Hint *hint;
   Eina_List *l;
   Eina_Bool send;

   if (!ec->comp_data) return;
   if (!ec->comp_data->aux_hint.changed) return;

   EINA_LIST_FOREACH(ec->comp_data->aux_hint.hints, l, hint)
     {
        if (!hint->changed) continue;
        send = EINA_FALSE;
        if (!strcmp(hint->hint, hint_names[E_POLICY_HINT_MSG_USE]))
          {
             if ((hint->deleted) || (!strcmp(hint->val, "0")))
               ec->comp_data->aux_hint.use_msg = EINA_FALSE;
             else if (!strcmp(hint->val, "1"))
               ec->comp_data->aux_hint.use_msg = EINA_TRUE;
          }
        if (send)
          _e_policy_wl_allowed_aux_hint_send(ec, hint->id);
     }
}

static void
_tzpol_iface_cb_aux_hint_add(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, int32_t id, const char *name, const char *value)
{
   E_Client *ec;
   Eina_Bool res = EINA_FALSE;


   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   res = e_hints_aux_hint_add(ec, id, name, value);

   if (res)
     {
        _e_policy_wl_aux_hint_apply(ec);
        efl_aux_hints_send_allowed_aux_hint(res_tzpol, surf, id);
        EC_CHANGED(ec);
     }
}

static void
_tzpol_iface_cb_aux_hint_change(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, int32_t id, const char *value)
{
   E_Client *ec;
   Eina_Bool res = EINA_FALSE;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   res = e_hints_aux_hint_change(ec, id, value);

   if (res)
     {
        _e_policy_wl_aux_hint_apply(ec);
        efl_aux_hints_send_allowed_aux_hint(res_tzpol, surf, id);
        EC_CHANGED(ec);
     }
}

static void
_tzpol_iface_cb_aux_hint_del(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf, int32_t id)
{
   E_Client *ec;
   unsigned int res = -1;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   res = e_hints_aux_hint_del(ec, id);

   if (res)
     {
        _e_policy_wl_aux_hint_apply(ec);
        EC_CHANGED(ec);
     }
}

static void
_tzpol_iface_cb_supported_aux_hints_get(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf)
{
   E_Client *ec;
   const Eina_List *hints_list;
   const Eina_List *l;
   struct wl_array hints;
   const char *hint_name;
   int len;
   char *p;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   hints_list = e_hints_aux_hint_supported_get();

   wl_array_init(&hints);
   EINA_LIST_FOREACH(hints_list, l, hint_name)
     {
        len = strlen(hint_name) + 1;
        p = wl_array_add(&hints, len);

        if (p == NULL)
          break;
        strncpy(p, hint_name, len);
     }

   efl_aux_hints_send_supported_aux_hints(res_tzpol, surf, &hints, eina_list_count(hints_list));
   wl_array_release(&hints);
}

E_API void
e_policy_wl_aux_message_send(E_Client *ec,
                             const char *key,
                             const char *val,
                             Eina_List *options)
{
   Eina_List *l;
   struct wl_client *wc;
   struct wl_resource *res;
   struct wl_array opt_array;
   const char *option;
   int len;
   char *p;

   if (!ec->comp_data) return;
   if (!ec->comp_data->aux_hint.use_msg) return;
   wc = wl_resource_get_client(ec->comp_data->surface);
   EINA_LIST_FOREACH(aux_hints, l, res)
     if (wl_resource_get_client(res) == wc) break;
   if (!res) return;

   wl_array_init(&opt_array);
   EINA_LIST_FOREACH(options, l, option)
     {
        len = strlen(option) + 1;
        p = wl_array_add(&opt_array, len);

        if (p == NULL)
          break;
        strncpy(p, option, len);
     }

   efl_aux_hints_send_aux_message(res, ec->comp_data->surface,
                                 key, val, &opt_array);
   wl_array_release(&opt_array);
}

void
e_policy_wl_aux_hint_init(void)
{
   int i, n;
   //E_Config_Aux_Hint_Supported *auxhint;
   //Eina_List *l;

   n = (sizeof(hint_names) / sizeof(char *));

   for (i = 0; i < n; i++)
     {
        e_hints_aux_hint_supported_add(hint_names[i]);
     }

   //EINA_LIST_FOREACH(e_config->aux_hint_supported, l, auxhint)
     //{
        //if (!auxhint->name) continue;
        //e_hints_aux_hint_supported_add(auxhint->name);
     //}

   return;
}

#define GLOBAL_BIND_CB(NAME, IFACE, ...) \
static void \
_e_comp_wl_##NAME##_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version EINA_UNUSED, uint32_t id) \
{ \
   struct wl_resource *res; \
\
   if (!(res = wl_resource_create(client, &(IFACE), 1, id))) \
     { \
        ERR("Could not create %s interface", #NAME);\
        wl_client_post_no_memory(client);\
        return;\
     }\
\
   wl_resource_set_implementation(res, &_e_##NAME##_interface, NULL, NULL);\
}

static const struct efl_aux_hints_interface _e_efl_aux_hints_interface =
{
   _tzpol_iface_cb_aux_hint_add,
   _tzpol_iface_cb_aux_hint_change,
   _tzpol_iface_cb_aux_hint_del,
   _tzpol_iface_cb_supported_aux_hints_get,
};

static void
_efl_aux_hints_destroy(struct wl_resource *res)
{
   aux_hints = eina_list_remove(aux_hints, res);
}

static void
_e_comp_wl_efl_aux_hints_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version EINA_UNUSED, uint32_t id)
{
   struct wl_resource *res;

   if (!(res = wl_resource_create(client, &efl_aux_hints_interface, 1, id)))
     {
        ERR("Could not create %s interface", "efl-aux-hints");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_efl_aux_hints_interface, NULL, _efl_aux_hints_destroy);
   aux_hints = eina_list_append(aux_hints, res);
}

#define GLOBAL_CREATE_OR_RETURN(NAME, IFACE, VERSION) \
   do { \
      struct wl_global *global; \
\
      global = wl_global_create(e_comp_wl->wl.disp, &(IFACE), VERSION, \
                                NULL, _e_comp_wl_##NAME##_cb_bind); \
      if (!global) \
        { \
           ERR("Could not add %s to wayland globals", #IFACE); \
           return EINA_FALSE; \
        } \
      e_comp_wl->extensions->NAME.global = global; \
   } while (0)

EINTERN Eina_Bool
e_comp_wl_extensions_tizen_init(void)
{
   GLOBAL_CREATE_OR_RETURN(efl_aux_hints, efl_aux_hints_interface, 1);
   e_policy_wl_aux_hint_init();
   return EINA_TRUE;
}
