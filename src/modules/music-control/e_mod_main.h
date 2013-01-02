#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include "config.h"
#include <e.h>
#include <eina_log.h>

typedef struct _E_Music_Control_Module_Context
{
   Eina_List *instances;
   EDBus_Connection *conn;
} E_Music_Control_Module_Context;

typedef struct _E_Music_Control_Instance
{
   E_Music_Control_Module_Context *ctxt;
   E_Gadcon_Client *gcc;
   Evas_Object *gadget;
} E_Music_Control_Instance;

EAPI extern E_Module_Api e_modapi;
EAPI void *e_modapi_init(E_Module *m);
EAPI int e_modapi_shutdown(E_Module *m);
EAPI int e_modapi_save(E_Module *m);

#endif
