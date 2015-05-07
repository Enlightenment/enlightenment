#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

typedef struct _Instance Instance;
struct _Instance
{
   E_Gadcon_Client *gcc;
   E_Menu  *menu;
   Evas_Object *o_bluez4;
   Evas_Object *found_list;
   E_Dialog *search_dialog;
   Evas_Object *adap_list;
   E_Dialog *adapters_dialog;
};

typedef struct _Config Config;
struct _Config
{
   const char *lock_dev_addr;
   const char *unlock_dev_addr;
};

extern Config *ebluez4_config;

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init(E_Module *m);
E_API int e_modapi_shutdown(E_Module *m);
E_API int e_modapi_save(E_Module *m);

void ebluez4_update_inst(Evas_Object *dest, Eina_List *src, Instance *inst);
void ebluez4_update_instances(Eina_List *src);
void ebluez4_update_all_gadgets_visibility(void);
void ebluez4_show_error(const char *err_name, const char *err_msg);
void ebluez4_adapter_settings_del(E_Dialog *dialog);
void ebluez4_adapter_properties_update(void *data);

#endif
