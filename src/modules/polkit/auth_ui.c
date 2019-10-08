#include "e_mod_main.h"

static Eina_Bool
_auth_cb_exit(void *data, int type EINA_UNUSED, void *event)
{
   Polkit_Session *ps = data;
   Ecore_Exe_Event_Del *ev = event;

   if (ev->pid != ps->auth_pid) return ECORE_CALLBACK_PASS_ON;
   ps->auth_pid = 0;
   if (ps->exe_exit_handler) ecore_event_handler_del(ps->exe_exit_handler);
   ps->exe_exit_handler = NULL;
   session_reply(ps);
   return ECORE_CALLBACK_PASS_ON;
}

static void
_cb_del(void *data EINA_UNUSED, Evas *evas EINA_UNUSED, Evas_Object *obj,
        void *event_info EINA_UNUSED)
{
   Polkit_Session *ps = evas_object_data_get(obj, "session");
   if (!ps) return;
   if (ps->exe_exit_handler)
     {
        ecore_event_handler_del(ps->exe_exit_handler);
        ps->exe_exit_handler = NULL;
     }
   if (ps->win)
     {
        ps->win = NULL;
        session_reply(ps);
     }
}

static void
_cb_ok(void *data EINA_UNUSED, Evas_Object *obj,
       void *event_info EINA_UNUSED)
{
   Polkit_Session *ps = evas_object_data_get(obj, "session");
   const char *str = elm_object_text_get(obj);

   if (!ps) return;
   if (ps->exe_exit_handler) return;
   ps->exe_exit_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                                  _auth_cb_exit, ps);
   if (str)
     {
        char *passwd = strdup(str);
        if (passwd)
          {
             ps->auth_pid = e_auth_polkit_begin(passwd, ps->cookie, ps->target_uid);
             free(passwd);
             return;
          }
     }
   evas_object_del(ps->win);
}

static void
_cb_cancel(void *data EINA_UNUSED, Evas_Object *obj,
           void *event_info EINA_UNUSED)
{
   Polkit_Session *ps = evas_object_data_get(obj, "session");
   if (!ps) return;
   if (ps->exe_exit_handler) return;
   session_reply(ps);
}

static void
_cb_button_ok(void *data, E_Dialog *dia EINA_UNUSED)
{
   _cb_ok(NULL, data, NULL);
}

static void
_cb_button_cancel(void *data, E_Dialog *dia EINA_UNUSED)
{
   _cb_cancel(NULL, data, NULL);
}

void
auth_ui(Polkit_Session *ps)
{
   E_Dialog *dia;
   Evas_Object *o, *win, *box, *ent;

   dia = e_dialog_new(NULL, "E", "_polkit_auth");
   e_dialog_title_set(dia, _("Please enter password"));

   win = dia->win;

   if ((!ps->icon_name) || (!ps->icon_name[0]))
     e_dialog_icon_set(dia, "enlightenment", 64);
   else
     e_dialog_icon_set(dia, ps->icon_name, 64);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _cb_del, NULL);
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_data_set(win, "session", ps);

   box = o = elm_box_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, -1.0, -1.0);
   elm_box_horizontal_set(o, EINA_FALSE);
   e_dialog_content_set(dia, o, 0, 0);
   evas_object_show(o);

/* XXX: lookup action and display something useful for it in future.
   o = elm_label_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_object_text_set(o, ps->action);
   elm_box_pack_end(box, o);
   evas_object_show(o);
 */

   o = elm_label_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.0);
   elm_object_text_set(o, ps->message);
   elm_box_pack_end(box, o);
   evas_object_show(o);

   ent = o = elm_entry_add(win);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, -1.0, 1.0);
   elm_entry_single_line_set(ent, EINA_TRUE);
   elm_entry_scrollable_set(ent, EINA_TRUE);
   elm_entry_password_set(ent, EINA_TRUE);
   elm_object_part_text_set(ent, "elm.guide", "Enter Password");
   evas_object_data_set(ent, "session", ps);
   evas_object_smart_callback_add(ent, "activated", _cb_ok, NULL);
   evas_object_smart_callback_add(ent, "aborted", _cb_cancel, NULL);
   elm_box_pack_end(box, o);
   evas_object_show(o);

   e_dialog_button_add(dia, _("OK"), NULL, _cb_button_ok, ent);
   e_dialog_button_add(dia, _("Cancel"), NULL, _cb_button_cancel, ent);
   e_dialog_button_focus_num(dia, 0);

   elm_object_focus_set(ent, EINA_TRUE);

   ps->win = win;
   ps->entry = ent;

   elm_win_center(win, 1, 1);
   e_dialog_show(dia);
   elm_win_activate(win);
}
