#include "luncher.h"
static Eina_List *handlers;
static Elm_Gengrid_Item_Class _grid_icon_class;
static void _grid_resize_job(void *data);

static void
_grid_aspect(Instance *inst)
{
   Evas_Coord w, h, square, size;

   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   if (!eina_list_count(inst->icons))
     return;
   evas_object_geometry_get(inst->o_main, 0, 0, &w, &h);
   size = 0;
   if ((w + h) > 1)
     {
        square = w + h;
        size = floor((square / eina_list_count(inst->icons)));
     }
   inst->size = size;
   elm_gengrid_item_size_set(inst->o_icon_con, size, size);
}

static Eina_Bool
_grid_check_modifiers(Evas_Modifier *modifiers)
{
   if ((evas_key_modifier_is_set(modifiers, "Alt")) ||
       (evas_key_modifier_is_set(modifiers, "Control")) ||
       (evas_key_modifier_is_set(modifiers, "Shift")))
     return EINA_TRUE;
   return EINA_FALSE;
}

static Evas_Object *
_grid_gadget_configure(Evas_Object *g)
{
   if (!luncher_config) return NULL;
   if (luncher_config->config_dialog) return NULL;
   Instance *inst = evas_object_data_get(g, "instance");
   return config_luncher(e_zone_current_get(), inst, EINA_TRUE);
}

static void
_grid_instance_watch(void *data, E_Exec_Instance *ex EINA_UNUSED, E_Exec_Watch_Type type)
{
   Icon *ic = data;

   switch (type)
     {
      case E_EXEC_WATCH_STARTED:
        if (ic->starting) elm_layout_signal_emit(ic->o_layout, "e,state,started", "e");
        ic->starting = EINA_FALSE;
        break;
      default:
        break;
     }
}

static void
_grid_icon_del(Instance *inst, Icon *ic)
{
   inst->icons = eina_list_remove(inst->icons, ic);
   evas_object_del(ic->o_icon);
   evas_object_del(ic->o_overlay);
   evas_object_del(ic->o_layout);
   if (ic->desktop)
     efreet_desktop_unref(ic->desktop);
   eina_stringshare_del(ic->icon);
   eina_stringshare_del(ic->key);
   if (ic->exec)
     e_exec_instance_watcher_del(ic->exec, _grid_instance_watch, ic);
   ic->exec = NULL;
   _grid_aspect(inst);
   E_FREE(ic);
}

static void
_grid_icon_drag_done(E_Drag *drag, int dropped)
{
   Instance *inst = e_object_data_get(E_OBJECT(drag));

   efreet_desktop_unref(drag->data);
   if (!inst) return;
   evas_object_smart_callback_call(e_gadget_site_get(inst->o_main), "gadget_site_unlocked", NULL);
   if (!dropped)
     grid_recalculate(inst);
}

static void
_grid_icon_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Icon *ic = data;
   Evas_Event_Mouse_Move *ev = event_data;
   int dx, dy;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_grid_check_modifiers(ev->modifiers)) return;

   if (!ic->drag.start) return;

   dx = ev->cur.output.x - ic->drag.x;
   dy = ev->cur.output.y - ic->drag.y;
   if (((dx * dx) + (dy * dy)) >
       (e_config->drag_resist * e_config->drag_resist))
     {
        E_Drag *d;
        Evas_Object *o;
        Evas_Coord x, y, w, h;
        unsigned int size;
        const char *drag_types[] = { "enlightenment/desktop" };

        ic->drag.dnd = 1;
        ic->drag.start = 0;

        evas_object_geometry_get(ic->o_icon, &x, &y, &w, &h);
        d = e_drag_new(x, y, drag_types, 1,
                       ic->desktop, -1, NULL, _grid_icon_drag_done);
        d->button_mask = evas_pointer_button_down_mask_get(e_comp->evas);
        efreet_desktop_ref(ic->desktop);
        size = MAX(w, h);
        o = e_util_desktop_icon_add(ic->desktop, size, e_drag_evas_get(d));
        e_drag_object_set(d, o);

        e_drag_resize(d, w, h);
        e_drag_start(d, ic->drag.x, ic->drag.y);
        e_object_data_set(E_OBJECT(d), ic->inst);
        if (ic->in_order)
          e_order_remove(ic->inst->order, ic->desktop);
     }
}

static Eina_Bool
_grid_drag_timer(void *data)
{
   Icon *ic = data;

   ic->drag_timer = NULL;
   ic->drag.start = 1;
   evas_object_smart_callback_call(e_gadget_site_get(ic->inst->o_main), "gadget_site_locked", NULL);
   return EINA_FALSE;
}

static void
_grid_icon_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Icon *ic = data;
   Evas_Event_Mouse_Up *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_grid_check_modifiers(ev->modifiers)) return;

   if (ev->button == 1)
     {
        ic->drag.x = ev->output.x;
        ic->drag.y = ev->output.y;
        E_FREE_FUNC(ic->drag_timer, ecore_timer_del);
        ic->drag_timer = ecore_timer_add(.35, _grid_drag_timer, ic);
     }
   if (ev->button == 3)
     {
        e_gadget_configure(ic->inst->o_main);
     }
}

static void
_grid_icon_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Icon *ic = data;
   Evas_Event_Mouse_Up *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_grid_check_modifiers(ev->modifiers)) return;

   if (ev->button == 1)
     {
        E_FREE_FUNC(ic->drag_timer, ecore_timer_del);
        if (ic->drag.dnd)
          {
             ic->drag.start = 0;
             ic->drag.dnd = 0;
             return;
          }
     }
   if (ev->button == 1 && ic->desktop)
     {
        if (ic->desktop->type == EFREET_DESKTOP_TYPE_APPLICATION)
          {
             E_Exec_Instance *ex;

             ex = e_exec(e_zone_current_get(), ic->desktop, NULL, NULL, "luncher");
             ic->exec = ex;
             e_exec_instance_watcher_add(ex, _grid_instance_watch, ic);
             if (!ic->starting) elm_layout_signal_emit(ic->o_layout, "e,state,starting", "e");
             ic->starting = EINA_TRUE;
          }
        else if (ic->desktop->type == EFREET_DESKTOP_TYPE_LINK)
          {
             if (!strncasecmp(ic->desktop->url, "file:", 5))
               {
                  E_Action *act;

                  act = e_action_find("fileman");
                  if (act)
                    act->func.go(NULL, ic->desktop->url + 5);
               }
          }
     }
}

static void
_grid_icon_mouse_in(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data)
{
   Icon *ic = data;
   Evas_Event_Mouse_In *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_grid_check_modifiers(ev->modifiers)) return;

   evas_object_raise(ic->o_layout);
   elm_object_tooltip_show(obj);
   ic->active = EINA_TRUE;
   elm_layout_signal_emit(ic->o_layout, "e,state,focused", "e");
}

static void
_grid_icon_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Icon *ic = data;

   elm_object_tooltip_hide(obj);
   elm_layout_signal_emit(ic->o_layout, "e,state,unfocused", "e");
}

static Evas_Object *
_gengrid_icon_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part)
{
   Icon *ic = data;
   if (strcmp(part, "elm.swallow.icon"))
     return NULL;
   return ic->o_layout;
}

static Icon *
_grid_icon_add(Instance *inst, Efreet_Desktop *desktop)
{
   const char *path = NULL, *k = NULL;
   char buf[4096];
   int len = 0;
   Icon *ic;

   ic = E_NEW(Icon, 1);
   efreet_desktop_ref(desktop);
   ic->desktop = desktop;
   ic->inst = inst;
   ic->preview = NULL;
   ic->preview_box = NULL;
   ic->mouse_in_timer = NULL;
   ic->mouse_out_timer = NULL;
   ic->active = EINA_FALSE;
   ic->starting = EINA_FALSE;
   ic->exec = NULL;

   ic->o_layout = elm_layout_add(inst->o_icon_con);
   e_theme_edje_object_set(ic->o_layout, "e/gadget/luncher/icon",
       "e/gadget/luncher/icon");
   E_FILL(ic->o_layout);
   evas_object_show(ic->o_layout);

   ic->o_icon = elm_icon_add(ic->o_layout);
   E_EXPAND(ic->o_icon);

   ic->o_overlay = elm_icon_add(ic->o_layout);
   E_EXPAND(ic->o_overlay);

   if (!desktop->icon)
     path = NULL;
   else if (strncmp(desktop->icon, "/", 1) && !ecore_file_exists(desktop->icon))
     {
        path = efreet_icon_path_find(e_config->icon_theme, desktop->icon, inst->size);
        if (!path)
          {
             if (e_util_strcmp(e_config->icon_theme, "hicolor"))
               path = efreet_icon_path_find("hicolor", desktop->icon, inst->size);
          }
     }
   else if (ecore_file_exists(desktop->icon))
     {
        path = desktop->icon;
     }
   if (!path)
     {
        snprintf(buf, sizeof(buf), "e/icons/%s", desktop->icon);
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
   elm_image_file_set(ic->o_icon, path, k);
   elm_object_tooltip_text_set(ic->o_icon, desktop->name);
   elm_object_tooltip_orient_set(ic->o_icon, ELM_TOOLTIP_ORIENT_CENTER);
   elm_object_tooltip_style_set(ic->o_icon, "luncher");
   evas_object_size_hint_aspect_set(ic->o_icon, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   elm_layout_content_set(ic->o_layout, "e.swallow.icon", ic->o_icon);
   evas_object_event_callback_add(ic->o_icon, EVAS_CALLBACK_MOUSE_UP,
       _grid_icon_mouse_up, ic);
   evas_object_event_callback_priority_add(ic->o_icon, EVAS_CALLBACK_MOUSE_DOWN, 0,
       _grid_icon_mouse_down, ic);
   evas_object_event_callback_add(ic->o_icon, EVAS_CALLBACK_MOUSE_MOVE,
       _grid_icon_mouse_move, ic);
   evas_object_event_callback_add(ic->o_icon, EVAS_CALLBACK_MOUSE_IN,
       _grid_icon_mouse_in, ic);
   evas_object_event_callback_add(ic->o_icon, EVAS_CALLBACK_MOUSE_OUT,
       _grid_icon_mouse_out, ic);
   evas_object_show(ic->o_icon);

   elm_image_file_set(ic->o_overlay, path, k);
   evas_object_size_hint_aspect_set(ic->o_overlay, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   elm_layout_content_set(ic->o_layout, "e.swallow.overlay", ic->o_overlay);
   evas_object_show(ic->o_overlay);

   elm_layout_sizing_eval(ic->o_layout);

   elm_gengrid_item_append(inst->o_icon_con, &_grid_icon_class, ic, NULL, NULL);
   _grid_aspect(inst);

   return ic;
}

static void
_grid_empty(Instance *inst)
{
   if (inst->icons)
     {
        Icon *ic;
        Eina_List *l;

        elm_gengrid_clear(inst->o_icon_con);
        EINA_LIST_FOREACH(inst->icons, l, ic)
          _grid_icon_del(inst, ic);
        eina_list_free(inst->icons);
        inst->icons = NULL;
     }
}

static void
_grid_fill(Instance *inst)
{
   Icon *ic;

   if (inst->order)
     {
        Efreet_Desktop *desktop;
        Eina_List *list;

        EINA_LIST_FOREACH(inst->order->desktops, list, desktop)
          {
             ic = _grid_icon_add(inst, desktop);
             ic->in_order = EINA_TRUE;
             inst->icons = eina_list_append(inst->icons, ic);
          }
     }
}

static void
_grid_resize_job(void *data)
{
   Instance *inst = data;
   Eina_List *l;
   Icon *ic;

   if (inst)
     {
        elm_layout_sizing_eval(inst->o_main);
        _grid_aspect(inst);
        EINA_LIST_FOREACH(inst->icons, l, ic)
          {
             const char *path = NULL, *key = NULL;
             int len = 0;

             if (ic->desktop)
               {
                  if (!ic->desktop->icon)
                    path = NULL;
                  else if (strncmp(ic->desktop->icon, "/", 1) && !ecore_file_exists(ic->desktop->icon))
                    {
                       path = efreet_icon_path_find(e_config->icon_theme, ic->desktop->icon, inst->size);
                       if (!path)
                         {
                            if (e_util_strcmp(e_config->icon_theme, "hicolor"))
                              path = efreet_icon_path_find("hicolor", ic->desktop->icon, inst->size);
                         }
                    }
                  else if (ecore_file_exists(ic->desktop->icon))
                    {
                       path = ic->desktop->icon;
                    }
                  if (!path)
                    {
                       elm_image_file_set(ic->o_icon, e_theme_edje_file_get("base/theme/icons", "e/icons/unknown"),
                           "e/icons/unknown");
                       elm_image_file_set(ic->o_overlay, e_theme_edje_file_get("base/theme/icons", "e/icons/unknown"),
                           "e/icons/unknown");
                    }
                  if (path && ic->desktop->icon)
                    {
                       len = strlen(ic->desktop->icon);
                       if ((len > 4) && (!strcasecmp(ic->desktop->icon + len - 4, ".edj")))
                         key = "icon";

                       elm_image_file_set(ic->o_icon, path, key);
                       elm_image_file_set(ic->o_overlay, path, key);
                    }
               }
             else if (ic->icon)
               {
                  if (strncmp(ic->icon, "/", 1) && !ecore_file_exists(ic->icon))
                    {
                       path = efreet_icon_path_find(e_config->icon_theme, ic->icon, inst->size);
                       if (!path)
                         {
                            if (e_util_strcmp(e_config->icon_theme, "hicolor"))
                              path = efreet_icon_path_find("hicolor", ic->icon, inst->size);
                         }
                    }
                  else if (ecore_file_exists(ic->icon))
                    {
                       path = ic->icon;
                    }
                  if (!path)
                    {
                       elm_image_file_set(ic->o_icon, e_theme_edje_file_get("base/theme/icons", "e/icons/unknown"),
                           "e/icons/unknown");
                       elm_image_file_set(ic->o_overlay, e_theme_edje_file_get("base/theme/icons", "e/icons/unknown"),
                           "e/icons/unknown");
                    }
                  else
                    {
                       elm_image_file_set(ic->o_icon, path, ic->key);
                       elm_image_file_set(ic->o_overlay, path, ic->key);
                    }
               }
             else
               {
                  elm_image_file_set(ic->o_icon, e_theme_edje_file_get("base/theme/icons", "e/icons/unknown"),
                           "e/icons/unknown");
                  elm_image_file_set(ic->o_overlay, e_theme_edje_file_get("base/theme/icons", "e/icons/unknown"),
                           "e/icons/unknown");
               }
          }
        inst->resize_job = NULL;
     }
}

static Eina_Bool
_grid_cb_update_icons(EINA_UNUSED void *data, EINA_UNUSED int ev_type, EINA_UNUSED void *ev)
{
   Instance *inst = NULL;
   Eina_List *l = NULL;

   EINA_LIST_FOREACH(luncher_instances, l, inst)
     {
        if (inst->bar) continue;
        if (inst->resize_job) return ECORE_CALLBACK_RENEW;
        inst->resize_job = ecore_job_add(_grid_resize_job, inst);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_grid_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->resize_job) return;
   inst->resize_job = ecore_job_add(_grid_resize_job, inst);
}

static void
_grid_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   e_object_del(E_OBJECT(inst->order));
   E_FREE_FUNC(inst->drop_handler, evas_object_del);
   luncher_instances = eina_list_remove(luncher_instances, inst);
   free(inst);
}

static void
_grid_drop_drop(void *data, const char *type, void *event_data)
{
   Instance *inst = data;
   E_Event_Dnd_Drop *ev = event_data;
   Efreet_Desktop *desktop = NULL;
   Eina_List *l = NULL;
   Icon *ic = NULL;

   evas_object_del(inst->place_holder);
   inst->place_holder = NULL;
   if (!strcmp(type, "enlightenment/desktop"))
     desktop = ev->data;
   else if (!strcmp(type, "enlightenment/border"))
     {
        E_Client *ec;

        ec = ev->data;
        desktop = ec->desktop;
        if (!desktop)
          {
             desktop = e_desktop_client_create(ec);
             efreet_desktop_save(desktop);
             e_desktop_edit(desktop);
          }
     }
   else if (!strcmp(type, "text/uri-list"))
     l = ev->data;

   ic = inst->drop_before;
   if (ic)
     {
        if (desktop)
          e_order_prepend_relative(inst->order, desktop, ic->desktop);
        else
          e_order_files_prepend_relative(inst->order, l, ic->desktop);
     }
   else
     {
        if (desktop)
          e_order_append(inst->order, desktop);
        else
          e_order_files_append(inst->order, l);
     }
}

static void
_grid_drop_leave(void *data, const char *type EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->inside = EINA_FALSE;
   evas_object_del(inst->place_holder);
   inst->place_holder = NULL;
}

static void
_grid_drop_move(void *data, const char *type EINA_UNUSED, void *event_data)
{
   Instance *inst = data;
   E_Event_Dnd_Move *ev = event_data;
   Evas_Coord x = ev->x, y = ev->y;
   Eina_List *l;
   Icon *ic;

   if (!inst->inside)
     return;

   EINA_LIST_FOREACH(inst->icons, l, ic)
     {
        Evas_Coord dx, dy, dw, dh;

        if (!ic->in_order) continue;
        evas_object_geometry_get(ic->o_layout, &dx, &dy, &dw, &dh);
        if (E_INSIDE(x, y, dx, dy, dw, dh))
          inst->drop_before = ic;
     }
   if (inst->drop_before)
     elm_gengrid_item_insert_before(inst->o_icon_con, &_grid_icon_class, inst->place_holder, inst->drop_before->o_layout, NULL, NULL);
}

static void
_grid_drop_enter(void *data, const char *type EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->inside = EINA_TRUE;
   inst->place_holder = evas_object_rectangle_add(evas_object_evas_get(inst->o_icon_con));
   evas_object_color_set(inst->place_holder, 0, 0, 0, 0);
   evas_object_size_hint_min_set(inst->place_holder, inst->size, inst->size);
   evas_object_size_hint_max_set(inst->place_holder, inst->size, inst->size);
   evas_object_show(inst->place_holder);
}

static void
_grid_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;
   char buf[4096];

   if (inst->o_main != event_data) return;
   if (e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s", inst->cfg->dir) >= sizeof(buf))
     return;

   luncher_config->items = eina_list_remove(luncher_config->items, inst->cfg);
   eina_stringshare_del(inst->cfg->dir);
   E_FREE(inst->cfg);
}

static void
_grid_anchor_changed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst && inst->o_icon_con)
     {
        grid_recalculate(inst);
     }
}

static void
_grid_order_update(void *data, E_Order *eo EINA_UNUSED)
{
   Instance *inst = data;

   if (inst && inst->o_icon_con)
     {
        grid_recalculate(inst);
     }
}

static void
_grid_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   char buf[4096];
   const char *drop[] = { "enlightenment/desktop", "enlightenment/border", "text/uri-list" };

   inst->o_icon_con = elm_gengrid_add(inst->o_main);
   elm_object_style_set(inst->o_icon_con, "no_inset_shadow");
   elm_gengrid_align_set(inst->o_icon_con, 0.5, 0.5);
   elm_gengrid_select_mode_set(inst->o_icon_con, ELM_OBJECT_SELECT_MODE_NONE);
   E_FILL(inst->o_icon_con);
   elm_layout_content_set(inst->o_main, "e.swallow.grid", inst->o_icon_con);
   evas_object_show(inst->o_icon_con);

   evas_object_data_set(inst->o_main, "instance", inst);
   evas_object_data_set(inst->o_icon_con, "instance", inst);

   e_gadget_configure_cb_set(inst->o_main, _grid_gadget_configure);
   evas_object_smart_callback_del_full(obj, "gadget_created", _grid_created_cb, data);

   if (!inst->cfg->dir)
     inst->cfg->dir = eina_stringshare_add("default");
   if (inst->cfg->dir[0] != '/')
     e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s/.order",
                         inst->cfg->dir);
   else
     eina_strlcpy(buf, inst->cfg->dir, sizeof(buf));

   inst->order = e_order_new(buf);
   e_order_update_callback_set(inst->order, _grid_order_update, inst);

   _grid_fill(inst);

   inst->drop_handler =
     e_gadget_drop_handler_add(inst->o_main, inst,
                        _grid_drop_enter, _grid_drop_move,
                        _grid_drop_leave, _grid_drop_drop,
                        drop, 3);
   elm_layout_content_set(inst->o_main, "e.swallow.drop", inst->drop_handler);
   evas_object_show(inst->drop_handler);

   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_RESIZE, _grid_resize, inst);
   _grid_aspect(inst);
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(luncher_config->items, l, ci)
          if (*id == ci->id) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(luncher_config->items)+1;
   else
     ci->id = -1;
   ci->dir = eina_stringshare_add("default");
   ci->style = NULL;
   luncher_config->items = eina_list_append(luncher_config->items, ci);

   return ci;
}

static void
_grid_recalculate_job(void *data)
{
   Instance *inst = data;

   if (inst)
     {
        if (inst->o_icon_con)
          {
             _grid_empty(inst);
             _grid_fill(inst);
          }
        inst->recalc_job = NULL;
     }
}

EINTERN void
grid_recalculate(Instance *inst)
{
   E_FREE_FUNC(inst->recalc_job, ecore_job_del);
   inst->recalc_job = ecore_job_add(_grid_recalculate_job, inst);
}

EINTERN void
grid_reorder(Instance *inst)
{
   char buf[4096];

   if (inst)
     {
        E_FREE_FUNC(inst->recalc_job, ecore_job_del);
        _grid_empty(inst);
        if (!inst->cfg->dir)
          inst->cfg->dir = eina_stringshare_add("default");
        if (inst->cfg->dir[0] != '/')
          e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s/.order",
                              inst->cfg->dir);
        else
          eina_strlcpy(buf, inst->cfg->dir, sizeof(buf));
        e_object_del(E_OBJECT(inst->order));
        inst->order = e_order_new(buf);
        e_order_update_callback_set(inst->order, _grid_order_update, inst);
        _grid_fill(inst);
     }
}

EINTERN Evas_Object *
grid_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->size = 0;
   inst->resize_job = NULL;
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->inside = EINA_FALSE;
   inst->bar = EINA_FALSE;

   _grid_icon_class.item_style = "luncher";
   _grid_icon_class.func.text_get = NULL;
   _grid_icon_class.func.content_get = _gengrid_icon_get;
   _grid_icon_class.func.state_get = NULL;
   _grid_icon_class.func.del = NULL;

   inst->o_main = elm_layout_add(parent);
   e_theme_edje_object_set(inst->o_main, "e/gadget/luncher/grid",
       "e/gadget/luncher/grid");
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, _grid_del, inst);
   evas_object_smart_callback_add(parent, "gadget_created", _grid_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_site_anchor", _grid_anchor_changed_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _grid_removed_cb, inst);
   evas_object_show(inst->o_main);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIG_ICON_THEME,
                         _grid_cb_update_icons, NULL);
   E_LIST_HANDLER_APPEND(handlers, EFREET_EVENT_ICON_CACHE_UPDATE,
                         _grid_cb_update_icons, NULL);

   if (inst->cfg->id < 0) return inst->o_main;
   luncher_instances = eina_list_append(luncher_instances, inst);

   return inst->o_main;
}
