#include "e_mod_main.h"

typedef enum {
   CATEGORY_UNKNOWN = 0,
   CATEGORY_SYSTEM_SERVICES,
   CATEGORY_LAST
} Category;

typedef enum {
   STATUS_UNKNOWN = 0,
   STATUS_ACTIVE,
   STATUS_PASSIVE,
   STATUS_ATTENTION,
   STATUS_LAST
} Status;

struct _Instance_Notifier_Host
{
   Instance *inst;
   Eina_Inlist *items_list;
   const Evas_Object *box;
   const Evas_Object *edje;
   EDBus_Connection *conn;
   EDBus_Proxy *watcher;
   E_Gadcon *gadcon;
};

typedef struct _Notifier_Item
{
   EINA_INLIST;
   const char *bus_id;
   const char *path;
   EDBus_Proxy *proxy;
   Category category;
   Status status;
   E_DBusMenu_Item *dbus_item;
   const char *id;
   const char *title;
   const char *icon_name;
   const char *attention_icon_name;
   const char *icon_path;
   Evas_Object *icon_object;
   Instance_Notifier_Host *host_inst;
   const char *menu_path;
   E_DBusMenu_Ctx *menu_data;
   Eina_List *signals;
   Eina_Bool in_box;
} Notifier_Item;

typedef void (*E_Notifier_Watcher_Item_Registered_Cb)(void *data, const char *service);
typedef void (*E_Notifier_Watcher_Item_Unregistered_Cb)(void *data, const char *service);

void systray_notifier_update_menu(void *data, E_DBusMenu_Item *new_root_item);
void systray_notifier_item_update(Notifier_Item *item);
void systray_notifier_item_free(Notifier_Item *item);

void systray_notifier_dbus_init(Instance_Notifier_Host *host_inst);
void systray_notifier_dbus_shutdown(Instance_Notifier_Host *host_inst);

void systray_notifier_dbus_watcher_start(EDBus_Connection *connection, E_Notifier_Watcher_Item_Registered_Cb registered, E_Notifier_Watcher_Item_Unregistered_Cb unregistered, const void *data);
void systray_notifier_dbus_watcher_stop(void);
