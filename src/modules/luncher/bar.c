#include "luncher.h"
static Eina_List *handlers;
static Evas_Object *current_preview;
static Eina_Bool current_preview_menu;
static Eina_Bool _bar_icon_preview_show(void *data);
static Eina_Bool _bar_icon_preview_hide(void *data);
static void      _bar_icon_del(Instance *inst, Icon *ic);

static void
_bar_aspect(Instance *inst)
{
   switch (e_gadget_site_orient_get(e_gadget_site_get(inst->o_main)))
     {
      case E_GADGET_SITE_ORIENT_VERTICAL:
        evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, 1, eina_list_count(inst->icons));
        break;
      default:
        evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, eina_list_count(inst->icons), 1);
     }
}

static Eina_Bool
_bar_check_for_iconic(Icon *ic)
{
   Eina_List *l, *ll, *clients = NULL;
   E_Client *ec;
   E_Exec_Instance *ex;

   EINA_LIST_FOREACH(ic->execs, l, ex)
     {
        EINA_LIST_FOREACH(ex->clients, ll, ec)
          clients = eina_list_append(clients, ec);
     }
   EINA_LIST_FOREACH(ic->clients, l, ec)
     clients = eina_list_append(clients, ec);

   EINA_LIST_FREE(clients, ec)
     {
          if (ec->iconic)
            return EINA_TRUE;
     }
   return EINA_FALSE;
}

static Eina_Bool
_bar_check_for_duplicates(Icon *ic, E_Client *dupe)
{
   Eina_List *l, *ll, *clients = NULL;
   E_Client *ec;
   E_Exec_Instance *ex;

   EINA_LIST_FOREACH(ic->execs, l, ex)
     {
        EINA_LIST_FOREACH(ex->clients, ll, ec)
          clients = eina_list_append(clients, ec);
     }
   EINA_LIST_FOREACH(ic->clients, l, ec)
     clients = eina_list_append(clients, ec);

   EINA_LIST_FREE(clients, ec)
     {
          if (ec == dupe)
            return EINA_TRUE;
     }
   return EINA_FALSE;
}

static Eina_Bool
_bar_check_modifiers(Evas_Modifier *modifiers)
{
   if ((evas_key_modifier_is_set(modifiers, "Alt")) ||
       (evas_key_modifier_is_set(modifiers, "Control")) ||
       (evas_key_modifier_is_set(modifiers, "Shift")))
     return EINA_TRUE;
   return EINA_FALSE;
}

static Evas_Object *
_bar_gadget_configure(Evas_Object *g)
{
   if (!luncher_config) return NULL;
   if (luncher_config->config_dialog) return NULL;
   Instance *inst = evas_object_data_get(g, "instance");
   return config_luncher(e_zone_current_get(), inst, EINA_TRUE);
}

static void 
_bar_popup_dismissed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_FREE_FUNC(obj, evas_object_del);
}

static const char *
_bar_location_get(Instance *inst)
{
   const char *s = "float";
 
   E_Gadget_Site_Orient orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));
   E_Gadget_Site_Anchor anchor = e_gadget_site_anchor_get(e_gadget_site_get(inst->o_main)); 

   if (anchor & E_GADGET_SITE_ANCHOR_LEFT)
     {
        if (anchor & E_GADGET_SITE_ANCHOR_TOP)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "top";
                  break;
                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "left";
                  break;
                case E_GADGET_SITE_ORIENT_NONE:
                  s = "left";
                  break;
               }
          }
        else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "bottom";
                  break;
                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "left";
                  break;
                case E_GADGET_SITE_ORIENT_NONE:
                  s = "left";
                  break;
               }
          }
        else
          s = "left";
     }
   else if (anchor & E_GADGET_SITE_ANCHOR_RIGHT)
     {
        if (anchor & E_GADGET_SITE_ANCHOR_TOP)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "top";
                  break;
                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "right";
                  break;
                case E_GADGET_SITE_ORIENT_NONE:
                  s = "right";
                  break;
               }
          }
        else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "bottom";
                  break;
                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "right";
                  break;
                case E_GADGET_SITE_ORIENT_NONE:
                  s = "right";
                  break;
               }
          }
        else
          s = "right";
     }
   else if (anchor & E_GADGET_SITE_ANCHOR_TOP)
     s = "top";
   else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
     s = "bottom";
   else
     {
        switch (orient)
          {
           case E_GADGET_SITE_ORIENT_HORIZONTAL:
             s = "bottom";
             break;
           case E_GADGET_SITE_ORIENT_VERTICAL:
             s = "left";
             break;
           default: break;
          }
     }
   return s;
}

static Icon *
_bar_icon_match(Instance *inst, E_Client *ec)
{
   Icon *ic = NULL, *ic2 = NULL;
   Eina_Bool has_desktop = EINA_FALSE;
   char ori[32];

   if (ec->exe_inst)
     {
        if (ec->exe_inst->desktop)
          has_desktop = EINA_TRUE;
     }
   if (has_desktop)
     {
        ic = eina_hash_find(inst->icons_desktop_hash, ec->exe_inst->desktop->orig_path);
        if ((ic) && (ic2 = eina_hash_find(inst->icons_clients_hash, ec)))
          {
             ic2->execs = eina_list_remove(ic2->execs, ec->exe_inst);
             ic2->clients = eina_list_remove(ic2->clients, ec);
             if (!eina_list_count(ic2->execs) && !eina_list_count(ic2->clients))
               {
                  eina_hash_del(inst->icons_clients_hash, ec, ic2);
                  snprintf(ori, sizeof(ori), "e,state,off,%s", _bar_location_get(inst));
                  elm_layout_signal_emit(ic2->o_layout, ori, "e");
                  if (!ic2->in_order)
                    _bar_icon_del(inst, ic2);
               }
          }
     }
   if (has_desktop && !ic)
     ic = eina_hash_find(inst->icons_clients_hash, &ec);
   if (!ic)
     ic = eina_hash_find(inst->icons_clients_hash, &ec);

   return ic;
}

static void
_bar_instance_watch(void *data, E_Exec_Instance *ex, E_Exec_Watch_Type type)
{
   Icon *ic = data;
   char ori[32];

   switch (type)
     {
      case E_EXEC_WATCH_STARTED:
        if (ic->starting) elm_layout_signal_emit(ic->o_layout, "e,state,started", "e");
        ic->starting = EINA_FALSE;
        if (!ic->execs)
          {
             snprintf(ori, sizeof(ori), "e,state,on,%s", _bar_location_get(ic->inst));
             elm_layout_signal_emit(ic->o_layout, ori, "e");
          }
        if (!eina_list_data_find(ic->execs, ex))
          ic->execs = eina_list_append(ic->execs, ex);
        break;
      default:
        break;
     }
}

static void
_bar_icon_del(Instance *inst, Icon *ic)
{
   inst->icons = eina_list_remove(inst->icons, ic);
   if (ic->preview)
     _bar_icon_preview_hide(ic);
   _bar_aspect(inst);
   evas_object_del(ic->o_icon);
   evas_object_del(ic->o_overlay);
   evas_object_del(ic->o_layout);
   eina_hash_del_by_data(inst->icons_desktop_hash, ic);
   eina_hash_del_by_data(inst->icons_clients_hash, ic);
   if (ic->desktop)
     efreet_desktop_unref(ic->desktop);
   eina_list_free(ic->execs);
   eina_list_free(ic->clients);
   eina_stringshare_del(ic->icon);
   eina_stringshare_del(ic->key);
   if (ic->exec)
     e_exec_instance_watcher_del(ic->exec, _bar_instance_watch, ic);
   ic->exec = NULL;
   E_FREE_FUNC(ic->mouse_in_timer, ecore_timer_del);
   E_FREE_FUNC(ic->mouse_out_timer, ecore_timer_del);
   E_FREE(ic);
}

static void
_bar_icon_menu_settings_clicked(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Icon *ic = data;
   Evas_Object *popup = evas_object_data_get(obj, "popup");
   Evas_Object *box = evas_object_data_get(obj, "content");

   evas_object_del(box);
   elm_ctxpopup_dismiss(popup);
   e_gadget_configure(ic->inst->o_main);
}

static void
_bar_icon_menu_add_clicked(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Icon *ic = data;
   Evas_Object *popup = evas_object_data_get(obj, "popup");
   Evas_Object *box = evas_object_data_get(obj, "content");

   evas_object_del(box);
   elm_ctxpopup_dismiss(popup);
   if (ic->desktop)
     e_order_append(ic->inst->order, ic->desktop);
}

static void
_bar_icon_menu_remove_clicked(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Icon *ic = data;
   Evas_Object *popup = evas_object_data_get(obj, "popup");
   Evas_Object *box = evas_object_data_get(obj, "content");

   evas_object_del(box);
   elm_ctxpopup_dismiss(popup);
   if (ic->desktop)
     e_order_remove(ic->inst->order, ic->desktop);
}

static void
_bar_icon_menu_properties_clicked(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Icon *ic = data;
   Evas_Object *popup = evas_object_data_get(obj, "popup");
   Evas_Object *box = evas_object_data_get(obj, "content");

   evas_object_del(box);
   elm_ctxpopup_dismiss(popup);
   if (ic->desktop)
     e_desktop_edit(ic->desktop);
}

static void
_bar_icon_menu_action_clicked(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Efreet_Desktop_Action *action = (Efreet_Desktop_Action*)data;
   Evas_Object *popup = evas_object_data_get(obj, "popup");
   Evas_Object *box = evas_object_data_get(obj, "content");

   evas_object_del(box);
   elm_ctxpopup_dismiss(popup);
   e_exec(e_zone_current_get(), NULL, action->exec, NULL, "luncher");
}

static void
_bar_icon_menu_icon_mouse_out(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   elm_layout_signal_emit(obj, "e,state,unfocused", "e");
}

static void
_bar_icon_menu_icon_mouse_in(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data)
{
   Evas_Event_Mouse_In *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_bar_check_modifiers(ev->modifiers)) return;

   elm_layout_signal_emit(obj, "e,state,focused", "e");
}

static Evas_Object *
_bar_icon_menu_item_new(Icon *ic, Evas_Object *popup, Evas_Object *parent, const char *name, const char *icon)
{
   const char *path = NULL, *k = NULL;
   char buf[4096];
   int len = 0;
   Evas_Object *layout, *label, *img;

   layout = elm_layout_add(parent);
   e_theme_edje_object_set(layout, "e/gadget/luncher/preview",
       "e/gadget/luncher/preview");
   evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_event_callback_add(layout, EVAS_CALLBACK_MOUSE_IN, _bar_icon_menu_icon_mouse_in, NULL);
   evas_object_event_callback_add(layout, EVAS_CALLBACK_MOUSE_OUT, _bar_icon_menu_icon_mouse_out, NULL);
   elm_layout_signal_emit(layout, "e,state,icon,menu", "e");
   elm_box_pack_end(parent, layout);
   evas_object_data_set(layout, "popup", popup);
   evas_object_data_set(layout, "content", parent);
   evas_object_show(layout);

   label = elm_label_add(layout);
   elm_object_style_set(label, "luncher_preview");
   elm_label_ellipsis_set(label, EINA_TRUE);
   elm_object_text_set(label, name);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   elm_layout_content_set(layout, "e.swallow.title", label);
   evas_object_show(label);

   img = elm_icon_add(layout);
   evas_object_size_hint_aspect_set(img, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   if (!icon)
     path = NULL;
   else if (strncmp(icon, "/", 1) && !ecore_file_exists(icon))
     {
        path = efreet_icon_path_find(e_config->icon_theme, icon, ic->inst->size);
        if (!path)
          {
             if (e_util_strcmp(e_config->icon_theme, "hicolor"))
             path = efreet_icon_path_find("hicolor", icon, ic->inst->size);
          }
     }
   else if (ecore_file_exists(icon))
     {
        path = icon;
     }
   if (!path)
     {
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
   elm_layout_content_set(layout, "e.swallow.icon", img);
   evas_object_show(img);

   elm_layout_sizing_eval(layout);

   return layout;
}

static void
_bar_icon_drag_done(E_Drag *drag, int dropped)
{
   Instance *inst = e_object_data_get(E_OBJECT(drag));

   efreet_desktop_unref(drag->data);
   if (!inst) return;
   evas_object_smart_callback_call(e_gadget_site_get(inst->o_main), "gadget_site_unlocked", NULL);
   if (!dropped)
     bar_recalculate(inst);
}

static Eina_Bool
_bar_icon_preview_hide(void *data)
{
   Icon *ic = data;

   if (!ic) return EINA_FALSE;

   ic->mouse_out_timer = NULL;

   if (!ic->preview || ic->preview_dismissed)
     return EINA_FALSE;

   E_FREE_FUNC(ic->preview_box, evas_object_del);
   E_FREE_FUNC(ic->preview_scroller, evas_object_del);
   elm_ctxpopup_dismiss(ic->preview);
   ic->preview_dismissed = EINA_TRUE;
   current_preview = NULL;
   current_preview_menu = EINA_FALSE;
   ic->active = EINA_FALSE;

   return EINA_FALSE;
}

static void
_bar_icon_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Icon *ic = data;
   Evas_Event_Mouse_Move *ev = event_data;
   int dx, dy;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_bar_check_modifiers(ev->modifiers)) return;

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

        _bar_icon_preview_hide(ic);
        ic->drag.dnd = 1;
        ic->drag.start = 0;

        evas_object_geometry_get(ic->o_icon, &x, &y, &w, &h);
        d = e_drag_new(x, y, drag_types, 1,
                       ic->desktop, -1, NULL, _bar_icon_drag_done);
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
_bar_drag_timer(void *data)
{
   Icon *ic = data;

   ic->drag_timer = NULL;
   ic->drag.start = 1;
   evas_object_smart_callback_call(e_gadget_site_get(ic->inst->o_main), "gadget_site_locked", NULL);
   return EINA_FALSE;
}

static void
_bar_icon_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Icon *ic = data;
   Evas_Event_Mouse_Up *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_bar_check_modifiers(ev->modifiers)) return;

   if (ev->button == 1)
     {
        E_FREE_FUNC(ic->mouse_in_timer, ecore_timer_del);
        E_FREE_FUNC(ic->mouse_out_timer, ecore_timer_del);
        ic->drag.x = ev->output.x;
        ic->drag.y = ev->output.y;
        E_FREE_FUNC(ic->drag_timer, ecore_timer_del);
        ic->drag_timer = ecore_timer_loop_add(.35, _bar_drag_timer, ic);
     }
   if (ev->button == 3)
     {
        Evas_Object *popup, *box, *item, *sep;
        Eina_List *l = NULL;
        Efreet_Desktop_Action *action;
        E_Gadget_Site_Orient orient = e_gadget_site_orient_get(e_gadget_site_get(ic->inst->o_main));

        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;

        popup = elm_ctxpopup_add(e_comp->elm);
        elm_object_style_set(popup, "noblock");
        evas_object_smart_callback_add(popup, "dismissed", _bar_popup_dismissed, NULL);
        evas_object_size_hint_min_set(popup, ic->inst->size, ic->inst->size);

        box = elm_box_add(popup);
        evas_object_size_hint_align_set(box, 0, 0);
        switch (orient)
          {
             case E_GADGET_SITE_ORIENT_HORIZONTAL:
               elm_box_horizontal_set(box, EINA_TRUE);
               break;
             case E_GADGET_SITE_ORIENT_VERTICAL:
               elm_box_horizontal_set(box, EINA_FALSE);
               break;
             default:
               elm_box_horizontal_set(box, EINA_FALSE);
          }
          
        if (ic->desktop)
          {
             if (ic->desktop->actions)
               {
                  EINA_LIST_FOREACH(ic->desktop->actions, l, action)
                    {
                       item = _bar_icon_menu_item_new(ic, popup, box, action->name, action->icon);
                       evas_object_event_callback_add(item, EVAS_CALLBACK_MOUSE_UP, _bar_icon_menu_action_clicked, action);
                    }
                  sep = elm_separator_add(box);
                  switch (orient)
                    {
                       case E_GADGET_SITE_ORIENT_HORIZONTAL:
                         elm_separator_horizontal_set(item, EINA_TRUE);
                         break;
                       case E_GADGET_SITE_ORIENT_VERTICAL:
                         elm_separator_horizontal_set(item, EINA_FALSE);
                         break;
                       default:
                         elm_separator_horizontal_set(item, EINA_FALSE);
                    }
                  elm_box_pack_end(box, sep);
                  evas_object_show(sep);
               }
          }
        if (ic->desktop)
          {
             item = _bar_icon_menu_item_new(ic, popup, box, _("Icon Properties"), "preferences-applications");
             evas_object_event_callback_add(item, EVAS_CALLBACK_MOUSE_UP, _bar_icon_menu_properties_clicked, ic);
          }
        if (ic->in_order)
          {
             item = _bar_icon_menu_item_new(ic, popup, box, _("Remove From Bar"), "list-remove");
             evas_object_event_callback_add(item, EVAS_CALLBACK_MOUSE_UP, _bar_icon_menu_remove_clicked, ic);
          }
        else if (!ic->in_order && ic->desktop)
          {
             item = _bar_icon_menu_item_new(ic, popup, box, _("Add To Bar"), "list-add");
             evas_object_event_callback_add(item, EVAS_CALLBACK_MOUSE_UP, _bar_icon_menu_add_clicked, ic);
          }
        item = _bar_icon_menu_item_new(ic, popup, box, _("Luncher Settings"), "configure");
        evas_object_event_callback_add(item, EVAS_CALLBACK_MOUSE_UP, _bar_icon_menu_settings_clicked, ic);

        elm_object_content_set(popup, box);
        evas_object_show(box);

        e_gadget_util_ctxpopup_place(ic->inst->o_main, popup, ic->o_layout);
        e_comp_object_util_autoclose(popup, NULL, NULL, NULL);
        evas_object_layer_set(popup, E_LAYER_POPUP);
        evas_object_show(popup);
     }
}

static void
_bar_icon_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Icon *ic = data;
   Evas_Event_Mouse_Up *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_bar_check_modifiers(ev->modifiers)) return;

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
             e_exec_instance_watcher_add(ex, _bar_instance_watch, ic);
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
   else if (ev->button == 1 && !ic->in_order)
     {
        _bar_icon_preview_show(ic);
     }
}

static void
_bar_icon_preview_item_mouse_out(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   elm_layout_signal_emit(obj, "e,state,unfocused", "e");
}

static void
_bar_icon_preview_item_mouse_in(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data)
{
   Evas_Event_Mouse_In *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_bar_check_modifiers(ev->modifiers)) return;

   elm_layout_signal_emit(obj, "e,state,focused", "e");
}

static void
_bar_icon_preview_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Icon *ic = data;

   if (current_preview_menu)
     return;
   E_FREE_FUNC(ic->mouse_out_timer, ecore_timer_del);
   ic->mouse_out_timer = ecore_timer_loop_add(0.25, _bar_icon_preview_hide, ic);
}

static void
_bar_icon_preview_mouse_in(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Icon *ic = data;
   Evas_Event_Mouse_In *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_bar_check_modifiers(ev->modifiers)) return;

   E_FREE_FUNC(ic->mouse_out_timer, ecore_timer_del);
}

static void
_bar_icon_preview_menu_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Icon *ic = data;

   evas_object_event_callback_del_full(obj, EVAS_CALLBACK_HIDE, _bar_icon_preview_menu_hide, ic);
   if (ic)
     {
        if (ic->preview)
          {
             current_preview_menu = EINA_FALSE;
             _bar_icon_preview_mouse_out(ic, NULL, NULL, NULL);
          }
     }
}

static void
_bar_icon_preview_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   E_Client *ec = data;
   Evas_Event_Mouse_Up *ev = event_data;
   Icon *ic = evas_object_data_get(current_preview, "icon");

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_bar_check_modifiers(ev->modifiers)) return;

   if (ev->button == 3)
     {
        e_int_client_menu_show(ec, ev->canvas.x, ev->canvas.y, 0, ev->timestamp);
        evas_object_event_callback_add(ec->border_menu->comp_object, EVAS_CALLBACK_HIDE,
            _bar_icon_preview_menu_hide, ic);
        current_preview_menu = EINA_TRUE;
        return;
     }
   e_client_activate(ec, 1);
   _bar_icon_preview_hide(ic);
}

static void
_bar_icon_preview_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Object *img = data;

   evas_object_del(img);
}

static void
_bar_icon_preview_client_add(Icon *ic, E_Client *ec)
{
   Evas_Object *layout, *label, *img;

   layout = elm_layout_add(ic->preview_box);
   e_theme_edje_object_set(layout, "e/gadget/luncher/preview",
       "e/gadget/luncher/preview");
   evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_event_callback_add(layout, EVAS_CALLBACK_MOUSE_IN,
       _bar_icon_preview_item_mouse_in, ic);
   evas_object_event_callback_add(layout, EVAS_CALLBACK_MOUSE_OUT,
       _bar_icon_preview_item_mouse_out, ic);
   elm_box_pack_end(ic->preview_box, layout);
   evas_object_show(layout);

   label = elm_label_add(layout);
   elm_object_style_set(label, "luncher_preview");
   elm_label_ellipsis_set(label, EINA_TRUE);
   elm_object_text_set(label, e_client_util_name_get(ec));
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   elm_layout_content_set(layout, "e.swallow.title", label);
   evas_object_show(label);

   img = e_comp_object_util_mirror_add(ec->frame);
   edje_extern_object_aspect_set(img, EDJE_ASPECT_CONTROL_BOTH, ec->client.w, ec->client.h);
   elm_layout_content_set(layout, "e.swallow.icon", img);
   evas_object_show(img);

   if (!(ec->desk->visible) || (ec->iconic))
     elm_layout_signal_emit(layout, "e,state,invisible", "e");

   evas_object_event_callback_add(layout, EVAS_CALLBACK_MOUSE_UP,
       _bar_icon_preview_mouse_up, ec);
   evas_object_event_callback_add(layout, EVAS_CALLBACK_DEL,
       _bar_icon_preview_del, img);
   elm_layout_sizing_eval(layout);
}

static Eina_Bool
_bar_icon_preview_show(void *data)
{
   Icon *ic = data;
   Eina_List *l, *ll;
   E_Client *ec;
   E_Exec_Instance *ex;
   Eina_List *clients = NULL;
   E_Gadget_Site_Orient orient;
   E_Zone *zone = e_zone_current_get();
   int count = 0;

   if (!ic)
     return EINA_FALSE;
   ic->mouse_in_timer = NULL;
   if (ic->drag.dnd)
     return EINA_FALSE;
   if (!ic->inst)
     return EINA_FALSE;
   if (!ic->inst->o_icon_con)
     return EINA_FALSE;

   orient = e_gadget_site_orient_get(e_gadget_site_get(ic->inst->o_main));

   if (current_preview && (current_preview != ic->preview))
     _bar_icon_preview_hide(evas_object_data_get(current_preview, "icon"));
   if (ic->preview && !ic->preview_dismissed)
     _bar_icon_preview_hide(ic);
   ic->preview_dismissed = EINA_FALSE;
   if (!eina_list_count(ic->execs) && !eina_list_count(ic->clients))
     return EINA_FALSE;

   ic->preview = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(ic->preview, "noblock");
   evas_object_size_hint_min_set(ic->preview, ic->inst->size, ic->inst->size);
   evas_object_smart_callback_add(ic->preview, "dismissed", _bar_popup_dismissed, NULL);
   evas_object_event_callback_add(ic->preview, EVAS_CALLBACK_MOUSE_IN,
       _bar_icon_preview_mouse_in, ic);
   evas_object_event_callback_add(ic->preview, EVAS_CALLBACK_MOUSE_OUT,
       _bar_icon_preview_mouse_out, ic);

   ic->preview_scroller = elm_scroller_add(ic->preview);
   elm_scroller_content_min_limit(ic->preview_scroller, EINA_TRUE, EINA_TRUE);
   evas_object_size_hint_max_set(ic->preview_scroller, zone->w - 15, zone->h - 15);
   elm_object_style_set(ic->preview_scroller, "no_inset_shadow");
   E_EXPAND(ic->preview_scroller);
   
   ic->preview_box = elm_box_add(ic->preview);
   evas_object_size_hint_align_set(ic->preview_box, 0, 0);
   switch (orient)
     {
        case E_GADGET_SITE_ORIENT_HORIZONTAL:
          elm_box_horizontal_set(ic->preview_box, EINA_TRUE);
          break;
        case E_GADGET_SITE_ORIENT_VERTICAL:
          elm_box_horizontal_set(ic->preview_box, EINA_FALSE);
          break;
        default:
          elm_box_horizontal_set(ic->preview_box, EINA_FALSE);
     }
   EINA_LIST_FOREACH(ic->execs, l, ex)
     {
        EINA_LIST_FOREACH(ex->clients, ll, ec)
          clients = eina_list_append(clients, ec);
     }
   EINA_LIST_FOREACH(ic->clients, l, ec)
     clients = eina_list_append(clients, ec);

   EINA_LIST_FREE(clients, ec)
     {
        if (!ec->netwm.state.skip_taskbar && !e_client_util_ignored_get(ec))
          {
             if (!e_client_util_is_popup(ec))
               {
                  _bar_icon_preview_client_add(ic, ec);

                  count++;
               }
          }
     }

   if (!count)
     {
        _bar_icon_preview_hide(ic);
        return EINA_FALSE;
     }
   elm_object_content_set(ic->preview_scroller, ic->preview_box);
   elm_object_content_set(ic->preview, ic->preview_scroller);
   evas_object_show(ic->preview_box);

   e_gadget_util_ctxpopup_place(ic->inst->o_main, ic->preview, ic->o_layout);
   evas_object_layer_set(ic->preview, E_LAYER_POPUP);

   evas_object_data_del(ic->preview, "icon");
   evas_object_data_set(ic->preview, "icon", ic);
   evas_object_show(ic->preview);
   current_preview = ic->preview;

   return EINA_FALSE;
}

static void
_bar_icon_mouse_in(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data)
{
   Icon *ic = data;
   Eina_Bool clients = EINA_FALSE;
   Evas_Event_Mouse_In *ev = event_data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_bar_check_modifiers(ev->modifiers)) return;

   evas_object_raise(ic->o_layout);
   elm_object_tooltip_show(obj);
   ic->active = EINA_TRUE;

   E_FREE_FUNC(ic->mouse_out_timer, ecore_timer_del);
   E_FREE_FUNC(ic->mouse_in_timer, ecore_timer_del);
   if (eina_list_count(ic->execs) || eina_list_count(ic->clients))
     clients = EINA_TRUE;
   if (clients && current_preview && !current_preview_menu)
     _bar_icon_preview_show(ic);
   else if (_bar_check_for_iconic(ic))
     _bar_icon_preview_show(ic);
   else if (clients && !current_preview)
     ic->mouse_in_timer = ecore_timer_loop_add(0.3, _bar_icon_preview_show, ic);
}
  
static void
_bar_icon_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Icon *ic = data;

   elm_object_tooltip_hide(obj);
   E_FREE_FUNC(ic->mouse_in_timer, ecore_timer_del);
   E_FREE_FUNC(ic->mouse_out_timer, ecore_timer_del);
   ic->mouse_out_timer = ecore_timer_loop_add(0.25, _bar_icon_preview_hide, ic);
}

static void
_bar_exec_new_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_data EINA_UNUSED)
{ 
   Icon *ic = data;
   E_Client *ec = e_comp_object_client_get(obj);

   if (ic->preview)
     _bar_icon_preview_client_add(ic, ec);
   evas_object_event_callback_del_full(ec->frame, EVAS_CALLBACK_SHOW, _bar_exec_new_show, ic);
}

static void
_bar_icon_resized(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   //This code is supposed to adjust aspect correctly when there is an effect happening.  Uncomment to test.
   /*Instance *inst = data;
   Icon *ic = NULL;
   Eina_List *l = NULL;
   Evas_Coord aspect = 0, large = 0, w, h;

   if (!inst->effect) return;
   switch (e_gadget_site_orient_get(e_gadget_site_get(inst->o_main)))
     {
      case E_GADGET_SITE_ORIENT_VERTICAL:
        EINA_LIST_FOREACH(inst->icons, l, ic)
          {
             evas_object_geometry_get(ic->o_icon, 0, 0, &w, &h);
             if (w > large)
               large = w;
             aspect += h;
          }
        evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, large, aspect);
        break;
      default:
        EINA_LIST_FOREACH(inst->icons, l, ic)
          {
             evas_object_geometry_get(ic->o_icon, 0, 0, &w, &h);
             if (h > large)
               large = h;
             aspect += w;
          }
        evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, aspect, large);
     }*/
}

static Icon *
_bar_icon_add(Instance *inst, Efreet_Desktop *desktop, E_Client *non_desktop_client)
{
   const char *path = NULL, *k = NULL;
   char buf[4096], ori[32];
   int len = 0;
   Icon *ic;
   const Eina_List *l;
   Edje_Message_String *msg;

   ic = E_NEW(Icon, 1);
   if (desktop)
     efreet_desktop_ref(desktop);
   ic->desktop = desktop;
   ic->inst = inst;
   ic->preview = NULL;
   ic->preview_box = NULL;
   ic->preview_scroller = NULL;
   ic->mouse_in_timer = NULL;
   ic->mouse_out_timer = NULL;
   ic->active = EINA_FALSE;
   ic->starting = EINA_FALSE;
   ic->preview_dismissed = EINA_FALSE;
   ic->exec = NULL;

   ic->o_layout = elm_layout_add(inst->o_icon_con);
   e_theme_edje_object_set(ic->o_layout, "e/gadget/luncher/icon",
       "e/gadget/luncher/icon");
   E_EXPAND(ic->o_layout);
   E_FILL(ic->o_layout);
   edje_object_signal_callback_add(elm_layout_edje_get(ic->o_layout), "e,state,resized", "e", _bar_icon_resized, inst);
   elm_box_pack_end(inst->o_icon_con, ic->o_layout);
   evas_object_show(ic->o_layout);

   snprintf(ori, sizeof(ori), "e,state,off,%s", _bar_location_get(inst));
   elm_layout_signal_emit(ic->o_layout, ori, "e");
   msg = alloca(sizeof(Edje_Message_String));
   if (inst->cfg->style)
     msg->str = strdup(inst->cfg->style);
   edje_object_message_send(elm_layout_edje_get(ic->o_layout), EDJE_MESSAGE_STRING, 1, msg);
   free(msg->str);

   ic->o_icon = elm_icon_add(ic->o_layout);
   E_EXPAND(ic->o_icon);

   ic->o_overlay = elm_icon_add(ic->o_layout);
   E_EXPAND(ic->o_overlay);

   if (desktop)
     {
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
        if (path && desktop->icon && !k)
          {
             len = strlen(desktop->icon);
             if ((len > 4) && (!strcasecmp(desktop->icon + len - 4, ".edj")))
               k = "icon";
          }
     }
   else
     {
        Evas_Object *tmp;
        const char *file, *group;
        tmp = e_client_icon_add(non_desktop_client, evas_object_evas_get(ic->o_layout));
        e_icon_file_get(tmp, &file, &group);
        eina_stringshare_replace(&ic->icon, file);
        eina_stringshare_replace(&ic->key, group);
        path = ic->icon;
        k = ic->key;
        evas_object_del(tmp);
     }
   elm_image_file_set(ic->o_icon, path, k);

   if (desktop)
     elm_object_tooltip_text_set(ic->o_icon, desktop->name);
   else if (non_desktop_client && non_desktop_client->icccm.class)
     elm_object_tooltip_text_set(ic->o_icon, non_desktop_client->icccm.class);
   else if (non_desktop_client && non_desktop_client->icccm.name)
     elm_object_tooltip_text_set(ic->o_icon, non_desktop_client->icccm.name);
   else if (non_desktop_client && non_desktop_client->icccm.title)
     elm_object_tooltip_text_set(ic->o_icon, non_desktop_client->icccm.title);
   else if (non_desktop_client && non_desktop_client->netwm.name)
     elm_object_tooltip_text_set(ic->o_icon, non_desktop_client->netwm.name);
   else
     elm_object_tooltip_text_set(ic->o_icon, _("Unknown"));

   elm_object_tooltip_orient_set(ic->o_icon, ELM_TOOLTIP_ORIENT_CENTER);
   elm_object_tooltip_style_set(ic->o_icon, "luncher");
   evas_object_size_hint_aspect_set(ic->o_icon, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   elm_layout_content_set(ic->o_layout, "e.swallow.icon", ic->o_icon);
   evas_object_event_callback_add(ic->o_icon, EVAS_CALLBACK_MOUSE_UP,
       _bar_icon_mouse_up, ic);
   evas_object_event_callback_priority_add(ic->o_icon, EVAS_CALLBACK_MOUSE_DOWN, 0,
       _bar_icon_mouse_down, ic);
   evas_object_event_callback_add(ic->o_icon, EVAS_CALLBACK_MOUSE_MOVE,
       _bar_icon_mouse_move, ic);
   evas_object_event_callback_add(ic->o_icon, EVAS_CALLBACK_MOUSE_IN,
       _bar_icon_mouse_in, ic);
   evas_object_event_callback_add(ic->o_icon, EVAS_CALLBACK_MOUSE_OUT,
       _bar_icon_mouse_out, ic);
   evas_object_show(ic->o_icon);

   elm_image_file_set(ic->o_overlay, path, k);
   evas_object_size_hint_aspect_set(ic->o_overlay, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   elm_layout_content_set(ic->o_layout, "e.swallow.overlay", ic->o_overlay);
   evas_object_show(ic->o_overlay);

   if (desktop)
     eina_hash_add(inst->icons_desktop_hash, eina_stringshare_add(desktop->orig_path), ic);
   else
     eina_hash_add(inst->icons_clients_hash, &non_desktop_client, ic);

   if (desktop)
     {
        l = e_exec_desktop_instances_find(desktop);
        if (l)
          {
             snprintf(ori, sizeof(ori), "e,state,on,%s", _bar_location_get(inst));
             elm_layout_signal_emit(ic->o_layout, ori, "e");
             ic->execs = eina_list_clone(l);
          }
     }
   else
     {
        if (!_bar_check_for_duplicates(ic, non_desktop_client))
          ic->clients = eina_list_append(ic->clients, non_desktop_client);
     }
   elm_layout_sizing_eval(ic->o_layout);
   _bar_aspect(inst);
   return ic;
}

static Eina_Bool
_bar_cb_client_remove(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Client_Property *ev)
{
   Instance *inst = NULL;
   Eina_List *l = NULL;
   char ori[32];

   EINA_LIST_FOREACH(luncher_instances, l, inst)
     {
        Icon *ic = NULL;

        if (!inst->bar) continue;
        if (ev->ec) ic = _bar_icon_match(inst, ev->ec);
        if (ic)
          {
             if (ic->starting) elm_layout_signal_emit(ic->o_layout, "e,state,started", "e");
             ic->starting = EINA_FALSE;
             if (ev->ec)
               ic->clients = eina_list_remove(ic->clients, ev->ec);
             if (ev->ec->exe_inst)
               ic->execs = eina_list_remove(ic->execs, ev->ec->exe_inst);
             if (!eina_list_count(ic->execs) && !eina_list_count(ic->clients))
               {
                  snprintf(ori, sizeof(ori), "e,state,off,%s", _bar_location_get(inst));
                  elm_layout_signal_emit(ic->o_layout, ori, "e");
                  if (!ic->in_order)
                    _bar_icon_del(inst, ic);
               }
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_bar_cb_exec_del(void *data EINA_UNUSED, int type EINA_UNUSED, E_Exec_Instance *ex)
{
   Instance *inst = NULL;
   Eina_List *l = NULL;
   E_Client *ec = NULL;
   char ori[32];

   EINA_LIST_FOREACH(ex->clients, l, ec)
     {
        if (!ec->netwm.state.skip_taskbar)
          {
             break;
          }
     }

   EINA_LIST_FOREACH(luncher_instances, l, inst)
     {
        Icon *ic = NULL;

        if (!inst->bar) continue;
        if (ex->desktop)
          {
             ic = eina_hash_find(inst->icons_desktop_hash, ex->desktop->orig_path);
          }
        if (ic)
          {
             if (ic->starting) elm_layout_signal_emit(ic->o_layout, "e,state,started", "e");
             ic->starting = EINA_FALSE;
             ic->execs = eina_list_remove(ic->execs, ex);
             ic->clients = eina_list_remove(ic->clients, ec);
             if (!eina_list_count(ic->execs) && !eina_list_count(ic->clients))
               {
                  snprintf(ori, sizeof(ori), "e,state,off,%s", _bar_location_get(inst));
                  elm_layout_signal_emit(ic->o_layout, ori, "e");
                  if (!ic->in_order)
                    _bar_icon_del(inst, ic);
               }
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_bar_cb_exec_client_prop(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Client_Property *ev)
{
   Instance *inst = NULL;
   E_Client *ec = NULL;
   Eina_List *l = NULL;
   Eina_Bool skip;
   Eina_Bool has_desktop = EINA_TRUE;

   if (e_client_util_ignored_get(ev->ec))
     return ECORE_CALLBACK_RENEW; 
   if ((!(ev->property & E_CLIENT_PROPERTY_NETWM_STATE)) && (!(ev->property & E_CLIENT_PROPERTY_ICON))
       && (!(ev->property & E_CLIENT_PROPERTY_URGENCY)))
     return ECORE_CALLBACK_RENEW;

   if (!ev->ec->exe_inst)
     has_desktop = EINA_FALSE;
   if (ev->ec->exe_inst)
     {
        if (!ev->ec->exe_inst->desktop)
          has_desktop = EINA_FALSE;
     }
   if (has_desktop)
     {
        skip = EINA_TRUE;
        EINA_LIST_FOREACH(ev->ec->exe_inst->clients, l, ec)
          {
             if (!ec->netwm.state.skip_taskbar)
               {
                  skip = EINA_FALSE;
                  break;
               }
          }
     }
   else
     skip = ev->ec->netwm.state.skip_taskbar;

   EINA_LIST_FOREACH(luncher_instances, l, inst)
     {
        Icon *ic = NULL;
        char ori[32];

        if (!inst->bar) continue;
        ic = _bar_icon_match(inst, ev->ec);
        if (skip && !ic) continue;
        if (!skip)
          {
             if (ic)
               {
                  if ((ev->property & E_CLIENT_PROPERTY_URGENCY))
                    {
                       if (ev->ec->urgent)
                         elm_layout_signal_emit(ic->o_layout, "e,state,urgent", "e");
                       else
                         elm_layout_signal_emit(ic->o_layout, "e,state,normal", "e");
                       return ECORE_CALLBACK_RENEW;
                    }
                  if (ic->starting) elm_layout_signal_emit(ic->o_layout, "e,state,started", "e");
                  ic->starting = EINA_FALSE;
                  snprintf(ori, sizeof(ori), "e,state,on,%s", _bar_location_get(inst));
                  elm_layout_signal_emit(ic->o_layout, ori, "e");
                  if (has_desktop)
                    {
                       if (!(_bar_check_for_duplicates(ic, ev->ec)))
                         ic->execs = eina_list_append(ic->execs, ev->ec->exe_inst);
                    }
                  else
                    {
                       if (!(_bar_check_for_duplicates(ic, ev->ec)))
                         ic->clients = eina_list_append(ic->clients, ev->ec);
                    }
               }
             else
               {
                  if (has_desktop && !ev->ec->internal_elm_win)
                    ic = _bar_icon_add(inst, ev->ec->exe_inst->desktop, NULL);
                  else
                    ic = _bar_icon_add(inst, NULL, ev->ec);
                  snprintf(ori, sizeof(ori), "e,state,on,%s", _bar_location_get(inst));
                  elm_layout_signal_emit(ic->o_layout, ori, "e");
                  ic->in_order = EINA_FALSE;
                  inst->icons = eina_list_append(inst->icons, ic);
                  _bar_aspect(inst);
               }
          }
        else
          {
             if (has_desktop)
               ic->execs = eina_list_remove(ic->execs, ev->ec->exe_inst);
             else
               ic->clients = eina_list_remove(ic->clients, ev->ec);
             if (!eina_list_count(ic->execs) && !eina_list_count(ic->clients))
               {
                  if (!ic->in_order)
                    _bar_icon_del(inst, ic);
                  else
                    {
                       snprintf(ori, sizeof(ori), "e,state,off,%s", _bar_location_get(inst));
                       elm_layout_signal_emit(ic->o_layout, ori, "e");
                    }
               }
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_bar_cb_exec_new(void *data EINA_UNUSED, int type, E_Exec_Instance *ex)
{
   Instance *inst = NULL;
   E_Client *ec = NULL;
   Eina_List *l = NULL;
   Eina_Bool skip;

   if (type == E_EVENT_EXEC_NEW_CLIENT)
     {
        ec = eina_list_data_get(ex->clients);
        skip = ec->netwm.state.skip_taskbar;
     }
   else
     {
        skip = EINA_TRUE;
        EINA_LIST_FOREACH(ex->clients, l, ec)
          {
             if (!ec->netwm.state.skip_taskbar)
               {
                  skip = EINA_FALSE;
                  break;
               }
          }
     }
   EINA_LIST_FOREACH(luncher_instances, l, inst)
     {
        Icon *ic = NULL;
        char ori[32];

        if (!inst->bar) continue;
        if (ec) ic = _bar_icon_match(inst, ec);
        if (ic)
          {
             if (skip) continue;
             if (ic->starting) elm_layout_signal_emit(ic->o_layout, "e,state,started", "e");
             ic->starting = EINA_FALSE;
             snprintf(ori, sizeof(ori), "e,state,on,%s", _bar_location_get(inst));
             elm_layout_signal_emit(ic->o_layout, ori, "e");
             if (ex->desktop)
               {
                  if (!(_bar_check_for_duplicates(ic, ec)))
                    ic->execs = eina_list_append(ic->execs, ex);
                  if (evas_object_visible_get(ec->frame))
                    _bar_exec_new_show(ic, NULL, ec->frame, NULL);
                  else
                    evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW,
                        _bar_exec_new_show, ic);
               }
             else
               {
                  if (!(_bar_check_for_duplicates(ic, ec)))
                    ic->clients = eina_list_append(ic->clients, ec);
                  if (evas_object_visible_get(ec->frame))
                    _bar_exec_new_show(ic, NULL, ec->frame, NULL);
                  else
                    evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW,
                        _bar_exec_new_show, ic);
               }
          }
        else
          {
             if (skip) continue;
             if (ex->desktop && !ec->internal_elm_win)
               ic = _bar_icon_add(inst, ex->desktop, NULL);
             else
               ic = _bar_icon_add(inst, NULL, ec);
             snprintf(ori, sizeof(ori), "e,state,on,%s", _bar_location_get(inst));
             elm_layout_signal_emit(ic->o_layout, ori, "e");
             ic->in_order = EINA_FALSE;
             inst->icons = eina_list_append(inst->icons, ic);
             _bar_aspect(inst);
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_bar_empty(Instance *inst)
{
   if (inst->icons)
     {
        elm_box_clear(inst->o_icon_con);
        while (inst->icons)
          _bar_icon_del(inst, eina_list_data_get(inst->icons));
        inst->icons = NULL;
     }
}

static void
_bar_fill(Instance *inst)
{
   const Eina_Hash *execs = e_exec_instances_get();
   Eina_Iterator *it;
   Eina_List *l, *ll, *lll;   
   E_Exec_Instance *ex;
   E_Client *ec;
   Icon *ic;
   char ori[32];

   if (inst->order)
     {
        Efreet_Desktop *desktop;
        Eina_List *list;

        EINA_LIST_FOREACH(inst->order->desktops, list, desktop)
          {
             ic = _bar_icon_add(inst, desktop, NULL);
             ic->in_order = EINA_TRUE;
             inst->icons = eina_list_append(inst->icons, ic);
          }
     }
   it = eina_hash_iterator_data_new(execs);
   EINA_ITERATOR_FOREACH(it, l)
     {
        EINA_LIST_FOREACH(l, ll, ex)
          {
             Eina_Bool skip = EINA_TRUE;

             EINA_LIST_FOREACH(ex->clients, lll, ec)
               {
                  if (!ec->netwm.state.skip_taskbar)
                    {
                       skip = EINA_FALSE;
                    }
                  if (skip) continue;
                  ic = _bar_icon_match(inst, ec);
                  if (ic)
                    {
                       if (!(_bar_check_for_duplicates(ic, ec)))
                         ic->execs = eina_list_append(ic->execs, ex);
                       continue;
                    }
                  if (!ec->internal_elm_win)
                    ic = _bar_icon_add(inst, ex->desktop, NULL);
                  else
                    ic = _bar_icon_add(inst, NULL, ec);
                  snprintf(ori, sizeof(ori), "e,state,on,%s", _bar_location_get(inst));
                  elm_layout_signal_emit(ic->o_layout, ori, "e");
                  ic->in_order = EINA_FALSE;
                  inst->icons = eina_list_append(inst->icons, ic);
               }
          }
     }
   eina_iterator_free(it);
   E_CLIENT_FOREACH(ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        if (ec->netwm.state.skip_taskbar) continue;
        ic = _bar_icon_match(inst, ec);
        if (!ic)
          {
             if (ec->exe_inst && ec->exe_inst->desktop && !ec->internal_elm_win)
               ic = _bar_icon_add(inst, ec->exe_inst->desktop, NULL);
             else
               ic = _bar_icon_add(inst, NULL, ec);
             snprintf(ori, sizeof(ori), "e,state,on,%s", _bar_location_get(inst));
             elm_layout_signal_emit(ic->o_layout, ori, "e");
             ic->in_order = EINA_FALSE;
             inst->icons = eina_list_append(inst->icons, ic);
          }
     }
   _bar_aspect(inst);
}

static void
_bar_resize_job(void *data)
{
   Instance *inst = data;
   Eina_List *l;
   Icon *ic;
   E_Gadget_Site_Orient orient;
   Evas_Coord x, y, w, h, size;

   if (inst)
     {
        if (inst->effect) return;
        orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));
        elm_layout_sizing_eval(inst->o_main);
        evas_object_geometry_get(inst->o_main, &x, &y, &w, &h);
        switch (orient)
          {
             case E_GADGET_SITE_ORIENT_HORIZONTAL:
               size = h;
               break;
             case E_GADGET_SITE_ORIENT_VERTICAL:
               size = w;
               break;
             default:
               size = h;
          }
        inst->size = size;
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
_bar_cb_update_icons(EINA_UNUSED void *data, EINA_UNUSED int ev_type, EINA_UNUSED void *ev)
{
   Instance *inst = NULL;
   Eina_List *l = NULL;

   EINA_LIST_FOREACH(luncher_instances, l, inst)
     {
        if (!inst->bar) continue;
        if (inst->resize_job) return ECORE_CALLBACK_RENEW;
        inst->resize_job = ecore_job_add(_bar_resize_job, inst);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_bar_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->resize_job) return;
   inst->resize_job = ecore_job_add(_bar_resize_job, inst);
}

static void
_bar_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Coord px, py;
   Eina_List *l = NULL;
   Icon *ic = NULL;
   const char *position = _bar_location_get(inst);
   int pos;

   inst->effect = EINA_TRUE;

   if (eina_streq(position, "left"))
     pos = 0;
   else if (eina_streq(position, "top"))
     pos = 1;
   else if (eina_streq(position, "right"))
     pos = 2;
   else
     pos = 3;

   evas_pointer_canvas_xy_get(evas_object_evas_get(inst->o_main), &px, &py);

   EINA_LIST_FOREACH(inst->icons, l, ic)
     {
        Edje_Message_Int_Set *msg;
        Evas_Coord x, y, w, h;

        evas_object_geometry_get(ic->o_icon, &x, &y, &w, &h);
        msg = alloca(sizeof(Edje_Message_Int_Set) + (7 * sizeof(int)));
        msg->count = 7;
        msg->val[0] = px;
        msg->val[1] = py;
        msg->val[2] = x;
        msg->val[3] = y;
        msg->val[4] = w;
        msg->val[5] = h;
        msg->val[6] = pos;
        edje_object_message_send(elm_layout_edje_get(ic->o_layout), EDJE_MESSAGE_INT_SET, 2, msg);
     }
}

static void
_bar_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Coord px, py, x, y, w, h;
   Eina_List *l = NULL;
   Icon *ic = NULL;

   evas_pointer_canvas_xy_get(evas_object_evas_get(inst->o_main), &px, &py);
   evas_object_geometry_get(inst->o_main, &x, &y, &w, &h);

   if (E_INSIDE(px, py, x, y, w, h)) return;

   EINA_LIST_FOREACH(inst->icons, l, ic)
     {
        elm_layout_signal_emit(ic->o_layout, "e,state,default", "e");
        elm_layout_signal_emit(ic->o_layout, "e,state,unfocused", "e");
     }
   inst->effect = EINA_FALSE;
   _bar_aspect(inst);
}

static void
_bar_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   e_object_del(E_OBJECT(inst->order));
   E_FREE_FUNC(inst->drop_handler, evas_object_del);
   eina_hash_free(inst->icons_desktop_hash);
   eina_hash_free(inst->icons_clients_hash);
   luncher_instances = eina_list_remove(luncher_instances, inst);
   free(inst);
}

static void
_bar_drop_drop(void *data, const char *type, void *event_data)
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
_bar_drop_leave(void *data, const char *type EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;

   inst->inside = EINA_FALSE;
   evas_object_del(inst->place_holder);
   inst->place_holder = NULL;
}

static void
_bar_drop_move(void *data, const char *type EINA_UNUSED, void *event_data)
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
   elm_box_unpack(inst->o_icon_con, inst->place_holder);
   if (inst->drop_before)
     elm_box_pack_before(inst->o_icon_con, inst->place_holder, inst->drop_before->o_layout);
}

static void
_bar_drop_enter(void *data, const char *type EINA_UNUSED, void *event_data EINA_UNUSED)
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
_bar_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;
   char buf[4096];

   if (inst->o_main != event_data) return;
   if (e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s", inst->cfg->dir) >= sizeof(buf))
     return;

   luncher_config->items = eina_list_remove(luncher_config->items, inst->cfg);
   eina_stringshare_del(inst->cfg->style);
   eina_stringshare_del(inst->cfg->dir);
   E_FREE(inst->cfg);
}

static void
_bar_iconify_end(void *data, Evas_Object *obj EINA_UNUSED, const char *sig EINA_UNUSED, const char *src EINA_UNUSED)
{
   E_Client *ec = data;

   evas_object_layer_set(ec->frame, ec->layer);
   ec->layer_block = 0;
   if (ec->iconic)
     evas_object_hide(ec->frame);
}

static Eina_Bool
_bar_iconify_start(void *data, Evas_Object *obj, const char *signal EINA_UNUSED)
{
   Instance *inst = data;
   Icon *ic = NULL;
   E_Client *ec;
   int ox, oy, ow, oh;

   ec = e_comp_object_client_get(obj);

   if (ec)
     ic = _bar_icon_match(inst, ec);

   if (!ic) return EINA_FALSE;

   ec->layer_block = 1;
   evas_object_layer_set(ec->frame, E_LAYER_CLIENT_PRIO);
   evas_object_geometry_get(ic->o_layout, &ox, &oy, &ow, &oh);
   e_comp_object_effect_set(ec->frame, "iconify/luncher");
   e_comp_object_effect_params_set(ec->frame, 1, (int[]){ec->x, ec->y, ec->w, ec->h, ox, oy, ow, oh}, 8);
   e_comp_object_effect_params_set(ec->frame, 0, (int[]){!!strcmp(signal, "e,action,iconify")}, 1);
   e_comp_object_effect_start(ec->frame, _bar_iconify_end, ec);
   return EINA_TRUE;
}

static void
_bar_anchor_changed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst && inst->o_icon_con)
     {
        bar_recalculate(inst);
     }
}

static void
_bar_order_update(void *data, E_Order *eo EINA_UNUSED)
{
   Instance *inst = data;

   if (inst && inst->o_icon_con)
     {
        bar_recalculate(inst);
     }
}

static void
_bar_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   char buf[4096];
   const char *drop[] = { "enlightenment/desktop", "enlightenment/border", "text/uri-list" };
   E_Gadget_Site_Orient orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   inst->o_icon_con = elm_box_add(inst->o_main);
   E_EXPAND(inst->o_icon_con);
   switch (orient)
     {
        case E_GADGET_SITE_ORIENT_HORIZONTAL:
           elm_box_horizontal_set(inst->o_icon_con, EINA_TRUE);
           break;
        case E_GADGET_SITE_ORIENT_VERTICAL:
           elm_box_horizontal_set(inst->o_icon_con, EINA_FALSE);
           break;
         default:
           elm_box_horizontal_set(inst->o_icon_con, EINA_TRUE);
     }
   elm_layout_content_set(inst->o_main, "e.swallow.bar", inst->o_icon_con);
   evas_object_show(inst->o_icon_con);

   evas_object_data_set(inst->o_main, "instance", inst);
   evas_object_data_set(inst->o_icon_con, "instance", inst);

   e_gadget_configure_cb_set(inst->o_main, _bar_gadget_configure);
   evas_object_smart_callback_del_full(obj, "gadget_created", _bar_created_cb, data);

   if (!inst->cfg->dir)
     inst->cfg->dir = eina_stringshare_add("default");
   if (inst->cfg->dir[0] != '/')
     e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s/.order",
                         inst->cfg->dir);
   else
     eina_strlcpy(buf, inst->cfg->dir, sizeof(buf));

   if (!inst->cfg->style)
     inst->cfg->style = eina_stringshare_add("default");

   inst->order = e_order_new(buf);
   e_order_update_callback_set(inst->order, _bar_order_update, inst);

   inst->iconify_provider = e_comp_object_effect_mover_add(80, "e,action,*iconify",
       _bar_iconify_start, inst);

   _bar_fill(inst);

   inst->drop_handler =
     e_gadget_drop_handler_add(inst->o_main, inst,
                        _bar_drop_enter, _bar_drop_move,
                        _bar_drop_leave, _bar_drop_drop,
                        drop, 3);
   elm_layout_content_set(inst->o_main, "e.swallow.drop", inst->drop_handler);
   evas_object_show(inst->drop_handler);

   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_RESIZE, _bar_resize, inst);
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
   ci->style = eina_stringshare_add("default");
   luncher_config->items = eina_list_append(luncher_config->items, ci);

   return ci;
}

static void
_bar_recalculate_job(void *data)
{
   Instance *inst = data;

   if (inst)
     {
        if (inst->o_icon_con)
          {
             _bar_empty(inst);
             _bar_fill(inst);
          }   
        inst->recalc_job = NULL;
     }
}

EINTERN void
bar_recalculate(Instance *inst)
{
   E_FREE_FUNC(inst->recalc_job, ecore_job_del);
   inst->recalc_job = ecore_job_add(_bar_recalculate_job, inst);
}

EINTERN void
bar_reorder(Instance *inst)
{
   char buf[4096];

   if (inst)
     {
        E_FREE_FUNC(inst->recalc_job, ecore_job_del);
        _bar_empty(inst);
        if (!inst->cfg->dir)
          inst->cfg->dir = eina_stringshare_add("default");
        if (inst->cfg->dir[0] != '/')
          e_user_dir_snprintf(buf, sizeof(buf), "applications/bar/%s/.order",
                              inst->cfg->dir);
        else
          eina_strlcpy(buf, inst->cfg->dir, sizeof(buf));
        e_object_del(E_OBJECT(inst->order));
        inst->order = e_order_new(buf);
        e_order_update_callback_set(inst->order, _bar_order_update, inst);
        _bar_fill(inst);
     }
}

EINTERN Evas_Object *
bar_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->size = 0;
   inst->resize_job = NULL;
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->inside = EINA_FALSE;
   inst->effect = EINA_FALSE;
   inst->bar = EINA_TRUE;
   inst->icons_desktop_hash = eina_hash_string_superfast_new(NULL);
   inst->icons_clients_hash = eina_hash_pointer_new(NULL);
   inst->o_main = elm_layout_add(parent);
   e_theme_edje_object_set(inst->o_main, "e/gadget/luncher/bar",
       "e/gadget/luncher/bar");
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_MOUSE_MOVE, _bar_mouse_move, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_MOUSE_OUT, _bar_mouse_out, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, _bar_del, inst);
   evas_object_smart_callback_add(parent, "gadget_created", _bar_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_site_anchor", _bar_anchor_changed_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", _bar_removed_cb, inst);
   evas_object_show(inst->o_main);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIG_ICON_THEME,
                         _bar_cb_update_icons, NULL);
   E_LIST_HANDLER_APPEND(handlers, EFREET_EVENT_ICON_CACHE_UPDATE,
                         _bar_cb_update_icons, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_EXEC_NEW,
                         _bar_cb_exec_new, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_EXEC_NEW_CLIENT,
                         _bar_cb_exec_new, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_PROPERTY,
                         _bar_cb_exec_client_prop, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_EXEC_DEL,
                         _bar_cb_exec_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_REMOVE,
                         _bar_cb_client_remove, NULL);

   if (inst->cfg->id < 0) return inst->o_main;
   luncher_instances = eina_list_append(luncher_instances, inst);

   current_preview = NULL;
   current_preview_menu = EINA_FALSE;

   return inst->o_main;
}

