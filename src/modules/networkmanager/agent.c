#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "e.h"
#include "e_networkmanager.h"
#include "e_mod_main.h"

/*
 * SecretAgent UI layer.
 *
 * All NM D-Bus SecretAgent handling lives in e_networkmanager.c.  This file
 * is the user-facing dialog that pops when NM asks for a WiFi password or VPN
 * credentials, and nothing more.  The bridge between the two layers is the
 * pair of callbacks registered via e_nm_agent_callbacks_set() from
 * enm_agent_ui_register().
 *
 * Widgets are Elementary — elm_frame / elm_box / elm_entry / elm_check —
 * with no legacy e_widget_* dependencies.
 */

/* State for one live dialog.  Freed in the dialog del callback. */
typedef struct _E_NM_Agent_Dialog E_NM_Agent_Dialog;
struct _E_NM_Agent_Dialog
{
   E_Dialog            *dialog;
   E_NM_Agent_Request  *req;
   Eina_Bool            is_vpn;
   /* Wifi mode: single field. */
   Evas_Object         *psk_entry;
   /* VPN mode: parallel arrays of size n_fields. */
   char               **field_names;
   Evas_Object        **field_entries;
   unsigned int         n_fields;
};

static E_NM_Agent_Dialog *_current_dialog = NULL;

/* -------------------------------------------------------------------------- */
/* Dialog callbacks                                                            */
/* -------------------------------------------------------------------------- */

static void
_dialog_send_ok(E_NM_Agent_Dialog *ad)
{
   if (!ad->req) { e_object_del(E_OBJECT(ad->dialog)); return; }

   if (!ad->is_vpn)
     {
        /* elm_entry stores markup internally; convert to plain UTF-8 so that
         * passwords containing '<' or '&' survive the round-trip to NM. */
        char *psk = elm_entry_markup_to_utf8(elm_entry_entry_get(ad->psk_entry));
        e_nm_agent_reply_secrets(ad->req, psk);
        free(psk);
     }
   else
     {
        const char **values = calloc(ad->n_fields, sizeof(*values));
        char **utf8 = calloc(ad->n_fields, sizeof(*utf8));
        if (!values || !utf8)
          {
             free(values); free(utf8);
             e_nm_agent_reply_cancel(ad->req);
          }
        else
          {
             for (unsigned int i = 0; i < ad->n_fields; i++)
               {
                  utf8[i] = elm_entry_markup_to_utf8(
                               elm_entry_entry_get(ad->field_entries[i]));
                  values[i] = utf8[i] ?: "";
               }
             e_nm_agent_reply_vpn_secrets(ad->req,
                   (const char *const *)ad->field_names,
                   values, ad->n_fields);
             for (unsigned int i = 0; i < ad->n_fields; i++) free(utf8[i]);
             free(utf8);
             free(values);
          }
     }
   ad->req = NULL;
   e_object_del(E_OBJECT(ad->dialog));
}

static void
_dialog_send_cancel(E_NM_Agent_Dialog *ad)
{
   if (ad->req)
     {
        e_nm_agent_reply_cancel(ad->req);
        ad->req = NULL;
     }
   e_object_del(E_OBJECT(ad->dialog));
}

static void
_dialog_ok_cb(void *data, E_Dialog *dialog EINA_UNUSED)
{
   _dialog_send_ok(data);
}

static void
_dialog_cancel_cb(void *data, E_Dialog *dialog EINA_UNUSED)
{
   _dialog_send_cancel(data);
}

static void
_dialog_key_down_cb(void *data, Evas *e EINA_UNUSED,
                    Evas_Object *o EINA_UNUSED, void *event)
{
   Evas_Event_Key_Down *ev = event;
   E_NM_Agent_Dialog *ad = data;

   /* Enter is handled by the entry's "activated" callback so we do not
    * ACK it here — doing both would run _dialog_send_ok twice on an
    * already-deleted dialog.  Escape always cancels regardless of focus. */
   if (!strcmp(ev->key, "Escape"))
     _dialog_send_cancel(ad);
}

static void
_entry_activated_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   /* Enter key inside the entry triggers Connect */
   _dialog_send_ok(data);
}

static void
_dialog_del_cb(void *data)
{
   E_Dialog *dialog = data;
   E_NM_Agent_Dialog *ad = e_object_data_get(E_OBJECT(dialog));

   if (!ad) return;

   /* If the dialog was closed via the WM (not OK/Cancel) the request is
    * still live — treat as user cancel. */
   if (ad->req)
     {
        e_nm_agent_reply_cancel(ad->req);
        ad->req = NULL;
     }
   if (_current_dialog == ad) _current_dialog = NULL;
   if (ad->field_names)
     {
        for (unsigned int i = 0; i < ad->n_fields; i++) free(ad->field_names[i]);
        free(ad->field_names);
     }
   free(ad->field_entries);
   free(ad);
}

static void
_show_password_cb(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *entry = data;
   elm_entry_password_set(entry, !elm_check_state_get(obj));
}

/* -------------------------------------------------------------------------- */
/* Dialog construction                                                         */
/* -------------------------------------------------------------------------- */

static E_NM_Agent_Dialog *
_dialog_new(E_NM_Agent_Request *req, const char *ssid)
{
   E_NM_Agent_Dialog *ad;
   Evas_Object *frame, *box, *entry, *check;
   E_Dialog    *dialog;
   char         header[128];

   dialog = e_dialog_new(NULL, "E", "nm_secret_agent");
   if (!dialog) return NULL;

   ad = E_NEW(E_NM_Agent_Dialog, 1);
   ad->dialog  = dialog;
   ad->req     = req;
   ad->is_vpn  = EINA_FALSE;

   e_dialog_resizable_set(dialog, 1);
   e_dialog_title_set(dialog, _("WiFi Password Required"));
   e_dialog_border_icon_set(dialog, "dialog-password");

   e_dialog_button_add(dialog, _("Connect"), NULL, _dialog_ok_cb, ad);
   e_dialog_button_add(dialog, _("Cancel"),  NULL, _dialog_cancel_cb, ad);

   /* Labelled frame containing the password row + show-password check */
   snprintf(header, sizeof(header),
            _("Password required for \"%s\":"), ssid ?: "network");
   frame = elm_frame_add(dialog->win);
   elm_object_text_set(frame, header);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   elm_box_padding_set(box, 0, 4 * e_scale);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(frame, box);
   evas_object_show(box);

   entry = elm_entry_add(box);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_password_set(entry, EINA_TRUE);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(entry, "activated",
                                   _entry_activated_cb, ad);
   elm_box_pack_end(box, entry);
   evas_object_show(entry);
   ad->psk_entry = entry;

   check = elm_check_add(box);
   elm_object_text_set(check, _("Show password"));
   elm_check_state_set(check, EINA_FALSE);
   evas_object_size_hint_align_set(check, 0.0, 0.5);
   evas_object_smart_callback_add(check, "changed",
                                   _show_password_cb, entry);
   elm_box_pack_end(box, check);
   evas_object_show(check);

   evas_object_show(frame);
   e_dialog_content_set(dialog, frame, 280, 100);
   e_dialog_show(dialog);

   evas_object_event_callback_add(dialog->bg_object, EVAS_CALLBACK_KEY_DOWN,
                                  _dialog_key_down_cb, ad);
   e_object_del_attach_func_set(E_OBJECT(dialog), _dialog_del_cb);
   e_object_data_set(E_OBJECT(dialog), ad);
   elm_object_focus_set(entry, EINA_TRUE);
   elm_win_center(dialog->win, 1, 1);

   return ad;
}

static E_NM_Agent_Dialog *
_vpn_dialog_new(E_NM_Agent_Request *req, const char *conn_name,
                const char *service_type,
                const char *const *fields, unsigned int n_fields)
{
   E_NM_Agent_Dialog *ad;
   Evas_Object *frame, *box;
   E_Dialog *dialog;
   char header[160];

   dialog = e_dialog_new(NULL, "E", "nm_secret_agent_vpn");
   if (!dialog) return NULL;

   ad = E_NEW(E_NM_Agent_Dialog, 1);
   ad->dialog = dialog;
   ad->req    = req;
   ad->is_vpn = EINA_TRUE;
   ad->n_fields = n_fields;
   ad->field_names = calloc(n_fields ?: 1, sizeof(*ad->field_names));
   ad->field_entries = calloc(n_fields ?: 1, sizeof(*ad->field_entries));
   for (unsigned int i = 0; i < n_fields; i++)
     ad->field_names[i] = strdup(fields[i]);

   e_dialog_resizable_set(dialog, 1);
   e_dialog_title_set(dialog, _("VPN Authentication Required"));
   e_dialog_border_icon_set(dialog, "dialog-password");
   e_dialog_button_add(dialog, _("Connect"), NULL, _dialog_ok_cb, ad);
   e_dialog_button_add(dialog, _("Cancel"),  NULL, _dialog_cancel_cb, ad);

   snprintf(header, sizeof(header), "%s — %s",
            conn_name ?: "VPN",
            enm_vpn_type_label(NULL, service_type));
   frame = elm_frame_add(dialog->win);
   elm_object_text_set(frame, header);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   elm_box_padding_set(box, 0, 6 * e_scale);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(frame, box);
   evas_object_show(box);

   for (unsigned int i = 0; i < n_fields; i++)
     {
        Evas_Object *row, *label, *entry;
        row = elm_box_add(box);
        elm_box_horizontal_set(row, EINA_TRUE);
        elm_box_padding_set(row, 8 * e_scale, 0);
        evas_object_size_hint_weight_set(row, EVAS_HINT_EXPAND, 0);
        evas_object_size_hint_align_set(row, EVAS_HINT_FILL, 0.5);
        evas_object_show(row);

        label = elm_label_add(row);
        elm_object_text_set(label, fields[i]);
        evas_object_size_hint_align_set(label, 0.0, 0.5);
        elm_box_pack_end(row, label);
        evas_object_show(label);

        entry = elm_entry_add(row);
        elm_entry_single_line_set(entry, EINA_TRUE);
        elm_entry_scrollable_set(entry, EINA_TRUE);
        elm_entry_password_set(entry, EINA_TRUE);
        evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0);
        evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, 0.5);
        evas_object_smart_callback_add(entry, "activated",
                                       _entry_activated_cb, ad);
        elm_box_pack_end(row, entry);
        evas_object_show(entry);

        ad->field_entries[i] = entry;
        elm_box_pack_end(box, row);
     }

   evas_object_show(frame);
   e_dialog_content_set(dialog, frame, 360, 60 + 32 * n_fields);
   e_dialog_show(dialog);

   evas_object_event_callback_add(dialog->bg_object, EVAS_CALLBACK_KEY_DOWN,
                                  _dialog_key_down_cb, ad);
   e_object_del_attach_func_set(E_OBJECT(dialog), _dialog_del_cb);
   e_object_data_set(E_OBJECT(dialog), ad);
   if (n_fields > 0) elm_object_focus_set(ad->field_entries[0], EINA_TRUE);
   elm_win_center(dialog->win, 1, 1);
   return ad;
}

/* -------------------------------------------------------------------------- */
/* Agent UI callback bridge                                                    */
/* -------------------------------------------------------------------------- */

static void
_agent_ui_request_cb(void *data EINA_UNUSED, E_NM_Agent_Request *req,
                     const char *ssid)
{
   /* Only one dialog at a time — drop any stale one.  The data layer has
    * already freed the previous request, so we just tear down widgets. */
   if (_current_dialog)
     {
        _current_dialog->req = NULL;   /* don't reply — request is gone */
        e_object_del(E_OBJECT(_current_dialog->dialog));
        _current_dialog = NULL;
     }

   _current_dialog = _dialog_new(req, ssid);
   if (!_current_dialog)
     {
        ERR("Failed to create SecretAgent dialog");
        e_nm_agent_reply_cancel(req);
        return;
     }
}

static void
_agent_ui_cancel_cb(void *data EINA_UNUSED,
                    E_NM_Agent_Request *req EINA_UNUSED)
{
   /* NM is withdrawing the pending request.  Dismiss the dialog without
    * sending any reply — the data layer frees the request after this
    * callback returns. */
   if (_current_dialog)
     {
        _current_dialog->req = NULL;
        e_object_del(E_OBJECT(_current_dialog->dialog));
        _current_dialog = NULL;
     }
}

static void
_agent_ui_vpn_request_cb(void *data EINA_UNUSED, E_NM_Agent_Request *req,
                         const char *conn_name, const char *service_type,
                         const char *const *fields, unsigned int n_fields)
{
   if (_current_dialog)
     {
        _current_dialog->req = NULL;
        e_object_del(E_OBJECT(_current_dialog->dialog));
        _current_dialog = NULL;
     }
   _current_dialog = _vpn_dialog_new(req, conn_name, service_type,
                                     fields, n_fields);
   if (!_current_dialog) { e_nm_agent_reply_cancel(req); return; }
}

static const E_NM_Agent_Callbacks _ui_cbs =
{
   .request     = _agent_ui_request_cb,
   .cancel      = _agent_ui_cancel_cb,
   .vpn_request = _agent_ui_vpn_request_cb,
};

void
enm_agent_ui_register(void)
{
   e_nm_agent_callbacks_set(&_ui_cbs, NULL);
}
