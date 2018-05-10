#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include <e.h>
#include "bz.h"

typedef struct _Instance Instance;
struct _Instance
{
   // common info
   Evas_Object *o_bluez5;
   // e_gadcon info
   E_Gadcon_Client *gcc;
   E_Gadcon_Popup *popup;
   // e_gadget info
   Evas_Object *pop;
   Evas_Object *popcontent;
   int id;
   E_Gadget_Site_Orient orient;
};

typedef struct _Config Config;
struct _Config
{
   const char *lock_dev_addr;
   const char *unlock_dev_addr;
};

extern Config *ebluez5_config;

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init(E_Module *m);
E_API int e_modapi_shutdown(E_Module *m);
E_API int e_modapi_save(E_Module *m);

void ebluez5_popups_show(void);

void ebluez5_popup_hide(Instance *inst);

Evas_Object *ebluez5_popup_content_add(Evas_Object *base, Instance *inst);
void ebluze5_popup_init(void);
void ebluze5_popup_shutdown(void);
void ebluze5_popup_clear(void);
void ebluez5_popup_adapter_add(Obj *o);
void ebluez5_popup_adapter_del(Obj *o);
void ebluez5_popup_adapter_change(Obj *o);
void ebluez5_popup_device_add(Obj *o);
void ebluez5_popup_device_del(Obj *o);
void ebluez5_popup_device_change(Obj *o);

void ebluez5_agent_agent_release(void);
void ebluez5_agent_agent_cancel(void);
void ebluez5_agent_agent_req_pin(Eldbus_Message *msg);
void ebluez5_agent_agent_disp_pin(Eldbus_Message *msg);
void ebluez5_agent_req_pass(Eldbus_Message *msg);
void ebluez5_agent_disp_pass(Eldbus_Message *msg);
void ebluez5_agent_req_confirm(Eldbus_Message *msg);
void ebluez5_agent_req_auth(Eldbus_Message *msg);
void ebluez5_agent_auth_service(Eldbus_Message *msg);

Evas_Object *util_obj_icon_add(Evas_Object *base, Obj *o, int size);
Evas_Object *util_obj_icon_rssi_add(Evas_Object *base, Obj *o, int size);
Evas_Object *util_check_add(Evas_Object *base, const char *text, const char *tip, Eina_Bool state);
Evas_Object *util_button_icon_add(Evas_Object *base, const char *icon, const char *tip);
const char  *util_obj_name_get(Obj *o);

#endif
