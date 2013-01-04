#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

typedef struct _Instance Instance;
struct _Instance
{
   E_Gadcon_Client *gcc;
   E_Gadcon_Popup  *popup;
   Evas_Object *o_bluez4;
   Evas_Object *created_list;
   Evas_Object *found_list;
   E_Dialog *search_dialog;
};

EAPI extern E_Module_Api e_modapi;

EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);
EAPI int e_modapi_save(E_Module *m);

void ebluez4_append_to_instances(void *data, int list_type);
void ebluez4_update_inst(Evas_Object *dest, Eina_List *src, int list_type);
void ebluez4_update_instances(Eina_List *src, int list_type);
void ebluez4_update_all_gadgets_visibility();

#endif
