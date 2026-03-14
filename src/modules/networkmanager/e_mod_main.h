#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include <e.h>

#include "e_networkmanager.h"

#define AGENT_PATH "/org/enlightenment/networkmanager/agent"

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
             Evas_Object            *enabled; /* "Wifi On" checkbox */
             Elm_Genlist_Item_Class *itc_group;
             Elm_Genlist_Item_Class *itc_ap;
             Elm_Genlist_Item_Class *itc_eth;
          } popup;
     } ui;
};

struct E_NM_Module_Context
{
   Eina_List          *instances;
   E_Config_Dialog    *conf_dialog;
   Ecore_Timer        *popup_update_timer;

   struct NM_Manager  *nm;
   int                 wireless_enabled; /* int for e_widget_check bitmask */

   /* Network activity indicator */
   Ecore_Timer        *traffic_timer;
   Ecore_Event_Handler *screensaver_on_handler;
   Ecore_Event_Handler *screensaver_off_handler;
   Ecore_Event_Handler *powersave_handler;
   unsigned long long   prev_rx;
   unsigned long long   prev_tx;
   int                  rx_level; /* 0=idle, 1=low, 2=medium, 3=high */
   int                  tx_level;
   Eina_Bool            screen_off : 1;
   Eina_Bool            powersave_high : 1;
};

E_API extern E_Module_Api e_modapi;
E_API void *e_modapi_init(E_Module *m);
E_API int   e_modapi_shutdown(E_Module *m);
E_API int   e_modapi_save(E_Module *m);

void        enm_popup_del(E_NM_Instance *inst);
void        enm_mod_aps_update_now(void);
const char *e_nm_theme_path(void);

E_NM_Agent *enm_agent_new(Eldbus_Connection *eldbus_conn) EINA_ARG_NONNULL(1);
void        enm_agent_del(E_NM_Agent *agent);

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
