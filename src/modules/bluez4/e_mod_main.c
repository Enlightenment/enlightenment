#include <e.h>
#include <EDBus.h>
#include "e_mod_main.h"
#include "ebluez4.h"

/* Local Variables */
static Eina_List *instances = NULL;
static E_Module *mod = NULL;
static char tmpbuf[1024];

EAPI E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Bluez4"};

/* Local Functions */
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
_ebluez4_cb_search_dialog_del(E_Win *win)
{
   E_Dialog *dialog = win->data;
   _ebluez4_search_dialog_del(dialog->data);
}

static void
_ebluez4_cb_paired(void *data, Eina_Bool success, const char *err_msg)
{
   Instance *inst = data;
   if (success)
     _ebluez4_search_dialog_del(inst);
   else
     ebluez4_show_error("Bluez Error", err_msg);
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
_ebluez4_cb_search(void *data, E_Menu *m, E_Menu_Item *mi)
{
   Instance *inst = data;
   E_Container *con;
   E_Dialog *dialog;
   Evas *evas;

   if (inst->search_dialog)
     _ebluez4_search_dialog_del(inst);

   con = e_container_current_get(e_manager_current_get());

   dialog = e_dialog_new(con, "Search Dialog", "search");
   e_dialog_title_set(dialog, "Searching for Devices...");
   e_dialog_resizable_set(dialog, EINA_TRUE);
   e_win_delete_callback_set(dialog->win, _ebluez4_cb_search_dialog_del);

   evas = e_win_evas_get(dialog->win);

   inst->found_list = e_widget_ilist_add(evas, 0, 0, NULL);

   e_dialog_content_set(dialog, inst->found_list, 250, 220);

   e_dialog_show(dialog);

   dialog->data = inst;
   inst->search_dialog = dialog;

   ebluez4_start_discovery();
   DBG("Starting discovery...");
}

static void
_ebluez4_cb_connect(void *data, E_Menu *m, E_Menu_Item *mi)
{
   ebluez4_connect_to_device(data);
}

static void
_ebluez4_cb_disconnect(void *data, E_Menu *m, E_Menu_Item *mi)
{
   ebluez4_disconnect_device(data);
}

static void
_ebluez4_cb_forget(void *data, E_Menu *m, E_Menu_Item *mi)
{
   Device *dev = data;
   ebluez4_remove_device(dev->obj);
}

static void
_menu_post_deactivate(void *data __UNUSED__, E_Menu *m)
{
   Eina_List *iter;
   E_Menu_Item *mi;

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
          e_menu_item_label_set(mi, "Paired Devices");
          e_menu_item_disabled_set(mi, EINA_TRUE);
          ret = EINA_TRUE;
          break;
       }

   EINA_LIST_FOREACH(ctxt->devices, iter, dev)
     if (dev->paired)
       {
          mi = e_menu_item_new(m);
          e_menu_item_label_set(mi, dev->name);
          e_menu_item_check_set(mi, 1);
          subm = e_menu_new();
          e_menu_post_deactivate_callback_set(subm, _menu_post_deactivate,
                                              NULL);
          e_menu_item_submenu_set(mi, subm);
          submi = e_menu_item_new(subm);
          if (dev->connected)
            {
               e_menu_item_toggle_set(mi, 1);
               e_menu_item_label_set(submi, "Disconnect");
               e_menu_item_callback_set(submi, _ebluez4_cb_disconnect, dev);
            }
          else
            {
               e_menu_item_toggle_set(mi, 0);
               e_menu_item_label_set(submi, "Connect");
               e_menu_item_callback_set(submi, _ebluez4_cb_connect, dev);
            }
          submi = e_menu_item_new(subm);
          e_menu_item_label_set(submi, "Forget");
          e_menu_item_callback_set(submi, _ebluez4_cb_forget, dev);
       }

   return ret;
}

static void
_ebluez4_menu_new(Instance *inst)
{
   E_Menu *m;
   E_Menu_Item *mi;
   E_Zone *zone;
   int x, y;

   m = e_menu_new();
   e_menu_post_deactivate_callback_set(m, _menu_post_deactivate, NULL);
   e_menu_title_set(m, "Bluez4");
   inst->menu = m;

   if (_ebluez4_add_devices(inst))
     {
        mi = e_menu_item_new(m);
        e_menu_item_separator_set(mi, 1);
     }

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, "Search New Devices");
   e_menu_item_callback_set(mi, _ebluez4_cb_search, inst);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, "Adapter Settings");

   zone = e_util_zone_current_get(e_manager_current_get());
   ecore_x_pointer_xy_get(zone->container->win, &x, &y);
   e_menu_activate_mouse(m, zone, x, y, 1, 1, E_MENU_POP_DIRECTION_DOWN,
                         ecore_x_current_time_get());
}

static void
_ebluez4_cb_mouse_down(void *data, Evas *evas, Evas_Object *obj, void *event)
{
   Instance *inst = NULL;
   Evas_Event_Mouse_Down *ev = event;

   if (!(inst = data)) return;
   if (ev->button != 1) return;
   if (!ctxt->adap_obj) return;

   _ebluez4_menu_new(inst);
}

static void
_ebluez4_set_mod_icon(Evas_Object *base)
{
   char edj_path[4096];
   char *group;

   snprintf(edj_path, sizeof(edj_path), "%s/e-module-bluez4.edj", mod->dir);
   if (ctxt->adap_obj)
     group = "modules/bluez4/main";
   else
     group = "modules/bluez4/inactive";

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

   e_menu_deactivate(inst->menu);
   _ebluez4_search_dialog_del(inst);

   E_FREE(inst);
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class)
{
    snprintf(tmpbuf, sizeof(tmpbuf), "bluez4.%d", eina_list_count(instances));
    return tmpbuf;
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   e_gadcon_client_aspect_set(gcc, 16, 16);
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class)
{
   return "Bluez4";
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas)
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

/* Module Functions */
EAPI void *
e_modapi_init(E_Module *m)
{
   mod = m;

   ebluez4_edbus_init();

   e_gadcon_provider_register(&_gc_class);

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m)
{
   ebluez4_edbus_shutdown();
   return 1;
}

EAPI int
e_modapi_save(E_Module *m)
{
   return 1;
}

/* Public Functions */
void
ebluez4_update_inst(Evas_Object *dest, Eina_List *src, Instance *inst)
{
   Device *dev;
   Eina_List *iter;

   e_widget_ilist_freeze(dest);
   e_widget_ilist_clear(dest);

   EINA_LIST_FOREACH(src, iter, dev)
     if (src == ctxt->found_devices && !dev->paired)
       e_widget_ilist_append(dest, NULL, dev->name, _ebluez4_cb_pair, inst,
                             dev->addr);

   e_widget_ilist_thaw(dest);
   e_widget_ilist_go(dest);
}

void
ebluez4_update_instances(Eina_List *src)
{
   Eina_List *iter;
   Instance *inst;

   if (src == ctxt->found_devices)
     {
        EINA_LIST_FOREACH(instances, iter, inst)
          if (inst->found_list)
            ebluez4_update_inst(inst->found_list, src, inst);
     }
}

void
ebluez4_update_all_gadgets_visibility()
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
          e_menu_deactivate(inst->menu);
          _ebluez4_search_dialog_del(inst);
       }
}

void
ebluez4_show_error(const char *err_name, const char *err_msg)
{
   E_Container *con;
   E_Dialog *dialog;
   Evas *evas;
   Evas_Object *box, *label;
   int mw, mh;

   con = e_container_current_get(e_manager_current_get());
   dialog = e_dialog_new(con, "Error Dialog", "error");
   e_dialog_title_set(dialog, "An error has ocurred");
   snprintf(tmpbuf, sizeof(tmpbuf), "%s: %s.", err_name, err_msg);
   evas = e_win_evas_get(dialog->win);
   label = e_widget_label_add(evas, tmpbuf);
   box = e_box_add(evas);
   e_box_pack_start(box, label);
   e_widget_size_min_get(label, &mw, &mh);
   e_dialog_content_set(dialog, box, mw+30, mh+30);
   e_dialog_show(dialog);
   e_dialog_border_icon_set(dialog, "dialog-error");
}
