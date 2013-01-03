#include "e_mod_notifier_host_private.h"

#define WATCHER_BUS "org.kde.StatusNotifierWatcher"
#define WATCHER_PATH "/StatusNotifierWatcher"
#define WATCHER_IFACE "org.kde.StatusNotifierWatcher"

#define ITEM_IFACE "org.kde.StatusNotifierItem"

const char *Category_Names[] = {
   "unknown", "SystemServices", NULL
};

const char *Status_Names[] = {
   "unknown", "Active", "Passive", "NeedsAttention", NULL
};

static const char *box_part_name = "e.dbus_notifier.box";

void
systray_notifier_item_free(Notifier_Item *item)
{
   EDBus_Object *obj;
   EDBus_Signal_Handler *sig;
   evas_object_del(item->icon_object);
   if (item->menu_path)
     {
        e_dbusmenu_unload(item->menu_data);
        //TODO free evas_object of menu
     }
   eina_stringshare_del(item->bus_id);
   eina_stringshare_del(item->path);
   if (item->attention_icon_name)
     eina_stringshare_del(item->attention_icon_name);
   if (item->icon_name)
     eina_stringshare_del(item->icon_name);
   if (item->icon_path)
     eina_stringshare_del(item->icon_path);
   if (item->id)
     eina_stringshare_del(item->id);
   if (item->menu_path)
     eina_stringshare_del(item->menu_path);
   if (item->title)
     eina_stringshare_del(item->title);
   EINA_LIST_FREE(item->signals, sig)
     edbus_signal_handler_del(sig);
   obj = edbus_proxy_object_get(item->proxy);
   edbus_proxy_unref(item->proxy);
   edbus_object_unref(obj);
   item->host_inst->items_list = eina_inlist_remove(item->host_inst->items_list,
                                                    EINA_INLIST_GET(item));
   systray_size_updated(item->host_inst->inst);
   free(item);
}

static void
image_load(const char *name, const char *path, Evas_Object *image)
{
   if (path && strlen(path))
     {
        char buf[1024];
        sprintf(buf, "%s/%s", path, name);
        if (!e_icon_file_set(image, buf))
          e_util_icon_theme_set(image, "dialog-error");
        return;
     }
   if (!e_util_icon_theme_set(image, name))
     e_util_icon_theme_set(image, "dialog-error");
}

void
_clicked_item_cb(void *data __UNUSED__, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Notifier_Item *item = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->button == 1)
     {
        DBG("left %s", item->id);
        return;
     }
}

static void
image_scale(Notifier_Item *item)
{
   Evas_Coord sz;
   switch (systray_gadcon_get(item->host_inst->inst)->orient)
     {
      case E_GADCON_ORIENT_HORIZ:
      case E_GADCON_ORIENT_TOP:
      case E_GADCON_ORIENT_BOTTOM:
      case E_GADCON_ORIENT_CORNER_TL:
      case E_GADCON_ORIENT_CORNER_TR:
      case E_GADCON_ORIENT_CORNER_BL:
      case E_GADCON_ORIENT_CORNER_BR:
        sz = systray_gadcon_get(item->host_inst->inst)->shelf->h;
        break;

      case E_GADCON_ORIENT_VERT:
      case E_GADCON_ORIENT_LEFT:
      case E_GADCON_ORIENT_RIGHT:
      case E_GADCON_ORIENT_CORNER_LT:
      case E_GADCON_ORIENT_CORNER_RT:
      case E_GADCON_ORIENT_CORNER_LB:
      case E_GADCON_ORIENT_CORNER_RB:
      default:
        sz = systray_gadcon_get(item->host_inst->inst)->shelf->w;
     }
   sz = sz - 5;
   evas_object_resize(item->icon_object, sz, sz);
}

void
systray_notifier_item_update(Notifier_Item *item)
{
   if (!item->icon_object)
     {
        item->icon_object = e_icon_add(evas_object_evas_get(item->host_inst->edje));
        EINA_SAFETY_ON_NULL_RETURN(item->icon_object);
        image_scale(item);
        systray_size_updated(item->host_inst->inst);
        evas_object_event_callback_add(item->icon_object, EVAS_CALLBACK_MOUSE_DOWN,
                                       _clicked_item_cb, item);
     }

   switch (item->status)
     {
      case STATUS_ACTIVE:
        {
           image_load(item->icon_name, item->icon_path, item->icon_object);
           if (!item->in_box)
             {
                systray_edje_box_append(item->host_inst->inst, box_part_name,
                                        item->icon_object);
                evas_object_show(item->icon_object);
             }
           item->in_box = EINA_TRUE;
           break;
        }
      case STATUS_PASSIVE:
        {
           if (item->in_box)
             {
                systray_edje_box_remove(item->host_inst->inst, box_part_name,
                                        item->icon_object);
                evas_object_hide(item->icon_object);
             }
           item->in_box = EINA_FALSE;
           break;
        }
      case STATUS_ATTENTION:
        {
           image_load(item->attention_icon_name, item->icon_path,
                      item->icon_object);
           if (!item->in_box)
             {
                systray_edje_box_append(item->host_inst->inst, box_part_name,
                                        item->icon_object);
                evas_object_show(item->icon_object);
             }
           item->in_box = EINA_TRUE;
           break;
        }
      default:
        {
           ERR("Status unexpected.");
           break;
        }
     }
   systray_size_updated(item->host_inst->inst);
}

Instance_Notifier_Host *
systray_notifier_host_new(Instance *inst, E_Gadcon *gadcon)
{
   Instance_Notifier_Host *host_inst = NULL;
   host_inst = calloc(1, sizeof(Instance_Notifier_Host));
   EINA_SAFETY_ON_NULL_RETURN_VAL(host_inst, NULL);
   host_inst->inst = inst;
   host_inst->edje = systray_edje_get(inst);
   host_inst->gadcon = gadcon;
   systray_notifier_dbus_init(host_inst);

   return host_inst;
}

void
systray_notifier_host_free(Instance_Notifier_Host *notifier)
{
   systray_notifier_dbus_shutdown(notifier);
   free(notifier);
}
