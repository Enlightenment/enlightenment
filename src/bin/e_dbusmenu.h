#ifndef _E_DBUSMENU_H_
#define _E_DBUSMENU_H_

#include <Eina.h>
#include <EDBus.h>

typedef enum {
   E_DBUSMENU_ITEM_TYPE_STANDARD = 0,
   E_DBUSMENU_ITEM_TYPE_SEPARATOR,
   E_DBUSMENU_ITEM_TYPE_LAST
} E_DBusMenu_Item_Type;

typedef enum {
   MENU_ITEM_TOOGLE_TYPE_NONE = 0,
   MENU_ITEM_TOOGLE_TYPE_CHECKMARK,
   MENU_ITEM_TOOGLE_TYPE_RADIO,
   MENU_ITEM_TOOGLE_TYPE_LAST
} E_DBusMenu_Item_Toogle_Type;

typedef enum {
   E_DBUSMENU_ITEM_DISPOSITION_NORMAL = 0,
   E_DBUSMENU_ITEM_DISPOSITION_INFORMATIVE,
   E_DBUSMENU_ITEM_DISPOSITION_WARNING,
   E_DBUSMENU_ITEM_DISPOSITION_ALERT,
   E_DBUSMENU_ITEM_DISPOSTION_LAST
} E_DBusMenu_Item_Disposition;

typedef struct _E_DBusMenu_Item  E_DBusMenu_Item;
typedef struct _E_DBusMenu_Ctx E_DBusMenu_Ctx;

struct _E_DBusMenu_Item
{
   EINA_INLIST;
   unsigned revision;
   int id;
   const char *label;
   E_DBusMenu_Item_Type type;
   E_DBusMenu_Item_Toogle_Type toogle_type;
   E_DBusMenu_Item_Disposition disposition;
   Eina_Bool toogle_state;
   Eina_Bool enabled;
   Eina_Bool visible;
   Eina_Bool is_submenu;
   const char *icon_name;
   unsigned char *icon_data;
   unsigned icon_data_size;
   Eina_Inlist *sub_items;
   E_DBusMenu_Item *parent;
   E_DBusMenu_Ctx *internal_ctx;
};

typedef void (*E_DBusMenu_Pop_Request_Cb)(void *data, const E_DBusMenu_Item *item);
typedef void (*E_DBusMenu_Itens_Update_Cb)(void *data, E_DBusMenu_Item *new_root_item);

typedef enum
{
   E_DBUSMENU_ITEM_EVENT_CLICKED = 0,
   E_DBUSMENU_ITEM_EVENT_HOVERED,
   E_DBUSMENU_ITEM_EVENT_OPENED,
   E_DBUSMENU_ITEM_EVENT_CLOSED,
   E_DBUSMENU_ITEM_EVENT_LAST
} E_DBus_Menu_Item_Event;

E_DBusMenu_Ctx * e_dbusmenu_load(EDBus_Connection *conn, const char *bus, const char *path, const void *data);
void e_dbusmenu_unload(E_DBusMenu_Ctx *menu_data);
void e_dbusmenu_update_callback_set(E_DBusMenu_Ctx *menu_data, E_DBusMenu_Itens_Update_Cb cb);
void e_dbusmenu_pop_request_callback_set(E_DBusMenu_Ctx *menu_data, E_DBusMenu_Pop_Request_Cb cb);
void e_dbusmenu_event_send(E_DBusMenu_Item *m, E_DBus_Menu_Item_Event event);

#endif
