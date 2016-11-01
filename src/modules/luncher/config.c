#include "luncher.h"

static void
_config_close(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   luncher_config->list = NULL;
   evas_object_del(luncher_config->config_dialog);
   luncher_config->config_dialog = NULL;
   e_config_save_queue();
}

static void
_config_source_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   const char *dir = elm_object_item_text_get(elm_list_selected_item_get(luncher_config->list));

   if (!strcmp(inst->cfg->dir, dir))
     return;
   if (inst->cfg->dir) eina_stringshare_del(inst->cfg->dir);
   inst->cfg->dir = NULL;
   if (dir) inst->cfg->dir = eina_stringshare_ref(dir); 
   bar_reorder(inst);
}

static void
_config_populate_order_list(Evas_Object *list, Instance *inst)
{
   Eina_List *dirs;
   char buf[4096], *file;
   size_t len;
   Elm_Object_Item *it;

   elm_list_clear(list);

   len = e_user_dir_concat_static(buf, "applications/bar");
   if (len + 2 >= sizeof(buf)) return;
   dirs = ecore_file_ls(buf);

   buf[len] = '/';
   len++;

   EINA_LIST_FREE(dirs, file)
     {
        if (file[0] == '.') continue;
        if (eina_strlcpy(buf + len, file, sizeof(buf) - len) >= sizeof(buf) - len)
          continue;
        if (ecore_file_is_dir(buf))
          {
             it = elm_list_item_append(list, file, NULL, NULL, _config_source_changed, inst);
             if ((inst->cfg->dir) && (!strcmp(inst->cfg->dir, file)))
               elm_list_item_selected_set(it, EINA_TRUE);
          }
        free(file);
     }
   elm_list_go(list);
}

static void
_config_source_cancel(void *data)
{
   Instance *inst = data;

   config_luncher(e_zone_current_get(), inst, luncher_config->bar);
}

static void
_config_source_added(void *data, char *text)
{
   Instance *inst = data;
   char buf[4096];
   char tmp[4096] = {0};
   FILE *f;
   size_t len;

   len = e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s", text);
   if (len + sizeof("/.order") >= sizeof(buf)) return;
   while (!ecore_file_exists(buf))
     {
        ecore_file_mkdir(buf);
        memcpy(buf + len, "/.order", sizeof("/.order"));
        e_user_dir_concat_static(tmp, "applications/bar/default/.order");
        if (ecore_file_cp(tmp, buf)) break;
        f = fopen(buf, "w");
        if (!f) break;
        /* Populate this .order file with some defaults */
        snprintf(tmp, sizeof(tmp),
                 "terminology.desktop\n"
                 "sylpheed.desktop\n"
                 "firefox.desktop\n"
                 "openoffice.desktop\n"
                 "xchat.desktop\n"
                 "gimp.desktop\n");
        fwrite(tmp, sizeof(char), strlen(tmp), f);
        fclose(f);
        break;
     }
   config_luncher(e_zone_current_get(), inst, luncher_config->bar);
} 

static void
_config_source_deleted(void *data)
{
   char buf[4096];
   Instance *inst = data;

   if (e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s", inst->cfg->dir) >= sizeof(buf))
     return;
   if (ecore_file_is_dir(buf))
     ecore_file_recursive_rm(buf);
   e_object_del(E_OBJECT(inst->order));
   config_luncher(e_zone_current_get(), inst, luncher_config->bar);
}

static void
_config_source_add(void *data, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   evas_object_del(luncher_config->config_dialog);
   e_entry_dialog_show(_("Create new Luncher source"), "enlightenment",
                       _("Enter a name for this new source:"), "", NULL, NULL,
                       _config_source_added, _config_source_cancel, inst);
}

static void
_config_source_del(void *data, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   char buf[4096];

   evas_object_del(luncher_config->config_dialog);
   snprintf(buf, sizeof(buf), _("You requested to delete \"%s\".<br><br>"
                                "Are you sure you want to delete this bar source?"),
            inst->cfg->dir);
   e_confirm_dialog_show(_("Are you sure you want to delete this bar source?"),
                                  "application-exit", buf, _("Delete"), _("Keep"),
                                  _config_source_deleted, _config_source_cancel, inst, inst,
                                  NULL, NULL);
}

static void
_config_contents(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Object *popup = evas_object_data_get(obj, "popup");

   if (e_configure_registry_exists("applications/ibar_applications"))
     {
        char path[PATH_MAX];
        e_user_dir_snprintf(path, sizeof(path), "applications/bar/%s/.order",
                       inst->cfg->dir);
        e_configure_registry_call("internal/ibar_other", NULL, path);
        evas_object_del(popup);
     }

}

static void
_config_create_icon(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Object *popup = evas_object_data_get(obj, "popup");

   if (e_configure_registry_exists("applications/new_application"))
     {
        char path[PATH_MAX];
        e_user_dir_snprintf(path, sizeof(path), "applications/bar/%s/.order",
                       inst->cfg->dir);
        e_configure_registry_call("applications/new_application", NULL, path);
        evas_object_del(popup);
     }
}

static void
_icon_theme_file_set(Evas_Object *img, const char *icon)
{
   const char *path = NULL, *k = NULL;
   int len = 0;

   if (!icon)
     path = NULL;
   else if (strncmp(icon, "/", 1) && !ecore_file_exists(icon))
     {
        path = efreet_icon_path_find(e_config->icon_theme, icon, 48);
        if (!path)
          {
             if (e_util_strcmp(e_config->icon_theme, "hicolor"))
             path = efreet_icon_path_find("hicolor", icon, 48);
          }
     }
   else if (ecore_file_exists(icon))
     {
        path = icon;
     }
   if (!path)
     {
        char buf[4096];
        snprintf(buf, sizeof(buf), "e/icons/%s", icon);
        if (eina_list_count(e_theme_collection_items_find("base/theme/icons", buf)))
          {
             path = e_theme_edje_file_get("base/theme/icons", buf);
             k = buf;
          }
        else
          {
             path = e_theme_edje_file_get("base/theme/icons", "e/icons/unknown");
             k =  "e/icons/unknown";
          }
     }
   if (path && icon)
     {
        len = strlen(icon);
        if ((len > 4) && (!strcasecmp(icon + len - 4, ".edj")))
        k = "icon";
     }
   elm_image_file_set(img, path, k);
}

EINTERN Evas_Object *
config_luncher(E_Zone *zone, Instance *inst, Eina_Bool bar)
{
   Evas_Object *popup, *tb, *lbl, *fr, *box, *list;
   Evas_Object *butbox, *sep, *hbox, *img, *but;

   luncher_config->bar = bar;

   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   E_EXPAND(tb);
   evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   lbl = elm_label_add(tb);
   evas_object_size_hint_weight_set(lbl, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lbl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(lbl, "marker");
   evas_object_show(lbl);
   elm_object_text_set(lbl, _("Luncher Configuration"));
   elm_table_pack(tb, lbl, 0, 0, 1, 1);

   box = elm_box_add(tb);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb, box, 0, 1, 1, 1);
   evas_object_show(box);

   fr = elm_frame_add(box);
   elm_object_text_set(fr, _("Icon Order"));
   E_EXPAND(fr);
   evas_object_size_hint_align_set(fr, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, fr);
   evas_object_show(fr);

   box = elm_box_add(fr);
   elm_box_horizontal_set(box, EINA_FALSE);
   E_EXPAND(box);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   hbox = elm_box_add(box);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   elm_box_homogeneous_set(hbox, EINA_FALSE);
   evas_object_size_hint_expand_set(hbox, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, hbox);
   evas_object_show(hbox);

   list = elm_list_add(hbox);
   E_ALIGN(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_box_pack_end(hbox, list);
   elm_list_select_mode_set(list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_scroller_content_min_limit(list, 1, 1);
   evas_object_show(list);
   luncher_config->list = list;
   _config_populate_order_list(list, inst);

   butbox = elm_box_add(hbox);
   elm_box_horizontal_set(butbox, EINA_FALSE);
   E_EXPAND(butbox);
   evas_object_size_hint_align_set(butbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(hbox, butbox);
   evas_object_show(butbox);

   sep = elm_separator_add(box);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   elm_box_pack_end(box, sep);
   evas_object_show(sep);

   img = elm_icon_add(butbox);
   evas_object_size_hint_aspect_set(img, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   _icon_theme_file_set(img, "list-add");
   evas_object_show(img);

   but = elm_button_add(butbox);
   elm_object_part_content_set(but, "icon", img);
   elm_object_text_set(but, _("Add"));
   E_EXPAND(but);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(but, "popup", popup);
   evas_object_smart_callback_add(but, "clicked", _config_source_add, inst);
   elm_box_pack_end(butbox, but);
   evas_object_show(but);

   img = elm_icon_add(butbox);
   evas_object_size_hint_aspect_set(img, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   _icon_theme_file_set(img, "list-remove");
   evas_object_show(img);

   but = elm_button_add(butbox);
   elm_object_part_content_set(but, "icon", img);
   elm_object_text_set(but, _("Delete"));
   E_EXPAND(but);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(but, "popup", popup);
   evas_object_smart_callback_add(but, "clicked", _config_source_del, inst);
   elm_box_pack_end(butbox, but);
   evas_object_show(but);

   hbox = elm_box_add(box);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   elm_box_homogeneous_set(hbox, EINA_TRUE);
   E_EXPAND(box);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, hbox);
   evas_object_show(hbox);

   img = elm_icon_add(hbox);
   evas_object_size_hint_aspect_set(img, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   _icon_theme_file_set(img, "document-new");
   evas_object_show(img);

   but = elm_button_add(hbox);
   elm_object_part_content_set(but, "icon", img);
   elm_object_text_set(but, _("Create New Icon"));
   E_EXPAND(but);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(but, "popup", popup);
   evas_object_smart_callback_add(but, "clicked", _config_create_icon, inst);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   img = elm_icon_add(hbox);
   evas_object_size_hint_aspect_set(img, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   _icon_theme_file_set(img, "list-add");
   evas_object_show(img);

   but = elm_button_add(hbox);
   elm_object_part_content_set(but, "icon", img);
   elm_object_text_set(but, _("Contents"));
   E_EXPAND(but);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(but, "popup", popup);
   evas_object_smart_callback_add(but, "clicked", _config_contents, inst);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   elm_object_content_set(fr, box);

   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center_on_zone(popup, zone);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, NULL);

   return luncher_config->config_dialog = popup;
}

