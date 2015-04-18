#include <e.h>
#include <Eldbus.h>
#include <Efreet.h>
#include "e_mod_main.h"
#include "e_mod_packagekit.h"


/* GUI */
void
packagekit_icon_update(E_PackageKit_Module_Context *ctxt,
                       Eina_Bool working)
{
   E_PackageKit_Instance *inst;
   E_PackageKit_Package *pkg;
   unsigned count = 0;
   const char *state;
   char buf[16];
   Eina_List *l;

   if (working)
     state = "packagekit,state,working";
   else if (ctxt->error)
     state = "packagekit,state,error";
   else
     {
        EINA_LIST_FOREACH(ctxt->packages, l, pkg)
          {
             switch (pkg->info)
               {
                  case PK_INFO_ENUM_LOW:
                  case PK_INFO_ENUM_ENHANCEMENT:
                  case PK_INFO_ENUM_NORMAL:
                  case PK_INFO_ENUM_BUGFIX:
                  case PK_INFO_ENUM_IMPORTANT:
                  case PK_INFO_ENUM_SECURITY:
                     count++;
                     break;
                  default:
                     break;
               }
          }

        if (count > 0)
          state = "packagekit,state,updates";
        else
          state = "packagekit,state,updated";
     }

   DBG("PKGKIT: IconUpdate, %d updates available (%s)", count, state);

   if (count) snprintf(buf, sizeof(buf), "%d", count);
   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     {
        edje_object_signal_emit(inst->gadget, state, "e");
        edje_object_part_text_set(inst->gadget, "num_updates", count ? buf : "");
     }
}

static void
_update_button_cb(void *data, void *data2 EINA_UNUSED)
{
   E_PackageKit_Instance *inst = data;
   packagekit_popup_del(inst);
   packagekit_create_transaction_and_exec(inst->ctxt, packagekit_refresh_cache);
}

static void
_run_button_cb(void *data, void *data2 EINA_UNUSED)
{
   E_PackageKit_Instance *inst = data;
   E_PackageKit_Module_Context *ctxt = inst->ctxt;
   packagekit_popup_del(inst);

   if (ctxt->config->manager_command && ctxt->config->manager_command[0])
     e_exec(inst->gcc->gadcon->zone, NULL, inst->ctxt->config->manager_command, NULL, NULL);
   else
     e_util_dialog_show(_("No package manager configured"),
                        _("You need to set your preferred package manager.<br>"
                          "Please open the module configuration and set<br>"
                          "the program to run.<br>"));
}

void
packagekit_popup_update(E_PackageKit_Instance *inst)
{
   E_PackageKit_Module_Context *ctxt = inst->ctxt;
   E_PackageKit_Package *pkg;
   Eina_List *l;
   unsigned num_updates = 0;
   const char *emblem_name;
   Efreet_Desktop *desktop;
   Evas *evas = e_comp->evas;
   Evas_Object *icon, *end;
   char buf[PATH_MAX];

   if (ctxt->error)
     {
        e_widget_label_text_set(inst->popup_label, _("No information available"));
        e_widget_ilist_append(inst->popup_ilist, NULL, ctxt->error, NULL, NULL, NULL);
        if ((ctxt->v_maj != -1) && (ctxt->v_min != -1) && (ctxt->v_mic != -1))
          {
             snprintf(buf, sizeof(buf), "PackageKit version: %d.%d.%d",
                      ctxt->v_maj, ctxt->v_min, ctxt->v_mic);
             e_widget_ilist_append(inst->popup_ilist, NULL, buf, NULL, NULL, NULL);
          }
        return;
     }

   EINA_LIST_FOREACH(ctxt->packages, l, pkg)
     {
        switch (pkg->info)
          {
             case PK_INFO_ENUM_LOW:
               emblem_name = "e/modules/packagekit/icon/low"; break;
             case PK_INFO_ENUM_ENHANCEMENT:
               emblem_name = "e/modules/packagekit/icon/enhancement"; break;
             case PK_INFO_ENUM_NORMAL:
               emblem_name = "e/modules/packagekit/icon/normal"; break;
             case PK_INFO_ENUM_BUGFIX:
               emblem_name = "e/modules/packagekit/icon/bugfix"; break;
             case PK_INFO_ENUM_IMPORTANT:
               emblem_name = "e/modules/packagekit/icon/important"; break;
             case PK_INFO_ENUM_SECURITY:
               emblem_name = "e/modules/packagekit/icon/security"; break;
             default:
               emblem_name = NULL; break;
          }
        if (emblem_name)
          {
             // try to find a desktop file that match the executable or the name
             desktop = efreet_util_desktop_exec_find(pkg->name);
             if (!desktop)
               desktop = efreet_util_desktop_name_find(pkg->name);

             if (desktop && desktop->icon)
               {
                  icon = e_icon_add(evas);
                  e_icon_fdo_icon_set(icon, desktop->icon);
                  efreet_desktop_free(desktop);
               }
             else
               icon = NULL;

             // get the priority icon from the theme
             end = edje_object_add(evas);
             e_theme_edje_object_set(end, "base/theme/modules/packagekit", emblem_name);

             e_widget_ilist_append_full(inst->popup_ilist, icon, end,
                     ctxt->config->show_description ? pkg->summary : pkg->name,
                     NULL, NULL, NULL);
             num_updates++;
          }
     }

   if (num_updates >= 1)
     snprintf(buf, sizeof(buf), P_("One update available", "%d updates available", num_updates), num_updates);
   else
     snprintf(buf, sizeof(buf), _("Your system is updated"));
   e_widget_label_text_set(inst->popup_label, buf);
}

static void
_popup_del_cb(void *obj)
{
   packagekit_popup_del(e_object_data_get(obj));
}

static void
_popup_autoclose_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
   packagekit_popup_del((E_PackageKit_Instance *)data);
}

void
packagekit_popup_new(E_PackageKit_Instance *inst)
{
   Evas_Object *table, *bt;
   Evas *evas;

   inst->popup = e_gadcon_popup_new(inst->gcc, EINA_FALSE);
   evas = e_comp->evas;

   table = e_widget_table_add(e_win_evas_win_get(evas), 0);

   inst->popup_label = e_widget_label_add(evas, NULL);
   e_widget_table_object_append(table, inst->popup_label, 0,0, 1,1, 1,0,1,0);

   bt = e_widget_button_add(evas, NULL, "view-refresh",
                            _update_button_cb, inst, NULL);
   e_widget_table_object_append(table, bt, 1,0, 1,1, 0,0,0,0);

   inst->popup_ilist = e_widget_ilist_add(evas, 24, 24, NULL);
   e_widget_size_min_set(inst->popup_ilist, 240, 200);
   e_widget_table_object_append(table, inst->popup_ilist, 0,1, 2,1, 1,1,0,0);

   bt = e_widget_button_add(evas, _("Run the package manager"), NULL,
                            _run_button_cb, inst, NULL);
   e_widget_table_object_append(table, bt, 0,3, 2,1, 1,0,0,0);

   packagekit_popup_update(inst);

   e_gadcon_popup_content_set(inst->popup, table);
   e_comp_object_util_autoclose(inst->popup->comp_object,
                                _popup_autoclose_cb, NULL, inst);
   e_object_data_set(E_OBJECT(inst->popup), inst);
   E_OBJECT_DEL_SET(inst->popup, _popup_del_cb);

   e_gadcon_popup_show(inst->popup);
}

void
packagekit_popup_del(E_PackageKit_Instance *inst)
{
   E_FREE_FUNC(inst->popup, e_object_del);
   inst->popup_ilist = inst->popup_label = NULL;
}


static void
_store_error(E_PackageKit_Module_Context *ctxt, const char *err)
{
   ERR("PKGKIT: ERROR: %s", err);
   if (ctxt->error)
      eina_stringshare_replace(&ctxt->error, err);
   else
      ctxt->error = eina_stringshare_add(err);
   packagekit_icon_update(ctxt, EINA_FALSE);
}


/* RefreshCache() */
static void
null_cb(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   E_PackageKit_Module_Context *ctxt = data;
   const char *error, *error_msg;

   if (eldbus_message_error_get(msg, &error, &error_msg))
     _store_error(ctxt, error_msg);
}

static void
signal_repo_detail_cb(void *data, const Eldbus_Message *msg)
{ /* RepoDetail ('s'repo_id, 's'description, 'b'enabled) */
   E_PackageKit_Module_Context *ctxt = data;
   const char *error, *error_msg, *repo_id, *desc;
   Eina_Bool enabled;

   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        _store_error(ctxt, error_msg);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "ssb", &repo_id, &desc, &enabled))
     {
        _store_error(ctxt, "could not get arguments (ssb)");
        return;
     }
   DBG("PKGKIT: RepoDetail: (%d) %s [ %s ]", enabled, repo_id, desc);
}

static void
signal_cache_finished_cb(void *data, const Eldbus_Message *msg)
{  /* Finished ('u'exit, 'u'runtime) */
   E_PackageKit_Module_Context *ctxt = data;
   const char *error, *error_msg;

   DBG("PKGKIT: Cache Finished CB");

   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        _store_error(ctxt, error_msg);
        return;
     }

   Eldbus_Object *obj = eldbus_proxy_object_get(ctxt->transaction);
   E_FREE_FUNC(ctxt->transaction, eldbus_proxy_unref);
   E_FREE_FUNC(obj, eldbus_object_unref);

   packagekit_create_transaction_and_exec(ctxt, packagekit_get_updates);
}

void
packagekit_refresh_cache(E_PackageKit_Module_Context *ctxt, const char *transaction)
{
   Eldbus_Object *obj;
   Eldbus_Proxy *proxy;
   Eldbus_Pending *pending;

   ctxt->config->last_update = ecore_time_unix_get();

   obj = eldbus_object_get(ctxt->conn, "org.freedesktop.PackageKit", transaction);
   proxy = eldbus_proxy_get(obj, "org.freedesktop.PackageKit.Transaction");
   pending = eldbus_proxy_call(proxy, "RefreshCache", null_cb, ctxt, -1, "b", 1);
   if (!pending)
     {
        _store_error(ctxt, "could not call RefreshCache()");
        return;
     }
   eldbus_proxy_signal_handler_add(proxy, "Finished", signal_cache_finished_cb, ctxt);
   eldbus_proxy_signal_handler_add(proxy, "RepoDetail", signal_repo_detail_cb, ctxt);
   ctxt->transaction = proxy;
}


/* GetUpdates() */
static void
_signal_package_cb(void *data, const Eldbus_Message *msg)
{  /* Package ('u'info, 's'package_id, 's'summary) */
   const char *error, *error_msg, *pkg_id, *summary, *info_str;
   E_PackageKit_Module_Context *ctxt = data;
   PackageKit_Package_Info info;
   unsigned num_elements = 0;
   char **splitted;
   Eina_Bool ret;

   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        _store_error(ctxt, error_msg);
        return;
     }
   if (PKITV07)
     ret = eldbus_message_arguments_get(msg, "sss", &info_str, &pkg_id, &summary);
   else
     ret = eldbus_message_arguments_get(msg, "uss", &info, &pkg_id, &summary);
   if (!ret)
     {
        _store_error(ctxt, "could not get package arguments");
        return;
     }
   if (PKITV07)
     { DBG("PKGKIT: Package: (%s) %s [ %s ]", info_str, pkg_id, summary); }
   else
     { DBG("PKGKIT: Package: (%d) %s [ %s ]", info, pkg_id, summary); }

   splitted = eina_str_split_full(pkg_id, ";", 2, &num_elements);
   if (num_elements == 2)
     {
        E_PackageKit_Package *pkg = E_NEW(E_PackageKit_Package, 1);
        pkg->name = eina_stringshare_add(splitted[0]);
        pkg->version = eina_stringshare_add(splitted[1]);
        pkg->summary = eina_stringshare_add(summary);
        if (PKITV07)
          {
             if (!info_str) pkg->info = PK_INFO_ENUM_NORMAL;
             else if (!strcmp(info_str, "normal"))      pkg->info = PK_INFO_ENUM_NORMAL;
             else if (!strcmp(info_str, "security"))    pkg->info = PK_INFO_ENUM_SECURITY;
             else if (!strcmp(info_str, "blocked"))     pkg->info = PK_INFO_ENUM_BLOCKED;
             else if (!strcmp(info_str, "low"))         pkg->info = PK_INFO_ENUM_LOW;
             else if (!strcmp(info_str, "enhancement")) pkg->info = PK_INFO_ENUM_ENHANCEMENT;
             else if (!strcmp(info_str, "bugfix"))      pkg->info = PK_INFO_ENUM_BUGFIX;
             else if (!strcmp(info_str, "important"))   pkg->info = PK_INFO_ENUM_IMPORTANT;
             else pkg->info = PK_INFO_ENUM_UNKNOWN;
          }
        else
          pkg->info = info;
        ctxt->packages = eina_list_append(ctxt->packages, pkg);
     }
   if (splitted)
     {
        free(splitted[0]);
        free(splitted);
     }
}

static void
_signal_finished_cb(void *data, const Eldbus_Message *msg)
{  /* Finished ('u'exit, 'u'runtime) */
   const char *error, *error_msg;
   E_PackageKit_Module_Context *ctxt = data;

   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        _store_error(ctxt, error_msg);
        return;
     }

   Eldbus_Object *obj = eldbus_proxy_object_get(ctxt->transaction);
   E_FREE_FUNC(ctxt->transaction, eldbus_proxy_unref);
   E_FREE_FUNC(obj, eldbus_object_unref);
   E_FREE_FUNC(ctxt->error, eina_stringshare_del);

   DBG("PKGKIT: PackageFinished");
   packagekit_icon_update(ctxt, EINA_FALSE);
}

void
packagekit_get_updates(E_PackageKit_Module_Context *ctxt, const char *transaction)
{
   Eldbus_Object *obj;
   Eldbus_Proxy *proxy;
   Eldbus_Pending *pending;
   E_PackageKit_Package *pkg;

   obj = eldbus_object_get(ctxt->conn, "org.freedesktop.PackageKit", transaction);
   proxy = eldbus_proxy_get(obj, "org.freedesktop.PackageKit.Transaction");
   if (PKITV07)
     pending = eldbus_proxy_call(proxy, "GetUpdates", null_cb, ctxt, -1, "s", "none");
   else
     pending = eldbus_proxy_call(proxy, "GetUpdates", null_cb, ctxt, -1, "t", 1);
   if (!pending)
     {
        _store_error(ctxt, "could not call GetUpdates()");
        return;
     }

   eldbus_proxy_signal_handler_add(proxy, "Package", _signal_package_cb, ctxt);
   eldbus_proxy_signal_handler_add(proxy, "Finished", _signal_finished_cb, ctxt);
   ctxt->transaction = proxy;

   EINA_LIST_FREE(ctxt->packages, pkg)
     {
        E_FREE_FUNC(pkg->name, eina_stringshare_del);
        E_FREE_FUNC(pkg->version, eina_stringshare_del);
        E_FREE_FUNC(pkg->summary, eina_stringshare_del);
     }
}


/* CreateTransaction() */
static void
_transaction_created_cb(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending)
{
   const char *error, *error_msg, *trans_path;
   E_PackageKit_Module_Context *ctxt = data;
   E_PackageKit_Transaction_Func func;
   Eina_Bool ret;

   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        _store_error(ctxt, error_msg);
        return;
     }

   if (PKITV07)
     ret = eldbus_message_arguments_get(msg, "s", &trans_path);
   else
     ret = eldbus_message_arguments_get(msg, "o", &trans_path);
   if (!ret)
     {
        _store_error(ctxt, "could not get transaction path");
        return;
     }

   func = eldbus_pending_data_get(pending, "func");
   func(ctxt, trans_path);
}

void
packagekit_create_transaction_and_exec(E_PackageKit_Module_Context *ctxt,
                                       E_PackageKit_Transaction_Func func)
{
   Eldbus_Pending *pending;

   DBG("PKGKIT: Version: %d.%d.%d", ctxt->v_maj, ctxt->v_min, ctxt->v_mic);

   if (ctxt->transaction)
     {
        WRN("PKGKIT: Another transaction in progress...");
        return;
     }

   pending = eldbus_proxy_call(ctxt->packagekit,
                               PKITV07 ? "GetTid" : "CreateTransaction",
                               _transaction_created_cb, ctxt, -1, "");
   if (!pending)
     {
        _store_error(ctxt, "could not call CreateTransaction()");
        return;
     }
   eldbus_pending_data_set(pending, "func", func);
   packagekit_icon_update(ctxt, EINA_TRUE);
}


/* PackageKit DBus */
static void
_signal_updates_changed_cb(void *data, const Eldbus_Message *msg EINA_UNUSED)
{
   E_PackageKit_Module_Context *ctxt = data;

   packagekit_create_transaction_and_exec(ctxt, packagekit_get_updates);
}

static void
_iterate_dict(void *data, const void *key, Eldbus_Message_Iter *var)
{
   E_PackageKit_Module_Context *ctxt = data;

   if (!strcmp(key, "VersionMajor"))
     eldbus_message_iter_arguments_get(var, "u", &(ctxt->v_maj));
   else if (!strcmp(key, "VersionMinor"))
     eldbus_message_iter_arguments_get(var, "u", &(ctxt->v_min));
   else if (!strcmp(key, "VersionMicro"))
     eldbus_message_iter_arguments_get(var, "u", &(ctxt->v_mic));
   else return;

   if ((ctxt->v_maj != -1) && (ctxt->v_min != -1) && (ctxt->v_mic != -1))
     packagekit_create_transaction_and_exec(ctxt, packagekit_get_updates);
}

static void
_prop_get_cb(void *data, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   E_PackageKit_Module_Context *ctxt = data;
   Eldbus_Message_Iter *array;
   const char *error, *error_msg;

   if (eldbus_message_error_get(msg, &error, &error_msg))
     {
        _store_error(ctxt, error_msg);
        return;
     }
   if (!eldbus_message_arguments_get(msg, "a{sv}", &array))
     {
        _store_error(ctxt, "could not get arguments (a{sv})");
        return;
     }
   eldbus_message_iter_dict_iterate(array, "sv", _iterate_dict, ctxt);
}

Eina_Bool
packagekit_dbus_connect(E_PackageKit_Module_Context *ctxt)
{
   Eldbus_Object *obj;

   DBG("PKGKIT: dbus_init()");
   eldbus_init();

   ctxt->conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   if (!ctxt->conn)
     {
        _store_error(ctxt, "could not connect to system bus");
        return EINA_FALSE;
     }

   obj = eldbus_object_get(ctxt->conn, "org.freedesktop.PackageKit",
                                       "/org/freedesktop/PackageKit");
   ctxt->packagekit = eldbus_proxy_get(obj, "org.freedesktop.PackageKit");
   if (!ctxt->packagekit)
   {
      _store_error(ctxt, "could not connect to PackageKit");
      return EINA_FALSE;
   }

   eldbus_proxy_property_get_all(ctxt->packagekit, _prop_get_cb, ctxt);
   eldbus_proxy_signal_handler_add(ctxt->packagekit, "UpdatesChanged",
                                   _signal_updates_changed_cb, ctxt);

   return EINA_TRUE;
}

void
packagekit_dbus_disconnect(E_PackageKit_Module_Context *ctxt)
{
   Eldbus_Object *obj;

   DBG("PKGKIT: dbus_shutdown()");

   if (ctxt->transaction)
     {
        // TODO abort the transaction ???
        obj = eldbus_proxy_object_get(ctxt->transaction);
        E_FREE_FUNC(ctxt->transaction, eldbus_proxy_unref);
        E_FREE_FUNC(obj, eldbus_object_unref);
     }

   obj = eldbus_proxy_object_get(ctxt->packagekit);
   E_FREE_FUNC(ctxt->packagekit, eldbus_proxy_unref);
   E_FREE_FUNC(obj, eldbus_object_unref);

   eldbus_connection_unref(ctxt->conn);
   eldbus_shutdown();
}
