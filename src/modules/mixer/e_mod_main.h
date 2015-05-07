#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "e.h"
#include "e_mod_mixer.h"

/* Increment for Major Changes */
#define MOD_CONFIG_FILE_EPOCH      1
/* Increment for Minor Changes (ie: user doesn't need a new config) */
#define MOD_CONFIG_FILE_GENERATION 1
#define MOD_CONFIG_FILE_VERSION    ((MOD_CONFIG_FILE_EPOCH * 1000000) + MOD_CONFIG_FILE_GENERATION)

typedef struct E_Mixer_Gadget_Config
{
   int lock_sliders;
   int show_locked;
   int keybindings_popup;
   const char *card;
   const char *channel_name;
   const char *id;
   E_Mixer_Channel_State state;
   Eina_Bool using_default;
   E_Config_Dialog *dialog;
   struct E_Mixer_Instance *instance;
} E_Mixer_Gadget_Config;

typedef struct E_Mixer_Module_Config
{
   int version;
   const char *default_gc_id;
   Eina_Hash *gadgets;
   int desktop_notification;
   int disable_pulse;
   int external_mixer_enabled;
   const char *external_mixer_command;
} E_Mixer_Module_Config;

typedef struct E_Mixer_Instance
{
   E_Gadcon_Client *gcc;
   E_Gadcon_Popup *popup;
   Ecore_Timer *popup_timer;

   struct
   {
      Evas_Object *gadget;
      Evas_Object *label;
      Evas_Object *left;
      Evas_Object *right;
      Evas_Object *mute;
      Evas_Object *table;
      Evas_Object *button;
   } ui;

   E_Mixer_System *sys;
   E_Mixer_Channel_Info *channel;
   E_Mixer_Channel_State mixer_state;
   E_Mixer_Gadget_Config *conf;

#ifdef HAVE_ENOTIFY
   unsigned int notification_id;
#endif
} E_Mixer_Instance;

typedef struct E_Mixer_Module_Context
{
   E_Config_DD *module_conf_edd;
   E_Config_DD *gadget_conf_edd;
   E_Mixer_Module_Config *conf;
   E_Config_Dialog *conf_dialog;
   E_Mixer_Instance *default_instance;
   Eina_List *instances;
   E_Dialog *mixer_dialog;
   double last_act_time;
   struct st_mixer_actions
   {
      E_Action *incr;
      E_Action *decr;
      E_Action *mute;
   } actions;
   int desktop_notification;
   int disable_pulse;
   int external_mixer_enabled;
   char *external_mixer_command;
} E_Mixer_Module_Context;

E_API extern E_Module_Api e_modapi;
E_API void *e_modapi_init(E_Module *m);
E_API int e_modapi_shutdown(E_Module *m);
E_API int e_modapi_save(E_Module *m);

E_Config_Dialog *e_mixer_config_module_dialog_new(Evas_Object *parent, E_Mixer_Module_Context *ctxt);
E_Config_Dialog *e_mixer_config_dialog_new(Evas_Object *parent, E_Mixer_Gadget_Config *conf);
E_Dialog *e_mixer_app_dialog_new(Evas_Object *parent, void (*func)(E_Dialog *dialog, void *data), void *data);
int e_mixer_app_dialog_select(E_Dialog *dialog, const char *card_name, const char *channel_name);

int e_mixer_update(E_Mixer_Instance *inst);
const char *e_mixer_theme_path(void);

void e_mod_mixer_pulse_ready(Eina_Bool);
void e_mod_mixer_pulse_update(void);

#endif
