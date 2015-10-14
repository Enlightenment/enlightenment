#include <e.h>
#include <Eldbus.h>
#include "e_mod_main.h"
#include "ebluez4.h"

/* Local Variables */
static Ecore_Exe *autolock_exe = NULL;
static Ecore_Poller *autolock_poller = NULL;
static Ecore_Event_Handler *autolock_die = NULL;
static Ecore_Event_Handler *autolock_out = NULL;
static Ecore_Event_Handler *autolock_desklock = NULL;
static Eina_Bool autolock_initted = EINA_FALSE;
static Eina_Bool autolock_waiting = EINA_TRUE;

static Eina_List *instances = NULL;
static E_Module *mod = NULL;
static char tmpbuf[1024];

/* Local config */
static E_Config_DD *conf_edd = NULL;
Config *ebluez4_config = NULL;

E_API E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Bluez4"};

/* Local Functions */
static Eina_Bool
_ebluez_l2ping_poller(void *data EINA_UNUSED)
{
   Eina_Strbuf *buf;
   const char *tmp = NULL;

   autolock_poller = NULL;

   buf = eina_strbuf_new();
   if (e_desklock_state_get())
     {
	if (!autolock_waiting)
	  tmp = ebluez4_config->unlock_dev_addr;
	else
	  tmp = ebluez4_config->lock_dev_addr;
     }
   else
     {
        if (!autolock_waiting)
	  tmp = ebluez4_config->lock_dev_addr;
	else
	  tmp = ebluez4_config->unlock_dev_addr;
     }

   if (tmp)
     {
        eina_strbuf_append_printf(buf, "%s/enlightenment/utils/enlightenment_sys l2ping %s",
				  e_prefix_lib_get(), tmp);
	autolock_exe = ecore_exe_run(eina_strbuf_string_get(buf), NULL);
     }

   eina_strbuf_free(buf);

   return 0;
}

static void
_ebluez4_search_dialog_del(Instance *inst)
{
   if (!inst->search_dialog) return;
   e_object_del(E_OBJECT(inst->search_dialog));
   inst->search_dialog = NULL;
   inst->found_list = NULL;

   if (ctxt->adap_obj)
     {
        ebluez4_stop_discovery();
        DBG("Stopping discovery...");
     }
}

static void
_ebluez4_cb_search_dialog_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Dialog *dialog = data;
   _ebluez4_search_dialog_del(dialog->data);
}

static void
_ebluez4_cb_paired(void *data, Eina_Bool success, const char *err_msg)
{
   Instance *inst = data;
   if (success)
     _ebluez4_search_dialog_del(inst);
   else
     ebluez4_show_error(_("Bluez Error"), err_msg);
}

static void
_ebluez4_cb_pair(void *data)
{
   Instance *inst = data;
   const char *addr = e_widget_ilist_selected_value_get(inst->found_list);

   if(!addr)
     return;
   ebluez4_pair_with_device(addr, _ebluez4_cb_paired, inst);
}

static void
_ebluez4_cb_search(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Instance *inst = data;
   E_Dialog *dialog;
   Evas *evas;

   if (inst->search_dialog)
     _ebluez4_search_dialog_del(inst);

   dialog = e_dialog_new(NULL, "Search Dialog", "search");
   e_dialog_title_set(dialog, _("Searching for Devices..."));
   evas_object_event_callback_add(dialog->win, EVAS_CALLBACK_DEL, _ebluez4_cb_search_dialog_del, dialog);
   e_dialog_resizable_set(dialog, EINA_TRUE);

   evas = evas_object_evas_get(dialog->win);

   inst->found_list = e_widget_ilist_add(evas, 100, 0, NULL);

   e_dialog_content_set(dialog, inst->found_list, 300, 200);

   e_dialog_show(dialog);

   dialog->data = inst;
   inst->search_dialog = dialog;

   ebluez4_start_discovery();
   DBG("Starting discovery...");
}

static void
_ebluez4_cb_adap_settings_dialog_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Dialog *dialog = data;
   ebluez4_adapter_settings_del(dialog);
}

static void
_ebluez4_check_changed(void *data, Evas_Object *obj, const char *prop_name)
{
   Adapter *adap = data;
   Eina_Bool value = e_widget_check_checked_get(obj);
   ebluez4_adapter_property_set(adap, prop_name, value);
}

static void
_ebluez4_powered_changed(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
   _ebluez4_check_changed(data, obj, "Powered");
}

static void
_ebluez4_visible_changed(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
   _ebluez4_check_changed(data, obj, "Discoverable");
}

static void
_ebluez4_pairable_changed(void *data, Evas_Object *obj, void *info EINA_UNUSED)
{
   _ebluez4_check_changed(data, obj, "Pairable");
}


static void
_ebluez4_cb_adap_settings(void *data)
{
   Adapter *adap = data;
   E_Dialog *dialog;
   Evas *evas;
   Evas_Object *list;
   Evas_Object *ck;
   int mw, mh;
   Eina_List *ck_list = NULL;

   if (adap->dialog)
      ebluez4_adapter_settings_del(adap->dialog);

   dialog = e_dialog_new(NULL, "Adapter Dialog", "adapter");
   e_dialog_title_set(dialog, _("Adapter Settings"));
   evas_object_event_callback_add(dialog->win, EVAS_CALLBACK_DEL, _ebluez4_cb_adap_settings_dialog_del, dialog);
   e_dialog_resizable_set(dialog, EINA_TRUE);

   evas = evas_object_evas_get(dialog->win);

   list = e_widget_list_add(evas, 0, 0);

   ck = e_widget_check_add(evas, _("Default"), NULL);
   e_widget_check_checked_set(ck, adap->is_default);
   e_widget_list_object_append(list, ck, 0, 0, 0);

   ck = e_widget_check_add(evas, _("Powered"), &(adap->powered_checked));
   e_widget_check_checked_set(ck, adap->powered);
   e_widget_list_object_append(list, ck, 0, 0, 0);
   evas_object_smart_callback_add(ck, "changed", _ebluez4_powered_changed,
                                  adap);
   ck_list = eina_list_append(ck_list, ck);


   ck = e_widget_check_add(evas, _("Visible"), &(adap->visible_checked));
   e_widget_check_checked_set(ck, adap->visible);
   e_widget_list_object_append(list, ck, 0, 0, 0);
   evas_object_smart_callback_add(ck, "changed",
                                  _ebluez4_visible_changed, adap);
   ck_list = eina_list_append(ck_list, ck);

   ck = e_widget_check_add(evas, _("Pairable"), &(adap->pairable_checked));
   e_widget_check_checked_set(ck, adap->pairable);
   e_widget_list_object_append(list, ck, 0, 0, 0);
   evas_object_smart_callback_add(ck, "changed",
                                  _ebluez4_pairable_changed, adap);
   ck_list = eina_list_append(ck_list, ck);

   e_dialog_show(dialog);
   e_widget_size_min_get(list, &mw, &mh);
   if(mw < 150) mw = 150;
   e_dialog_content_set(dialog, list, mw, mh);

   dialog->data = adap;
   adap->dialog = dialog;
   e_object_data_set(E_OBJECT(dialog), ck_list);
}

static void
_ebluez4_adap_list_dialog_del(Instance *inst)
{
   if (!inst->adapters_dialog) return;
   e_object_del(E_OBJECT(inst->adapters_dialog));
   inst->adapters_dialog = NULL;
   inst->adap_list = NULL;
}

static void
_ebluez4_cb_adap_list_dialog_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Dialog *dialog = data;
   _ebluez4_adap_list_dialog_del(dialog->data);
}

static void
_ebluez4_cb_adap_list(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Instance *inst = data;
   E_Dialog *dialog;
   Evas *evas;

   if (inst->adapters_dialog)
      _ebluez4_adap_list_dialog_del(inst);

   dialog = e_dialog_new(NULL, "Adapters Dialog", "adapters");
   e_dialog_title_set(dialog, _("Adapters Available"));
   evas_object_event_callback_add(dialog->win, EVAS_CALLBACK_DEL, _ebluez4_cb_adap_list_dialog_del, dialog);
   e_dialog_resizable_set(dialog, EINA_TRUE);

   evas = evas_object_evas_get(dialog->win);

   inst->adap_list = e_widget_ilist_add(evas, 0, 0, NULL);

   e_dialog_content_set(dialog, inst->adap_list, 250, 220);
   ebluez4_update_instances(ctxt->adapters);

   e_dialog_show(dialog);

   dialog->data = inst;
   inst->adapters_dialog = dialog;
}

static void
_ebluez4_cb_connect(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   ebluez4_connect_to_device(data);
}

static void
_ebluez4_cb_disconnect(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   ebluez4_disconnect_device(data);
}

static void
_ebluez4_cb_forget(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Device *dev = data;
   ebluez4_remove_device(dev->obj);
}

#ifdef HAVE_BLUETOOTH
static void
_ebluez4_cb_lock(void *data,
		 E_Menu *m EINA_UNUSED,
		 E_Menu_Item *mi)
{
   Device *dev = data;
   int tog;

   tog = e_menu_item_toggle_get(mi);
   eina_stringshare_replace(&ebluez4_config->lock_dev_addr,
			    tog ? dev->addr : NULL);
   e_config_save_queue();

   if (autolock_exe)
     ecore_exe_kill(autolock_exe);
   autolock_exe = NULL;
   if (!autolock_poller && (ebluez4_config->lock_dev_addr || ebluez4_config->unlock_dev_addr))
     autolock_poller = ecore_poller_add(ECORE_POLLER_CORE, 32, _ebluez_l2ping_poller, NULL);
}

static void
_ebluez4_cb_unlock(void *data,
		   E_Menu *m EINA_UNUSED,
		   E_Menu_Item *mi)
{
   Device *dev = data;
   int tog;

   tog = e_menu_item_toggle_get(mi);
   eina_stringshare_replace(&ebluez4_config->unlock_dev_addr,
			    tog ? dev->addr : NULL);
   e_config_save_queue();

   if (autolock_exe)
     ecore_exe_kill(autolock_exe);
   autolock_exe = NULL;
   if (!autolock_poller && (ebluez4_config->lock_dev_addr || ebluez4_config->unlock_dev_addr))
     autolock_poller = ecore_poller_add(ECORE_POLLER_CORE, 32, _ebluez_l2ping_poller, NULL);
}
#endif

static void
_menu_post_deactivate(void *data EINA_UNUSED, E_Menu *m)
{
   Eina_List *iter;
   E_Menu_Item *mi;
   Instance *inst = data;

   if (!(m->parent_item) || !(m->parent_item->menu))
     {
        e_gadcon_locked_set(inst->gcc->gadcon, 0);
        inst->menu = NULL;
     }
   EINA_LIST_FOREACH(m->items, iter, mi)
     if (mi->submenu) e_menu_deactivate(mi->submenu);
   e_object_del(E_OBJECT(m));
}

static Eina_Bool
_ebluez4_add_devices(Instance *inst)
{
   Device *dev;
   Eina_List *iter;
   E_Menu *m, *subm;
   E_Menu_Item *mi, *submi;
   Eina_Bool ret = EINA_FALSE;

   m = inst->menu;

   EINA_LIST_FOREACH(ctxt->devices, iter, dev)
     if (dev->paired)
       {
          mi = e_menu_item_new(m);
          e_menu_item_label_set(mi, _("Paired Devices"));
          e_menu_item_disabled_set(mi, EINA_TRUE);
          ret = EINA_TRUE;
          break;
       }

   EINA_LIST_FOREACH(ctxt->devices, iter, dev)
     if (dev->paired)
       {
#ifdef HAVE_BLUETOOTH
	  Eina_Bool chk;
#endif

	  mi = e_menu_item_new(m);
          e_menu_item_label_set(mi, dev->name);
          e_menu_item_check_set(mi, 1);
          subm = e_menu_new();
          e_menu_post_deactivate_callback_set(subm, _menu_post_deactivate,
                                              inst);
          e_menu_item_submenu_set(mi, subm);
          submi = e_menu_item_new(subm);
          if (dev->connected)
            {
               e_menu_item_toggle_set(mi, 1);
               e_menu_item_label_set(submi, _("Disconnect"));
               e_menu_item_callback_set(submi, _ebluez4_cb_disconnect, dev);
            }
          else
            {
               e_menu_item_toggle_set(mi, 0);
               e_menu_item_label_set(submi, _("Connect"));
               e_menu_item_callback_set(submi, _ebluez4_cb_connect, dev);
            }
          submi = e_menu_item_new(subm);
          e_menu_item_label_set(submi, _("Forget"));
          e_menu_item_callback_set(submi, _ebluez4_cb_forget, dev);

#ifdef HAVE_BLUETOOTH
	  if (autolock_initted)
	    {
	      /* Auto lock when away */
	      submi = e_menu_item_new(subm);
	      e_menu_item_check_set(submi, 1);
	      e_menu_item_label_set(submi, _("Lock on disconnect"));
	      e_menu_item_callback_set(submi, _ebluez4_cb_lock, dev);
	      chk = ebluez4_config->lock_dev_addr && dev->addr &&
		!strcmp(dev->addr, ebluez4_config->lock_dev_addr);
	      e_menu_item_toggle_set(submi, !!chk);

	      submi = e_menu_item_new(subm);
	      e_menu_item_check_set(submi, 1);
	      e_menu_item_label_set(submi, _("Unlock on disconnect"));
	      e_menu_item_callback_set(submi, _ebluez4_cb_unlock, dev);
	      chk = ebluez4_config->unlock_dev_addr && dev->addr &&
		!strcmp(dev->addr, ebluez4_config->unlock_dev_addr);
	      e_menu_item_toggle_set(submi, !!chk);
	    }
#endif
       }

   return ret;
}

static void
_ebluez4_menu_new(Instance *inst)
{
   E_Menu *m;
   E_Menu_Item *mi;

   m = e_menu_new();
   e_menu_post_deactivate_callback_set(m, _menu_post_deactivate, inst);
   e_menu_title_set(m, _("Bluez4"));
   inst->menu = m;

   if (_ebluez4_add_devices(inst))
     {
        mi = e_menu_item_new(m);
        e_menu_item_separator_set(mi, 1);
     }

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Search New Devices"));
   e_menu_item_callback_set(mi, _ebluez4_cb_search, inst);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Adapter Settings"));
   e_menu_item_callback_set(mi, _ebluez4_cb_adap_list, inst);
}

static void
_ebluez4_cb_mouse_down(void *data, Evas *evas EINA_UNUSED,
                       Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = NULL;
   Evas_Event_Mouse_Down *ev = event;
   int x, y;

   if (!(inst = data)) return;
   if (ev->button != 1) return;
   if (!ctxt->adap_obj) return;

   _ebluez4_menu_new(inst);

   e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);
   e_menu_activate_mouse(inst->menu, 
                         e_zone_current_get(), 
                         x + ev->output.x, y + ev->output.y, 1, 1, 
                         E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
   evas_event_feed_mouse_up(inst->gcc->gadcon->evas, ev->button, 
                            EVAS_BUTTON_NONE, ev->timestamp, NULL);

   e_gadcon_locked_set(inst->gcc->gadcon, 1);
}

static void
_ebluez4_set_mod_icon(Evas_Object *base)
{
   char edj_path[4096];
   char *group;

   snprintf(edj_path, sizeof(edj_path), "%s/e-module-bluez4.edj", mod->dir);
   if (ctxt->adap_obj)
     group = "e/modules/bluez4/main";
   else
     group = "e/modules/bluez4/inactive";

   if (!e_theme_edje_object_set(base, "base/theme/modules/bluez4", group))
     edje_object_file_set(base, edj_path, group);
}

/* Gadcon */
static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Instance *inst = NULL;

   inst = E_NEW(Instance, 1);

   inst->o_bluez4 = edje_object_add(gc->evas);
   _ebluez4_set_mod_icon(inst->o_bluez4);

   inst->gcc = e_gadcon_client_new(gc, name, id, style, inst->o_bluez4);
   inst->gcc->data = inst;

   e_gadcon_client_util_menu_attach(inst->gcc);
   evas_object_event_callback_add(inst->o_bluez4, EVAS_CALLBACK_MOUSE_DOWN,
                                  _ebluez4_cb_mouse_down, inst);

   instances = eina_list_append(instances, inst);

   return inst->gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst = NULL;

   if (!(inst = gcc->data)) return;
   instances = eina_list_remove(instances, inst);

   if (inst->o_bluez4)
     {
        evas_object_event_callback_del(inst->o_bluez4, EVAS_CALLBACK_MOUSE_DOWN,
                                       _ebluez4_cb_mouse_down);
        evas_object_del(inst->o_bluez4);
     }

   if (inst->menu) e_menu_deactivate(inst->menu);
   _ebluez4_search_dialog_del(inst);
   _ebluez4_adap_list_dialog_del(inst);

   E_FREE(inst);
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
    snprintf(tmpbuf, sizeof(tmpbuf), "bluez4.%d", eina_list_count(instances));
    return tmpbuf;
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   e_gadcon_client_aspect_set(gcc, 16, 16);
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _("Bluez4");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o = NULL;
   char buf[4096];

   snprintf(buf, sizeof(buf), "%s/e-module-bluez4.edj", mod->dir);

   o = edje_object_add(evas);

   edje_object_file_set(o, buf, "icon");

   return o;
}

static const E_Gadcon_Client_Class _gc_class =
{
   GADCON_CLIENT_CLASS_VERSION, "bluez4",
     {_gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon,
          _gc_id_new, NULL, NULL},
   E_GADCON_CLIENT_STYLE_PLAIN
};

static Eina_Bool
_ebluez_exe_die(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *event_info)
{
   Ecore_Exe_Event_Del *ev = event_info;

   if (ev->exe != autolock_exe)
     return ECORE_CALLBACK_PASS_ON;

   if (!autolock_initted)
     {
        if (ev->exit_code == 0)
	  {
	     autolock_initted = EINA_TRUE;
	  }
     }
   else
     {
        if (e_desklock_state_get()) // Locked state ?
	  {
	     if (!autolock_waiting)
	       {
 		  // Not waiting yet for the auto unlock device to appear before unlock
		  if (ev->exit_code == 0 && ebluez4_config->unlock_dev_addr)
		    {
		       e_desklock_hide();
		    }
	       }
	     else if (ev->exit_code == 1)
	       {
		  // The device just disapeared, now we can wait for it to disapear
		  autolock_waiting = EINA_FALSE;
	       }
	  }
	else
	  {
	    if (!autolock_waiting)
	      {
		 // Not waiting yet for the auto lock device to disappear before locking
		 if (ev->exit_code == 1 && ebluez4_config->lock_dev_addr)
		   {
		      e_desklock_show(EINA_FALSE);
		   }
	      }
	    else if (ev->exit_code == 0)
	      {
		 // The device just appeared, now we can wait for it to disapear
		 autolock_waiting = EINA_FALSE;
	      }
	  }
     }

   if (autolock_initted)
     autolock_poller = ecore_poller_add(ECORE_POLLER_CORE, 32, _ebluez_l2ping_poller, NULL);

   autolock_exe = NULL;

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ebluez_exe_out(void *data EINA_UNUSED, int ev_type EINA_UNUSED,
                void *ev EINA_UNUSED)
{
   /* FIXME: Need experiment, but we should be able to use latency to somehow estimate distance, right ? */
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ebluez_desklock(void *data EINA_UNUSED, int ev_type EINA_UNUSED,
                 void *ev EINA_UNUSED)
{
   if (autolock_exe)
     ecore_exe_kill(autolock_exe);
   autolock_exe = NULL;

   if (!autolock_poller && autolock_initted && (ebluez4_config->lock_dev_addr || ebluez4_config->unlock_dev_addr))
     autolock_poller = ecore_poller_add(ECORE_POLLER_CORE, 32, _ebluez_l2ping_poller, NULL);

   autolock_waiting = EINA_TRUE;

   return ECORE_CALLBACK_PASS_ON;
}

/* Module Functions */
E_API void *
e_modapi_init(E_Module *m)
{
   Eina_Strbuf *buf;

   mod = m;

   conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T           
#undef D           
#define T Config   
#define D conf_edd 
   E_CONFIG_VAL(D, T, lock_dev_addr, STR);
   E_CONFIG_VAL(D, T, unlock_dev_addr, STR);

   ebluez4_config = e_config_domain_load("module.ebluez4", conf_edd);
   if (!ebluez4_config)
     ebluez4_config = E_NEW(Config, 1);

   ebluez4_eldbus_init();

   e_gadcon_provider_register(&_gc_class);

   autolock_die = ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _ebluez_exe_die, NULL);
   autolock_out = ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _ebluez_exe_out, NULL);
   autolock_desklock = ecore_event_handler_add(E_EVENT_DESKLOCK, _ebluez_desklock, NULL);

   buf = eina_strbuf_new();
   eina_strbuf_append_printf(buf, "%s/enlightenment/utils/enlightenment_sys -t l2ping",
			     e_prefix_lib_get());
   autolock_exe = ecore_exe_run(eina_strbuf_string_get(buf), NULL);
   eina_strbuf_free(buf);

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   E_CONFIG_DD_FREE(conf_edd);

   if (autolock_exe) ecore_exe_kill(autolock_exe);
   autolock_exe = NULL;
   E_FREE_FUNC(autolock_poller, ecore_poller_del);

   ecore_event_handler_del(autolock_die);
   ecore_event_handler_del(autolock_out);
   ecore_event_handler_del(autolock_desklock);

   eina_stringshare_del(ebluez4_config->lock_dev_addr);
   eina_stringshare_del(ebluez4_config->unlock_dev_addr);
   free(ebluez4_config);
   ebluez4_config = NULL;

   ebluez4_eldbus_shutdown();
   e_gadcon_provider_unregister(&_gc_class);
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.ebluez4",
			conf_edd, ebluez4_config);
   return 1;
}

/* Public Functions */
void
ebluez4_update_inst(Evas_Object *dest, Eina_List *src, Instance *inst)
{
   Device *dev;
   Evas_Object *o_type;
   Adapter *adap;
   Eina_List *iter;

   e_widget_ilist_freeze(dest);
   e_widget_ilist_clear(dest);

   if (src == ctxt->found_devices)
     {
        EINA_LIST_FOREACH(src, iter, dev)
          if (!dev->paired)
            {
               o_type = e_widget_label_add(evas_object_evas_get(dest),
                                           dev->type);
               e_widget_ilist_append_full(dest, NULL, o_type, dev->name,
                                          _ebluez4_cb_pair, inst, dev->addr);

            }
     }
   else if (src == ctxt->adapters)
     {
        EINA_LIST_FOREACH(src, iter, adap)
          e_widget_ilist_append(dest, NULL, adap->name,
                                _ebluez4_cb_adap_settings, adap, NULL);
     }

   e_widget_ilist_go(dest);
   e_widget_ilist_thaw(dest);
}

void
ebluez4_update_instances(Eina_List *src)
{
   Eina_List *iter;
   Instance *inst;

   EINA_LIST_FOREACH(instances, iter, inst)
     if (src == ctxt->found_devices && inst->found_list)
       ebluez4_update_inst(inst->found_list, src, inst);
     else if (src == ctxt->adapters && inst->adap_list)
       ebluez4_update_inst(inst->adap_list, src, inst);
}

void
ebluez4_update_all_gadgets_visibility(void)
{
   Eina_List *iter;
   Instance *inst;

   if (ctxt->adap_obj)
     EINA_LIST_FOREACH(instances, iter, inst)
       _ebluez4_set_mod_icon(inst->o_bluez4);
   else
     EINA_LIST_FOREACH(instances, iter, inst)
       {
          _ebluez4_set_mod_icon(inst->o_bluez4);
          if (inst->menu) e_menu_deactivate(inst->menu);
          _ebluez4_search_dialog_del(inst);
          _ebluez4_adap_list_dialog_del(inst);
       }
}

void
ebluez4_show_error(const char *err_name, const char *err_msg)
{
   snprintf(tmpbuf, sizeof(tmpbuf), "%s: %s.", err_name, err_msg);
   e_util_dialog_internal(_("An error has ocurred"), tmpbuf);
}

void
ebluez4_adapter_settings_del(E_Dialog *dialog)
{
   Adapter *adap;
   Eina_List *ck_list;

   if (!dialog) return;
   adap = dialog->data;
   ck_list = e_object_data_get(E_OBJECT(dialog));
   eina_list_free(ck_list);
   e_object_del(E_OBJECT(dialog));
   adap->dialog = NULL;
}

void
ebluez4_adapter_properties_update(void *data)
{
   Eina_List *ck_list;
   Evas_Object *ck;
   Adapter *adap = data;

   if (!adap->dialog) return;
   ck_list = e_object_data_get(E_OBJECT(adap->dialog));
   ck = eina_list_nth(ck_list, 0);
   e_widget_check_checked_set(ck, adap->powered);
   ck = eina_list_nth(ck_list, 1);
   e_widget_check_checked_set(ck, adap->visible);
   ck = eina_list_nth(ck_list, 2);
   e_widget_check_checked_set(ck, adap->pairable);
}
