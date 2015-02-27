#include "e.h"

static Eina_List *items; // list of E_Config_Panel_Item
static Evas_Object *win; // if a window is opened this one is the opened one ;)

EAPI void
e_config_panel_init()
{
   /* Init standart structure of items*/

   /*Init internal "config modules" )*/
   e_int_config_modules_init();
}

static int
 _insert_sorted_item(const void *data1, const void *data2)
 {
     const E_Config_Panel_Item *part1, *part2;

     part1 = data1;
     part2 = data2;

     if (part1->priority > part2->priority)
       return -1;
     else
       return 1;
 }

EAPI void
e_config_panel_item_add(const char *path, const char *icon, const char *label, const char *help, int priority, const char *keyword)
{
   E_Config_Panel_Item *it;

   it = E_NEW(E_Config_Panel_Item, 1);

   it->path = eina_stringshare_add(path);
   it->icon = eina_stringshare_add(icon);
   it->label = eina_stringshare_add(label);
   it->help = eina_stringshare_add(help);
   it->keywords = eina_stringshare_add(keyword);
   it->priority = priority;

   items = eina_list_sorted_insert(items, _insert_sorted_item, it);
}

EAPI void
e_config_panel_item_del(const char *path)
{
  Eina_List *node;
  E_Config_Panel_Item *it;

  EINA_LIST_FOREACH(items, node, it)
    {
       if (!!strcmp(it->path, path)) continue;

       items = eina_list_remove_list(items, node);

       eina_stringshare_del(it->path);
       eina_stringshare_del(it->icon);
       eina_stringshare_del(it->label);
       eina_stringshare_del(it->help);
       eina_stringshare_del(it->keywords);

       free(it);
       return;
    }
}

static E_Config_Panel_Item*
_item_search(const char *path)
{
   Eina_List *node;
   E_Config_Panel_Item *val = NULL;

   EINA_LIST_FOREACH(items, node, val)
     {
        if (!!strcmp(val->path, path)) continue;
        return val;
     }

   ERR("Item not found");
   return NULL;
}

static int
 _insert_sorted_part(const void *data1, const void *data2)
 {
     const E_Config_Panel_Part *part1, *part2;

     part1 = data1;
     part2 = data2;

     if (part1->priority > part2->priority)
       return -1;
     else
       return 1;
 }

/*
 * If you pass NULL as free_cb, a simple free will be called on the result of create_cb (if it isnt NULL)
 *
 */

EAPI void
e_config_panel_part_add(const char *path, const char *part_name, const char *title, const char *help,
                                  int priority, const char *keywords,
                                  E_Config_Panel_Create_Cb create_cb,
                                  E_Config_Panel_Apply_Cb apply_cb,
                                  E_Config_Panel_Data_Create_Cb data_cb,
                                  E_Config_Panel_Data_Free_Cb free_cb,
                                  void *data)
{
   E_Config_Panel_Part *part = NULL;
   E_Config_Panel_Item *item = NULL;

   item = _item_search(path);

   if (!item)
     return;

   part = E_NEW(E_Config_Panel_Part, 1);

   part->name = eina_stringshare_add(part_name);
   part->title = eina_stringshare_add(title);
   part->help = eina_stringshare_add(help);
   part->keywords = eina_stringshare_add(keywords);

   part->priority = priority;
   part->create_func = create_cb;
   part->apply_func = apply_cb;
   part->data_func = data_cb;
   part->free_func = free_cb;
   part->data = data;

   item->parts = eina_list_sorted_insert(item->parts, _insert_sorted_part, part);
}

EAPI void
e_config_panel_part_changed_set(const char *path, const char *part, Eina_Bool val)
{
   E_Config_Panel_Item *item;
   Eina_List *node;
   E_Config_Panel_Part *ppart;

   item = _item_search(path);

   if (!item) return;

   elm_settingspane_item_changed_set(item->item, val);

   EINA_LIST_FOREACH(item->parts, node, ppart)
     {
        if (!!strcmp(ppart->name, part)) continue;
        ppart->changed = val;
        return;
     }
}

EAPI void
e_config_panel_part_del(const char *path, const char *part)
{
   E_Config_Panel_Item *item = NULL;
   Eina_List *node;
   E_Config_Panel_Part *ppart = NULL;

   item = _item_search(path);

   if (!item)
     return;

   EINA_LIST_FOREACH(item->parts, node, ppart)
     {
        if (!!strcmp(ppart->name, part)) continue;
        item->parts = eina_list_remove_list(item->parts, node);

        eina_stringshare_del(ppart->name);
        eina_stringshare_del(ppart->title);
        eina_stringshare_del(ppart->help);
        eina_stringshare_del(ppart->keywords);

        free(ppart);
     }
}

static void
string_path_item_split(const char *str, char **path, const char **item)
{
   int len = strlen(str);
   int i = 0;
   char buf[PATH_MAX];

   if (len < 2)
     return;

   for(i = len -2 ; i > 0; i--)
     {
        if (str[i] == '/')
          break;
     }
   if (i == 0)
     *path = NULL;
   else
     {
         *path = calloc(i +1, sizeof(char));
         memcpy(*path, str, i);
         (*path)[i] = '\0';
     }

   *item = str+i+1;
}

static Evas_Object*
settingswidget_part_gen(Evas_Object *par, Evas_Object *content, const char *title, const char *help)
{
   Evas_Object *frame, *pane, *table, *lb, *ic, *bx;

   frame = elm_frame_add(par);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(frame, title);
   evas_object_show(frame);

   table = elm_table_add(par);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_content_set(frame, table);

   bx = elm_box_add(frame);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_box_horizontal_set(bx, EINA_TRUE);
   evas_object_show(bx);

   ic = elm_icon_add(bx);
   evas_object_size_hint_align_set(ic, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(ic, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_icon_standard_set(ic, "system-help");
   elm_box_pack_end(bx, ic);
   evas_object_show(ic);

   lb = elm_label_add(bx);
   elm_label_line_wrap_set(lb, ELM_WRAP_WORD);
   evas_object_size_hint_align_set(lb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(lb, help);
   elm_box_pack_end(bx, lb);
   evas_object_show(lb);

   pane = elm_panel_add(frame);
   evas_object_size_hint_align_set(pane, EVAS_HINT_FILL, 0.0);
   evas_object_size_hint_weight_set(pane, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_panel_orient_set(pane, ELM_PANEL_ORIENT_TOP);
   elm_panel_hidden_set(pane, EINA_TRUE);
   elm_object_content_set(pane, bx);
   evas_object_show(pane);

   elm_table_pack(table, content, 0, 0, 1, 1);
   elm_table_pack(table, pane, 0, 0, 1, 1);
   return frame;
}

static Evas_Object*
content_cb(Evas_Object *par, Eo *item, void *data)
{
   E_Config_Panel_Item *it;
   E_Config_Panel_Part *part;
   Eina_List *node;
   Evas_Object *box, *res;
   Eina_Bool single = EINA_FALSE;

   it = data;
   box = elm_box_add(par);
   evas_object_size_hint_align_set(box, 0.5, 0.0);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   if (eina_list_count(it->parts) == 1)
     single = EINA_TRUE;

   EINA_LIST_FOREACH(it->parts, node, part)
     {
        Evas_Object *content, *result;
        if (part->data_func)
          part->cbdata = part->data_func(it->path, part->name, part->data);
        if (part->create_func) //WTF - what does a part should do if there is nor create_func ?
          part->tmp = content = part->create_func(it->path, part->name, par, part->cbdata, part->data);
        part->realized = EINA_TRUE;

        evas_object_show(content);
        if (!single)
          result = settingswidget_part_gen(par, content, it->label, it->help);
        else
          result = content;
        elm_box_pack_end(box, result);
     }

   res = elm_scroller_add(par);
   evas_object_size_hint_weight_set(res, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_content_set(res, box);
   evas_object_show(res);

   return res;
}

static void
reset_cb(void *data, Evas_Object *obj, void *event_info)
{
   E_Config_Panel_Item *it = data;
   elm_settingspane_item_recreate(it->item);
}

static void
apply_cb(void *data, Evas_Object *obj, void *event_info)
{
   E_Config_Panel_Item *it = data;
   E_Config_Panel_Part *part;
   Eina_List *node;

   EINA_LIST_FOREACH(it->parts, node, part)
     {
        part->apply_func(it->path, part->name, part->tmp, part->cbdata, part->data);
     }
}

static void
settingswidget_fill(Evas_Object *obj)
{
   Eina_Hash *table;
   Eina_List *node;
   E_Config_Panel_Item *it;

   table =  eina_hash_string_superfast_new(NULL);
   EINA_LIST_FOREACH(items, node, it)
     {
        const char *file;
        char *path;
        char buf[PATH_MAX], tmp[PATH_MAX];

        string_path_item_split(it->path, &path, &file);

        Elm_Settingspane_Item *item = NULL;

        if (path)
          {
             item = eina_hash_find(table, path);
             if (!item)
               {
                 ERR("This is bad, dangling category %s", it->path);
                 continue;
               }
          }

        it->item = item = elm_settingspane_item_append(obj, it, it->label, it->help,
                                                       NULL, it->icon, item);
        snprintf(buf, sizeof(buf), "%s", it->keywords);

        if (it->parts)
          {
            elm_settingspane_item_attach_panel(item, content_cb, reset_cb, apply_cb);


            /* assembling the keywords from every part and every item */
            {
              Eina_List *node, *node2;
              E_Config_Panel_Part *p;

              EINA_LIST_FOREACH(it->parts, node2, p)
                {
                   /* attach keywords */
                   strcpy(tmp, buf);
                   snprintf(buf, sizeof(buf), "%s,%s", tmp, p->keywords);
                }
            }
            elm_settingspane_item_keywords_set(item, eina_stringshare_add(buf));
          }
        else
          eina_hash_add(table, it->path, item);

    }
  eina_hash_free(table);
}

static void
_e_config_panel_close(void *data, Evas_Object *obj, void *event)
{
   Eina_List *node, *node2;
   E_Config_Panel_Item *it;
   E_Config_Panel_Part *p;

   if (!win) return; /* check is needed because close cb is called twice when close button is doubleclicked */

   EINA_LIST_FOREACH(items, node, it)
     {
        EINA_LIST_FOREACH(it->parts, node2, p)
          {
            if (!p->realized) continue;
            if (p->free_func)
              p->free_func(it->path, p->name, p->cbdata, p->data);
            else if (p->cbdata)
              free(p->cbdata);
            p->realized = EINA_FALSE;
            p->cbdata = NULL;
          }
     }

  win = NULL;
}

EAPI void
e_config_panel_show(const char *itemname)
{
   Evas_Object *settings;
   E_Config_Panel_Item *item;

   if (win)
     {
        elm_win_raise(win);
        elm_win_urgent_set(win, EINA_TRUE);
        return;
     }

   win = elm_win_util_standard_add("Settings", "E - Settings");
   evas_object_smart_callback_add(win, "delete,request", _e_config_panel_close, NULL);

   settings = elm_settingspane_add(win);
   evas_object_size_hint_weight_set(settings, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(settings, EVAS_HINT_FILL, EVAS_HINT_FILL);
   settingswidget_fill(settings);
   evas_object_show(settings);
   elm_win_resize_object_add(win, settings);

   evas_object_resize(win, 200, 200);
   evas_object_show(win);

   if (!itemname) return;

   item = _item_search(itemname);

   if (!item) return;

   elm_settingspane_item_focus(item->item);
}
