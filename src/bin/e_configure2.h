#ifndef E_CONFIGURE2_H
#define E_CONFIGURE2_H

/**
 * How does this work ?:
 * An Item is a "voice" in the panel tree menu
 * A Panel is the whole page associated with a given Item
 * A Part is single "piece" of a page
 *
 * Example:
 * Item: "Look"
 * Item: "Look/Theme"
 * Part: "Look/Theme" "gtk_theme"
 * Part: "Look/Theme" "icon_theme"
 *
 * TODO: need_save callback?
 *
 */

typedef Evas_Object* (*E_Config_Panel_Create_Cb)(const char *path, const char *part, Evas_Object *parent, void *cbdata, void *data);
typedef Eina_Bool (*E_Config_Panel_Apply_Cb)(const char *path, const char *part, Evas_Object *obj, void *cbdata, void *data);
typedef void* (*E_Config_Panel_Data_Create_Cb)(const char *path, const char *part, void *data);
typedef void (*E_Config_Panel_Data_Free_Cb)(const char *path,const char *part, void *cbdata, void *data);

typedef struct
{
   const char *path;
   const char *icon;
   const char *label;
   const char *help;
   const char *keywords;
   int priority;

   Eina_List *parts;
   Elm_Settingspane_Item *item; //< if the ui is opened this is a item from the settingspane
} E_Config_Panel_Item;

typedef struct
{
   const char *name;
   const char *title;
   const char *help;
   const char *keywords;
   int priority;

   E_Config_Panel_Create_Cb create_func;
   E_Config_Panel_Apply_Cb apply_func;
   E_Config_Panel_Data_Create_Cb data_func;
   E_Config_Panel_Data_Free_Cb free_func;
   void *data;
   void *cbdata;

   Eina_Bool changed;
   Eina_Bool realized;
   Evas_Object *tmp;
} E_Config_Panel_Part;
/*
 * Init the config panel
 */

EAPI void e_config_panel_init();

/*
 * Will add a item to the configuration tree
 *
 * @param path
 *    Should be in the form:
 *    /someitem1/someitem2/someitem3/path
 *    The parent items should exists, otherwise they will not be displayed.
 *
 * @param icon
 *    Name of the icon which is displayed
 *
 * @param label
 *    Name of the item which will be displayed
 *
 * @param help
 *    The text which is displayed in the help section
 *
 * @param priority
 *     The higher the priority is the higher is the item in the list of items in a parent.
 *
 * @param search
 *     A list of eina_stringshare´s you want to add as search-key-words for the search from all items
 */
EAPI void e_config_panel_item_add(const char *path, const char *icon, const char *label, const char *help, int priority, const char *keywords);

/*
 * Delete the given item
 *
 * @param path
 *    The path you have added the item
 */
EAPI void e_config_panel_item_del(const char *path);


EAPI void e_config_panel_part_add(const char *path, const char *part, const char *title, const char *help,
                                  int priority, const char *keywords,
                                  E_Config_Panel_Create_Cb create_cb,
                                  E_Config_Panel_Apply_Cb apply_cb,
                                  E_Config_Panel_Data_Create_Cb data_cb,
                                  E_Config_Panel_Data_Free_Cb free_cb,
                                  void *data);

/*
 * Set if the part has unsaved changes (= is changed) or not
 *
 * @param path
 *    The path where to find the part
 *
 * @param part
 *    The part name of the part to change the auto changed value
 *
 * @param val
 *    The value, - EINA_FALSE = nothing changed, EINA_TRUE = something changed!!
 */
EAPI void e_config_panel_part_changed_set(const char *path, const char *part, Eina_Bool val);

/*
 * Delete the given part
 *
 * @param path
 *    The path of the item you want to delete from the part
 *
 * @param part
 *    The name of the part you want to delete
 */

EAPI void e_config_panel_part_del(const char *path, const char *part);

EAPI void e_config_panel_show(const char *item); // NULL to start in the main menu
#endif