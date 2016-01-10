#include "e_mod_main.h"
#include "e_mod_notifier_host_private.h"

const char *Category_Names[] = {
   "unknown", "SystemServices", NULL
};

const char *Status_Names[] = {
   "unknown", "Active", "Passive", "NeedsAttention", NULL
};

static Eina_List *traybars; //known traybars
static Eina_List *items; //list of known Notifier_Item items
 static void _systray_size_apply_do(Edje_Object *obj);

static void
_systray_theme(Evas_Object *o, const char *shelf_style, const char *gc_style)
{
   const char base_theme[] = "base/theme/modules/systray";
   char path[PATH_MAX];
   char buf[128], *p;
   size_t len, avail;

   snprintf(path, sizeof(path), "%s/e-module-systray.edj", e_module_dir_get(systray_mod));

   len = eina_strlcpy(buf, "e/modules/systray/main", sizeof(buf));
   if (len >= sizeof(buf))
     goto fallback;
   p = buf + len;
   *p = '/';
   p++;
   avail = sizeof(buf) - len - 2;

   if (shelf_style && gc_style)
     {
        size_t r;
        r = snprintf(p, avail, "%s/%s", shelf_style, gc_style);
        if (r < avail && e_theme_edje_object_set(o, base_theme, buf))
          return;
     }

   if (shelf_style)
     {
        size_t r;
        r = eina_strlcpy(p, shelf_style, avail);
        if (r < avail && e_theme_edje_object_set(o, base_theme, buf))
          return;
     }

   if (gc_style)
     {
        size_t r;
        r = eina_strlcpy(p, gc_style, avail);
        if (r < avail && e_theme_edje_object_set(o, base_theme, buf))
          return;
     }

   if (e_theme_edje_object_set(o, base_theme, "e/modules/systray/main"))
     return;

   if (shelf_style && gc_style)
     {
        size_t r;
        r = snprintf(p, avail, "%s/%s", shelf_style, gc_style);
        if (r < avail && edje_object_file_set(o, path, buf))
          return;
     }

   if (shelf_style)
     {
        size_t r;
        r = eina_strlcpy(p, shelf_style, avail);
        if (r < avail && edje_object_file_set(o, path, buf))
          return;
     }

   if (gc_style)
     {
        size_t r;
        r = eina_strlcpy(p, gc_style, avail);
        if (r < avail && edje_object_file_set(o, path, buf))
          return;
     }

fallback:
   edje_object_file_set(o, path, "e/modules/systray/main");
}

static void
image_load(const char *name, const char *path, uint32_t *imgdata, int w, int h, Evas_Object *image)
{
   const char **ext, *exts[] =
   {
      ".png",
      ".jpg",
      NULL
   };
   if (path && path[0])
     {
        char buf[PATH_MAX];
        const char **theme, *themes[] = { e_config->icon_theme, "hicolor", NULL };

        for (theme = themes; *theme; theme++)
          {
             struct stat st;
             unsigned int *i, sizes[] = { 16, 22, 24, 32, 36, 40, 48, 64, 72, 96, 128, 192, 256, 512, 0 };

             snprintf(buf, sizeof(buf), "%s/%s", path, *theme);
             if (stat(buf, &st)) continue;
             for (i = sizes; *i; i++)
               {
                  snprintf(buf, sizeof(buf), "%s/%s/%ux%u", path, *theme, *i, *i);
                  if (stat(buf, &st)) continue;
                  for (ext = exts; *ext; ext++)
                    {
                       snprintf(buf, sizeof(buf), "%s/%s/%ux%u/apps/%s%s", path, *theme, *i, *i, name, *ext);
                       if (e_icon_file_set(image, buf)) return;
                    }
               }
          }
     }
   if (name && name[0] && e_util_icon_theme_set(image, name)) return;
   if (imgdata)
     {
        Evas_Object *o;

        o = evas_object_image_filled_add(evas_object_evas_get(image));
        evas_object_image_alpha_set(o, 1);
        evas_object_image_size_set(o, w, h);
        evas_object_image_data_set(o, imgdata);
        e_icon_image_object_set(image, o);
     }
   else
     e_util_icon_theme_set(image, "dialog-error");
}

static void
image_scale(Evas_Object *icon)
{
    //TODO make it the size of the shelf
    //FIXME delayed on "after" zmike rewriting gadgets
    evas_object_size_hint_min_set(icon, 30, 30);
}
static void
_sub_item_clicked_cb(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   E_DBusMenu_Item *item = data;
   e_dbusmenu_event_send(item, E_DBUSMENU_ITEM_EVENT_CLICKED);
}

static void
_menu_post_deactivate(void *data, E_Menu *m)
{
   Eina_List *iter;
   E_Menu_Item *mi;

   EINA_LIST_FOREACH(m->items, iter, mi)
     {
        if (mi->submenu)
          e_menu_deactivate(mi->submenu);
     }
   e_object_del(E_OBJECT(m));
}

static E_Menu *
_item_submenu_new(E_DBusMenu_Item *item, E_Menu_Item *mi)
{
   E_DBusMenu_Item *child;
   E_Menu *m;
   E_Menu_Item *submi;

   m = e_menu_new();
   e_menu_post_deactivate_callback_set(m, _menu_post_deactivate, NULL);
   if (mi)
     e_menu_item_submenu_set(mi, m);

   EINA_INLIST_FOREACH(item->sub_items, child)
     {
        if (!child->visible) continue;
        submi = e_menu_item_new(m);
        if (child->type == E_DBUSMENU_ITEM_TYPE_SEPARATOR)
          e_menu_item_separator_set(submi, 1);
        else
          {
             e_menu_item_label_set(submi, child->label);
             e_menu_item_callback_set(submi, _sub_item_clicked_cb, child);
             if (!child->enabled) e_menu_item_disabled_set(submi, 1);
             if (child->toggle_type == E_DBUSMENU_ITEM_TOGGLE_TYPE_CHECKMARK)
               e_menu_item_check_set(submi, 1);
             else if (child->toggle_type == E_DBUSMENU_ITEM_TOGGLE_TYPE_RADIO)
               e_menu_item_radio_set(submi, 1);
             if (child->toggle_type)
               e_menu_item_toggle_set(submi, child->toggle_state);
             if (child->sub_items)
               _item_submenu_new(child, submi);
             e_util_menu_item_theme_icon_set(submi, child->icon_name);
          }
     }
   return m;
}

void
_clicked_item_cb(void *data, Evas *evas, Evas_Object *obj EINA_UNUSED, void *event)
{
   Notifier_Item *item = data;
   Evas_Event_Mouse_Down *ev = event;
   E_DBusMenu_Item *root_item;
   E_Menu *m;
   int x, y;

   if (ev->button != 1) return;

   root_item = item->dbus_item;
   EINA_SAFETY_ON_FALSE_RETURN(root_item->is_submenu);

   m = _item_submenu_new(root_item, NULL);

   ecore_evas_pointer_xy_get(e_comp->ee, &x, &y);
   e_menu_activate_mouse(m, e_zone_current_get(), x, y, 1, 1, E_MENU_POP_DIRECTION_DOWN, ev->timestamp);
   evas_event_feed_mouse_up(evas, ev->button,
                         EVAS_BUTTON_NONE, ev->timestamp, NULL);
}
static Evas_Object*
_visualize_add(Evas_Object *parent, Notifier_Item *item)
{
    Evas_Object *obj;

    obj = e_icon_add(parent);
    image_load(item->icon_name,
               item->icon_path,
               item->imgdata,
               30, 30, obj);
    e_util_icon_theme_set(obj, "dialog-error");
    image_scale(obj);
    evas_object_show(obj);
    evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_DOWN,
                                   _clicked_item_cb, item);

    return obj;
}

static void
_visualize_update(Evas_Object *icon, Notifier_Item *item)
{
   switch (item->status)
     {
      case STATUS_ACTIVE:
        {
           image_load(item->icon_name, item->icon_path, item->imgdata, item->imgw, item->imgh, icon);
           evas_object_show(icon);
           break;
        }
      case STATUS_PASSIVE:
        {
           evas_object_hide(icon);
           break;
        }
      case STATUS_ATTENTION:
        {
           image_load(item->attention_icon_name, item->icon_path, item->attnimgdata, item->attnimgw, item->attnimgh, icon);
           evas_object_show(icon);
           break;
        }
      default:
        {
           WRN("unhandled status");
           break;
        }
     }
}

static Eina_Bool
_host_del_cb(void *data, Eo *obj, const Eo_Event_Description2 *desc, void *event)
{
   traybars = eina_list_remove(traybars, obj);
   return EO_CALLBACK_CONTINUE;
}

Evas_Object*
systray_notifier_host_new(const char *shelfstyle, const char *style)
{
   Edje_Object *gadget;

   gadget = edje_object_add(e_comp->evas);
   _systray_theme(gadget, shelfstyle, style);
   evas_object_show(gadget);

   //add a new visualization to every single item for this tray bar
   {
      Eina_List *node;
      Notifier_Item *item;

      EINA_LIST_FOREACH(items, node, item)
        {
           Evas_Object *icon;
           //create new icon
           icon = _visualize_add(gadget, item);

           //add data
           _visualize_update(icon, item);

           item->icons = eina_list_append(item->icons, icon);

           edje_object_part_box_append(gadget, "box", icon);
           _systray_size_apply_do(gadget);
        }
   }

   traybars = eina_list_append(traybars, gadget);

   return gadget;
}

static Context_Notifier_Host *ctx;

void
systray_notifier_host_init(void)
{

   ctx = calloc(1, sizeof(Context_Notifier_Host));
   systray_notifier_dbus_init(ctx);
}

void
systray_notifier_host_shutdown(void)
{
   systray_notifier_dbus_shutdown(ctx);
}

void
systray_notifier_update_menu(void *data, E_DBusMenu_Item *new_root_item)
{
   Notifier_Item *item = data;
   item->dbus_item = new_root_item;
}

void
systray_notifier_item_update(Notifier_Item *item)
{
   Eina_List *node;
   Evas_Object *icon;

   EINA_LIST_FOREACH(item->icons, node, icon)
     {
        _visualize_update(icon, item);
     }
}
void
systray_notifier_item_free(Notifier_Item *item)
{
    Evas_Object *icon;

    EINA_LIST_FREE(item->icons, icon)
      evas_object_del(icon);
}

void
systray_notifier_item_new(Notifier_Item *item)
{
    Evas_Object *traybar;
    Eina_List *node;

    EINA_LIST_FOREACH(traybars, node, traybar)
      {
         Evas_Object *icon;

         icon = _visualize_add(traybar, item);
         edje_object_part_box_append(traybar, "box", icon);
         _systray_size_apply_do(traybar);

         item->icons = eina_list_append(item->icons, icon);
      }
}

/**
 * Hacky code to recalc the minsize of a object
 */
 static void
_systray_size_apply_do(Edje_Object *obj)
{
   Evas_Coord w, h;

   edje_object_message_signal_process(obj);
   edje_object_size_min_calc(obj, &w, &h);
   evas_object_size_hint_min_set(obj, w, h);
   _hack_get_me_the_correct_min_size(obj);
}