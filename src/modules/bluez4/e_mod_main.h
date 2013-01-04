#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

typedef struct _Instance Instance;
struct _Instance
{
   E_Gadcon_Client *gcc;
   E_Gadcon_Popup  *popup;
   Evas_Object *o_bluez4, *bt;
   Evas_Object *found_list;
};

EAPI extern E_Module_Api e_modapi;

EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);
EAPI int e_modapi_save(E_Module *m);

void ebluez4_disabled_set_all_search_buttons(Eina_Bool disabled);
void ebluez4_append_to_instances(const char *addr, const char *name);

#endif
