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
 * is the user-facing dialog that pops when NM asks for a WiFi password, and
 * nothing more.  The bridge between the two layers is the pair of callbacks
 * registered via e_nm_agent_callbacks_set() from enm_agent_ui_register().
 */

/* One input field rendered inside the password page */
typedef struct _E_NM_Agent_Input E_NM_Agent_Input;
struct _E_NM_Agent_Input
{
   char *key;
   char *value;
   int   show_password;
};

/* State for one live dialog.  Owned by the dialog; freed in the del cb. */
typedef struct _E_NM_Agent_Dialog E_NM_Agent_Dialog;
struct _E_NM_Agent_Dialog
{
   E_Dialog           *dialog;
   E_NM_Agent_Request *req;  /* borrowed pointer — NULL after reply/cancel */
};

static E_NM_Agent_Dialog *_current_dialog = NULL;

/* -------------------------------------------------------------------------- */
/* Dialog callbacks                                                            */
/* -------------------------------------------------------------------------- */

static const char *
_dialog_first_psk(E_Dialog *dialog)
{
   Evas_Object *toolbook, *list;
   Eina_List *input_list;
   E_NM_Agent_Input *input;

   toolbook = dialog->content_object;
   list = evas_object_data_get(toolbook, "psk");
   if (!list) list = evas_object_data_get(toolbook, "password");
   if (!list) return NULL;

   input_list = evas_object_data_get(list, "input_list");
   if (!input_list) return NULL;
   input = eina_list_data_get(input_list);
   return input ? input->value : NULL;
}

static void
_dialog_ok_cb(void *data, E_Dialog *dialog)
{
   E_NM_Agent_Dialog *ad = data;
   const char *psk;

   psk = _dialog_first_psk(dialog);
   if (ad->req)
     {
        e_nm_agent_reply_secrets(ad->req, psk);
        ad->req = NULL;
     }
   e_object_del(E_OBJECT(dialog));
}

static void
_dialog_cancel_cb(void *data, E_Dialog *dialog)
{
   E_NM_Agent_Dialog *ad = data;

   if (ad->req)
     {
        e_nm_agent_reply_cancel(ad->req);
        ad->req = NULL;
     }
   e_object_del(E_OBJECT(dialog));
}

static void
_dialog_key_down_cb(void *data, Evas *e EINA_UNUSED,
                    Evas_Object *o EINA_UNUSED, void *event)
{
   Evas_Event_Key_Down *ev = event;
   E_NM_Agent_Dialog *ad = data;

   if (!strcmp(ev->key, "Return"))
     _dialog_ok_cb(ad, ad->dialog);
   else if (!strcmp(ev->key, "Escape"))
     _dialog_cancel_cb(ad, ad->dialog);
}

static void
_dialog_del_cb(void *data)
{
   E_Dialog *dialog = data;
   E_NM_Agent_Dialog *ad = e_object_data_get(E_OBJECT(dialog));

   /* If the dialog was closed via the WM (not via OK/Cancel buttons) the
    * request is still live — treat as user cancel. */
   if (ad->req)
     {
        e_nm_agent_reply_cancel(ad->req);
        ad->req = NULL;
     }
   if (_current_dialog == ad) _current_dialog = NULL;
   free(ad);
}

static void
_page_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
          Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_NM_Agent_Input *input;
   Eina_List *input_list;

   input_list = evas_object_data_get(obj, "input_list");
   EINA_LIST_FREE(input_list, input)
     {
        free(input->key);
        /* input->value is NOT freed here: it is a pointer into the EFL entry
         * widget's internal buffer (set via e_widget_entry_add's &value
         * parameter).  The widget owns the allocation; freeing it here would
         * be a double-free once the widget itself is destroyed. */
        free(input);
     }
}

static void
_show_password_cb(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   Evas_Object *entry = data;
   int hidden;

   hidden = !e_widget_check_checked_get(obj);
   e_widget_entry_password_set(entry, hidden);
}

/* -------------------------------------------------------------------------- */
/* Dialog construction                                                         */
/* -------------------------------------------------------------------------- */

static void
_dialog_psk_add(E_Dialog *dialog, const char *ssid)
{
   Evas_Object *toolbook, *list, *framelist, *entry, *check;
   E_NM_Agent_Input *input;
   Eina_List *input_list;
   char header[128];
   Evas *evas;

   evas     = evas_object_evas_get(dialog->win);
   toolbook = dialog->content_object;

   input      = E_NEW(E_NM_Agent_Input, 1);
   input->key = strdup("psk");
   entry = e_widget_entry_add(dialog->win, &(input->value),
                              NULL, NULL, NULL);
   evas_object_show(entry);
   e_widget_entry_password_set(entry, 1);

   list = evas_object_data_get(toolbook, "psk");
   if (!list)
     {
        list = e_widget_list_add(evas, 0, 0);
        e_widget_toolbook_page_append(toolbook, NULL,
                                      _("WiFi Password"),
                                      list, 1, 1, 1, 1, 0.5, 0.0);
        evas_object_data_set(toolbook, "psk", list);
        e_widget_toolbook_page_show(toolbook, 0);
        evas_object_event_callback_add(list, EVAS_CALLBACK_DEL,
                                       _page_del, NULL);
        e_widget_focus_set(entry, 1);
     }

   input_list = evas_object_data_get(list, "input_list");
   input_list = eina_list_append(input_list, input);
   evas_object_data_set(list, "input_list", input_list);

   snprintf(header, sizeof(header),
            _("Password required for \"%s\":"), ssid ?: "network");

   framelist = e_widget_framelist_add(evas, header, 0);
   evas_object_show(framelist);
   e_widget_list_object_append(list, framelist, 1, 1, 0.5);
   e_widget_framelist_object_append(framelist, entry);

   check = e_widget_check_add(evas, _("Show password"),
                               &(input->show_password));
   evas_object_show(check);
   e_widget_framelist_object_append(framelist, check);
   evas_object_smart_callback_add(check, "changed",
                                  _show_password_cb, entry);

   e_util_win_auto_resize_fill(dialog->win);
}

static E_NM_Agent_Dialog *
_dialog_new(E_NM_Agent_Request *req, const char *ssid)
{
   E_NM_Agent_Dialog *ad;
   Evas_Object *toolbook;
   E_Dialog    *dialog;
   int          mw, mh;

   dialog = e_dialog_new(NULL, "E", "nm_secret_agent");
   if (!dialog) return NULL;

   ad = E_NEW(E_NM_Agent_Dialog, 1);
   ad->dialog = dialog;
   ad->req    = req;

   e_dialog_resizable_set(dialog, 1);
   e_dialog_title_set(dialog, _("WiFi Password Required"));
   e_dialog_border_icon_set(dialog, "dialog-password");

   e_dialog_button_add(dialog, _("Connect"), NULL, _dialog_ok_cb, ad);
   e_dialog_button_add(dialog, _("Cancel"),  NULL, _dialog_cancel_cb, ad);

   toolbook = e_widget_toolbook_add(
                evas_object_evas_get(dialog->win),
                48 * e_scale, 48 * e_scale);
   evas_object_show(toolbook);

   e_widget_size_min_get(toolbook, &mw, &mh);
   if (mw < 280) mw = 280;
   if (mh < 140) mh = 140;
   e_dialog_content_set(dialog, toolbook, mw, mh);
   e_dialog_show(dialog);

   evas_object_event_callback_add(dialog->bg_object, EVAS_CALLBACK_KEY_DOWN,
                                  _dialog_key_down_cb, ad);
   e_object_del_attach_func_set(E_OBJECT(dialog), _dialog_del_cb);
   e_object_data_set(E_OBJECT(dialog), ad);
   e_dialog_button_focus_num(dialog, 0);
   elm_win_center(dialog->win, 1, 1);

   _dialog_psk_add(dialog, ssid);

   return ad;
}

/* -------------------------------------------------------------------------- */
/* Agent UI callback bridge                                                    */
/* -------------------------------------------------------------------------- */

static void
_agent_ui_request_cb(void *data EINA_UNUSED, E_NM_Agent_Request *req,
                     const char *ssid)
{
   /* Only one dialog at a time — drop any stale one.  The data layer
    * already freed the previous request when it arrived, so just tear
    * down the widgets here. */
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
    * sending any reply — the data layer will free the request after this
    * callback returns. */
   if (_current_dialog)
     {
        _current_dialog->req = NULL;
        e_object_del(E_OBJECT(_current_dialog->dialog));
        _current_dialog = NULL;
     }
}

static const E_NM_Agent_Callbacks _ui_cbs =
{
   .request = _agent_ui_request_cb,
   .cancel  = _agent_ui_cancel_cb,
};

void
enm_agent_ui_register(void)
{
   e_nm_agent_callbacks_set(&_ui_cbs, NULL);
}
