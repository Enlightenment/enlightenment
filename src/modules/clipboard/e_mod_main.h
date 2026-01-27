#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "e.h"

/* Setup the E Module Version, Needed to check if module can run. */
/* The version is stored at compilation time in the module, and is checked
 * by E in order to know if the module is compatible with the actual version */
EAPI extern E_Module_Api e_modapi;

/* E API Module Interface Declarations
 *
 * e_modapi_init:     it is called when e17 initialize the module, note that
 *                    a module can be loaded but not initialized (running)
 *                    Note that this is not the same as _gc_init, that is called
 *                    when the module appears on his container
 * e_modapi_shutdown: it is called when e17 is closing, so calling the modules
 *                    to finish
 * e_modapi_save:     this is called when e17 or by another reason is requested
 *                    to save the configuration file                      */
EAPI void *e_modapi_init     (E_Module *m);
EAPI int   e_modapi_shutdown (E_Module *m EINA_UNUSED);
EAPI int   e_modapi_save     (E_Module *m EINA_UNUSED);

/////////////////////////////////////////////////////////////////////////////

// Key Board Bindings action names
#define ACT_FLOAT   _("Show History")
#define ACT_CONFIG  _("Show Settings")
#define ACT_CLEAR   _("Clear History")

// Macros used for config file versioning
// You can increment the EPOCH value if the old configuration is not
// compatible anymore, it creates an entire new one.
// You need to increment GENERATION when you add new values to the
// configuration file but is not needed to delete the existing conf
#define MOD_CONFIG_FILE_EPOCH      2
#define MOD_CONFIG_FILE_GENERATION 4
#define MOD_CONFIG_FILE_VERSION    ((MOD_CONFIG_FILE_EPOCH * 1000000) + MOD_CONFIG_FILE_GENERATION)

typedef struct _Instance    Instance;
typedef struct _Mod         Mod;
typedef struct _Config      Config;
typedef struct _Config_Item Config_Item;

struct _Instance
{ // gadget instances
  E_Gadcon_Client *gcc;
  Evas_Object     *o_button;
  Evas_Object     *table;
  E_Gadcon_Popup  *popup;
};

struct _Mod
{ // global module state
  Evas_Object  *ewin; // window for cnp events
  Eina_List    *handles; // event handlers
  Elm_Sel_Type  sel_type; // type of last sel
  Eina_List    *instances; // all instances of gadgets
};

#define LABEL_MIN  5
#define LABEL_MAX  100

#define HIST_MIN   5
#define HIST_MAX   100

struct _Config
{ // runtime stuff we don't store
  E_Module *module;
  E_Config_Dialog *config_dialog;
  Eina_Bool label_length_changed;

  // stored data
  int version;
  Eina_List *items; // saved sel items
  unsigned int hist_items; // max number of items
  unsigned int label_length; // max label length for display
  // these are booleans really...
  unsigned char clip_copy; // store ctrl+c/x
  unsigned char clip_select; // store mouse hilight
  unsigned char hist_reverse; // reverse order in popup
};

struct _Config_Item
{
  const char *str; // stored string (stringshare)
};

E_Config_Dialog *config_clipboard_module(Evas_Object *parent, const char *params EINA_UNUSED);

extern           Config *cfg;

Eina_Bool        config_init(void);
void             config_shutdown(void);
Eina_Bool        conifg_new_limit(void);
void             config_free(void);
void             config_save(void);
void             config_hist_limit(void);
void             config_clip_data_free(Config_Item *cd);

#endif
