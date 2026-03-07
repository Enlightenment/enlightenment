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
             Evas_Object *list;
             Evas_Object *ip_label;
             Evas_Object *enabled; /* "Wifi On" checkbox */
          } popup;
     } ui;
};

struct E_NM_Module_Context
{
   Eina_List          *instances;
   E_Config_Dialog    *conf_dialog;

   struct NM_Manager  *nm;
   int                 wireless_enabled; /* int for e_widget_check bitmask */
};

E_API extern E_Module_Api e_modapi;
E_API void *e_modapi_init(E_Module *m);
E_API int   e_modapi_shutdown(E_Module *m);
E_API int   e_modapi_save(E_Module *m);

void        enm_popup_del(E_NM_Instance *inst);
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
