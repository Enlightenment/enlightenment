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
#define MOD_CONFIG_FILE_EPOCH      1
#define MOD_CONFIG_FILE_GENERATION 4
#define MOD_CONFIG_FILE_VERSION    ((MOD_CONFIG_FILE_EPOCH * 1000000) + MOD_CONFIG_FILE_GENERATION)

/* Stuff for convenience to compress code */
#define IF_TRUE_RETURN(exp) \
   do {                     \
     if (exp) return;       \
   } while(0)

typedef struct _Clip_Data Clip_Data;
typedef struct _Instance Instance;
typedef struct _Mod_Inst Mod_Inst;
typedef struct _Config Config;
typedef struct _Config_Item Config_Item;

struct _Clip_Data
{ // structure used for storing clipboard data in
  char *name;
  char *content;
};

struct _Instance
{ // gadget instances
  E_Gadcon_Client *gcc;
  Evas_Object *o_button;
  Evas_Object *table;
  E_Gadcon_Popup *popup;
};

struct _Mod_Inst
{ // sructure to store a global module instance in
  // complete with a hidden window for event notification purposes
  Instance *inst;
  Evas_Object *ewin; // window to send clipboard events to
  Eina_List *handle; // for handling clipboard events
  Eina_List *items; // clipboard history
  Elm_Sel_Type sel_type; // type of sel we last saw change
};

#define LABEL_MIN  5
#define LABEL_MAX  100

#define HIST_MIN   5
#define HIST_MAX   100

/* We create a structure config for our module, and also a config structure
 * for every item element (you can have multiple gadgets for the same module) */
struct _Config
{
  Eina_List *items;
  E_Module *module;
  E_Config_Dialog *config_dialog;
  const char *log_name;
  Eina_Bool label_length_changed; // Flag indicating a need to update all clip labels as configfuration changed.

  int version;          /* Configuration version                           */
  unsigned int hist_items;   /* Number of history items to store                */
  unsigned int label_length; /* Number of characters of item to display         */
  // these are booleans really...
  int clip_copy;        /* Clipboard to use                                */
  int clip_select;      /* Clipboard to use                                */
  int persistence;      /* History file persistance                        */
  int hist_reverse;     /* Order to display History                        */
  int confirm_clear;    /* Display history confirmation dialog on deletion */
  int ignore_ws;        /* Should we ignore White space in label           */
  int ignore_ws_copy;   /* Should we not copy White space only             */
  int trim_ws;          /* Should we trim White space from selection       */
  int trim_nl;          /* Should we trim new lines from selection         */
};

struct _Config_Item
{
  const char *id;
};

extern Config *clip_cfg;

Eet_Error        clip_save(Eina_List *items);
void             free_clip_data(Clip_Data *clip);
E_Config_Dialog *config_clipboard_module(Evas_Object *parent, const char *params EINA_UNUSED);
Eet_Error        truncate_history(const unsigned int n);
Eet_Error        read_history(Eina_List **items, unsigned int ignore_ws, unsigned int label_length);
Eet_Error        save_history(Eina_List *items);
Eina_Bool        set_clip_content(char **content, char *text, int mode);
Eina_Bool        set_clip_name(char **name, char *text, int mode, int n);
Eina_Bool        is_empty(const char *str);

#endif
