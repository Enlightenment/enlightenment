#include "xkbswitch.h"

typedef struct _Instance
{
   Evas_Object     *o_main;
   Evas_Object     *o_xkbswitch;
   Evas_Object     *o_xkbflag;
   Evas_Object     *menu;
   Evas_Object     *popup;
   E_Gadget_Site_Orient orient;
   E_Config_XKB_Layout *layout;
} Instance;

static Eina_List *ginstances = NULL;
static Ecore_Event_Handler *xkbg_change_handle = NULL;

void
_xkbg_update_icon(int cur_group)
{
   Instance *inst;
   Eina_List *l;
   E_Config_XKB_Layout *cl;

   EINA_SAFETY_ON_NULL_RETURN(e_config->xkb.used_layouts);
   //INF("ui: %d", cur_group);
   cl = eina_list_nth(e_config->xkb.used_layouts, cur_group);
   EINA_SAFETY_ON_NULL_RETURN(cl);
   if (!e_config_xkb_layout_eq(cl, e_config->xkb.current_layout))
     {
        e_config_xkb_layout_free(e_config->xkb.current_layout);
        e_config->xkb.current_layout = e_config_xkb_layout_dup(cl);
     }

   if (e_config->xkb.only_label)
     {
        EINA_LIST_FOREACH(ginstances, l, inst)
          {
             if (!e_config_xkb_layout_eq(e_config->xkb.current_layout, inst->layout))
               inst->layout = e_config->xkb.current_layout;
             E_FREE_FUNC(inst->o_xkbflag, evas_object_del);
             e_theme_edje_object_set(inst->o_xkbswitch,
                                     "base/theme/modules/xkbswitch",
                                     "e/modules/xkbswitch/noflag");
             elm_layout_text_set(inst->o_xkbswitch,
                                 "e.text.label", cl->name);
          }
     }
   else
     {
        EINA_LIST_FOREACH(ginstances, l, inst)
          {
             if (!e_config_xkb_layout_eq(e_config->xkb.current_layout, inst->layout))
               inst->layout = e_config->xkb.current_layout;
             if (!inst->o_xkbflag)
               inst->o_xkbflag = e_icon_add(evas_object_evas_get(inst->o_xkbswitch));
             e_theme_edje_object_set(inst->o_xkbswitch,
                                     "base/theme/modules/xkbswitch",
                                     "e/modules/xkbswitch/main");
             e_xkb_e_icon_flag_setup(inst->o_xkbflag, cl->name);
             elm_layout_content_set(inst->o_xkbswitch, "e.swallow.flag",
                                    inst->o_xkbflag);
             elm_layout_text_set(inst->o_xkbswitch, "e.text.label",
                                 e_xkb_layout_name_reduce(cl->name));
          }
     }
}

static Eina_Bool
_xkbg_changed_state(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   _xkbg_update_icon(e_config->xkb.cur_group);
   return ECORE_CALLBACK_PASS_ON;
}

static Evas_Object *
_xkbg_gadget_configure(Evas_Object *g EINA_UNUSED)
{
   if (e_configure_registry_exists("keyboard_and_mouse/xkbswitch"))
     {
        e_configure_registry_call("keyboard_and_mouse/xkbswitch", NULL, NULL);
     }
   return NULL;
}

static void
_xkbg_cb_menu_set(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = evas_object_data_get(obj, "inst");
   Eina_List *l;
   int cur_group = -1, grp = -1;
   E_Config_XKB_Layout *cl2, *cl = data;

   if (inst->popup)
     elm_ctxpopup_dismiss(inst->popup);

   EINA_LIST_FOREACH(e_config->xkb.used_layouts, l, cl2)
     {
        grp++;
        if (cl2 == cl) cur_group = grp;
     }
   if (cur_group == -1) return;
   if (e_config_xkb_layout_eq(cl, e_xkb_layout_get())) return;
   e_xkb_layout_set(cl);
   e_config_xkb_layout_free(e_config->xkb.sel_layout);
   e_config->xkb.sel_layout = e_config_xkb_layout_dup(cl);
}

static void
_xkbg_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);
   inst->popup = NULL;
}

static void
_xkbg_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->popup = NULL;
}

static void
_xkbg_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Instance *inst = data;

   if (!inst) return;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button == 3)
     {
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        e_gadget_configure(inst->o_main);
     }
   else if ((ev->button == 1) && (inst->popup))
     {
        elm_ctxpopup_dismiss(inst->popup);
        return;
     }
   else if ((ev->button == 1) && (!inst->popup)) /* Left-click layout menu */
     {
        if (!e_config->xkb.dont_touch_my_damn_keyboard)
          {
             E_Config_XKB_Layout *cl, *cur;
             Eina_List *l;
             Elm_Object_Item *mi;
             char buf[4096], buf2[4096];

             cur = e_xkb_layout_get();

             inst->popup = elm_ctxpopup_add(e_comp->elm);
             elm_object_style_set(inst->popup, "noblock");
             evas_object_smart_callback_add(inst->popup, "dismissed", _xkbg_popup_dismissed, inst);
             evas_object_event_callback_add(inst->popup, EVAS_CALLBACK_DEL, _xkbg_popup_deleted, inst);

             inst->menu = elm_list_add(inst->popup);
             elm_list_select_mode_set(inst->menu, ELM_OBJECT_SELECT_MODE_ALWAYS);
             evas_object_data_set(inst->menu, "inst", inst);
             elm_object_content_set(inst->popup, inst->menu);
             E_EXPAND(inst->menu);
             E_FILL(inst->menu);

             /* Append all the layouts */
             EINA_LIST_FOREACH(e_config->xkb.used_layouts, l, cl)
               {
                  const char *name = cl->name;
                  Evas_Object *ic;

                  e_xkb_flag_file_get(buf, sizeof(buf), name);
                  if (cl->variant)
                    snprintf(buf2, sizeof(buf2), "%s (%s, %s)", cl->name, cl->model, cl->variant);
                  else
                    snprintf(buf2, sizeof(buf2), "%s (%s)", cl->name, cl->model);

                  ic = elm_icon_add(inst->menu);
                  elm_image_file_set(ic, buf, NULL);
                  evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
                  evas_object_show(ic);

                  mi = elm_list_item_append(inst->menu, buf2, ic, NULL, _xkbg_cb_menu_set, cl);

                  if (e_config_xkb_layout_eq(cur, cl))
                    elm_list_item_selected_set(mi, EINA_TRUE);
               }

             evas_object_show(inst->menu);
             evas_object_size_hint_min_set(inst->popup, 200 * e_scale, 100 * e_scale);
             e_gadget_util_ctxpopup_place(inst->o_main, inst->popup, inst->o_xkbswitch);
             evas_object_show(inst->popup);
          }
     }
   else if (ev->button == 2)
     e_xkb_layout_next();
}

static void
_xkbg_gadget_created_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->o_main)
     {
        e_gadget_configure_cb_set(inst->o_main, _xkbg_gadget_configure);
//        _xkbdswitch_orient(inst, e_gadget_site_orient_get(obj));

        inst->o_xkbswitch = elm_layout_add(inst->o_main);
        E_EXPAND(inst->o_xkbswitch);
        E_FILL(inst->o_xkbswitch);

        inst->layout = e_xkb_layout_get();
        if (e_config->xkb.only_label || (!inst->layout))
          e_theme_edje_object_set(inst->o_xkbswitch,
                             "base/theme/modules/xkbswitch",
                             "e/modules/xkbswitch/noflag");
        else
          e_theme_edje_object_set(inst->o_xkbswitch,
                             "base/theme/modules/xkbswitch",
                             "e/modules/xkbswitch/main");
        elm_layout_text_set(inst->o_xkbswitch, "e.text.label",
                                 inst->layout ? e_xkb_layout_name_reduce(inst->layout->name) : _("NONE"));
        if (inst->layout && (!e_config->xkb.only_label))
          {
             inst->o_xkbflag = e_icon_add(evas_object_evas_get(inst->o_xkbswitch));
             E_EXPAND(inst->o_xkbflag);
             E_FILL(inst->o_xkbflag);
             e_xkb_e_icon_flag_setup(inst->o_xkbflag, inst->layout->name);
             /* The icon is part of the gadget. */
             elm_layout_content_set(inst->o_xkbswitch, "e.swallow.flag",
                                    inst->o_xkbflag);
          }
        else inst->o_xkbflag = NULL;
        evas_object_event_callback_add(inst->o_xkbswitch, EVAS_CALLBACK_MOUSE_DOWN,
                                       _xkbg_cb_mouse_down, inst);
        elm_box_pack_end(inst->o_main, inst->o_xkbswitch);
        evas_object_show(inst->o_xkbswitch);
     }
   evas_object_smart_callback_del_full(obj, "gadget_created", _xkbg_gadget_created_cb, data);
}

static void
xkbg_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->popup)
     elm_ctxpopup_dismiss(inst->popup);
   ginstances = eina_list_remove(ginstances, inst);
   free(inst);
}

EINTERN Evas_Object *
xkbg_gadget_create(Evas_Object *parent, int *id EINA_UNUSED, E_Gadget_Site_Orient orient)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->o_main = elm_box_add(parent);
   inst->orient = orient;
   E_EXPAND(inst->o_main);
   E_FILL(inst->o_main);
   evas_object_size_hint_aspect_set(inst->o_main, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   evas_object_smart_callback_add(parent, "gadget_created", _xkbg_gadget_created_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL, xkbg_del, inst);

   ginstances = eina_list_append(ginstances, inst);
   return inst->o_main;
}

EINTERN void
xkbg_init(void)
{
   xkbg_change_handle = ecore_event_handler_add(E_EVENT_XKB_CHANGED, _xkbg_changed_state, NULL);
}

EINTERN void
xkbg_shutdown(void)
{
   if (_xkbg.evh) ecore_event_handler_del(_xkbg.evh);
   if (_xkbg.cfd) e_object_del(E_OBJECT(_xkbg.cfd));
   _xkbg.cfd = NULL;
   ecore_event_handler_del(xkbg_change_handle);
}

