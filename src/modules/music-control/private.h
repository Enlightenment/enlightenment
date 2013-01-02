#ifndef MUSIC_CONTROL_PRIVATE_H
#define MUSIC_CONTROL_PRIVATE_H

#include "e_mod_main.h"

typedef struct _E_Music_Control_Module_Context
{
   Eina_List *instances;
   EDBus_Connection *conn;
   Eina_Bool playning:1;
} E_Music_Control_Module_Context;

typedef struct _E_Music_Control_Instance
{
   E_Music_Control_Module_Context *ctxt;
   E_Gadcon_Client *gcc;
   Evas_Object *gadget;
   E_Gadcon_Popup *popup;
   Evas_Object *content_popup;
} E_Music_Control_Instance;

void music_control_mouse_down_cb(void *data, Evas *evas, Evas_Object *obj, void *event);
const char *music_control_edj_path_get(void);
void music_control_popup_del(E_Music_Control_Instance *inst);
void music_control_state_update_all(E_Music_Control_Module_Context *ctxt);

#endif
