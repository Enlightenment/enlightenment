#include "pager.h"

#define BUTTON_DRAG    0
#define BUTTON_NOPLACE 1
#define BUTTON_DESK    2

typedef struct _Config_Objects Config_Objects;
struct _Config_Objects
{
   Evas_Object     *general_page;
   Evas_Object     *popup_page;
   Evas_Object     *urgent_page;
   Evas_Object     *o_popup;
   Evas_Object     *o_popup_speed;
   Evas_Object     *o_popup_urgent;
   Evas_Object     *o_popup_urgent_stick;
   Evas_Object     *o_popup_urgent_focus;
   Evas_Object     *o_popup_urgent_speed;
   Evas_Object     *o_show_desk_names;
   Evas_Object     *o_popup_act_height;
   Evas_Object     *o_popup_height;
   Evas_Object     *o_drag_resist;
   Evas_Object     *o_btn_drag;
   Evas_Object     *o_btn_noplace;
   Evas_Object     *o_btn_desk;
   Evas_Object     *o_flip_desk;
   E_Grab_Dialog   *grab_dia;
   int              grab_btn;
   int              w, h;
};
Config_Objects *pager_gadget_config_objects = NULL;

static void
_config_close(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   cfg_dialog = NULL;
   E_FREE(pager_gadget_config_objects);
}

static void
_config_show_general(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_hide(pager_gadget_config_objects->popup_page);
   evas_object_hide(pager_gadget_config_objects->urgent_page);
   evas_object_show(pager_gadget_config_objects->general_page);
}

static void
_config_show_popup(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_hide(pager_gadget_config_objects->general_page);
   evas_object_hide(pager_gadget_config_objects->urgent_page);
   evas_object_show(pager_gadget_config_objects->popup_page);
}

static void
_config_show_urgent(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_hide(pager_gadget_config_objects->general_page);
   evas_object_hide(pager_gadget_config_objects->popup_page);
   evas_object_show(pager_gadget_config_objects->urgent_page);
}

static void
_config_value_changed(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   pager_config->popup =
       elm_check_state_get(pager_gadget_config_objects->o_popup);
   pager_config->popup_speed =
       elm_slider_value_get(pager_gadget_config_objects->o_popup_speed);
   pager_config->flip_desk = 
       elm_check_state_get(pager_gadget_config_objects->o_flip_desk);
   pager_config->popup_urgent = 
       elm_check_state_get(pager_gadget_config_objects->o_popup_urgent);
   pager_config->popup_urgent_stick = 
       elm_check_state_get(pager_gadget_config_objects->o_popup_urgent_stick);
   pager_config->popup_urgent_focus =
       elm_check_state_get(pager_gadget_config_objects->o_popup_urgent_focus);
   pager_config->popup_urgent_speed = 
       elm_slider_value_get(pager_gadget_config_objects->o_popup_urgent_speed);
   pager_config->show_desk_names = 
       elm_check_state_get(pager_gadget_config_objects->o_show_desk_names);
   pager_config->popup_height = 
       elm_slider_value_get(pager_gadget_config_objects->o_popup_height);
   pager_config->popup_act_height = 
       elm_slider_value_get(pager_gadget_config_objects->o_popup_act_height);
   pager_config->drag_resist = 
       elm_slider_value_get(pager_gadget_config_objects->o_drag_resist);
   _pager_cb_config_gadget_updated();
   _pager_cb_config_updated();
   e_config_save_queue();

   elm_object_disabled_set(pager_gadget_config_objects->o_popup_speed,
       !pager_config->popup);
   elm_object_disabled_set(pager_gadget_config_objects->o_popup_act_height,
       !pager_config->popup);
   elm_object_disabled_set(pager_gadget_config_objects->o_popup_height,
       !pager_config->popup);
   elm_object_disabled_set(pager_gadget_config_objects->o_popup_urgent_stick,
       !pager_config->popup_urgent);
   elm_object_disabled_set(pager_gadget_config_objects->o_popup_urgent_focus,
       !pager_config->popup_urgent);
   elm_object_disabled_set(pager_gadget_config_objects->o_popup_urgent_speed,
       !pager_config->popup_urgent);
}

static void
_config_update_btn(Evas_Object *button, const int mouse_button)
{
   char lbl[256];
   char *icon = NULL;
   Evas_Object *ic = NULL;

   switch (mouse_button)
     {
      case 0:
        snprintf(lbl, sizeof(lbl), _("Click to set"));
        break;
      case 1:
        if (e_config->mouse_hand == E_MOUSE_HAND_RIGHT)
          {
             snprintf(lbl, sizeof(lbl), _("Left button"));
             icon = "preferences-desktop-mouse-left";
          }
        else if (e_config->mouse_hand == E_MOUSE_HAND_LEFT)
          {
             snprintf(lbl, sizeof(lbl), _("Right button"));
             icon = "preferences-desktop-mouse-right";
          }
        else
          {
             snprintf(lbl, sizeof(lbl), _("Button %i"), mouse_button);
             icon = "preferences-desktop-mouse-extra";
          }
        break;
      case 2:
        snprintf(lbl, sizeof(lbl), _("Middle button"));
        icon = "preferences-desktop-mouse-middle";
        break;
      case 3:
        if (e_config->mouse_hand == E_MOUSE_HAND_RIGHT)
          {
             snprintf(lbl, sizeof(lbl), _("Right button"));
             icon = "preferences-desktop-mouse-right";
          }
        else if (e_config->mouse_hand == E_MOUSE_HAND_LEFT)
          {
             snprintf(lbl, sizeof(lbl), _("Left button"));
             icon = "preferences-desktop-mouse-left";
          }
        else
          {
             snprintf(lbl, sizeof(lbl), _("Button %i"), mouse_button);
             icon = "preferences-desktop-mouse-extra";
          }
        break;
      default:
        snprintf(lbl, sizeof(lbl), _("Button %i"), mouse_button);
        icon = "preferences-desktop-mouse-extra";
        break;
     }
   elm_object_text_set(button, lbl);
   if (icon)
     {
        ic = elm_icon_add(evas_object_evas_get(button));
        elm_icon_standard_set(ic, icon);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
     }
   elm_object_part_content_set(button, "icon", ic);
}

static void
_config_update_btns(void)
{
#if 0
   _config_update_btn(pager_gadget_config_objects->o_btn_drag, pager_config->btn_drag);
#endif

   _config_update_btn(pager_gadget_config_objects->o_btn_noplace, pager_config->btn_noplace);
   _config_update_btn(pager_gadget_config_objects->o_btn_desk, pager_config->btn_desk);
}

static void
_config_grab_window_del(void *data EINA_UNUSED)
{
   evas_object_show(cfg_dialog);
   _config_update_btns();
}

static Eina_Bool
_config_grab_cb_mouse_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Mouse_Button *ev;

   ev = event;

   if (ev->buttons == 3)
     {
        e_util_dialog_show(_("Attention"),
                           _("You cannot use the right mouse button in the<br>"
                             "gadget for this as it is already taken by internal<br>"
                             "code for context menus.<br>"
                             "This button only works in the popup."));
     }
   else
     {
        if (ev->buttons == pager_config->btn_drag)
          pager_config->btn_drag = 0;
        else if (ev->buttons == pager_config->btn_noplace)
          pager_config->btn_noplace = 0;
        else if (ev->buttons == pager_config->btn_desk)
          pager_config->btn_desk = 0;

        if (pager_gadget_config_objects->grab_btn == 1)
          pager_config->btn_drag = ev->buttons;
        else if (pager_gadget_config_objects->grab_btn == 2)
          pager_config->btn_noplace = ev->buttons;
        else
          pager_config->btn_desk = ev->buttons;
     }

   e_object_del(E_OBJECT(pager_gadget_config_objects->grab_dia));
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_config_grab_cb_key_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Key *ev;

   ev = event;
   if (!strcmp(ev->key, "Delete"))
     {
        if (pager_gadget_config_objects->grab_btn == 1)
          pager_config->btn_drag = 0;
        else if (pager_gadget_config_objects->grab_btn == 2)
          pager_config->btn_noplace = 0;
        else
          pager_config->btn_desk = 0;
     }
   e_object_del(E_OBJECT(pager_gadget_config_objects->grab_dia));
   return ECORE_CALLBACK_PASS_ON;
}

static void
_config_grab_window_show(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{

   evas_object_hide(cfg_dialog);
   pager_gadget_config_objects->grab_btn = 0;
   if ((long)data == BUTTON_DRAG)
     pager_gadget_config_objects->grab_btn = 1;
   else if ((long)data == BUTTON_NOPLACE)
     pager_gadget_config_objects->grab_btn = 2;

   pager_gadget_config_objects->grab_dia = e_grab_dialog_show(NULL, EINA_TRUE,
       _config_grab_cb_key_down, _config_grab_cb_mouse_down, NULL, NULL);
   e_object_del_attach_func_set(E_OBJECT(pager_gadget_config_objects->grab_dia), _config_grab_window_del);
}

static Evas_Object *
_config_create_pages(Evas_Object *parent)
{
   Evas_Object *m, *tb, *ow;
   int row = 4;

   m = elm_table_add(parent);
   E_EXPAND(m);
   evas_object_show(m);

   /* General Page */
   tb = elm_table_add(m);
   E_EXPAND(tb);
   evas_object_show(tb);

   ow = elm_check_add(tb);
   elm_object_text_set(ow, _("Flip desktop on mouse wheel"));
   evas_object_size_hint_align_set(ow, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(ow, pager_config->flip_desk);
   elm_table_pack(tb, ow, 0, 0, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_flip_desk = ow;

   ow = elm_check_add(tb);
   elm_object_text_set(ow, _("Always show desktop names"));
   evas_object_size_hint_align_set(ow, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(ow, pager_config->show_desk_names);
   elm_table_pack(tb, ow, 0, 1, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_show_desk_names = ow;

   ow = elm_label_add(tb);
   elm_object_text_set(ow, _("Resistance to dragging"));
   elm_table_pack(tb, ow, 0, 2, 1, 1);
   E_ALIGN(ow, 0.5, 0.5);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);

   ow = elm_slider_add(tb);
   elm_slider_min_max_set(ow, 0, 10);
   elm_slider_step_set(ow, 1);
   elm_slider_value_set(ow, pager_config->drag_resist);
   elm_slider_unit_format_set(ow, _("%.0f pixels"));
   elm_table_pack(tb, ow, 0, 3, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "delay,changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_drag_resist = ow;

#if 0
   ow = elm_label_add(tb);
   elm_object_text_set(ow, _("Select and Slide button"));
   elm_table_pack(tb, ow, 0, row, 1, 1);
   E_ALIGN(ow, 0.5, 0.5);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);
   row++;

   ow = elm_button_add(tb);
   elm_object_text_set(ow, _("Click to set"));
   evas_object_smart_callback_add(ow, "clicked",
       _config_grab_window_show, (void *)BUTTON_DRAG);
   elm_table_pack(tb, ow, 0, row, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);
   pager_gadget_config_objects->o_btn_drag = ow;
   row++;
#endif

   ow = elm_label_add(tb);
   elm_object_text_set(ow, _("Drag and Drop button"));
   elm_table_pack(tb, ow, 0, row, 1, 1);
   E_ALIGN(ow, 0.5, 0.5);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);
   row++;

   ow = elm_button_add(tb);
   elm_object_text_set(ow, _("Click to set"));
   evas_object_smart_callback_add(ow, "clicked",
       _config_grab_window_show, (void *)BUTTON_NOPLACE);
   elm_table_pack(tb, ow, 0, row, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);
   pager_gadget_config_objects->o_btn_noplace = ow;
   row++;

   ow = elm_label_add(tb);
   elm_object_text_set(ow, _("Drag whole desktop"));
   elm_table_pack(tb, ow, 0, row, 1, 1);
   E_ALIGN(ow, 0.5, 0.5);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);
   row++;

   ow = elm_button_add(tb);
   elm_object_text_set(ow, _("Click to set"));
   evas_object_smart_callback_add(ow, "clicked",
       _config_grab_window_show, (void *)BUTTON_DESK);
   elm_table_pack(tb, ow, 0, row, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);
   pager_gadget_config_objects->o_btn_desk = ow;

   _config_update_btns();

   elm_table_pack(m, tb, 0, 0, 1, 1);
   pager_gadget_config_objects->general_page = tb;

   /* Popup Page */
   tb = elm_table_add(m);
   E_EXPAND(tb);
   evas_object_show(tb);

   ow = elm_check_add(tb);
   elm_object_text_set(ow, _("Show popup on desktop change"));
   evas_object_size_hint_align_set(ow, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(ow, pager_config->show_desk_names);
   elm_table_pack(tb, ow, 0, 0, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_popup = ow;

   ow = elm_label_add(tb);
   elm_object_text_set(ow, _("Popup pager height"));
   elm_object_disabled_set(ow, !pager_config->popup);
   elm_table_pack(tb, ow, 0, 1, 1, 1);
   E_ALIGN(ow, 0.5, 0.5);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);

   ow = elm_slider_add(tb);
   elm_object_disabled_set(ow, !pager_config->popup);
   elm_slider_min_max_set(ow, 20, 200);
   elm_slider_step_set(ow, 1);
   elm_slider_value_set(ow, pager_config->popup_height);
   elm_slider_unit_format_set(ow, _("%.0f pixels"));
   elm_table_pack(tb, ow, 0, 2, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "delay,changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_popup_height = ow;
   
   ow = elm_label_add(tb);
   elm_object_text_set(ow, _("Popup duration"));
   elm_object_disabled_set(ow, !pager_config->popup);
   elm_table_pack(tb, ow, 0, 3, 1, 1);
   E_ALIGN(ow, 0.5, 0.5);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);

   ow = elm_slider_add(tb);
   elm_object_disabled_set(ow, !pager_config->popup);
   elm_slider_min_max_set(ow, 0.1, 10);
   elm_slider_step_set(ow, 0.1);
   elm_slider_value_set(ow, pager_config->popup_speed);
   elm_slider_unit_format_set(ow, _("%1.1f seconds"));
   elm_table_pack(tb, ow, 0, 4, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "delay,changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_popup_speed = ow;

   ow = elm_label_add(tb);
   elm_object_text_set(ow, _("Pager action popup height"));
   elm_object_disabled_set(ow, !pager_config->popup);
   elm_table_pack(tb, ow, 0, 5, 1, 1);
   E_ALIGN(ow, 0.5, 0.5);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);

   ow = elm_slider_add(tb);
   elm_object_disabled_set(ow, !pager_config->popup);
   elm_slider_min_max_set(ow, 20, 200);
   elm_slider_step_set(ow, 1);
   elm_slider_value_set(ow, pager_config->popup_act_height);
   elm_slider_unit_format_set(ow, _("%.0f pixels"));
   elm_table_pack(tb, ow, 0, 6, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "delay,changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_popup_act_height = ow;

   elm_table_pack(m, tb, 0, 0, 1, 1);
   pager_gadget_config_objects->popup_page = tb;

   /* Urgent Page */
   tb = elm_table_add(m);
   E_EXPAND(tb);
   evas_object_show(tb);

   ow = elm_check_add(tb);
   elm_object_text_set(ow, _("Show popup for urgent window"));
   evas_object_size_hint_align_set(ow, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(ow, pager_config->popup_urgent);
   elm_table_pack(tb, ow, 0, 0, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_popup_urgent = ow;

   ow = elm_check_add(tb);
   elm_object_disabled_set(ow, !pager_config->popup_urgent);
   elm_object_text_set(ow, _("Urgent popup sticks on screen"));
   evas_object_size_hint_align_set(ow, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(ow, pager_config->popup_urgent_stick);
   elm_table_pack(tb, ow, 0, 1, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_popup_urgent_stick = ow;

   ow = elm_check_add(tb);
   elm_object_disabled_set(ow, !pager_config->popup_urgent);
   elm_object_text_set(ow, _("Show popup for focused windows"));
   evas_object_size_hint_align_set(ow, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(ow, pager_config->popup_urgent_focus);
   elm_table_pack(tb, ow, 0, 2, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_popup_urgent_focus = ow;

   ow = elm_label_add(tb);
   elm_object_text_set(ow, _("Urgent Popup Duration"));
   elm_object_disabled_set(ow, !pager_config->popup_urgent);
   elm_table_pack(tb, ow, 0, 3, 1, 1);
   E_ALIGN(ow, 0.5, 0.5);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_show(ow);

   ow = elm_slider_add(tb);
   elm_object_disabled_set(ow, !pager_config->popup_urgent);
   elm_slider_min_max_set(ow, 0.1, 10);
   elm_slider_step_set(ow, 0.1);
   elm_slider_value_set(ow, pager_config->popup_urgent_speed);
   elm_slider_unit_format_set(ow, _("%1.1f seconds"));
   elm_table_pack(tb, ow, 0, 4, 1, 1);
   E_ALIGN(ow, EVAS_HINT_FILL, EVAS_HINT_FILL);
   E_WEIGHT(ow, EVAS_HINT_EXPAND, 0);
   evas_object_smart_callback_add(ow, "delay,changed",
       _config_value_changed, NULL);
   evas_object_show(ow);
   pager_gadget_config_objects->o_popup_urgent_speed = ow;

   elm_table_pack(m, tb, 0, 0, 1, 1);
   pager_gadget_config_objects->urgent_page = tb;

   return m;
}

EINTERN Evas_Object *
config_pager(E_Zone *zone)
{
   Evas_Object *popup, *tb, *lbl, *list, *fr;
   Elm_Object_Item *it;

   pager_gadget_config_objects = E_NEW(Config_Objects, 1);
   pager_gadget_config_objects->w = 0;
   pager_gadget_config_objects->h = 0;

   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   lbl = elm_label_add(tb);
   elm_object_style_set(lbl, "marker");
   evas_object_show(lbl);
   elm_object_text_set(lbl, _("Pager Configuration"));
   elm_table_pack(tb, lbl, 0, 0, 2, 1);

   list = elm_list_add(tb);
   E_ALIGN(list, 0, EVAS_HINT_FILL);
   E_WEIGHT(list, 0, EVAS_HINT_EXPAND);
   elm_table_pack(tb, list, 0, 1, 1, 1);
   elm_list_select_mode_set(list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_scroller_content_min_limit(list, 1, 1);
   it = elm_list_item_append(list, _("General"), NULL, NULL,
       _config_show_general, NULL);
   elm_list_item_selected_set(it, 1);
   it = elm_list_item_append(list, _("Popup"), NULL, NULL,
       _config_show_popup,  NULL);
   it = elm_list_item_append(list, _("Urgent"), NULL, NULL,
       _config_show_urgent, NULL);
   elm_list_go(list);
   evas_object_show(list);

   fr = elm_frame_add(tb);
   E_EXPAND(fr);
   elm_table_pack(tb, fr, 1, 1, 1, 1);
   evas_object_show(fr);
   
   elm_object_content_set(fr,
     _config_create_pages(fr));
   _config_show_general(NULL, NULL, NULL);

   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center_on_zone(popup, zone);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _config_close, NULL);

   return cfg_dialog = popup;
}

