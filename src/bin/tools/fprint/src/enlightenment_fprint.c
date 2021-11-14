#ifdef HAVE_CONFIG_H
#include "config.h"

#endif

/* NOTE: Respecting header order is important for portability.
 * Always put system first, then EFL, then your public header,
 * and finally your private one. */

#include <Ecore_Getopt.h>
#include <Elementary.h>
#include <Elementary_Cursor.h>
#include "eldbus_fprint_device.h"
#include "eldbus_fprint_manager.h"

Eldbus_Connection *conn;
Eldbus_Proxy *new_proxy;
Eldbus_Proxy *new_proxy1;
Eldbus_Pending *p;
Eldbus_Pending *p1;

Evas_Object *ly;
Evas_Object *ly_popup;
Evas_Object *win;
Evas_Object *lb_status;

const char *default_device = NULL;
const char *device_type = NULL;
const char *currentuser;
const char *currentfinger;
double enroll_count = 0.0;
int enroll_num;
int step = 1;
double enroll_count_value;

Eina_Value array;

static void enrolled_fingers_cb(Eldbus_Proxy *proxy, void *data, Eldbus_Pending *pending, Eldbus_Error_Info *error, Eina_Value *args);
static void _update_theme(void);
static void claim_device(Eldbus_Proxy *proxy, void *data, Eldbus_Pending *pending, Eldbus_Error_Info *error);
static void _enroll_stopp_cb(Eldbus_Proxy *proxy, void *data, Eldbus_Pending *pending, Eldbus_Error_Info *error);
static void _verify_stopp_cb(Eldbus_Proxy *proxy, void *data, Eldbus_Pending *pending, Eldbus_Error_Info *error);

const char*
_to_readable_fingername(void *data)
{
   const char *name;
   Eina_Strbuf *buffer = eina_strbuf_new();

   eina_strbuf_append(buffer, data);
   eina_strbuf_replace_all(buffer, "-", " ");
   eina_strbuf_replace(buffer, "right", "Right", 1);
   eina_strbuf_replace(buffer, "left", "Left", 1);
   name = eina_strbuf_string_get(buffer);

   return name;
}

const char*
_to_fprint_fingername(const char *data)
{
   const char *name;
   Eina_Strbuf *buffer = eina_strbuf_new();

   eina_strbuf_append(buffer, data);
   eina_strbuf_replace_all(buffer, " ", "-");
   eina_strbuf_replace(buffer, "Right", "right", 1);
   eina_strbuf_replace(buffer, "Left", "left", 1);
   name = eina_strbuf_string_get(buffer);

   return name;
}

static void
_close_verify_popup(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   fprint_device_enroll_stop_call(new_proxy1, _verify_stopp_cb, NULL);

   if (data) evas_object_del(data);

   edje_object_signal_emit(ly, "reset_finger", "reset_finger"); // for GROUP hands/left_hand/right/hand
   edje_object_signal_emit(ly, "not_enrolled_finger", "not_enrolled_finger"); // for GROUP finger
   fprint_device_list_enrolled_fingers_call(new_proxy1, enrolled_fingers_cb, NULL, currentuser);
   _update_theme();
}

static void
_close_enroll_popup(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   fprint_device_enroll_stop_call(new_proxy1, _enroll_stopp_cb, NULL);

   if (data)
     {
        evas_object_del(data);
        data = NULL;
     }

   edje_object_signal_emit(ly, "reset_finger", "reset_finger"); // for GROUP hands/left_hand/right/hand
   edje_object_signal_emit(ly, "not_enrolled_finger", "not_enrolled_finger"); // for GROUP finger
   //       enrolled__failed
   fprint_device_list_enrolled_fingers_call(new_proxy1, enrolled_fingers_cb, NULL, currentuser);
   _update_theme();
}

static void
_dismiss_hover(void *data, Evas_Object *obj EINA_UNUSED,
               void *event_info EINA_UNUSED)
{
   Evas_Object *hv = data;
   elm_hover_dismiss(hv);
}

static void
_enroll_prop_get(void *data EINA_UNUSED, Eldbus_Pending *pending EINA_UNUSED, const char *propname EINA_UNUSED, Eldbus_Proxy *proxy EINA_UNUSED, Eldbus_Error_Info *error_info, int value)
{
   if (error_info)
     {
        printf("MESSAGE _enroll_prop_get: %s\n", error_info->message);
        printf("ERROR _enroll_prop_get: %s\n", error_info->error);
        //TODO: display the error
     }
   else
     {
        enroll_num = value;
        enroll_count_value = 10 / (double)enroll_num;
     }
}

static void
_verify_start_cb(Eldbus_Proxy *proxy EINA_UNUSED, void *data, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error)
{
   Evas_Object *popup;

   if (error)
     {
        printf("MESSAGE _verify_start_cb: %s\n", error->message);
        printf("ERROR _verify_start_cb: %s\n", error->error);
        //TODO: display the error
        popup = data;
        evas_object_del(popup);
        popup = NULL;
    }
}

static void
_enroll_start_cb(Eldbus_Proxy *proxy EINA_UNUSED, void *data, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error)
{
   Evas_Object *popup = data;

   if (error)
     {
        printf("MESSAGE _enroll_start_cb: %s\n", error->message);
        printf("ERROR _enroll_start_cb: %s\n", error->error);
        //TODO: display the error
        if (popup) evas_object_del(popup);
        popup = NULL;
     }
}

static void
_popup_verify_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
   Evas_Object *popup, *box, *lb, *sep, *button;
   char buf[PATH_MAX];
   char buf1[PATH_MAX];
   const char *fingername;

   fingername = _to_readable_fingername(data);

   popup = elm_popup_add(win);
   elm_popup_scrollable_set(popup, EINA_FALSE);

   box = elm_box_add(popup);
   evas_object_show(box);

   lb = elm_label_add(box);
   elm_object_text_set(lb, "Verify:");
   evas_object_show(lb);
   elm_box_pack_end(box, lb);

   snprintf(buf, sizeof(buf), "<bigger>%s</bigger>", fingername);

   lb = elm_label_add(box);
   elm_object_text_set(lb, buf);
   evas_object_show(lb);
   elm_box_pack_end(box, lb);

   ly_popup = elm_layout_add(box);
   snprintf(buf, sizeof(buf), "%s/themes/enlightenment_fprint.edj", elm_app_data_dir_get());
   elm_layout_file_set(ly_popup, buf, "verify");
   evas_object_size_hint_weight_set(ly_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ly_popup, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(ly_popup);
   elm_box_pack_end(box, ly_popup);

   snprintf(buf1, sizeof(buf1), "please %s on device", device_type);
   lb = elm_label_add(box);
   elm_object_text_set(lb, buf1);
   evas_object_show(lb);
   elm_box_pack_end(box, lb);

   sep = elm_separator_add(box);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   evas_object_show(sep);
   elm_box_pack_end(box, sep);

   lb_status = elm_label_add(box);
   elm_object_text_set(lb_status, "<color=white>waiting for enroll</color>"); //TODO: swipe or press auslesen
   evas_object_show(lb_status);
   elm_box_pack_end(box, lb_status);

   sep = elm_separator_add(box);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   evas_object_show(sep);
   elm_box_pack_end(box, sep);

   button = elm_button_add(box);
   elm_object_text_set(button, "close");
   evas_object_smart_callback_add(button, "clicked", _close_verify_popup, popup);
   evas_object_show(button);
   elm_box_pack_end(box, button);

   elm_object_content_set(popup, box);

   evas_object_show(popup);

   evas_object_smart_callback_add(popup, "block,clicked", _close_verify_popup, popup);
   fprint_device_verify_start_call(new_proxy1, _verify_start_cb, NULL, data);
}

static void
_popup_enroll_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
   Evas_Object *popup, *box, *lb, *sep, *button;
   char buf[PATH_MAX];
   char buf1[PATH_MAX];
   const char *fingername;

   fingername = _to_readable_fingername(data);

   popup = elm_popup_add(win);
   elm_popup_scrollable_set(popup, EINA_FALSE);
   evas_object_smart_callback_add(popup, "block,clicked", _close_enroll_popup, popup);

   box = elm_box_add(popup);
   evas_object_show(box);

   lb = elm_label_add(box);
   elm_object_text_set(lb, "Enroll:");
   evas_object_show(lb);
   elm_box_pack_end(box, lb);

   snprintf(buf, sizeof(buf), "<bigger>%s</bigger>", fingername);

   lb = elm_label_add(box);
   elm_object_text_set(lb, buf);
   evas_object_show(lb);
   elm_box_pack_end(box, lb);

   ly_popup = elm_layout_add(box);
   snprintf(buf, sizeof(buf), "%s/themes/enlightenment_fprint.edj", elm_app_data_dir_get());
   elm_layout_file_set(ly_popup, buf, "enroll");
   evas_object_size_hint_weight_set(ly_popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ly_popup, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(ly_popup);
   elm_box_pack_end(box, ly_popup);

   snprintf(buf1, sizeof(buf1), "please %s on device", device_type);
   lb = elm_label_add(box);
   elm_object_text_set(lb, buf1);
   evas_object_show(lb);
   elm_box_pack_end(box, lb);

   sep = elm_separator_add(box);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   evas_object_show(sep);
   elm_box_pack_end(box, sep);

   lb_status = elm_label_add(box);
   elm_object_text_set(lb_status, "<color=white>waiting for enroll</color>"); //TODO: swipe or press auslesen
   evas_object_show(lb_status);
   elm_box_pack_end(box, lb_status);

   sep = elm_separator_add(box);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_weight_set(sep, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, 0.0);
   evas_object_show(sep);
   elm_box_pack_end(box, sep);

   button = elm_button_add(box);
   elm_object_text_set(button, "close");
   evas_object_smart_callback_add(button, "clicked", _close_enroll_popup, popup);
   evas_object_show(button);
   elm_box_pack_end(box, button);

   elm_object_content_set(popup, box);

   evas_object_show(popup);

   fprint_device_enroll_start_call(new_proxy1, _enroll_start_cb, popup, data);
}

static void
delete_selected_finger(Eldbus_Proxy *proxy EINA_UNUSED, void *data, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error)
{
   if (error)
     {
       printf("MESSAGE delete_selected_finger: %s\n", error->message);
       printf("ERROR delete_selected_finger: %s\n", error->error);
    }
   else
     {
       edje_object_signal_emit(ly, "reset_finger", "reset_finger"); // for GROUP hands/left_hand/right/hand
       edje_object_signal_emit(ly, "not_enrolled_finger", "not_enrolled_finger"); // for GROUP finger
       fprint_device_list_enrolled_fingers_call(new_proxy1, enrolled_fingers_cb, NULL, currentuser);

       _dismiss_hover(data, NULL, NULL);
       _update_theme();
     }
}

static void
delete_all_finger(Eldbus_Proxy *prox EINA_UNUSED, void *data, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error)
{
   if (error)
     {
       printf("MESSAGE delete_all_finger: %s\n", error->message);
       printf("ERROR delete_all_finger: %s\n", error->error);
     }
   else
     {
       edje_object_signal_emit(ly, "reset_finger", "reset_finger"); // for GROUP hands/left_hand/right/hand
       edje_object_signal_emit(ly, "not_enrolled_finger", "not_enrolled_finger"); // for GROUP finger
       fprint_device_list_enrolled_fingers_call(new_proxy1, enrolled_fingers_cb, NULL, currentuser);

      _dismiss_hover(data, NULL, NULL);
    }
}

static void
_verify_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
           void *event_info EINA_UNUSED)
{
   _popup_verify_cb(data, NULL);
}

static void
_enroll_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   _popup_enroll_cb(data, NULL);
}

static void
_delete_selected_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   fprint_device_delete_enrolled_finger_call(new_proxy1, delete_selected_finger, NULL, data);
}

static void
_delete_all_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   fprint_device_delete_enrolled_fingers2_call(new_proxy1, delete_all_finger, data);
}

void
fingerprint_clicked_finger_mode(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *hv, *bt, *bx, *lb;
   char buf[PATH_MAX];
   const char *layout;
   const char *fingername;
   const char *txt;
   int i, found = 0;

   Evas_Object *left_list = evas_object_data_get(data, "left_list");
   Evas_Object *right_list = evas_object_data_get(data, "right_list");
   Elm_Object_Item *selected_item;

   selected_item = elm_list_selected_item_get(left_list);

   if (selected_item == NULL)
     selected_item = elm_list_selected_item_get(right_list);

   if (selected_item == NULL) return;

   fingername = _to_fprint_fingername(elm_object_item_text_get(selected_item));

   currentfinger = strdup(fingername);

   snprintf(buf, sizeof(buf), "<color=white>%s</color>", elm_object_item_text_get(selected_item));

   hv = elm_hover_add(win);
   bx = elm_box_add(win);

   elm_layout_file_get(ly, NULL, &layout);

   if (strcmp(layout, "finger") == 0)
     elm_object_part_content_set(hv, "middle", bx);
   else
     elm_object_part_content_set(hv, "bottom", bx);

   evas_object_show(bx);

   lb = elm_label_add(bx);
   elm_object_text_set(lb, buf);
   evas_object_show(lb);
   elm_box_pack_end(bx, lb);

   for (i = 0; i < (int)eina_value_array_count(&array); i++)
     {
        eina_value_array_get(&array, i, &txt);

        if (!strcmp(txt, fingername)) found = 1;
     }

   if (found == 1)
     {
        bt = elm_button_add(win);
        elm_object_text_set(bt, "verify");
        evas_object_smart_callback_add(bt, "clicked", _dismiss_hover, hv);
        evas_object_smart_callback_add(bt, "clicked", _verify_cb, fingername);
        elm_box_pack_end(bx, bt);
        evas_object_show(bt);

        bt = elm_button_add(win);
        elm_object_text_set(bt, "delete");
        evas_object_smart_callback_add(bt, "clicked", _dismiss_hover, hv);
        evas_object_smart_callback_add(bt, "clicked", _delete_selected_cb, fingername);
        elm_box_pack_end(bx, bt);
        evas_object_show(bt);
     }
   else
     {
        bt = elm_button_add(win);
        elm_object_text_set(bt, "enroll");
        evas_object_smart_callback_add(bt, "clicked", _dismiss_hover, hv);
        evas_object_smart_callback_add(bt, "clicked", _enroll_cb, fingername);

        elm_box_pack_end(bx, bt);
        evas_object_show(bt);
     }

   if (eina_value_array_count(&array) != 0)
     {
        bt = elm_button_add(win);
        elm_object_text_set(bt, "delete all");
        evas_object_smart_callback_add(bt, "clicked", _delete_all_cb, hv);
        elm_box_pack_end(bx, bt);
        evas_object_show(bt);
     }

   elm_hover_parent_set(hv, win);
   elm_hover_target_set(hv, obj);

   evas_object_show(hv);
}

void
fingerprint_clicked(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *hv, *bt, *bx, *lb;
   char buf[PATH_MAX];
   const char *layout;
   const char *fingername;
   const char *txt;
   int i, found = 0;

   currentfinger = data;

   printf("CURRENTFINGER: %s\n", currentfinger);

   fingername = _to_readable_fingername(data);
   snprintf(buf, sizeof(buf), "<color=white>%s</color>", fingername);

   hv = elm_hover_add(win);
   bx = elm_box_add(win);

   elm_layout_file_get(ly, NULL, &layout);

   if (strcmp(layout, "finger") == 0)
     elm_object_part_content_set(hv, "middle", bx);
   else
     elm_object_part_content_set(hv, "bottom", bx);

   evas_object_show(bx);

   lb = elm_label_add(bx);
   elm_object_text_set(lb, buf);
   evas_object_show(lb);
   elm_box_pack_end(bx, lb);

   for (i = 0; i < (int)eina_value_array_count(&array); i++)
     {
        eina_value_array_get(&array, i, &txt);

        if (!strcmp(txt, data)) found = 1;
     }

   if (found == 1)
     {
        bt = elm_button_add(win);
        elm_object_text_set(bt, "verify");
        evas_object_smart_callback_add(bt, "clicked", _dismiss_hover, hv);
        evas_object_smart_callback_add(bt, "clicked", _verify_cb, data);
        elm_box_pack_end(bx, bt);
        evas_object_show(bt);

        bt = elm_button_add(win);
        elm_object_text_set(bt, "delete");
        evas_object_smart_callback_add(bt, "clicked", _dismiss_hover, hv);
        evas_object_smart_callback_add(bt, "clicked", _delete_selected_cb, data);
        elm_box_pack_end(bx, bt);
        evas_object_show(bt);
     }
   else
     {
        bt = elm_button_add(win);
        elm_object_text_set(bt, "enroll");
        evas_object_smart_callback_add(bt, "clicked", _dismiss_hover, hv);
        evas_object_smart_callback_add(bt, "clicked", _enroll_cb, data);

        elm_box_pack_end(bx, bt);
        evas_object_show(bt);
     }

   if (eina_value_array_count(&array) != 0)
     {
        bt = elm_button_add(win);
        elm_object_text_set(bt, "delete all");
        evas_object_smart_callback_add(bt, "clicked", _delete_all_cb, hv);
        elm_box_pack_end(bx, bt);
        evas_object_show(bt);
     }

   elm_hover_parent_set(hv, win);
   elm_hover_target_set(hv, obj);

   evas_object_show(hv);
}

static void
_switch_hand(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf), "%s/themes/enlightenment_fprint.edj", elm_app_data_dir_get());
   elm_layout_file_set(ly, buf, data);

   fprint_device_list_enrolled_fingers_call(new_proxy1, enrolled_fingers_cb, NULL, currentuser);
   _update_theme();
}

static void
_finger_mode_select(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   const char *txt, *fingername;
   unsigned i;
   const Eina_List *l, *items;
   Elm_Object_Item *list_it, *selected_item;

   items = elm_list_items_get(data);

   EINA_LIST_FOREACH(items, l, list_it)
     {
        elm_list_item_selected_set(list_it, EINA_FALSE);
     }

   selected_item = elm_list_selected_item_get(obj);

   printf("FINGERNAME LIST FPRINT FINGERNAME1: %s\n", elm_object_item_text_get(selected_item));

   fingername = _to_fprint_fingername(elm_object_item_text_get(selected_item));

   printf("FINGERNAME LIST FPRINT FINGERNAME: %s\n", fingername);

   for (i = 0; i < eina_value_array_count(&array); i++)
     {
        eina_value_array_get(&array, i, &txt);

        printf("\t%s:%s\n", txt, fingername);

        if (!strcmp(txt, fingername))
          {
             edje_object_signal_emit(ly, "enrolled_finger", "enrolled_finger");
             break;
          }
        edje_object_signal_emit(ly, "not_enrolled_finger", "not_enrolled_finger");
     }
}

static void
_update_theme(void)
{
   Evas_Object *swallow_button, *right_list = NULL, *left_list, *leftright_list, *icon = NULL;
   char buf[PATH_MAX];

   // ALL 10 FINGERS
   // LEFT
   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "left-little-finger");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_left-little-finger", swallow_button);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "left-ring-finger");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_left-ring-finger", swallow_button);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "left-middle-finger");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_left-middle-finger", swallow_button);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "left-index-finger");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_left-index-finger", swallow_button);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "left-thumb");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_left-thumb", swallow_button);

   // RIGHT
   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "right-little-finger");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_right-little-finger", swallow_button);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "right-ring-finger");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_right-ring-finger", swallow_button);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "right-middle-finger");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_right-middle-finger", swallow_button);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "right-index-finger");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_right-index-finger", swallow_button);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");
   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked, "right-thumb");
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_right-thumb", swallow_button);

   // SWITCH LEFT/RIGHT HAND
   leftright_list = elm_list_add(win);
   elm_list_multi_select_set(leftright_list, EINA_FALSE);
   elm_list_select_mode_set(leftright_list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_size_hint_weight_set(leftright_list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   elm_list_mode_set(leftright_list, ELM_LIST_EXPAND);
   elm_list_item_append(leftright_list, "Left Hand", NULL, NULL, _switch_hand, "left_hand");
   elm_list_item_append(leftright_list, "Right Hand", NULL, NULL, _switch_hand, "right_hand");
   evas_object_show(leftright_list);
   elm_object_part_content_set(ly, "swallow_hand_switch", leftright_list);

   // ONE FINGER
   left_list = elm_list_add(win);
   elm_list_multi_select_set(left_list, EINA_FALSE);
   elm_list_select_mode_set(left_list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_size_hint_weight_set(left_list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_list_mode_set(left_list, ELM_LIST_EXPAND);

   right_list = elm_list_add(win);
   elm_list_multi_select_set(right_list, EINA_FALSE);
   elm_list_select_mode_set(right_list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_size_hint_weight_set(right_list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_list_mode_set(right_list, ELM_LIST_EXPAND);

   ////
   Eina_List *left_hand_list = NULL;
   Eina_List *right_hand_list = NULL;
   Eina_List *l;
   const char *fingername;
   unsigned i = 0;
   int found;
   const char *txt;
   const char *list_item;

   elm_list_clear(right_list);
   elm_list_clear(left_list);

   if (!eina_list_count(left_hand_list) || (eina_list_count(left_hand_list) == 0))
     {
        left_hand_list = eina_list_append(left_hand_list, "Left little finger");
        left_hand_list = eina_list_append(left_hand_list, "Left ring finger");
        left_hand_list = eina_list_append(left_hand_list, "Left middle finger");
        left_hand_list = eina_list_append(left_hand_list, "Left index finger");
        left_hand_list = eina_list_append(left_hand_list, "Left thumb");
     }

   if (!eina_list_count(right_hand_list) || (eina_list_count(right_hand_list) == 0))
     {
        right_hand_list = eina_list_append(right_hand_list, "Right little finger");
        right_hand_list = eina_list_append(right_hand_list, "Right ring finger");
        right_hand_list = eina_list_append(right_hand_list, "Right middle finger");
        right_hand_list = eina_list_append(right_hand_list, "Right index finger");
        right_hand_list = eina_list_append(right_hand_list, "Right thumb");
     }


   EINA_LIST_FOREACH(left_hand_list, l, list_item)
     {
        icon = elm_icon_add(win);
        snprintf(buf, sizeof(buf), "%s/themes/enlightenment_fprint.edj", elm_app_data_dir_get());
        elm_image_file_set(icon, buf, "icon");
        evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_min_set(icon, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
        evas_object_size_hint_max_set(icon, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
        evas_object_show(icon);

        printf("LEFT LIST ITEM:\n");
        found = 0;
        fingername = _to_fprint_fingername(list_item);

        for (i = 0; i < eina_value_array_count(&array); i++)
          {
             eina_value_array_get(&array, i, &txt);

             if (!strcmp(txt, fingername))
               {
                  printf("LEFT LIST ITEM: %s\n",fingername);
                  elm_list_item_append(left_list, fingername, icon, NULL, _finger_mode_select, right_list);
                  found = 1;
               }
          }

        if (found != 1)
          elm_list_item_append(left_list, fingername, NULL, NULL, _finger_mode_select, right_list);
     }

   EINA_LIST_FOREACH(right_hand_list, l, list_item)
     {
        icon = elm_icon_add(win);
        snprintf(buf, sizeof(buf), "%s/themes/enlightenment_fprint.edj", elm_app_data_dir_get());
        elm_image_file_set(icon, buf, "icon");
        evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_min_set(icon, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
        evas_object_size_hint_max_set(icon, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
        evas_object_show(icon);

        printf("RIGHT LIST ITEM:\n");
        found = 0;
        fingername = _to_fprint_fingername(list_item);
        for (i = 0; i < eina_value_array_count(&array); i++)
          {
             eina_value_array_get(&array, i, &txt);

             if (!strcmp(txt, fingername))
               {
                  printf("RIGHT LIST ITEM: %s\n",fingername);
                  elm_list_item_append(right_list, fingername, icon, NULL, _finger_mode_select, left_list);
                  found = 1;
               }
          }
        if (found != 1)
          elm_list_item_append(right_list, fingername, NULL, NULL, _finger_mode_select, left_list);
     }

   elm_list_go(left_list);
   evas_object_show(left_list);
   elm_object_part_content_set(ly, "swallow_select-finger-left", left_list);

   elm_list_go(right_list);
   evas_object_show(right_list);
   elm_object_part_content_set(ly, "swallow_select-finger-right", right_list);

   swallow_button = elm_button_add(win);
   elm_object_style_set(swallow_button, "blank");

   evas_object_data_set(swallow_button, "left_list", left_list);
   evas_object_data_set(swallow_button, "right_list", right_list);

   evas_object_smart_callback_add(swallow_button, "clicked", fingerprint_clicked_finger_mode, swallow_button);
   evas_object_show(swallow_button);
   elm_object_part_content_set(ly, "swallow_select-finger", swallow_button);
}

static void
_select_mode(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   char buf[PATH_MAX];

   snprintf(buf, sizeof(buf), "%s/themes/enlightenment_fprint.edj", elm_app_data_dir_get());
   elm_layout_file_set(ly, buf, data);

   fprint_device_list_enrolled_fingers_call(new_proxy1, enrolled_fingers_cb, NULL, currentuser);
   _update_theme();
}

/*
static void
get_devices(Eldbus_Proxy *proxy, void *data, Eldbus_Pending *pending, Eldbus_Error_Info *error, Eina_Value *args)
{
   if (error)
      printf("ERROR: %s\n", error->error);
   if (error)
   printf("MESSAGE: %s\n", error->message);
}*/

static void
get_default_device(Eldbus_Proxy *proxy EINA_UNUSED, void *data EINA_UNUSED, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error, const char *device)
{
   printf("DEFAULT: %s\n", device);

   if (device)
     default_device = strdup(device);

   printf("DDEFAULT: %s\n", default_device);
   if (error)
     printf("ERROR get_default_device: %s\n", error->error);
   if (error)
     printf("MESSAGE get_default_device: %s\n", error->message);
}

static void
enrolled_fingers_cb(Eldbus_Proxy *proxy EINA_UNUSED, void *data EINA_UNUSED, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error EINA_UNUSED, Eina_Value *args)
{
   const char *txt;
   unsigned i;

   eina_value_flush(&array);
   if (error)
     {
        printf("ERROR enrolled_fingers_cb: %s\n", error->error);
        printf("MESSAGE enrolled_fingers_cb: %s\n", error->message);
     }
   else
     {
        eina_value_struct_value_get(args, "arg0", &array);
        for (i = 0; i < eina_value_array_count(&array); i++)
          {
             eina_value_array_get(&array, i, &txt);
             edje_object_signal_emit(ly, "enrolled_finger", txt);
          }
     }
}

static void
get_device_proberties(void *data, Eldbus_Pending *pending EINA_UNUSED, const char *propname EINA_UNUSED, Eldbus_Proxy *proxy EINA_UNUSED, Eldbus_Error_Info *error_info EINA_UNUSED, const char *value)
{
   char buf[PATH_MAX];
   Evas_Object *lb = data;

   if (value)
     {
        printf("NAME: %s\n", value);
        snprintf(buf, sizeof(buf), "Device Name: <color=white>%s</color>", value);
        elm_object_text_set(lb, buf);
     }
   else
     elm_object_text_set(lb, "NO DEVICE");
}

static void
get_device_type(void *data, Eldbus_Pending *pending EINA_UNUSED, const char *propname EINA_UNUSED, Eldbus_Proxy *proxy EINA_UNUSED, Eldbus_Error_Info *error_info EINA_UNUSED, const char *value)
{
   char buf[PATH_MAX];
   Evas_Object *lb1 = data;

   if (value)
     {
        device_type = strdup(value);
        printf("Type: %s\n", device_type);
        snprintf(buf, sizeof(buf), "Device Type: <color=white>%s</color>", device_type);
        elm_object_text_set(lb1, buf);
     }
}

static void
retry_claim_device(void* data, Evas_Object* o EINA_UNUSED, void* event EINA_UNUSED)
{
   Evas_Object *notify = data;

   if (notify) evas_object_del(notify);

   fprint_device_claim_call(new_proxy1, claim_device, NULL, currentuser);
}

static void
claim_device(Eldbus_Proxy *proxy EINA_UNUSED, void *data EINA_UNUSED, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error)
{
   if (error)
     {
        printf("ERROR claim_device: %s\n", error->error);
        printf("MESSAGE claim_device: %s\n", error->message);

        Evas_Object *notify, *bx, *bxv, *o;

        notify = elm_notify_add(win);

        bx = elm_box_add(notify);

        o = elm_label_add(bx);
        elm_object_text_set(o, "Could not claim device<br>Please cancel all other fprint sessions<br>and press retry<br>");
        evas_object_show(o);
        elm_box_pack_end(bx, o);

        bxv = elm_box_add(notify);
        o = elm_button_add(bxv);
        elm_object_text_set(o, "retry");
        evas_object_smart_callback_add(o, "clicked", retry_claim_device, notify);
        evas_object_show(o);
        elm_box_pack_end(bxv, o);

        evas_object_show(bxv);

        elm_box_pack_end(bx, bxv);

        evas_object_show(bx);
        elm_object_content_set(notify, bx);
        evas_object_show(notify);
     }
}

static void
_enroll_stopp_cb(Eldbus_Proxy *proxy EINA_UNUSED, void *data EINA_UNUSED, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error)
{
   if (error)
     {
        printf("MESSAGE _enroll_stopp_cb: %s\n", error->message);
        printf("ERROR _enroll_stopp_cb: %s\n", error->error);
     }
}

static void
_verify_stopp_cb(Eldbus_Proxy *proxy EINA_UNUSED, void *data EINA_UNUSED, Eldbus_Pending *pending EINA_UNUSED, Eldbus_Error_Info *error)
{
   if (error)
     {
        printf("MESSAGE _verify_stopp_cb: %s\n", error->message);
        printf("ERROR _verify_stopp_cb: %s\n", error->error);
     }
}

static void
_restart_verify(void)
{
   fprint_device_verify_start_call(new_proxy1, _verify_start_cb, NULL, currentfinger);
}

static void
_verify_status(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   char buf[PATH_MAX];
   const char *status;

   printf("C-FINGER VERFIY STATUS: %s\n", currentfinger);

   EINA_SAFETY_ON_TRUE_RETURN(eldbus_message_error_get(msg, NULL, NULL));

   if (!eldbus_message_arguments_get(msg, "s", &status))
     {
        fprintf(stderr, "Error: could not get entry contents\n");
        return;
     }

   if (!strcmp(status, "verify-match"))
     {
        snprintf(buf, sizeof(buf), "<color=green>%s</color>", status);
        elm_object_text_set(lb_status, buf);//FIXME lb_status ist nicht mehr vorhanden wenn über block,clicked das popup gelöscht worden ist.

        edje_object_signal_emit(ly_popup, "success", "success");

        const char *layout;
        elm_layout_file_get(ly_popup, NULL, &layout);

        fprint_device_verify_stop_call(new_proxy1, _verify_stopp_cb, NULL);

        _restart_verify();
     }
   else if (!strcmp(status, "verify-retry-scan"))
     {
        snprintf(buf, sizeof(buf), "<color=white>%s</color>", status);
        elm_object_text_set(lb_status, buf);
     }
   else if (!strcmp(status, "verify-swipe-too-short"))
     {
        snprintf(buf, sizeof(buf), "<color=white>%s</color>", status);
        elm_object_text_set(lb_status, buf);
     }
   else if (!strcmp(status, "verify-finger-not-centered"))
     {
        snprintf(buf, sizeof(buf), "<color=white>%s</color>", status);
        elm_object_text_set(lb_status, buf);
     }
   else if (!strcmp(status, "verify-remove-and-retry"))
     {
        snprintf(buf, sizeof(buf), "<color=white>%s</color>", status);
        elm_object_text_set(lb_status, buf);
     }
   else if (!strcmp(status, "verify-disconnected"))
     {
        snprintf(buf, sizeof(buf), "<color=red>%s</color>", status);
        elm_object_text_set(lb_status, buf);

        edje_object_signal_emit(ly_popup, "failed", "failed");// FIXME ly_popup ist nicht mehr vorhanden wenn über block,clicked das popup gelöscht worden ist.

        fprint_device_verify_stop_call(new_proxy1, _verify_stopp_cb, NULL);
        _restart_verify();
     }
   else if (!strcmp(status, "verify-no-match"))
     {
        snprintf(buf, sizeof(buf), "<color=red>%s</color><br>retry", status);
        elm_object_text_set(lb_status, buf);

        edje_object_signal_emit(ly_popup, "failed", "failed");

        fprint_device_verify_stop_call(new_proxy1, _verify_stopp_cb, NULL);
        _restart_verify();
     }
   else
     {
        elm_object_text_set(lb_status, "<color=red>unknown error</color>");

        edje_object_signal_emit(ly_popup, "failed", "failed");

        fprint_device_verify_stop_call(new_proxy1, _verify_stopp_cb, NULL);
     }
}

static void
_enroll_status(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   //TODO: Theme an die Anzahl der verlangten enrolls anpassen. Theme = 5, 

   const char *status;
   char buf[PATH_MAX];
   char buf1[PATH_MAX];
   EINA_SAFETY_ON_TRUE_RETURN(eldbus_message_error_get(msg, NULL, NULL));

   if (!eldbus_message_arguments_get(msg, "s", &status))
     {
        fprintf(stderr, "Error: could not get entry contents\n");
        return;
     }

   snprintf(buf1, sizeof(buf1), "%i", step);

   if (!strcmp(status, "enroll-stage-passed"))
     {
        snprintf(buf, sizeof(buf), "<color=green>Enroll %i of %i<br>%s</color>", step, enroll_num, status);
        elm_object_text_set(lb_status, buf); //FIXME lb_status ist nicht mehr vorhanden wenn über block,clicked das popup gelöscht worden ist.

        enroll_count = enroll_count + enroll_count_value;

        if (enroll_count <= 4)
          edje_object_signal_emit(ly_popup, "success", "1");
        else if (enroll_count >= 4 && (enroll_count <= 6))
          edje_object_signal_emit(ly_popup, "success", "2");
        else if (enroll_count >= 6 && (enroll_count <= 8))
          edje_object_signal_emit(ly_popup, "success", "3");
        else if (enroll_count >= 8 && (enroll_count <= 10))
          edje_object_signal_emit(ly_popup, "success", "4");
        else if (enroll_count >= 10)
          edje_object_signal_emit(ly_popup, "success", "5");

        const char *layout;
        elm_layout_file_get(ly_popup, NULL, &layout);
        printf("LAYOUT %s\n", layout);

        step++;
     }
   else if (!strcmp(status, "enroll-completed"))
     {
        snprintf(buf, sizeof(buf), "<color=green>Enroll %i of %i<br>%s</color>", step, enroll_num, status);
        elm_object_text_set(lb_status, buf);

        edje_object_signal_emit(ly_popup, "success", "5");
        fprint_device_list_enrolled_fingers_call(new_proxy1, enrolled_fingers_cb, NULL, currentuser);
        enroll_count = 1;
        step = 1;
     }
   else if (!strcmp(status, "enroll-swipe-too-short") || !strcmp(status, "enroll-retry-scan") || !strcmp(status, "enroll-finger-not-centered") || !strcmp(status, "enroll-remove-and-retry") || !strcmp(status, "enroll-remove-and-retry"))
     {
        snprintf(buf, sizeof(buf), "<color=red>Enroll %i of %i<br>%s</color>", step, enroll_num, status);
        elm_object_text_set(lb_status, buf);

        if (enroll_count <= 4)
          edje_object_signal_emit(ly_popup, "failed", "1");
        else if (enroll_count >= 4 && (enroll_count <= 6))
          edje_object_signal_emit(ly_popup, "failed", "2");
        else if (enroll_count >= 6 && (enroll_count <= 8))
          edje_object_signal_emit(ly_popup, "failed", "3");
        else if (enroll_count >= 8 && (enroll_count <= 10))
          edje_object_signal_emit(ly_popup, "failed", "4");
        else if (enroll_count >= 10)
          edje_object_signal_emit(ly_popup, "failed", "5");
     }
   else if (!strcmp(status, "enroll-failed"))
     {
        elm_object_text_set(lb_status, "<color=red>enroll failed</color>");

        edje_object_signal_emit(ly_popup, "failed", buf1); // FIXME ly_popup ist nicht mehr vorhanden wenn über block,clicked das popup gelöscht worden ist.
        step = 1;

        fprint_device_enroll_stop_call(new_proxy1, _enroll_stopp_cb, NULL);
     }
   else if (!strcmp(status, "enroll-disconnected"))
     {
        elm_object_text_set(lb_status, "<color=red>enroll disconnected</color>");

        edje_object_signal_emit(ly_popup, "enrolled__failed", "enrolled__failed");
        enroll_count = 1;
        step = 1;

        fprint_device_enroll_stop_call(new_proxy1, _enroll_stopp_cb, NULL);
     }
   else if (!strcmp(status, "enroll-data-full"))
     {
        elm_object_text_set(lb_status, "<color=red>enroll.data full<br> No further prints can be enrolled on this device</color>");

        edje_object_signal_emit(ly_popup, "enrolled__failed", "enrolled__failed");
        step = 1;

        fprint_device_enroll_stop_call(new_proxy1, _enroll_stopp_cb, NULL);
     }
   else
     {
        elm_object_text_set(lb_status, "<color=red>unknown error</color>");

        edje_object_signal_emit(ly_popup, "enrolled__failed", "enrolled__failed");
        enroll_count = 1;
        step = 1;
        fprint_device_enroll_stop_call(new_proxy1, _enroll_stopp_cb, NULL);
     }
}

int
e_auth_shutdown(void)
{
   if (conn) eldbus_connection_unref(conn);
   conn = NULL;
   return 1;
}

EAPI_MAIN int
elm_main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   Evas_Object *box, *lb, *lb1, *lb2, *h_box, *panel, *hv, *p_box, *sep;
   char buf[PATH_MAX];
   char buf1[PATH_MAX];

   eina_value_array_setup(&array, EINA_VALUE_TYPE_STRING, 1);

   elm_app_compile_bin_dir_set(PACKAGE_BIN_DIR);
   elm_app_compile_lib_dir_set(PACKAGE_LIB_DIR);
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   eldbus_init();

   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   if (!conn)
     {
        fprintf(stderr, "Error: could not get system bus\n");
        return EXIT_FAILURE;
     }

   currentuser = getenv("USER");
   currentfinger = "";
   ly_popup = NULL;
   lb_status = NULL;

   new_proxy = fprint_manager_proxy_get(conn, "net.reactivated.Fprint", "/net/reactivated/Fprint/Manager");

   p1 = fprint_manager_get_default_device_call(new_proxy, get_default_device, NULL); 
   default_device = "/net/reactivated/Fprint/Device/0"; //FIXME wenn ich default_device über die funkion hole ist die variable nicht gefüllt für fprint_device_proxy_get //FIXME
   printf("DEFAULT DEVICE %s\n\n", default_device);

   new_proxy1 = fprint_device_proxy_get(conn, "net.reactivated.Fprint", default_device);

   fprint_device_claim_call(new_proxy1, claim_device, NULL, "");

   eldbus_signal_handler_add(conn, "net.reactivated.Fprint", default_device, "net.reactivated.Fprint.Device", "EnrollStatus", _enroll_status, NULL);
   eldbus_signal_handler_add(conn, "net.reactivated.Fprint", default_device, "net.reactivated.Fprint.Device", "VerifyStatus", _verify_status, NULL);

//    p = fprint_manager_get_devices_call(new_proxy, get_devices, NULL);

   fprint_device_list_enrolled_fingers_call(new_proxy1, enrolled_fingers_cb, NULL, "");

   fprint_device_num_enroll_stages_propget(new_proxy1, _enroll_prop_get, NULL); //NUM enroll states needed
   printf("NUM enroll states: %i\n", enroll_num);

   // set app informations
   elm_app_info_set(elm_main, "enlightenment", "COPYING");

   win = elm_win_util_standard_add("main", "Fingerprint Password Settings");
   elm_win_title_set(win, "Fingerprint Password Settings");
   elm_win_autodel_set(win, EINA_TRUE);

   box = elm_box_add(win);
   evas_object_show(box);

   h_box = elm_box_add(win);
   elm_box_homogeneous_set(h_box, EINA_TRUE);
   elm_box_horizontal_set(h_box, EINA_TRUE);

   lb = elm_label_add(win);
   elm_object_text_set(lb, "Choose finger and click on fingerprint to select action");
   evas_object_show(lb);
   elm_box_pack_end(box, lb);

   ly = elm_layout_add(h_box);
   snprintf(buf, sizeof(buf), "%s/themes/enlightenment_fprint.edj", elm_app_data_dir_get());
   elm_layout_file_set(ly, buf, "right_hand");
   evas_object_show(ly);

   _update_theme();

   elm_box_pack_end(h_box, ly);

   evas_object_show(h_box);

   elm_box_pack_end(box, h_box);

   h_box = elm_box_add(win);
   elm_box_homogeneous_set(h_box, EINA_TRUE);
   elm_box_horizontal_set(h_box, EINA_TRUE);
   evas_object_size_hint_align_set( h_box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   hv = elm_hoversel_add(h_box);
   elm_object_text_set(hv, "One hand");
   elm_hoversel_auto_update_set(hv, EINA_TRUE);
   elm_hoversel_hover_parent_set(hv, win);
   elm_hoversel_item_add(hv, "One hand", NULL, ELM_ICON_NONE, _select_mode, "right_hand");
   elm_hoversel_item_add(hv, "Both hands", NULL, ELM_ICON_NONE, _select_mode, "hands");
   elm_hoversel_item_add(hv, "One finger", NULL, ELM_ICON_NONE, _select_mode, "finger");
   evas_object_show(hv);
   elm_box_pack_end(h_box, hv);

   evas_object_show(h_box);

   elm_box_pack_end(box, h_box);

   panel = elm_panel_add(box);
   elm_panel_orient_set(panel, ELM_PANEL_ORIENT_BOTTOM);
   evas_object_size_hint_weight_set(panel, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(panel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(panel);

   p_box = elm_box_add(panel);
   elm_box_horizontal_set(p_box, EINA_TRUE);
   evas_object_size_hint_align_set(p_box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   lb = elm_label_add(panel);
   fprint_device_name_propget(new_proxy1, get_device_proberties, lb); // DEVICE NAME
   evas_object_show(lb);
   elm_box_pack_end(p_box, lb);

   sep = elm_separator_add(panel);
   elm_separator_horizontal_set(sep, EINA_FALSE);
   evas_object_show(sep);
   elm_box_pack_end(p_box, sep);

   lb1 = elm_label_add(panel);
   fprint_device_scan_type_propget(new_proxy1, get_device_type, lb1); // DEVICE TYPE
   evas_object_show(lb1);
   elm_box_pack_end(p_box, lb1);

   sep = elm_separator_add(panel);
   elm_separator_horizontal_set(sep, EINA_FALSE);
   evas_object_show(sep);
   elm_box_pack_end(p_box, sep);

   snprintf(buf1, sizeof(buf1), "User: <hilight>%s</>", currentuser);

   lb2 = elm_label_add(panel);
   elm_object_text_set(lb2, buf1);
   evas_object_show(lb2);
   elm_box_pack_end(p_box, lb2);

   elm_object_content_set(panel, p_box);

   elm_box_pack_end(box, panel);

   elm_win_resize_object_add(win, box);
   evas_object_show(win);

   elm_run();

   if (conn) eldbus_connection_unref(conn);
   conn = NULL;

   return 0;
}
ELM_MAIN()
