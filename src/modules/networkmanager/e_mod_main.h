#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include <e.h>

#include "e_networkmanager.h"

#define AGENT_PATH "/org/freedesktop/NetworkManager/SecretAgent"

extern E_Module *networkmanager_mod;
extern int _e_nm_log_dom;

typedef struct E_NM_Instance       E_NM_Instance;
typedef struct E_NM_Module_Context E_NM_Module_Context;

struct E_NM_Instance
{
   E_NM_Module_Context *ctxt;
   E_Gadcon_Client     *gcc;

   E_Gadcon_Popup *popup;

   struct
     {
        Evas_Object *gadget;

        struct
          {
             Evas_Object            *genlist;
             Evas_Object            *ip_label;
             Elm_Genlist_Item_Class *itc_group;
             Elm_Genlist_Item_Class *itc_group_wifi;
             Elm_Genlist_Item_Class *itc_ap;
             Elm_Genlist_Item_Class *itc_eth;
             Ecore_Timer            *deselect_timer;
             Elm_Object_Item        *deselect_item;
          } popup;
     } ui;
};

struct E_NM_Module_Context
{
   Eina_List          *instances;
   E_Config_Dialog    *conf_dialog;
   Ecore_Timer        *popup_update_timer;

   struct NM_Manager  *nm;

   /* Network activity indicator.  Polling happens on a background thread
    * via ecore_thread_feedback_run; samples are delivered back to the main
    * loop which computes traffic levels and emits edje signals. */
   Ecore_Thread        *traffic_thread;
   char                *traffic_iface;   /* strdup of iface thread is reading */
   Ecore_Pipe          *pipe;
   int                  pipe_fd;
   Ecore_Event_Handler *powersave_handler;
   void                *worker;
   unsigned long long   prev_rx;
   unsigned long long   prev_tx;
   double               poll_time;
   int                  rx_level;        /* 0=idle, 1=low, 2=medium, 3=high */
   int                  tx_level;
   Eina_Bool            powersave_high : 1;
};

E_API extern E_Module_Api e_modapi;
E_API void *e_modapi_init(E_Module *m);
E_API int   e_modapi_shutdown(E_Module *m);
E_API int   e_modapi_save(E_Module *m);

void        enm_popup_del(E_NM_Instance *inst);
void        enm_mod_aps_update_now(void);
const char *e_nm_theme_path(void);

/* Register the password-dialog UI callbacks with the agent subsystem. */
void        enm_agent_ui_register(void);

/**
 * @addtogroup Optional_Devices
 * @{
 *
 * @defgroup Module_NetworkManager NetworkManager
 *
 * Controls network connections for ethernet, wifi, bluetooth
 * and mobile broadband via the NetworkManager D-Bus API.
 *
 * @see https://networkmanager.dev/
 * @}
 */

#endif /* E_MOD_MAIN_H */
