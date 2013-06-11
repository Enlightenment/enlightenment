#ifdef E_TYPEDEFS

typedef enum _E_Menu_Pop_Direction E_Menu_Pop_Direction;
typedef struct _E_Menu E_Menu;
typedef struct _E_Menu_Item E_Menu_Item;
typedef struct _E_Menu_Category_Callback E_Menu_Category_Callback;

#else
# ifndef E_MENU_H
#  define E_MENU_H

#  define E_MENU_TYPE 0xE0b01009
#  define E_MENU_ITEM_TYPE 0xE0b0100a

enum _E_Menu_Pop_Direction
{
   E_MENU_POP_DIRECTION_NONE,
   E_MENU_POP_DIRECTION_LEFT,
   E_MENU_POP_DIRECTION_RIGHT,
   E_MENU_POP_DIRECTION_UP,
   E_MENU_POP_DIRECTION_DOWN,
   E_MENU_POP_DIRECTION_AUTO,
   E_MENU_POP_DIRECTION_LAST
};

typedef void (*E_Menu_Cb) (void *data, E_Menu *m, E_Menu_Item *mi);

struct _E_Menu
{
   E_Object e_obj_inherit;

   Ecore_Evas *ee;
   Evas *evas;
   Evas_Object *o_bg, *o_con;

   E_Zone *zone;
   E_Menu_Item *parent_item;
   Eina_List *items;

   unsigned int id;
   const char *category;

   struct 
     {
        int x, y, w, h;
        Eina_Bool visible : 1;
     } cur, prev;

   int frozen;
   int cx, cy, cw, ch;

   struct 
     {
        const char *title, *icon_file;
        Evas_Object *icon;
     } header;

   struct 
     {
        void *data;
        void (*func) (void *data, E_Menu *m);
     } pre_activate_cb, post_deactivate_cb;

   Eina_Bool fast_mouse : 1;
   Eina_Bool shaped : 1;
   Eina_Bool realized : 1;
   Eina_Bool active : 1;
   Eina_Bool changed : 1;
   Eina_Bool have_submenu : 1;
   Eina_Bool pending_new_submenu : 1;
};

struct _E_Menu_Item 
{
   E_Object e_obj_inherit;

   E_Menu *menu, *submenu;

   Evas_Object *o_bg, *o_con;
   Evas_Object *o_separator;
   Evas_Object *o_icon_bg, *o_icon;
   Evas_Object *o_lbl, *o_toggle;
   Evas_Object *o_submenu, *o_event;

   int x, y, w, h;
   const char *icon, *icon_key;
   const char *label;

   int lw, lh, iw, ih;
   int sepw, seph, subw, subh;
   int tw, th;

   struct 
     {
        void *data;
        E_Menu_Cb func;
     } cb, realize_cb;

   struct 
     {
        void *data;
        E_Menu_Cb func;
     } submenu_pre_cb, submenu_post_cb;

   Eina_Bool separator : 1;
   Eina_Bool radio : 1;
   Eina_Bool radio_group : 1;
   Eina_Bool check : 1;
   Eina_Bool toggle : 1;
   Eina_Bool changed : 1;
   Eina_Bool active : 1;
   Eina_Bool disabled : 1;
};

struct _E_Menu_Category_Callback
{
   const char *category;
   void *data;
   void (*create) (E_Menu *m, void *category_data, void *data);
   void (*free) (void *data);
};

EINTERN int e_menu_init(void);
EINTERN int e_menu_shutdown(void);

EAPI E_Menu *e_menu_new(void);
EAPI void e_menu_activate_key(E_Menu *m, E_Zone *zone, int x, int y, int w, int h, int dir);
EAPI void e_menu_activate_mouse(E_Menu *m, E_Zone *zone, int x, int y, int w, int h, int dir, unsigned int activate_time);
EAPI void e_menu_activate(E_Menu *m, E_Zone *zone, int x, int y, int w, int h, int dir);
EAPI void e_menu_deactivate(E_Menu *m);
EAPI int e_menu_freeze(E_Menu *m);
EAPI int e_menu_thaw(E_Menu *m);
EAPI void e_menu_title_set(E_Menu *m, char *title);
EAPI void e_menu_icon_file_set(E_Menu *m, char *icon);
EAPI void e_menu_category_set(E_Menu *m, char *category);
EAPI void e_menu_category_data_set(char *category, void *data);
EAPI E_Menu_Category_Callback *e_menu_category_callback_add(char *category, void (*create) (E_Menu *m, void *category_data, void *data), void (free) (void *data), void *data);
EAPI void e_menu_category_callback_del(E_Menu_Category_Callback *cb);
EAPI void e_menu_pre_activate_callback_set(E_Menu *m,  void (*func) (void *data, E_Menu *m), void *data);
EAPI void e_menu_post_deactivate_callback_set(E_Menu *m,  void (*func) (void *data, E_Menu *m), void *data);
EAPI E_Menu *e_menu_root_get(E_Menu *m);
EAPI void e_menu_idler_before(void);

EAPI E_Menu_Item *e_menu_item_new(E_Menu *m);
EAPI E_Menu_Item *e_menu_item_new_relative(E_Menu *m, E_Menu_Item *rel);
EAPI E_Menu_Item *e_menu_item_nth(E_Menu *m, int n);
EAPI int e_menu_item_num_get(const E_Menu_Item *mi);
EAPI void e_menu_item_icon_file_set(E_Menu_Item *mi, const char *icon);
EAPI void e_menu_item_icon_edje_set(E_Menu_Item *mi, const char *icon, const char *key);
EAPI void e_menu_item_label_set(E_Menu_Item *mi, const char *label);
EAPI void e_menu_item_submenu_set(E_Menu_Item *mi, E_Menu *sub);
EAPI void e_menu_item_separator_set(E_Menu_Item *mi, int sep);
EAPI void e_menu_item_check_set(E_Menu_Item *mi, int chk);
EAPI void e_menu_item_radio_set(E_Menu_Item *mi, int rad);
EAPI void e_menu_item_radio_group_set(E_Menu_Item *mi, int radg);
EAPI void e_menu_item_toggle_set(E_Menu_Item *mi, int tog);
EAPI int e_menu_item_toggle_get(E_Menu_Item *mi);
EAPI void e_menu_item_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data);
EAPI void e_menu_item_realize_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data);
EAPI void e_menu_item_submenu_pre_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data);
EAPI void e_menu_item_submenu_post_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data);
EAPI void e_menu_item_drag_callback_set(E_Menu_Item *mi,  E_Menu_Cb func, void *data);
EAPI void e_menu_item_active_set(E_Menu_Item *mi, int active);
EAPI void e_menu_item_disabled_set(E_Menu_Item *mi, int disable);

# endif
#endif
