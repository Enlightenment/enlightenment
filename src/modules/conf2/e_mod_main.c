#include "e.h"
#include "e_mod_main.h"

typedef struct _Instance Instance;
struct _Instance
{
   E_Gadcon_Client *gcc;
   Evas_Object     *o_toggle;
};

/* actual module specifics */

//static void             _e_mod_conf_cb(void *data, E_Menu *m, E_Menu_Item *mi);
//static void             _e_mod_run_cb(void *data, E_Menu *m, E_Menu_Item *mi);
//static void             _config_pre_activate_cb(void *data, E_Menu *m);

/* gadcon requirements */
static E_Gadcon_Client *_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style);
static void             _gc_shutdown(E_Gadcon_Client *gcc);
static void             _gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient);
static const char      *_gc_label(const E_Gadcon_Client_Class *client_class);
static Evas_Object     *_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas);
static const char      *_gc_id_new(const E_Gadcon_Client_Class *client_class);
static void             _cb_action_conf(void *data, Evas_Object *obj, const char *emission, const char *source);

static E_Module *conf_module = NULL;
static E_Action *act = NULL;
static E_Int_Menu_Augmentation *maug = NULL;
static Eina_List *instances = NULL;

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Conf2" };

/* and actually define the gadcon class that this module provides (just 1) */
static const E_Gadcon_Client_Class _gadcon_class =
{
   GADCON_CLIENT_CLASS_VERSION, "configuration",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->o_toggle = edje_object_add(gc->evas);
   e_theme_edje_object_set(inst->o_toggle,
                           "base/theme/modules/conf2",
                           "e/modules/conf2/main");

   inst->gcc = e_gadcon_client_new(gc, name, id, style, inst->o_toggle);
   inst->gcc->data = inst;

   edje_object_signal_callback_add(inst->o_toggle, "e,action,conf2", "",
                                   _cb_action_conf, inst);

   instances = eina_list_append(instances, inst);
   e_gadcon_client_util_menu_attach(inst->gcc);

   return inst->gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst;

   if (!(inst = gcc->data)) return;
   instances = eina_list_remove(instances, inst);
   if (inst->o_toggle) evas_object_del(inst->o_toggle);
   E_FREE(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   Evas_Coord mw, mh;

   edje_object_size_min_get(gcc->o_base, &mw, &mh);
   if ((mw < 1) || (mh < 1))
     edje_object_size_min_calc(gcc->o_base, &mw, &mh);
   if (mw < 4) mw = 4;
   if (mh < 4) mh = 4;
   e_gadcon_client_aspect_set(gcc, mw, mh);
   e_gadcon_client_min_size_set(gcc, mw, mh);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _("Settings");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   char buf[PATH_MAX];

   o = edje_object_add(evas);
   snprintf(buf, sizeof(buf), "%s/e-module-conf.edj",
            e_module_dir_get(conf_module));
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _gadcon_class.name;
}

/*
   static void
   _cb_button_click(void *data EINA_UNUSED, void *data2 EINA_UNUSED)
   {
   E_Action *a;

   a = e_action_find("configuration");
   if ((a) && (a->func.go)) a->func.go(NULL, NULL);
   }
 */

static void
_cb_action_conf(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Instance *inst;
   E_Action *a;

   if (!(inst = data)) return;
   a = e_action_find("configuration");
   if ((a) && (a->func.go)) a->func.go(NULL, NULL);
}

#if 0
static void
_e_mod_run_cb(void *data, E_Menu *m, E_Menu_Item *mi EINA_UNUSED)
{
   Eina_List *l;
   E_Configure_Cat *ecat;

   EINA_LIST_FOREACH(e_configure_registry, l, ecat)
     {
        if ((ecat->pri >= 0) && (ecat->items))
          {
             E_Configure_It *eci;
             Eina_List *ll;

             EINA_LIST_FOREACH(ecat->items, ll, eci)
               {
                  char buf[1024];

                  if ((eci->pri >= 0) && (eci == data))
                    {
                       snprintf(buf, sizeof(buf), "%s/%s", ecat->cat, eci->item);
                       e_configure_registry_call(buf, m->zone->container, NULL);
                    }
               }
          }
     }
}

static void
_config_pre_activate_cb(void *data, E_Menu *m)
{
   E_Configure_Cat *ecat = data;
   E_Configure_It *eci;
   Eina_List *l;
   E_Menu_Item *mi;

   e_menu_pre_activate_callback_set(m, NULL, NULL);

   EINA_LIST_FOREACH(ecat->items, l, eci)
     {
        if (eci->pri >= 0)
          {
             mi = e_menu_item_new(m);
             e_menu_item_label_set(mi, eci->label);
             if (eci->icon)
               {
                  if (eci->icon_file)
                    e_menu_item_icon_edje_set(mi, eci->icon_file, eci->icon);
                  else
                    e_util_menu_item_theme_icon_set(mi, eci->icon);
               }
             e_menu_item_callback_set(mi, _e_mod_run_cb, eci);
          }
     }
}

static void
_config_item_activate_cb(void *data, E_Menu *m, E_Menu_Item *mi EINA_UNUSED)
{
   E_Configure_Cat *ecat = data;
   e_configure_show(m->zone->container, ecat->cat);
}

static void
_config_all_pre_activate_cb(void *data EINA_UNUSED, E_Menu *m)
{
   const Eina_List *l;
   E_Configure_Cat *ecat;

   e_menu_pre_activate_callback_set(m, NULL, NULL);

   EINA_LIST_FOREACH(e_configure_registry, l, ecat)
     {
        E_Menu_Item *mi;
        E_Menu *sub;

        if ((ecat->pri < 0) || (!ecat->items)) continue;

        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, ecat->label);
        if (ecat->icon)
          {
             if (ecat->icon_file)
               e_menu_item_icon_edje_set(mi, ecat->icon_file, ecat->icon);
             else
               e_util_menu_item_theme_icon_set(mi, ecat->icon);
          }
        e_menu_item_callback_set(mi, _config_item_activate_cb, ecat);
        sub = e_menu_new();
        e_menu_item_submenu_set(mi, sub);
        e_object_unref(E_OBJECT(sub));
        e_menu_pre_activate_callback_set(sub, _config_pre_activate_cb, ecat);
     }
}

/* menu item add hook */
void
e_mod_config_menu_add(void *data EINA_UNUSED, E_Menu *m)
{
   E_Menu_Item *mi;
   E_Menu *sub;

   e_menu_pre_activate_callback_set(m, NULL, NULL);

   sub = e_menu_new();
   e_menu_pre_activate_callback_set(sub, _config_all_pre_activate_cb, NULL);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("All"));
   e_menu_item_submenu_set(mi, sub);
   e_object_unref(E_OBJECT(sub));
}
#endif
static void
_e_mod_action_conf_cb(E_Object *obj, const char *params)
{
   E_Zone *zone = NULL;

   if (obj)
     {
        if (obj->type == E_MANAGER_TYPE)
          zone = e_util_zone_current_get((E_Manager *)obj);
        else if (obj->type == E_CONTAINER_TYPE)
          zone = e_util_zone_current_get(((E_Container *)obj)->manager);
        else if (obj->type == E_ZONE_TYPE)
          zone = ((E_Zone *)obj);
        else
          zone = e_util_zone_current_get(e_manager_current_get());
     }
   if (!zone) zone = e_util_zone_current_get(e_manager_current_get());
   if ((zone) && (params))
     e_configure_registry_call(params, zone->container, params);
   else if (zone)
     e_conf2_show(zone->container, params);
}

static void
_e_mod_menu_cb(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   e_conf2_show(NULL, NULL);
}

static void
_e_mod_menu_add(void *data EINA_UNUSED, E_Menu *m)
{
   E_Menu_Item *mi;

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Configuration"));
   e_util_menu_item_theme_icon_set(mi, "preferences-system");
   e_menu_item_callback_set(mi, _e_mod_menu_cb, NULL);
}

EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[PATH_MAX];

   if (!e_win_elm_available()) return NULL;
   if (e_action_find("configuration"))
     {
        e_util_dialog_show(_("Error"), _("conf2 module cannot be loaded when conf module is already loaded!"));
        return NULL;
     }
   conf_module = m;

   /* add module supplied action */
   act = e_action_add("configuration");
   if (act)
     {
        act->func.go = _e_mod_action_conf_cb;
        e_action_predef_name_set(N_("Launch"), N_("Control Panel"),
                                 "configuration", NULL, NULL, 0);
     }
   maug =
     e_int_menus_menu_augmentation_add_sorted("config/0", _("Configuration"),
                                              _e_mod_menu_add, NULL, NULL, NULL);
   e_module_delayed_set(m, 1);

   snprintf(buf, sizeof(buf), "%s/e-module-conf2.edj", e_module_dir_get(conf_module));
   elm_theme_extension_add(NULL, buf);

   //e_configure_registry_category_add("settings", 80, _("Settings"),
                                     //NULL, "preferences-settings");
   //e_configure_registry_item_add("settings/conf2", 110, _("Control Panel"),
                                 //NULL, buf, e_int_config_conf2);
/*
   if (conf->menu_augmentation)
     {
        conf->aug =
          e_int_menus_menu_augmentation_add
            ("config/2", e_mod_config_menu_add, NULL, NULL, NULL);
     }
*/
   e_gadcon_provider_register(&_gadcon_class);
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   char buf[PATH_MAX];

   e_conf2_hide();

   snprintf(buf, sizeof(buf), "%s/e-module-conf2.edj", e_module_dir_get(conf_module));
   elm_theme_extension_del(NULL, buf);

   e_configure_registry_item_del("advanced/conf2");
   e_configure_registry_category_del("advanced");

   e_gadcon_provider_unregister(&_gadcon_class);

   /* remove module-supplied menu additions */
   if (maug)
     {
        e_int_menus_menu_augmentation_del("config/0", maug);
        maug = NULL;
     }
/*
   if (conf->aug)
     {
        e_int_menus_menu_augmentation_del("config/2", conf->aug);
        conf->aug = NULL;
     }
*/
   /* remove module-supplied action */
   if (act)
     {
        e_action_predef_name_del(N_("Launch"), N_("Control Panel"));
        e_action_del("configuration");
        act = NULL;
     }
   conf_module = NULL;

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}
