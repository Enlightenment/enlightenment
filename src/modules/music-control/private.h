#ifndef MUSIC_CONTROL_PRIVATE_H
#define MUSIC_CONTROL_PRIVATE_H

#include "e_mod_main.h"
#include "gen/edbus_media_player2_player.h"
#include "gen/edbus_mpris_media_player2.h"

typedef struct _Music_Control_Config
{
   int player_selected;
} Music_Control_Config;

typedef struct _E_Music_Control_Module_Context
{
   Eina_List *instances;
   EDBus_Connection *conn;
   Eina_Bool playning:1;
   EDBus_Proxy *mrpis2;
   EDBus_Proxy *mpris2_player;
   E_Config_DD *conf_edd;
   Music_Control_Config *config;
} E_Music_Control_Module_Context;

typedef struct _E_Music_Control_Instance
{
   E_Music_Control_Module_Context *ctxt;
   E_Gadcon_Client *gcc;
   Evas_Object *gadget;
   E_Gadcon_Popup *popup;
   Evas_Object *content_popup;
   Ecore_X_Window win;
   Ecore_Event_Handler *mouse_up;
   Ecore_Event_Handler *key_down;
} E_Music_Control_Instance;

void music_control_mouse_down_cb(void *data, Evas *evas, Evas_Object *obj, void *event);
const char *music_control_edj_path_get(void);
void music_control_popup_del(E_Music_Control_Instance *inst);
void music_control_state_update_all(E_Music_Control_Module_Context *ctxt);
Eina_Bool music_control_dbus_init(E_Music_Control_Module_Context *ctxt, const char *bus);

typedef struct _Player {
   const char *name;
   const char *dbus_name;
} Player;

#endif
