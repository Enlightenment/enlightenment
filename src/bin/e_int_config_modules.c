#include "e.h"
#include <assert.h>

typedef struct _CFModule CFModule;
typedef struct _CFType   CFType;
typedef struct _CFTypes  CFTypes;

struct _CFModule
{
   const char  *short_name, *name, *comment;
   const char  *icon, *orig_path;
   E_Module    *module;
   Elm_Object_Item *item;
   Evas_Object *end;
   int          idx;
   Eina_Bool    enabled : 1;
};

struct _CFType
{
   const char *key, *name, *icon;
   Elm_Object_Item *gindex, *tbitem;
   Eina_Hash  *modules_hash; /* just used before constructing list */
   Eina_List  *modules; /* sorted and ready to be used */
};

struct _E_Config_Dialog_Data
{
   Eina_List           *types;
};

struct _CFTypes
{
   size_t      key_len;
   const char *key, *name, *icon;
};

/* pre defined types (used to specify icon and i18n name) */
static const CFTypes _types[] =
{
#define _CFT(k, n, i) \
  {sizeof(k) - 1, k, n, i}
   _CFT("utils", N_("Utilities"), "application-utilities"),
   _CFT("system", N_("System"), "application-system"),
   _CFT("look", N_("Look"), "modules-look"),
   _CFT("files", N_("Files"), "folder"),
   _CFT("launcher", N_("Launcher"), "modules-launcher"),
   _CFT("core", N_("Core"), "e-logo"),
   _CFT("mobile", N_("Mobile"), "modules-mobile"),
   _CFT("settings", N_("Settings"), "application-desktop"),
#undef _CFT
   {0, NULL, NULL, NULL}
};

/* local function protos */
static void         _cftype_free(CFType *cft);

static void        *_create_data(E_Config_Dialog *cfd);
static void         _free_data(E_Config_Dialog *cfd, E_Config_Dialog_Data *cfdata);
static Evas_Object *_basic_create(E_Config_Dialog *cfd, Evas *evas, E_Config_Dialog_Data *cfdata);
static void         _fill_cat_list(E_Config_Dialog_Data *cfdata);
static void         _module_end_state_apply(CFModule *cfm);

static CFType      *_cftype_find(E_Config_Dialog_Data *cfdata, const char *key, const char *name, const char *icon);
static CFType      *_cftype_new(const char *key, const char *name, const char *icon);
static void         _load_module(E_Module_Desktop *md, Eina_Hash *types_hash);
static Eina_Bool    _types_list_create_foreach_cb(const Eina_Hash *hash __UNUSED__, const void *key __UNUSED__, void *data, void *fdata);
static int          _types_list_sort(const void *data1, const void *data2);

static Evas_Object *e_int_config_modules_create_cb(const char *path, const char *part, Evas_Object *parent, void *cbdata, void *data);
static Eina_Bool e_int_config_modules_apply_cb(const char *path, const char *part, Evas_Object *obj, void *cbdata, void *data);
static void* e_int_config_modules_data_cb(const char *path, const char *part, void *data);
static void e_int_config_modules_free_cb(const char *path, const char *part, void *cbdata, void *data);

EAPI void
e_int_config_modules_init(void)
{
   e_config_panel_item_add("modules", "preferences-plugin", _("Modules"), "Extensions to extend Enlightenments functions", 0, "modules");
   e_config_panel_part_add("modules", "modules", _("Modules"), _("Mark a module as loaded to load them on startup"), 1, "extensions",
                           e_int_config_modules_create_cb, e_int_config_modules_apply_cb, e_int_config_modules_data_cb, e_int_config_modules_free_cb, NULL);
}

static void*
e_int_config_modules_data_cb(const char *path, const char *part, void *data)
{
   Eina_Hash *types_hash;
   Eina_List *mods;
   E_Module_Desktop *md;
   E_Config_Dialog_Data *cfdata;

   cfdata = E_NEW(E_Config_Dialog_Data, 1);

   types_hash = eina_hash_string_superfast_new(NULL);
   mods = e_module_desktop_list();
   if (!mods)
     {
        eina_hash_free(types_hash);
        return cfdata;
     }

   EINA_LIST_FREE(mods, md)
     {
        _load_module(md, types_hash);
        e_module_desktop_free(md);
     }

   eina_hash_foreach(types_hash, _types_list_create_foreach_cb, cfdata);
   eina_hash_free(types_hash);
   cfdata->types = eina_list_sort(cfdata->types, -1, _types_list_sort);

   return cfdata;
}

static void
e_int_config_modules_free_cb(const char *path, const char *part, void *cbdata, void *data)
{
   E_Config_Dialog_Data *cfdata = cbdata;
   CFType *cft;

   EINA_LIST_FREE(cfdata->types, cft)
     _cftype_free(cft);

   free(cfdata);
}

static Eina_Bool
_module_update(E_Config_Dialog_Data *cfdata, int type __UNUSED__, E_Event_Module_Update *ev)
{
   CFType *cft;
   Eina_List *l, *ll;
   CFModule *cfm;

   EINA_LIST_FOREACH(cfdata->types, l, cft)
     {
        EINA_LIST_FOREACH(cft->modules, ll, cfm)
          if (cfm->short_name == ev->name)
            {
               cfm->enabled = ev->enabled;
               _module_end_state_apply(cfm);
               return ECORE_CALLBACK_RENEW;
            }
     }

   return ECORE_CALLBACK_RENEW;
}

static CFModule *
_module_new(const char *short_name, const Efreet_Desktop *desk)
{
   CFModule *cfm = E_NEW(CFModule, 1);

   if (!cfm) return NULL;
   cfm->short_name = eina_stringshare_add(short_name);

   if (desk->name)
     cfm->name = eina_stringshare_add(desk->name);
   else
     cfm->name = eina_stringshare_ref(cfm->short_name);

   cfm->icon = eina_stringshare_add(desk->icon);
   cfm->comment = eina_stringshare_add(desk->comment);
   cfm->orig_path = eina_stringshare_add(desk->orig_path);
   return cfm;
}

static void
_module_free(CFModule *cfm)
{
   eina_stringshare_del(cfm->short_name);
   eina_stringshare_del(cfm->name);
   eina_stringshare_del(cfm->icon);
   eina_stringshare_del(cfm->comment);
   eina_stringshare_del(cfm->orig_path);
   E_FREE(cfm);
}

static void
_module_end_state_apply(CFModule *cfm)
{
   const char *sig;

   if (!cfm->end) return;
   sig = cfm->enabled ? "e,state,checked" : "e,state,unchecked";
   edje_object_signal_emit(cfm->end, sig, "e");
}

static CFType *
_cftype_find(E_Config_Dialog_Data *cfdata, const char *key, const char *name, const char *icon)
{
   CFType *cft;
   Eina_List *l;

   EINA_LIST_FOREACH(cfdata->types, l, cft)
     if ((!strcmp(cft->key, key)) && (!strcmp(cft->name, name)) && (!strcmp(cft->icon, icon))) return cft;
   return NULL;
}

static CFType *
_cftype_new(const char *key, const char *name, const char *icon)
{
   CFType *cft = E_NEW(CFType, 1);

   if (!cft) return NULL;
   cft->key = eina_stringshare_add(key);
   cft->name = eina_stringshare_add(name);
   cft->icon = eina_stringshare_add(icon);
   //INF("CFT NEW: key(%s) name(%s) icon(%s)", key, name, icon);
   return cft;
}

static void
_cftype_free(CFType *cft)
{
   CFModule *cfm;

   assert(cft->modules_hash == NULL); // must do it before calling this function
   EINA_LIST_FREE(cft->modules, cfm)
     _module_free(cfm);

   eina_stringshare_del(cft->key);
   eina_stringshare_del(cft->name);
   eina_stringshare_del(cft->icon);
   E_FREE(cft);
}

static CFType *
_cftype_new_from_key(const char *key)
{
   const CFTypes *itr;
   char name[1024], icon[1024];
   size_t key_len;

   if (!key) return NULL;

   key_len = strlen(key);
   for (itr = _types; itr->key_len > 0; itr++)
     {
        if (key_len != itr->key_len) continue;
        if (strcmp(itr->key, key) != 0) continue;
        return _cftype_new(itr->key, itr->name, itr->icon);
     }

   if ((key_len + 1) >= sizeof(name)) return NULL;
   if ((key_len + sizeof("enlightenment/")) >= sizeof(icon)) return NULL;

   memcpy(name, key, key_len + 1);
   name[0] = toupper(name[0]);

   snprintf(icon, sizeof(icon), "enlightenment/%s", key);
   return _cftype_new(key, name, icon);
}

static void
_load_module(E_Module_Desktop *md, Eina_Hash *types_hash)
{
   CFType *cft;
   CFModule *cfm;
   const char *type, *mod;
   Eina_Bool new_type = EINA_FALSE;

   mod = ecore_file_file_get(md->dir);
   if (md->desktop->x)
     type = eina_hash_find(md->desktop->x, "X-Enlightenment-ModuleType");
   else
     type = NULL;
   if (!type) type = "utils";  // todo: warn?

   cft = eina_hash_find(types_hash, type);
   if (cft)
     {
        new_type = EINA_FALSE;
        if ((cft->modules_hash) &&
            (eina_hash_find(cft->modules_hash, mod)))
          return;
     }
   else
     {
        cft = _cftype_new_from_key(type);
        if (cft) new_type = EINA_TRUE;
        else return;
     }

   cfm = _module_new(mod, md->desktop);
   if (!cfm)
     {
        if (new_type) _cftype_free(cft);
        return;
     }

   if (!cft->modules_hash)
     cft->modules_hash = eina_hash_string_superfast_new(NULL);
   eina_hash_direct_add(cft->modules_hash, cfm->short_name, cfm);
   cft->modules = eina_list_append(cft->modules, cfm);
   cfm->module = e_module_find(mod);
   if (cfm->module)
     cfm->enabled = e_module_enabled_get(cfm->module);
   else
     cfm->enabled = 0;

   if (new_type)
     eina_hash_direct_add(types_hash, cft->key, cft);
}

static int
_modules_list_sort(const void *data1, const void *data2)
{
   const CFModule *m1 = data1, *m2 = data2;
   return strcmp(m1->name, m2->name);
}

static Eina_Bool
_types_list_create_foreach_cb(const Eina_Hash *hash __UNUSED__, const void *key __UNUSED__, void *data, void *fdata)
{
   E_Config_Dialog_Data *cfdata = fdata;
   CFType *cft = data;

   // otherwise it should not be here
   assert(cft->modules);
   assert(cft->modules_hash);

   eina_hash_free(cft->modules_hash);
   cft->modules_hash = NULL;

   cft->modules = eina_list_sort(cft->modules, -1, _modules_list_sort);
   cfdata->types = eina_list_append(cfdata->types, cft);
   // TODO be paranoid about list append failure, otherwise leaks memory
   return EINA_TRUE;
}

static int
_types_list_sort(const void *data1, const void *data2)
{
   const CFType *t1 = data1, *t2 = data2;
   return strcmp(t1->name, t2->name);
}

static void
_toolbar_selected_cb(void *data, Evas_Object *obj, void *event)
{
  CFType *cft = data;

  if (evas_object_data_get(obj, "__anim")) return;

  elm_genlist_item_bring_in(cft->gindex, ELM_GENLIST_ITEM_SCROLLTO_TOP);
}

static void
toolbar_item(Evas_Object *obj, CFType *cft)
{
  cft->tbitem = elm_toolbar_item_append(obj, cft->icon, cft->name, _toolbar_selected_cb, cft);
}

static char *
_group_item_label_get(void *data, Evas_Object *obj, const char *part)
{
  CFType *cft = data;

  return strdup(cft->name);
}

static char *
_item_label_get(void *data, Evas_Object *obj, const char *part)
{
  CFModule *cfm = data;

  return strdup(cfm->name);
}

static Eina_Bool
_item_gen(CFModule *cfm, Evas_Object *icon)
{
   if (!cfm->icon)
     return EINA_FALSE;
   if (!elm_icon_standard_set(icon, cfm->icon))
     {
        if (cfm->orig_path)
          {
             char *dir = ecore_file_dir_get(cfm->orig_path);
             char buf[PATH_MAX];
             snprintf(buf, sizeof(buf), "%s/%s.edj", dir, cfm->icon);
             if (!ecore_file_exists(buf))
               {
                  free(dir);
                  return EINA_FALSE;
               }
             else
               elm_image_file_set(icon, buf, "icon");
             free(dir);
             return EINA_TRUE;
          }
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

static Evas_Object*
_item_content_get(void *data, Evas_Object *obj, const char *part)
{
   CFModule *cfm = data;
   Evas_Object *icon;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        if (!cfm->icon)
          icon = NULL;
        else
          {
             icon = elm_icon_add(obj);
             _item_gen(cfm, icon);
             evas_object_size_hint_min_set(icon, 64, 64);
          }
     }
   else
     {
        cfm->end = icon = elm_check_add(obj);
        elm_object_style_set(icon, "led");
        evas_object_freeze_events_set(icon, EINA_TRUE);
        elm_check_state_set(icon, cfm->enabled);
        //TODO delete hook!!
     }

  return icon;
}

static void
_list_selected(void *data, Evas_Object *obj, void *event)
{
  CFModule *mod = elm_object_item_data_get(event);
  Evas_Object *w = data;
  Evas_Object *load = evas_object_data_get(w, "__load_btn");
  Evas_Object *unload = evas_object_data_get(w, "__unload_btn");

  elm_object_disabled_set(load, mod->enabled);
  elm_object_disabled_set(unload, !mod->enabled);
}

static void
_load_cb(void *data, Evas_Object *obj, void *event)
{
   Evas_Object *list = data;
   Evas_Object *w = elm_object_parent_widget_get(list);
   Evas_Object *unload = evas_object_data_get(w, "__unload_btn");
   Elm_Object_Item *sel = elm_genlist_selected_item_get(list);
   CFModule *cfm = elm_object_item_data_get(sel);

   if (!cfm->module)
     cfm->module = e_module_find(cfm->short_name);
   if (!cfm->module)
     cfm->module = e_module_new(cfm->short_name);
   if (cfm->module)
     {
        e_module_enable(cfm->module);
        cfm->enabled = e_module_enabled_get(cfm->module);
     }

   elm_check_state_set(cfm->end, EINA_TRUE);
   elm_object_disabled_set(unload, EINA_FALSE);
   elm_object_disabled_set(obj,  EINA_TRUE);
}

static void
_unload_cb(void *data, Evas_Object *obj, void *event)
{
   Evas_Object *list = data;
   Evas_Object *w = elm_object_parent_widget_get(list);
   Evas_Object *load = evas_object_data_get(w, "__load_btn");
   Elm_Object_Item *sel = elm_genlist_selected_item_get(list);
   CFModule *cfm = elm_object_item_data_get(sel);

   if (!cfm->module)
     cfm->module = e_module_find(cfm->short_name);

   if (cfm->module)
     {
        e_module_disable(cfm->module);
        cfm->enabled = e_module_enabled_get(cfm->module);
     }

   elm_check_state_set(cfm->end, EINA_FALSE);
   elm_object_disabled_set(load, EINA_FALSE);
   elm_object_disabled_set(obj,  EINA_TRUE);
}

static void
_list_scroll_start_cb(void *data, Evas_Object *obj, void *event)
{
  evas_object_data_set(data, "__anim", (void*) 1);
}

static void
_list_scroll_stop_cb(void *data, Evas_Object *obj, void *event)
{
  evas_object_data_del(data, "__anim");
}

static void
_list_scroll_cb(void *data, Evas_Object *obj, void *event)
{
   int x, y, w, h;
   Elm_Object_Item *it, *gi;
   CFType *cft;



   evas_object_geometry_get(obj, &x, &y, &w ,&h);
   it = elm_genlist_at_xy_item_get(obj, x + 3, y + 3, NULL);

   if(!it) return; //something went wrong ...

   if (elm_genlist_item_type_get(it) == ELM_GENLIST_ITEM_GROUP)
     gi = it;
   else
     gi = elm_genlist_item_parent_get(it);

   cft = elm_object_item_data_get(gi);

   elm_toolbar_item_selected_set(cft->tbitem, EINA_TRUE);

}

static Evas_Object*
e_int_config_modules_create_cb(const char *path, const char *part, Evas_Object *parent, void *cbdata, void *data)
{
   Evas_Object *tab, *bx, *tb, *list, *lb, *load, *unload;


   tab = elm_table_add(parent);
   evas_object_size_hint_align_set(tab, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(tab, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(tab);

   tb = elm_toolbar_add(parent);
   elm_toolbar_select_mode_set(tb, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_toolbar_horizontal_set(tb, EINA_FALSE);
   elm_toolbar_shrink_mode_set(tb, ELM_TOOLBAR_SHRINK_NONE);
   elm_toolbar_icon_order_lookup_set(tb, ELM_ICON_LOOKUP_THEME_FDO);
   evas_object_size_hint_align_set(tb, 0.0, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(tb, 0.0, EVAS_HINT_EXPAND);
   elm_table_pack(tab, tb, 0, 0, 1, 1);
   evas_object_show(tb);

   list = elm_genlist_add(parent);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_smart_callback_add(list, "scroll", _list_scroll_cb, tb);
   evas_object_smart_callback_add(list, "scroll,anim,start", _list_scroll_start_cb, tb);
   evas_object_smart_callback_add(list, "scroll,anim,stop", _list_scroll_stop_cb, tb);
   evas_object_data_set(list, "__toolbar", tb);
   elm_table_pack(tab, list, 1, 0, 1, 1);
   evas_object_show(list);

   bx = elm_box_add(parent);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, 0.0);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0.0);
   elm_box_horizontal_set(bx, EINA_TRUE);
   elm_table_pack(tab, bx, 0, 1, 2, 1);
   evas_object_show(bx);

   load = elm_button_add(parent);
   elm_object_text_set(load, _("load"));
   elm_object_disabled_set(load, EINA_TRUE);
   evas_object_data_set(tab, "__load_btn", load);
   evas_object_smart_callback_add(load, "clicked", _load_cb, list);
   evas_object_size_hint_align_set(load, EVAS_HINT_FILL, 0.0);
   evas_object_size_hint_weight_set(load, EVAS_HINT_EXPAND, 0.0);
   elm_box_pack_end(bx, load);
   evas_object_show(load);

   unload = elm_button_add(parent);
   elm_object_text_set(unload, _("unload"));
   elm_object_disabled_set(unload, EINA_TRUE);
   evas_object_data_set(tab, "__unload_btn", unload);
   evas_object_smart_callback_add(unload, "clicked", _unload_cb, list);
   evas_object_size_hint_align_set(unload, EVAS_HINT_FILL, 0.0);
   evas_object_size_hint_weight_set(unload, EVAS_HINT_EXPAND, 0.0);
   elm_box_pack_end(bx, unload);
   evas_object_show(unload);

   {
      E_Config_Dialog_Data *cfdata = cbdata;
      Eina_List *node;
      CFType *cft;
      Elm_Genlist_Item_Class *itc_group, *itc_item;

      /*groupindex class*/
      itc_group = elm_genlist_item_class_new();
      itc_group->item_style = "group_index";
      itc_group->func.text_get = _group_item_label_get;
      itc_group->func.content_get = NULL;
      itc_group->func.state_get = NULL;
      itc_group->func.del = NULL;

      itc_item = elm_genlist_item_class_new();
      itc_item->item_style = "default";
      itc_item->func.text_get = _item_label_get;
      itc_item->func.content_get = _item_content_get;
      itc_item->func.state_get = NULL;
      itc_item->func.del = NULL;

      EINA_LIST_FOREACH(cfdata->types, node, cft)
        {
           CFModule *cfm;
           Eina_List *l_module;
           Elm_Object_Item *item, *it;

           cft->gindex = item = elm_genlist_item_append(list, itc_group, cft, NULL, ELM_GENLIST_ITEM_GROUP, NULL, NULL);
           elm_genlist_item_select_mode_set(item, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);

           toolbar_item(tb, cft);

           EINA_LIST_FOREACH(cft->modules, l_module, cfm)
             {
                cfm->item = it = elm_genlist_item_append(list, itc_item, cfm,
                                             item, ELM_GENLIST_ITEM_NONE,
                                             _list_selected, bx);
                elm_genlist_item_tooltip_text_set(it, cfm->comment);
             }
        }
   }

   return tab;
}

static Eina_Bool
e_int_config_modules_apply_cb(const char *path, const char *part, Evas_Object *obj, void *cbdata, void *data)
{
   //Nothing to do here we save things directly
}
