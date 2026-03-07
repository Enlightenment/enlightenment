/* Setup if we need connman or networkmanager? */
#include "e_wizard.h"
#include "e_wizard_api.h"

static void
_remove_module(const char *name)
{
   E_Config_Module *em;
   Eina_List *l;

   EINA_LIST_FOREACH(e_config->modules, l, em)
     {
        if (!em->name) continue;
        if (!strcmp(em->name, name))
          {
             e_config->modules = eina_list_remove_list(e_config->modules, l);
             if (em->name) eina_stringshare_del(em->name);
             free(em);
             break;
          }
     }
}

static void
_add_module(const char *name)
{
   E_Config_Module *em;
   Eina_List *l;

   EINA_LIST_FOREACH(e_config->modules, l, em)
     {
        if ((em->name) && (!strcmp(em->name, name)))
          return;
     }
   em = E_NEW(E_Config_Module, 1);
   em->name = eina_stringshare_add(name);
   em->enabled = 1;
   e_config->modules = eina_list_append(e_config->modules, em);
}

static void
_recommend_nm(E_Wizard_Page *pg EINA_UNUSED)
{
   Evas_Object *of, *ob;

   api->wizard_title_set(_("Network Management"));

   of = elm_frame_add(e_comp->elm);
   ob = elm_label_add(of);
   elm_object_content_set(of, ob);
#if defined(USE_MODULE_CONNMAN) || defined(USE_MODULE_WIRELESS) || defined(USE_MODULE_NETWORKMANAGER)
   elm_object_text_set(of, _("No network service found"));
   elm_object_text_set(ob,
     _("Install/Enable ConnMan or NetworkManager for network management support"));
#else
   elm_object_text_set(of, _("Network modules disabled"));
   elm_object_text_set(ob,
     _("Enlightenment supports ConnMan and NetworkManager.<br>"
       "Install one of these modules for network management support"));
#endif
   evas_object_show(ob);
   evas_object_show(of);

   api->wizard_page_show(of);

   api->wizard_button_next_enable_set(1);
}

static Eldbus_Connection *conn;
static Eldbus_Pending *pending_connman;
static Eldbus_Pending *pending_nm;
static Ecore_Timer *check_timeout = NULL;

static Eina_Bool connman_found = EINA_FALSE;
static Eina_Bool nm_found = EINA_FALSE;
static Eina_Bool connman_done = EINA_FALSE;
static Eina_Bool nm_done = EINA_FALSE;

static void _check_results(E_Wizard_Page *pg);

static Eina_Bool
_page_next_call(void *data EINA_UNUSED)
{
   api->wizard_next();
   return ECORE_CALLBACK_CANCEL;
}

static void
_check_results(E_Wizard_Page *pg)
{
   if (!connman_done || !nm_done) return;

   if (check_timeout)
     {
        ecore_timer_del(check_timeout);
        check_timeout = NULL;
     }

   if (connman_found)
     {
        _remove_module("networkmanager");
        e_config_save_queue();
        api->wizard_button_next_enable_set(1);
        ecore_idler_add(_page_next_call, NULL);
     }
   else if (nm_found)
     {
        _remove_module("connman");
        _add_module("networkmanager");
        e_config_save_queue();
        api->wizard_button_next_enable_set(1);
        ecore_idler_add(_page_next_call, NULL);
     }
   else
     {
        _remove_module("connman");
        _remove_module("networkmanager");
        e_config_save_queue();
        _recommend_nm(pg);
     }
}

static Eina_Bool
_check_fail(void *data)
{
   check_timeout = NULL;
   if (pending_connman)
     {
        eldbus_pending_cancel(pending_connman);
        pending_connman = NULL;
     }
   if (pending_nm)
     {
        eldbus_pending_cancel(pending_nm);
        pending_nm = NULL;
     }
   connman_done = EINA_TRUE;
   nm_done = EINA_TRUE;
   _check_results(data);
   return EINA_FALSE;
}

static void
_check_connman_owner(void *data, const Eldbus_Message *msg,
                     Eldbus_Pending *pending EINA_UNUSED)
{
   const char *id;
   pending_connman = NULL;
   connman_done = EINA_TRUE;

   if (eldbus_message_error_get(msg, NULL, NULL))
     goto done;
   if (!eldbus_message_arguments_get(msg, "s", &id))
     goto done;
   if (id[0] != ':')
     goto done;

   connman_found = EINA_TRUE;
done:
   _check_results(data);
}

static void
_check_nm_owner(void *data, const Eldbus_Message *msg,
                Eldbus_Pending *pending EINA_UNUSED)
{
   const char *id;
   pending_nm = NULL;
   nm_done = EINA_TRUE;

   if (eldbus_message_error_get(msg, NULL, NULL))
     goto done;
   if (!eldbus_message_arguments_get(msg, "s", &id))
     goto done;
   if (id[0] != ':')
     goto done;

   nm_found = EINA_TRUE;
done:
   _check_results(data);
}

E_API int
wizard_page_show(E_Wizard_Page *pg)
{
   Eina_Bool checking = EINA_FALSE;

   connman_found = EINA_FALSE;
   nm_found = EINA_FALSE;
   connman_done = EINA_FALSE;
   nm_done = EINA_FALSE;

#if defined(USE_MODULE_CONNMAN) || defined(USE_MODULE_WIRELESS) || defined(USE_MODULE_NETWORKMANAGER)
   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
#endif
   if (conn)
     {
#if defined(USE_MODULE_CONNMAN) || defined(USE_MODULE_WIRELESS)
        pending_connman = eldbus_name_owner_get(conn, "net.connman",
                                                _check_connman_owner, pg);
        checking = EINA_TRUE;
#else
        connman_done = EINA_TRUE;
#endif
#if defined(USE_MODULE_NETWORKMANAGER)
        pending_nm = eldbus_name_owner_get(conn, "org.freedesktop.NetworkManager",
                                           _check_nm_owner, pg);
        checking = EINA_TRUE;
#else
        nm_done = EINA_TRUE;
#endif
     }

   if (checking)
     {
        if (check_timeout)
          ecore_timer_del(check_timeout);
        check_timeout = ecore_timer_loop_add(0.5, _check_fail, pg);
        api->wizard_button_next_enable_set(0);
     }
   else
     {
        connman_done = EINA_TRUE;
        nm_done = EINA_TRUE;
        _check_results(pg);
     }

   api->wizard_title_set(_("Checking for network services"));
   return 1; /* 1 == show ui, and wait for user, 0 == just continue */
}

E_API int
wizard_page_hide(E_Wizard_Page *pg EINA_UNUSED)
{
   if (pending_connman)
     {
        eldbus_pending_cancel(pending_connman);
        pending_connman = NULL;
     }
   if (pending_nm)
     {
        eldbus_pending_cancel(pending_nm);
        pending_nm = NULL;
     }
   if (check_timeout)
     {
        ecore_timer_del(check_timeout);
        check_timeout = NULL;
     }
   if (conn)
     eldbus_connection_unref(conn);
   conn = NULL;

   return 1;
}
