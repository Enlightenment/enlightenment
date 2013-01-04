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
}

static void
_ebluez4_cb_search_dialog_del(E_Win *win)
{
   E_Dialog *dialog = win->data;

   _ebluez4_search_dialog_del(dialog->data);

   ebluez4_stop_discovery();
   DBG("Stopping discovery...");
}

static void
_ebluez4_cb_pair(void *data, E_Dialog *dialog)
{
   Instance *inst = data;
   const char *addr = e_widget_ilist_selected_value_get(inst->found_list);

   if(!addr)
     return;
   ebluez4_pair_with_device(addr);
}

static void
_ebluez4_cb_search(void *data, void *data2 __UNUSED__)
{
   Instance *inst = data;
   E_Container *con;
   E_Dialog *dialog;
   Evas *evas;

   if (inst->search_dialog)
     _ebluez4_cb_search_dialog_del(inst->search_dialog->win);

   con = e_container_current_get(e_manager_current_get());

   dialog = e_dialog_new(con, "Search Dialog", "search");
   e_dialog_title_set(dialog, "Searching for Devices...");
   e_dialog_resizable_set(dialog, EINA_TRUE);
   e_win_delete_callback_set(dialog->win, _ebluez4_cb_search_dialog_del);

   evas = e_win_evas_get(dialog->win);

   inst->found_list = e_widget_ilist_add(evas, 0, 0, NULL);

   e_dialog_content_set(dialog, inst->found_list, 250, 220);
   e_dialog_button_add(dialog, "Pair", NULL, _ebluez4_cb_pair, inst);

   e_dialog_show(dialog);

   dialog->data = inst;
   inst->search_dialog = dialog;

   ebluez4_start_discovery();
   DBG("Starting discovery...");
}

static void
_ebluez4_cb_connect(void *data, void *data2 __UNUSED__)
{
   Instance *inst = data;
   const char *addr = e_widget_ilist_selected_value_get(inst->created_list);

   if(!addr)
     return;
   e_gadcon_popup_hide(inst->popup);
   ebluez4_connect_to_device(addr);
}

static void
_ebluez4_cb_remove(void *data, void *data2 __UNUSED__)
{
   Instance *inst = data;
   const char *addr = e_widget_ilist_selected_value_get(inst->created_list);

   if(!addr)
     return;
   ebluez4_remove_device(addr);
}

static void
_ebluez4_popup_new(Instance *inst)
{
   Evas_Object *list, *tb, *conn_bt, *blank, *adap_bt, *search_bt, *rem_bt;
   Evas_Coord mw, mh;
   Evas *evas;

   EINA_SAFETY_ON_FALSE_RETURN(inst->popup == NULL);

   inst->popup = e_gadcon_popup_new(inst->gcc);
   evas = inst->popup->win->evas;

   list = e_widget_list_add(evas, 0, 0);
   inst->created_list = e_widget_ilist_add(evas, 0, 0, NULL);
   e_widget_list_object_append(list, inst->created_list, 1, 1, 0.5);
   ebluez4_update_instances(ctxt->devices, LIST_TYPE_CREATED_DEVICES);

   conn_bt = e_widget_button_add(evas, "Connect", NULL, _ebluez4_cb_connect,
                                 inst, NULL);
   rem_bt = e_widget_button_add(evas, "Remove", NULL, _ebluez4_cb_remove, inst,
                                NULL);
   search_bt = e_widget_button_add(evas, "Search New Devices", NULL,
                                   _ebluez4_cb_search, inst, NULL);
   adap_bt = e_widget_button_add(evas, "Adapters Settings", NULL, NULL, inst,
                                 NULL);

   blank = e_widget_add(evas);
   e_widget_size_min_set(blank, 0, 10);

   tb = e_widget_table_add(evas, 0);

   e_widget_table_object_append(tb, conn_bt, 0, 0, 1, 1, 1, 1, 1, 1);
   e_widget_table_object_append(tb, rem_bt, 1, 0, 1, 1, 1, 1, 1, 1);
   e_widget_table_object_append(tb, blank, 0, 1, 1, 1, 1, 1, 1, 1);
   e_widget_table_object_append(tb, search_bt, 0, 2, 1, 1, 1, 1, 1, 1);
   e_widget_table_object_append(tb, adap_bt, 1, 2, 1, 1, 1, 1, 1, 1);
   e_widget_list_object_append(list, tb, 1, 0, 0.5);

   e_widget_size_min_get(list, &mw, &mh);
   if (mh < 220)
     mh = 220;
   if (mw < 250)
     mw = 250;
   e_widget_size_min_set(list, mw, mh);

   e_gadcon_popup_content_set(inst->popup, list);
   e_gadcon_popup_show(inst->popup);
}

static void
_ebluez4_popup_del(Instance *inst)
{
   if (!inst->popup) return;
   e_object_del(E_OBJECT(inst->popup));
   inst->popup = NULL;
}

static void
_ebluez4_cb_mouse_down(void *data, Evas *evas, Evas_Object *obj, void *event)
{
   Instance *inst = NULL;
   Evas_Event_Mouse_Down *ev = event;

   if (!(inst = data)) return;
   if (ev->button != 1) return;
   if (!ctxt->adap_obj) return;

   if (!inst->popup)
     _ebluez4_popup_new(inst);
   else if (inst->popup->win->visible)
     e_gadcon_popup_hide(inst->popup);
   else
     e_gadcon_popup_show(inst->popup);
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

   _ebluez4_popup_del(inst);
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
ebluez4_append_to_instances(void *data, int list_type)
{
   Eina_List *iter;
   Instance *inst;
   Device *dev = data;

   if (list_type == LIST_TYPE_FOUND_DEVICES)
     EINA_LIST_FOREACH(instances, iter, inst)
       e_widget_ilist_append(inst->found_list, NULL, dev->name, NULL, NULL,
                             dev->addr);
   else if (list_type == LIST_TYPE_CREATED_DEVICES)
     EINA_LIST_FOREACH(instances, iter, inst)
       if (dev->paired)
         e_widget_ilist_append(inst->created_list, NULL, dev->name, NULL, NULL,
                               dev->addr);
}

void
ebluez4_update_inst(Evas_Object *dest, Eina_List *src, int list_type)
{
   Device *dev;
   Eina_List *iter;

   e_widget_ilist_freeze(dest);
   e_widget_ilist_clear(dest);

   EINA_LIST_FOREACH(src, iter, dev)
     if (dev->paired || list_type == LIST_TYPE_FOUND_DEVICES)
       e_widget_ilist_append(dest, NULL, dev->name, NULL, NULL, dev->addr);

   e_widget_ilist_thaw(dest);
   e_widget_ilist_go(dest);
}

void
ebluez4_update_instances(Eina_List *src, int list_type)
{
   Eina_List *iter;
   Instance *inst;

   if (list_type == LIST_TYPE_FOUND_DEVICES)
     {
        EINA_LIST_FOREACH(instances, iter, inst)
          if (inst->found_list)
            ebluez4_update_inst(inst->found_list, src, list_type);
     }
   else if (list_type == LIST_TYPE_CREATED_DEVICES)
     {
        EINA_LIST_FOREACH(instances, iter, inst)
          if (inst->created_list)
            ebluez4_update_inst(inst->created_list, src, list_type);
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
          e_gadcon_popup_hide(inst->popup);
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
