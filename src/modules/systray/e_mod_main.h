#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "e.h"

E_API extern E_Module_Api e_modapi;

E_API void *e_modapi_init(E_Module *m);
E_API int   e_modapi_shutdown(E_Module *m);
E_API int   e_modapi_save(E_Module *m);

typedef struct _Instance Instance;
typedef struct _Context_Notifier_Host Context_Notifier_Host;
typedef struct _Instance_Notifier_Host Instance_Notifier_Host;
typedef struct _Notifier_Item Notifier_Item;

struct _Instance
{
   E_Gadcon_Client *gcc;
   Evas            *evas;
   Instance_Notifier_Host *notifier;
   struct
   {
      Evas_Object *gadget;
   } ui;
   struct
   {
      Ecore_Job *size_apply;
   } job;
};

void systray_notifier_host_init(void);
void systray_notifier_host_shutdown(void);

Evas_Object* systray_notifier_host_new(const char *shelfstyle, const char *style);

extern E_Module *systray_mod;
/**
 * @addtogroup Optional_Gadgets
 * @{
 *
 * @defgroup Module_Systray Systray (System Icons Tray)
 *
 * Shows system icons in a box.
 *
 * The icons come from the FreeDesktop.Org systray specification.
 *
 * @see http://standards.freedesktop.org/systemtray-spec/systemtray-spec-latest.html
 * @}
 */
#endif
