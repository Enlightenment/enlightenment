#include "e.h"
#include "e_mod_main.h"
#include "e_networkmanager.h"
#include "e_networkmanager_vpn.h"
#include "e_networkmanager_import.h"

E_Module *networkmanager_mod = NULL;
E_NM_Config *networkmanager_config = NULL;
static Eina_Stringshare *_theme_path = NULL;
static E_Config_DD *conf_edd = NULL;

const char _e_nm_name[] = "networkmanager";
const char _e_nm_Name[] = N_("NetworkManager");
int _e_nm_log_dom = -1;

/* Forward declarations for traffic monitor */
static void _enm_traffic_timer_start(E_NM_Module_Context *ctxt);
static void _enm_traffic_timer_stop(E_NM_Module_Context *ctxt);
static void _enm_configure_registry_register(void);
static void _enm_configure_registry_unregister(void);

typedef struct _Enm_Traffic_Worker
{
   E_NM_Module_Context *ctxt;
   char                *iface;   /* strdup; worker-only read */
   Eina_Lock            lock;
   unsigned long long   rx;
   unsigned long long   tx;
   int                  pipe_fd;
   Eina_Bool            have_sample : 1;
} Enm_Traffic_Worker;

static void
_enm_poll_wake(E_NM_Module_Context *ctxt)
{
  char buf[1] = { 1 };

  if (ctxt->pipe) ecore_pipe_write(ctxt->pipe, buf, 1);
}

void
enm_config_poll_time_set(double tim)
{
   E_NM_Module_Context *ctxt;

   if (!networkmanager_mod) return;

   ctxt = networkmanager_mod->data;
   if (!ctxt) return;

   ctxt->poll_time = tim;
   if (ctxt->worker)
     {
        Enm_Traffic_Worker *w = ctxt->worker;

        eina_lock_take(&w->lock);
        if (w->ctxt) w->ctxt->poll_time = tim;
        eina_lock_release(&w->lock);
     }
   _enm_poll_wake(ctxt);
}

void
enm_config_dialog_show(E_NM_Instance *inst)
{
   if (!networkmanager_config) return;
   if (networkmanager_config->config_dialog) return;
   if (inst && inst->popup) enm_popup_del(inst);
   e_int_config_networkmanager_module(NULL, NULL);
}

/* Try our own theme first, fall back to connman's so the gadget keeps working
 * on installs that only ship the legacy theme. */
static Eina_Bool
_enm_theme_edje_object_set(Evas_Object *o, const char *group)
{
   char buf[256];

   snprintf(buf, sizeof(buf), "e/modules/networkmanager/%s", group);
   if (e_theme_edje_object_set(o, "base/theme/modules/networkmanager", buf))
     return EINA_TRUE;
   snprintf(buf, sizeof(buf), "e/modules/connman/%s", group);
   return e_theme_edje_object_set(o, "base/theme/modules/connman", buf);
}

/* Same fallback chain but for elm_layout, so the theme's group min:
 * propagates into the layout's evas size hint min (elm_table/genlist will
 * then honor it). */
static Eina_Bool
_enm_theme_layout_file_set(Evas_Object *layout, const char *group)
{
   char buf[256];
   const char *file;

   snprintf(buf, sizeof(buf), "e/modules/networkmanager/%s", group);
   file = elm_theme_group_path_find(NULL, buf);
   if (!file)
     {
        snprintf(buf, sizeof(buf), "e/modules/connman/%s", group);
        file = elm_theme_group_path_find(NULL, buf);
        if (!file) return EINA_FALSE;
     }
   return elm_layout_file_set(layout, file, buf);
}

const char *
e_nm_theme_path(void)
{
   char buf[PATH_MAX];

   if (_theme_path) return _theme_path;
   if (!networkmanager_mod || !networkmanager_mod->dir) return NULL;
   snprintf(buf, sizeof(buf), "%s/e-module-networkmanager.edj",
            networkmanager_mod->dir);
   _theme_path = eina_stringshare_add(buf);
   return _theme_path;
}

/* --- popup ---------------------------------------------------------------- */

void
enm_popup_del(E_NM_Instance *inst)
{
   E_FREE_FUNC(inst->ui.popup.deselect_timer, ecore_timer_del);
   inst->ui.popup.deselect_item = NULL;
   E_FREE_FUNC(inst->popup, e_object_del);
   E_FREE_FUNC(inst->ctxt->popup_update_timer, ecore_timer_del);
   inst->ui.popup.genlist = NULL;
   /* Section-header items live inside the genlist that just died with
    * the popup — clear the cached pointers so the next popup starts
    * fresh in _enm_popup_update. */
   inst->ui.popup.group_eth = NULL;
   inst->ui.popup.group_wifi = NULL;
   inst->ui.popup.group_bt = NULL;
   inst->ui.popup.group_vpn = NULL;
   E_FREE_FUNC(inst->ui.popup.itc_group, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_group_wifi, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_bt, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_group_vpn, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_ap, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_eth, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_vpn, elm_genlist_item_class_free);
}

static void
_enm_popup_del_cb(void *obj)
{
   enm_popup_del(e_object_data_get(obj));
}

static void
_enm_popup_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   E_NM_Instance *inst = data;

   if (!inst->popup) return;
   E_FREE_FUNC(inst->popup, e_object_del);
}

static void
_enm_popup_settings_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *info EINA_UNUSED)
{
   enm_config_dialog_show(data);
}


static Eina_Bool _enm_ssid_is_active(struct NM_Manager *nm, const char *ssid);

/* Forward declarations for icon/forget-button factories used in item class
 * callbacks defined before the factory implementations. */
static Evas_Object *_enm_ap_icon_new(struct NM_Manager *nm,
                                      struct NM_Access_Point *ap,
                                      Evas_Object *parent);
static Evas_Object *_enm_ap_end_new(struct NM_Manager *nm,
                                     struct NM_Access_Point *ap,
                                     Evas_Object *parent);
static Evas_Object *_enm_eth_icon_new(struct NM_Device *dev,
                                       Evas_Object *parent);
static Evas_Object *_enm_bt_icon_new(struct NM_Manager *nm,
                                      struct NM_Bluetooth_Connection *bc,
                                      struct NM_Device *dev,
                                      Evas_Object *parent);

/* Per-item data for genlist AP and ethernet rows */
typedef struct _Enm_Item_Data
{
   struct NM_Manager      *nm;
   struct NM_Access_Point *ap;   /* NULL for ethernet */
   struct NM_Device       *dev;
   struct NM_Bluetooth_Connection *bt;
   const char             *ap_path;   /* stringshare — AP D-Bus object path */
   const char             *ssid;      /* stringshare */
   const char             *connection_path; /* stringshare — bluetooth settings path */
} Enm_Item_Data;

static void
_enm_item_data_free(Enm_Item_Data *id)
{
   if (!id) return;
   eina_stringshare_del(id->ap_path);
   eina_stringshare_del(id->ssid);
   eina_stringshare_del(id->connection_path);
   free(id);
}

/* Genlist item class del callback — frees per-item data */
static void
_enm_itc_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   _enm_item_data_free(data);
}

/* Genlist text_get for AP rows: returns SSID on elm.text; on elm.text.sub
 * returns the assigned IP when this is the active AP, or the security type
 * otherwise. */
static char *
_enm_itc_ap_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                      const char *part)
{
   Enm_Item_Data *id = data;

   if (!strcmp(part, "elm.text"))
     return id->ssid ? strdup(id->ssid) : NULL;

   if (!strcmp(part, "elm.text.sub"))
     {
        struct NM_Manager *nm = id->nm;

        /* Active AP with a known IP → show the IP */
        if (nm && id->ap_path && nm->active_ap_path &&
            !strcmp(id->ap_path, nm->active_ap_path) && nm->ip_address)
          return strdup(nm->ip_address);

        /* Inactive or no IP → show security type */
        if (id->ap)
          {
             const char *sec = enm_ap_security_to_str(id->ap->wpa_flags,
                                                       id->ap->rsn_flags);
             if (sec) return strdup(sec);
          }
        return NULL;
     }

   return NULL;
}

/* Genlist content_get for AP rows: icon in elm.swallow.icon, forget button
 * in elm.swallow.end */
static Evas_Object *
_enm_itc_ap_content_get(void *data, Evas_Object *obj, const char *part)
{
   Enm_Item_Data *id = data;

   if (!id->ap) return NULL;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        Evas_Object *ic, *tbl, *rect;

        tbl = elm_table_add(obj);

        ic = _enm_ap_icon_new(id->nm, id->ap, obj);
        if (!ic)
          {
             evas_object_del(tbl);
             return NULL;
          }
        evas_object_show(ic);
        elm_table_pack(tbl, ic, 0, 0, 1, 1);

        rect = evas_object_rectangle_add(evas_object_evas_get(obj));
        evas_object_color_set(rect, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rect, ELM_SCALE_SIZE(32),
                                      ELM_SCALE_SIZE(32));
        elm_table_pack(tbl, rect, 0, 0, 1, 1);

        return tbl;
     }
   if (!strcmp(part, "elm.swallow.end"))
     return _enm_ap_end_new(id->nm, id->ap, obj);
   return NULL;
}

/* Genlist text_get for ethernet rows: returns interface name on elm.text;
 * on elm.text.sub returns the assigned IP when this is the active ethernet
 * device, or NULL otherwise. */
static char *
_enm_itc_eth_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                       const char *part)
{
   Enm_Item_Data *id = data;

   if (!strcmp(part, "elm.text"))
     return id->dev ? strdup(id->dev->interface ?: _("Wired")) : NULL;

   if (!strcmp(part, "elm.text.sub"))
     {
        struct NM_Manager *nm = id->nm;

        /* Show IP only when this ethernet device is the active connection */
        if (nm && id->dev &&
            nm->state >= NM_STATE_CONNECTED_LOCAL &&
            nm->active_conn_type == NM_DEVICE_TYPE_ETHERNET &&
            nm->ip_address)
          return strdup(nm->ip_address);

        return NULL;
     }

   return NULL;
}

/* Genlist content_get for ethernet rows: icon only */
static Evas_Object *
_enm_itc_eth_content_get(void *data, Evas_Object *obj, const char *part)
{
   Enm_Item_Data *id = data;

   if (!id->dev) return NULL;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        Evas_Object *ic, *tbl, *rect;

        tbl = elm_table_add(obj);

        ic = _enm_eth_icon_new(id->dev, obj);
        if (!ic)
          {
             evas_object_del(tbl);
             return NULL;
          }
        evas_object_show(ic);
        elm_table_pack(tbl, ic, 0, 0, 1, 1);

        rect = evas_object_rectangle_add(evas_object_evas_get(obj));
        evas_object_color_set(rect, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rect, ELM_SCALE_SIZE(32),
                                      ELM_SCALE_SIZE(32));
        elm_table_pack(tbl, rect, 0, 0, 1, 1);

        return tbl;
     }
   return NULL;
}

static char *
_enm_itc_bt_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                     const char *part)
{
   Enm_Item_Data *id = data;

   if (!strcmp(part, "elm.text"))
     return id->bt ? strdup(id->bt->name ?: _("Bluetooth")) : NULL;

   if (!strcmp(part, "elm.text.sub"))
     {
        struct NM_Manager *nm = id->nm;

        if (nm && id->bt && id->dev &&
            nm->state >= NM_STATE_CONNECTED_LOCAL &&
            nm->active_conn_type == NM_DEVICE_TYPE_BLUETOOTH &&
            nm->ip_address)
          return strdup(nm->ip_address);

        return id->dev ? strdup(id->dev->interface ?: _("Bluetooth")) : NULL;
     }

   return NULL;
}

static Evas_Object *
_enm_itc_bt_content_get(void *data, Evas_Object *obj, const char *part)
{
   Enm_Item_Data *id = data;

   if (!id->bt) return NULL;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        Evas_Object *ic, *tbl, *rect;

        tbl = elm_table_add(obj);

        ic = _enm_bt_icon_new(id->nm, id->bt, id->dev, obj);
        if (!ic)
          {
             evas_object_del(tbl);
             return NULL;
          }
        evas_object_show(ic);
        elm_table_pack(tbl, ic, 0, 0, 1, 1);

        rect = evas_object_rectangle_add(evas_object_evas_get(obj));
        evas_object_color_set(rect, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rect, ELM_SCALE_SIZE(32),
                                      ELM_SCALE_SIZE(32));
        elm_table_pack(tbl, rect, 0, 0, 1, 1);

        return tbl;
     }
   return NULL;
}

/* ---- VPN item class callbacks --------------------------------------------- */

static void
_enm_vpn_import_done_cb(void *data EINA_UNUSED, Eina_Bool ok, const char *err)
{
   E_Dialog *err_dlg;
   if (ok) { INF("VPN import succeeded"); return; }

   ERR("VPN import failed: %s", err ?: "(no output)");

   err_dlg = e_dialog_new(NULL, "E", "nm_vpn_import_error");
   if (!err_dlg) return;
   e_dialog_title_set(err_dlg, _("VPN import failed"));
   /* e_dialog_text_set uses an Elementary label that recognises <br>
    * but not raw \n; convert so multi-line nmcli stderr is readable. */
   if (err)
     {
        Eina_Strbuf *b = eina_strbuf_new();
        if (b)
          {
             eina_strbuf_append(b, err);
             eina_strbuf_replace_all(b, "\n", "<br>");
             e_dialog_text_set(err_dlg, eina_strbuf_string_get(b));
             eina_strbuf_free(b);
          }
        else
          e_dialog_text_set(err_dlg, err);
     }
   else
     e_dialog_text_set(err_dlg, _("Unknown error"));
   e_dialog_button_add(err_dlg, _("Close"), NULL, NULL, NULL);
   elm_win_center(err_dlg->win, 1, 1);
   e_dialog_show(err_dlg);
}

static void
_enm_vpn_fs_done_cb(void *data, Evas_Object *fs EINA_UNUSED, void *event)
{
   E_Dialog *dialog = data;
   const char *file = event;

   if (!file)
     { e_object_del(E_OBJECT(dialog)); return; }

   const char *type = enm_import_detect_type(file);
   if (!type)
     {
        _enm_vpn_import_done_cb(NULL, EINA_FALSE,
              _("Could not detect VPN type from file extension. "
                "Use a .conf (WireGuard) or .ovpn (OpenVPN) file."));
     }
   else
     {
        enm_import_run(type, file, _enm_vpn_import_done_cb, NULL);
     }
   e_object_del(E_OBJECT(dialog));
}

static void
_enm_vpn_import_clicked_cb(void *data, Evas_Object *o EINA_UNUSED,
                           void *einfo EINA_UNUSED)
{
   E_NM_Instance *inst = data;
   E_Dialog *dialog;
   Evas_Object *fs;
   const char *dir = efreet_download_dir_get();

   dialog = e_dialog_new(NULL, "E", "nm_vpn_import_picker");
   if (!dialog) return;
   e_dialog_resizable_set(dialog, 1);
   e_dialog_title_set(dialog, _("Import VPN Configuration"));

   fs = elm_fileselector_add(dialog->win);
   elm_fileselector_expandable_set(fs, EINA_FALSE);
   elm_fileselector_is_save_set(fs, EINA_FALSE);
   if (dir) elm_fileselector_path_set(fs, dir);
   evas_object_size_hint_weight_set(fs, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(fs, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(fs, "done", _enm_vpn_fs_done_cb, dialog);
   evas_object_smart_callback_add(fs, "activated", _enm_vpn_fs_done_cb, dialog);
   evas_object_show(fs);
   e_dialog_content_set(dialog, fs, ELM_SCALE_SIZE(360), ELM_SCALE_SIZE(320));
   elm_win_center(dialog->win, 1, 1);
   e_dialog_show(dialog);
   enm_popup_del(inst);
}

/* VPN group header: text "VPN" */
static char *
_enm_itc_group_vpn_text_get(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                             const char *part EINA_UNUSED)
{
   return strdup(_("VPN"));
}

/* VPN group header: [+] button in the end slot */
static Evas_Object *
_enm_itc_group_vpn_content_get(void *data, Evas_Object *obj, const char *part)
{
   E_NM_Instance *inst = data;
   Evas_Object *btn, *o;

   if (strcmp(part, "elm.swallow.end")) return NULL;

   btn = elm_button_add(obj);

   o = elm_icon_add(obj);
   elm_icon_standard_set(o, "add");
   elm_object_content_set(btn, o);
   evas_object_show(o);

   if (!enm_import_nmcli_path())
     {
        elm_object_disabled_set(btn, EINA_TRUE);
        elm_object_tooltip_text_set(btn,
            _("nmcli not installed — install NetworkManager CLI tools to import VPN configs."));
     }
   else
     {
        evas_object_smart_callback_add(btn, "clicked",
                                       _enm_vpn_import_clicked_cb, inst);
     }
   evas_object_propagate_events_set(btn, EINA_FALSE);
   return btn;
}

/* VPN row text: connection name (elm.text) and type+IP subtitle (elm.text.sub) */
static char *
_enm_itc_vpn_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                       const char *part)
{
   struct NM_VPN_Connection *vc = data;

   if (!strcmp(part, "elm.text"))
     return strdup(vc->name ? vc->name : _("VPN"));
   if (!strcmp(part, "elm.text.sub"))
     {
        /* Subtitle is just the tunnel IP when active. */
        if (vc->active_path && vc->ip_address)
          return strdup(vc->ip_address);
        return NULL;
     }
   if (!strcmp(part, "elm.text.protocol"))
     return strdup(enm_vpn_type_label(vc->conn_type, vc->service_type));
   return NULL;
}

/* Forward declaration needed for _enm_itc_vpn_content_get */
static void _enm_vpn_forget_btn_click_cb(void *data, Evas_Object *o EINA_UNUSED,
                                         void *einfo EINA_UNUSED);

/* VPN row content: shield icon in icon slot, edit-delete forget button in end */
static Evas_Object *
_enm_itc_vpn_content_get(void *data, Evas_Object *obj, const char *part)
{
   struct NM_VPN_Connection *vc = data;
   E_NM_Instance *inst = evas_object_data_get(obj, "instance");

   if (!strcmp(part, "elm.swallow.icon"))
     {
        /* Reuse elementary's stock shield icons: security-high (gold)
         * for active VPN, security-low (gray) for inactive. */
        Evas_Object *ic = elm_icon_add(obj);
        if (!ic) return NULL;
        elm_icon_standard_set(ic,
            vc->vpn_state == NM_VPN_STATE_ACTIVATED
              ? "security-high" : "security-low");
        evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(24), ELM_SCALE_SIZE(24));
        return ic;
     }
   if (!strcmp(part, "elm.swallow.end"))
     {
        /* Forget (delete) button — same edit-delete icon used by wifi rows. */
        Evas_Object *btn = elm_button_add(obj);
        Evas_Object *ic = elm_icon_add(btn);
        elm_icon_standard_set(ic, "edit-delete");
        elm_object_content_set(btn, ic);
        evas_object_show(ic);
        evas_object_smart_callback_add(btn, "clicked",
                                       _enm_vpn_forget_btn_click_cb, vc);
        evas_object_data_set(btn, "instance", inst);
        evas_object_propagate_events_set(btn, EINA_FALSE);
        return btn;
     }
   return NULL;
}

/* ---- VPN right-click context menu ---------------------------------------- */

/* Autoconnect toggle callback: flip the autoconnect flag for the VPN. */
static void
_enm_vpn_menu_autoconn_cb(void *data, E_Menu *m EINA_UNUSED,
                           E_Menu_Item *mi)
{
   struct NM_VPN_Connection *vc = data;
   E_NM_Module_Context *ctxt = networkmanager_mod ?
       networkmanager_mod->data : NULL;
   if (!ctxt || !ctxt->nm) return;
   enm_vpn_set_autoconnect(ctxt->nm, vc, e_menu_item_toggle_get(mi));
}

/* Small context struct that carries a copy of the VPN uuid into the
 * confirmation dialog callbacks.  Using a uuid copy instead of a raw vc
 * pointer means a ConnectionRemoved event that frees vc between the
 * right-click and the OK click cannot cause a dangling-pointer dereference. */
struct _enm_vpn_forget_ctx
{
   char *uuid;
};

static void
_enm_vpn_forget_ctx_free(struct _enm_vpn_forget_ctx *ctx)
{
   if (!ctx) return;
   free(ctx->uuid);
   free(ctx);
}

/* Fired when the dialog is destroyed without going through OK or Cancel
 * (e.g. window-manager close button). */
static void
_enm_vpn_forget_dialog_del_cb(void *obj)
{
   struct _enm_vpn_forget_ctx *ctx = e_object_data_get(E_OBJECT(obj));
   _enm_vpn_forget_ctx_free(ctx);
}

/* Confirmation dialog "Forget" button callback. */
static void
_enm_vpn_forget_ok_cb(void *data, E_Dialog *d)
{
   struct _enm_vpn_forget_ctx *ctx = data;
   E_NM_Module_Context *ctxt = networkmanager_mod ?
       networkmanager_mod->data : NULL;
   if (ctxt && ctxt->nm && ctx && ctx->uuid)
     {
        struct NM_VPN_Connection *vc =
            enm_vpn_find_by_uuid(ctxt->nm, ctx->uuid);
        if (vc) enm_vpn_delete(ctxt->nm, vc);
     }
   /* Clear the dialog's data ref before delete so the del-attach cb
    * cannot double-free the ctx. */
   e_object_data_set(E_OBJECT(d), NULL);
   _enm_vpn_forget_ctx_free(ctx);
   e_object_del(E_OBJECT(d));
}

static void
_enm_vpn_forget_cancel_cb(void *data, E_Dialog *d)
{
   struct _enm_vpn_forget_ctx *ctx = data;
   e_object_data_set(E_OBJECT(d), NULL);
   _enm_vpn_forget_ctx_free(ctx);
   e_object_del(E_OBJECT(d));
}

/* Pop the "Forget this VPN?" confirmation dialog.  Captures vc->uuid now
 * so the dialog can survive a ConnectionRemoved race that frees vc. */
static void
_enm_vpn_show_forget_dialog(struct NM_VPN_Connection *vc)
{
   struct _enm_vpn_forget_ctx *ctx;
   E_Dialog *d;
   char body[256];

   if (!vc) return;
   ctx = E_NEW(struct _enm_vpn_forget_ctx, 1);
   if (!ctx) return;
   ctx->uuid = vc->uuid ? strdup(vc->uuid) : NULL;
   if (!ctx->uuid) { free(ctx); return; }

   d = e_dialog_new(NULL, "E", "nm_vpn_forget_confirm");
   if (!d) { _enm_vpn_forget_ctx_free(ctx); return; }
   e_dialog_title_set(d, _("Forget VPN"));
   snprintf(body, sizeof(body),
            _("Delete the VPN connection \"%s\"?  This cannot be undone."),
            vc->name ? vc->name : "VPN");
   e_dialog_text_set(d, body);
   e_dialog_button_add(d, _("Forget"), NULL, _enm_vpn_forget_ok_cb, ctx);
   e_dialog_button_add(d, _("Cancel"), NULL, _enm_vpn_forget_cancel_cb, ctx);
   e_object_data_set(E_OBJECT(d), ctx);
   e_object_del_attach_func_set(E_OBJECT(d), _enm_vpn_forget_dialog_del_cb);
   elm_win_center(d->win, 1, 1);
   e_dialog_show(d);
}

static void
_enm_vpn_menu_forget_cb(void *data, E_Menu *m EINA_UNUSED,
                         E_Menu_Item *mi EINA_UNUSED)
{
   _enm_vpn_show_forget_dialog(data);
}

/* "edit-delete" forget button on each VPN row (mirrors the wifi forget UX). */
static void
_enm_vpn_forget_btn_click_cb(void *data, Evas_Object *o,
                              void *einfo EINA_UNUSED)
{
   E_NM_Instance *inst = evas_object_data_get(o, "instance");
   enm_popup_del(inst);
   _enm_vpn_show_forget_dialog(data);
}

/* Genlist "clicked,right" smart callback: pops the VPN context menu for
 * right-clicks on VPN rows.  event_info is the Elm_Object_Item. */
static void
_enm_vpn_genlist_clicked_right_cb(void *data, Evas_Object *gl EINA_UNUSED,
                                   void *event_info)
{
   E_NM_Instance *inst = data;
   Elm_Object_Item *it = event_info;
   struct NM_VPN_Connection *vc;
   E_Menu *m;
   E_Menu_Item *mi;
   Evas_Coord x, y;

   if (!it) return;
   if (elm_genlist_item_item_class_get(it) != inst->ui.popup.itc_vpn) return;

   vc = elm_object_item_data_get(it);
   if (!vc) return;

   evas_pointer_canvas_xy_get(evas_object_evas_get(gl), &x, &y);

   m = e_menu_new();

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Connect automatically"));
   e_menu_item_check_set(mi, EINA_TRUE);
   e_menu_item_toggle_set(mi, vc->autoconnect);
   e_menu_item_callback_set(mi, _enm_vpn_menu_autoconn_cb, vc);

   mi = e_menu_item_new(m);
   e_menu_item_separator_set(mi, EINA_TRUE);

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Forget"));
   e_menu_item_callback_set(mi, _enm_vpn_menu_forget_cb, vc);

   e_menu_activate_mouse(m, e_zone_current_get(),
                         x, y, 1, 1,
                         E_MENU_POP_DIRECTION_DOWN,
                         ecore_x_current_time_get());
}

/* Genlist "realized" smart callback: when a VPN row's view exists, ask the
 * item theme to reveal the protocol pill.  The default state hides the pill
 * parts so non-VPN rows (wifi/ethernet) leave the slot collapsed. */
static void
_enm_vpn_genlist_realized_cb(void *data, Evas_Object *gl EINA_UNUSED,
                              void *event_info)
{
   E_NM_Instance *inst = data;
   Elm_Object_Item *it = event_info;

   if (!it) return;
   if (elm_genlist_item_item_class_get(it) != inst->ui.popup.itc_vpn) return;
   elm_object_item_signal_emit(it, "e,pill,show", "e");
}

/* Genlist text_get for group headers: data is a string literal */
static char *
_enm_itc_group_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                         const char *part)
{
   if (!strcmp(part, "elm.text"))
     return data ? strdup(data) : NULL;
   return NULL;
}

/* Genlist text_get for the wireless group header */
static char *
_enm_itc_group_wifi_text_get(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                              const char *part)
{
   if (!strcmp(part, "elm.text")) return strdup(_("Wireless"));
   return NULL;
}

/* Toggle callback for the wireless group header on/off switch */
static void
_enm_wifi_toggle_changed(void *data, Evas_Object *obj,
                          void *info EINA_UNUSED)
{
   E_NM_Instance *inst = data;

   if (!inst || !inst->ctxt || !inst->ctxt->nm) return;
   enm_wireless_enabled_set(inst->ctxt->nm, elm_check_state_get(obj));
   enm_mod_aps_update_now();
}

/* Genlist content_get for the wireless group header: toggle in end slot */
static Evas_Object *
_enm_itc_group_wifi_content_get(void *data, Evas_Object *obj,
                                 const char *part)
{
   E_NM_Instance *inst = data;
   Evas_Object *ck;

   if (!inst || !inst->ctxt || !inst->ctxt->nm) return NULL;
   if (strcmp(part, "elm.swallow.end")) return NULL;

   ck = elm_check_add(obj);
   elm_check_state_set(ck, inst->ctxt->nm->wireless_enabled);
   evas_object_smart_callback_add(ck, "changed", _enm_wifi_toggle_changed, inst);
   evas_object_propagate_events_set(ck, EINA_FALSE);
   evas_object_show(ck);
   return ck;
}

static Eina_Bool
_enm_deselect_timer_cb(void *data)
{
   E_NM_Instance *inst = data;

   if (inst->ui.popup.deselect_item)
     {
        Elm_Object_Item *it = inst->ui.popup.deselect_item;
        Eina_Bool keep_selected = EINA_FALSE;
        const Elm_Genlist_Item_Class *icl;

        icl = elm_genlist_item_item_class_get(it);
        if (icl == inst->ui.popup.itc_vpn)
          {
             struct NM_VPN_Connection *vc = elm_object_item_data_get(it);
             keep_selected = !!(vc && vc->active_path);
          }
        else
          {
             Enm_Item_Data *id = elm_object_item_data_get(it);

             if (id && id->nm)
               {
                  if (id->ap)
                    keep_selected = _enm_ssid_is_active(id->nm, id->ap->ssid);
                  else if (id->bt && id->dev)
                    keep_selected =
                       (id->dev->state >= NM_DEVICE_STATE_ACTIVATED) &&
                       (id->nm->active_conn_type == NM_DEVICE_TYPE_BLUETOOTH) &&
                       id->nm->active_connection_path &&
                       id->dev->active_conn_path &&
                       !strcmp(id->nm->active_connection_path,
                               id->dev->active_conn_path);
                  else if (id->dev)
                    keep_selected =
                       (id->dev->type == NM_DEVICE_TYPE_ETHERNET) &&
                       (id->dev->state >= NM_DEVICE_STATE_ACTIVATED);
               }
          }

        if (!keep_selected)
          elm_genlist_item_selected_set(it, EINA_FALSE);
     }
   inst->ui.popup.deselect_item = NULL;
   inst->ui.popup.deselect_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_enm_deselect_timer_schedule(E_NM_Instance *inst, Elm_Object_Item *it)
{
   E_FREE_FUNC(inst->ui.popup.deselect_timer, ecore_timer_del);
   inst->ui.popup.deselect_item = it;
   inst->ui.popup.deselect_timer =
      ecore_timer_add(0.5, _enm_deselect_timer_cb, inst);
}

/* Genlist click smart callback — handles connect/disconnect on row tap */
static void
_enm_item_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info)
{
   E_NM_Instance *inst = data;
   Elm_Object_Item *it = event_info;
   Enm_Item_Data *id;
   struct NM_Manager *nm;
   struct NM_Device *dev;
   Eina_Bool selected = elm_genlist_item_selected_get(it);

   if (!it) return;
   if (inst->ui.popup.syncing_selection) return;
   /* VPN rows carry a struct NM_VPN_Connection*, not Enm_Item_Data*.
    * Dispatch them inline and bail before any cast-as-Enm_Item_Data. */
   if (elm_genlist_item_item_class_get(it) == inst->ui.popup.itc_vpn)
     {
        struct NM_VPN_Connection *vc = elm_object_item_data_get(it);
        E_NM_Module_Context *ctxt = networkmanager_mod ?
            networkmanager_mod->data : NULL;
        _enm_deselect_timer_schedule(inst, it);
        if (ctxt && ctxt->nm && vc)
          {
             /* Tap on an active row disconnects; tap on an inactive row
              * activates.  Mirrors the wifi/ethernet row-tap UX. */
             if (vc->active_path)
               {
                  INF("VPN row tapped (active): deactivating '%s'\n",
                      vc->name ?: "?");
                  if (!selected) enm_vpn_deactivate(ctxt->nm, vc);
               }
             else
               {
                  INF("VPN row tapped (inactive): activating '%s'",
                      vc->name ?: "?");
                  if (selected) enm_vpn_activate(ctxt->nm, vc);
               }
          }
        return;
     }
   /* Delay deselect so the user sees their click landed. */
   _enm_deselect_timer_schedule(inst, it);
   id = elm_object_item_data_get(it);
   if (!id) return;

   nm = inst->ctxt->nm;
   if (!nm) return;

   if (id->bt)
     {
        if (!id->dev || !id->connection_path) return;

        if (nm->active_conn_type == NM_DEVICE_TYPE_BLUETOOTH &&
            id->dev->state >= NM_DEVICE_STATE_ACTIVATED)
          {
             INF("Disconnect bluetooth connection %s",
                 id->bt->name ?: id->connection_path);
             if (!selected) enm_ap_disconnect(nm);
          }
        else
          {
             enm_disconnect_type(nm, NM_DEVICE_TYPE_ETHERNET);
             enm_disconnect_type(nm, NM_DEVICE_TYPE_WIFI);
             INF("Connect bluetooth profile %s on device %s",
                 id->bt->name ?: id->connection_path,
                 id->dev->interface ?: id->dev->path);
             if (selected) enm_bluetooth_connect(nm, id->dev, id->connection_path);
          }
        return;
     }

   if (!id->ap)
     {
        if (!id->dev || id->dev->type != NM_DEVICE_TYPE_ETHERNET) return;

        if (id->dev->state >= NM_DEVICE_STATE_ACTIVATED &&
            id->dev->active_conn_path)
          {
             INF("Disconnect ethernet device %s",
                 id->dev->interface ?: id->dev->path);
             if (!selected) enm_disconnect_type(nm, NM_DEVICE_TYPE_ETHERNET);
          }
        else
          {
             enm_disconnect_type(nm, NM_DEVICE_TYPE_BLUETOOTH);
             enm_disconnect_type(nm, NM_DEVICE_TYPE_WIFI);
             INF("Connect ethernet device %s",
                 id->dev->interface ?: id->dev->path);
             if (selected) enm_ethernet_connect(nm, id->dev);
          }
        return;
     }

   /* If this AP's SSID is currently active, disconnect */
   if (nm->active_ap_path && _enm_ssid_is_active(nm, id->ap->ssid))
     {
        INF("Disconnect from %s", id->ap->ssid ?: id->ap_path);
        if (!selected) enm_ap_disconnect(nm);
        return;
     }

   /* Walk devices to find which one owns this AP, then connect */
   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        struct NM_Access_Point *a;
        EINA_INLIST_FOREACH(dev->access_points, a)
          {
            if (a == id->ap)
               {
                  enm_disconnect_type(nm, NM_DEVICE_TYPE_ETHERNET);
                  enm_disconnect_type(nm, NM_DEVICE_TYPE_BLUETOOTH);
                  INF("Connect to %s on device %s",
                      id->ap->ssid ?: id->ap_path,
                      dev->interface ?: dev->path);
                  if (selected) enm_ap_connect(nm, dev, id->ap);
                  return;
               }
          }
     }
}

static Evas_Object *
_enm_ap_icon_new(struct NM_Manager *nm, struct NM_Access_Point *ap,
                  Evas_Object *parent)
{
   Edje_Message_Int_Set *msg;
   Evas_Object *icon, *ed;
   int state_val;

   icon = elm_layout_add(parent);
   _enm_theme_layout_file_set(icon, "icon/wifi");
   evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
   ed = elm_layout_edje_get(icon);

   /* Map active AP to ONLINE(5), otherwise IDLE(1) — ConnMan theme values */
   state_val = (ap->ssid && _enm_ssid_is_active(nm, ap->ssid)) ? 5 : 1;

   msg = malloc(sizeof(*msg) + sizeof(int));
   if (msg)
     {
        msg->count = 2;
        msg->val[0] = state_val;
        msg->val[1] = ap->strength;
        edje_object_message_send(ed, EDJE_MESSAGE_INT_SET, 1, msg);
        free(msg);
     }

   /* Emit security signal for lock overlay */
   {
      const char *sec;
      char secbuf[128];

      sec = enm_ap_security_to_str(ap->wpa_flags, ap->rsn_flags);
      if (sec && strcmp(sec, "open"))
        {
           if (!strcmp(sec, "wpa") || !strcmp(sec, "wpa2") || !strcmp(sec, "sae"))
             snprintf(secbuf, sizeof(secbuf), "e,security,psk");
           else if (!strcmp(sec, "wep"))
             snprintf(secbuf, sizeof(secbuf), "e,security,wep");
           else if (!strcmp(sec, "802.1x"))
             snprintf(secbuf, sizeof(secbuf), "e,security,ieee8021x");
           else
             snprintf(secbuf, sizeof(secbuf), "e,security,%s", sec);
           elm_layout_signal_emit(icon, secbuf, "e");
        }
      else
        elm_layout_signal_emit(icon, "e,security,off", "e");
   }

   /* Set frequency band label */
   if (ap->frequency > 0)
     {
        const char *band;

        if (ap->frequency >= 5925)
          band = "6";
        else if (ap->frequency >= 3000)
          band = "5";
        else
          band = "2.4";

        elm_object_part_text_set(icon, "e.text.band-label", band);
     }

   return icon;
}

struct _Enm_Forget_Data
{
   struct NM_Manager *nm;
   const char *connection_path; /* stringshare */
   const char *ssid;            /* stringshare — for optimistic hash removal */
};

static void
_enm_forget_click_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   struct _Enm_Forget_Data *fd = data;
   struct NM_Manager *nm = fd->nm;
   /* Take a ref now: enm_mod_aps_update_now() may delete the forget button
    * widget (replacing it with NULL), which fires EVAS_CALLBACK_DEL and frees
    * fd — including its connection_path stringshare — before we use it. */
   const char *conn_path = eina_stringshare_ref(fd->connection_path);

   INF("Forget connection: %s (ssid=%s)", conn_path, fd->ssid ?: "(null)");

   /* Optimistic update: remove SSID from local hash immediately so the
    * forget icon disappears at once without waiting for D-Bus round trips.
    * The authoritative refresh happens later via the ConnectionRemoved signal. */
   if (nm->saved_connections && fd->ssid)
     eina_hash_del_by_key(nm->saved_connections, fd->ssid);

   /* Rebuild all open popups right now, bypassing the 0.5s throttle timer.
    * This deletes the forget button widget, which frees fd — do not touch
    * fd after this point. */
   enm_mod_aps_update_now();

   /* Kick off the actual async delete — ConnectionRemoved signal will
    * trigger enm_saved_connections_get once NM has committed the removal. */
   enm_connection_delete(nm, conn_path);
   eina_stringshare_del(conn_path);
}

static void
_enm_forget_data_free_cb(void *data, Evas *e EINA_UNUSED,
                          Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   struct _Enm_Forget_Data *fd = data;
   eina_stringshare_del(fd->connection_path);
   eina_stringshare_del(fd->ssid);
   free(fd);
}

static Evas_Object *
_enm_ap_end_new(struct NM_Manager *nm, struct NM_Access_Point *ap,
                Evas_Object *parent)
{
   Evas_Object *end, *ic;
   const char *conn_path;
   struct _Enm_Forget_Data *fd;

   /* Only show forget for saved (known) networks */
   if (!nm->saved_connections || !ap->ssid)
     {
        DBG("forget: no hash (%p) or no ssid (%p)", nm->saved_connections, ap->ssid);
        return NULL;
     }

   conn_path = eina_hash_find(nm->saved_connections, ap->ssid);
   if (!conn_path)
     {
        DBG("forget: ssid '%s' not in saved_connections hash (size=%d)",
            ap->ssid, nm->saved_connections ? eina_hash_population(nm->saved_connections) : -1);
        return NULL;
     }

   INF("forget: creating button for ssid '%s' -> %s", ap->ssid, conn_path);

   fd = malloc(sizeof(*fd));
   if (!fd) return NULL;
   fd->nm = nm;
   fd->connection_path = eina_stringshare_add(conn_path);
   if (!fd->connection_path)
     {
        free(fd);
        return NULL;
     }
   fd->ssid = eina_stringshare_add(ap->ssid);

   end = elm_button_add(parent);
   ic = elm_icon_add(end);
   elm_icon_standard_set(ic, "edit-delete");
   elm_object_content_set(end, ic);
   evas_object_show(ic);
   evas_object_smart_callback_add(end, "clicked", _enm_forget_click_cb, fd);
   evas_object_event_callback_add(end, EVAS_CALLBACK_DEL,
                                  _enm_forget_data_free_cb, fd);

   return end;
}

static Evas_Object *
_enm_eth_icon_new(struct NM_Device *dev, Evas_Object *parent)
{
   Edje_Message_Int_Set *msg;
   Evas_Object *icon, *ed;
   int state_val;

   icon = elm_layout_add(parent);
   _enm_theme_layout_file_set(icon, "icon/ethernet");
   evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
   ed = elm_layout_edje_get(icon);

   /* NM device state >= activated → ONLINE(5), otherwise IDLE(1) */
   state_val = (dev->state >= NM_DEVICE_STATE_ACTIVATED) ? 5 : 1;

   msg = malloc(sizeof(*msg) + sizeof(int));
   if (msg)
     {
        msg->count = 2;
        msg->val[0] = state_val;
        msg->val[1] = 100; /* ethernet has no signal strength concept */
        edje_object_message_send(ed, EDJE_MESSAGE_INT_SET, 1, msg);
        free(msg);
     }

   return icon;
}

static Evas_Object *
_enm_bt_icon_new(struct NM_Manager *nm, struct NM_Bluetooth_Connection *bc,
                 struct NM_Device *dev, Evas_Object *parent)
{
   Edje_Message_Int_Set *msg;
   Evas_Object *icon, *ed;
   int state_val = 0;

   icon = elm_layout_add(parent);
   _enm_theme_layout_file_set(icon, "icon/bluetooth");
   evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
   ed = elm_layout_edje_get(icon);

   if (nm && bc && dev &&
       nm->state >= NM_STATE_CONNECTED_LOCAL &&
       nm->active_conn_type == NM_DEVICE_TYPE_BLUETOOTH &&
       dev->state >= NM_DEVICE_STATE_ACTIVATED)
     state_val = 5;
   else if (dev)
     state_val = 1;

   msg = malloc(sizeof(*msg) + sizeof(int));
   if (msg)
     {
        msg->count = 2;
        msg->val[0] = state_val;
        msg->val[1] = 100;
        edje_object_message_send(ed, EDJE_MESSAGE_INT_SET, 1, msg);
        free(msg);
     }

   return icon;
}

/* Find the best AP for a given SSID across all devices.
 * Prefers the active AP if it matches, otherwise picks highest strength.
 * Used to deduplicate multiple stations broadcasting the same network name. */
static struct NM_Access_Point *
_enm_best_ap_for_ssid(struct NM_Manager *nm, const char *ssid)
{
   struct NM_Device *dev;
   struct NM_Access_Point *best = NULL;

   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        struct NM_Access_Point *ap;
        if (dev->type != NM_DEVICE_TYPE_WIFI) continue;
        EINA_INLIST_FOREACH(dev->access_points, ap)
          {
             if (!ap->ssid || !ap->ssid[0]) continue;
             if (strcmp(ap->ssid, ssid)) continue;
             /* Always prefer the active AP */
             if (nm->active_ap_path && nm->active_ap_path == ap->path)
               return ap;
             if (!best || ap->strength > best->strength)
               best = ap;
          }
     }
   return best;
}

/* Check if the given SSID matches the currently connected network */
static Eina_Bool
_enm_ssid_is_active(struct NM_Manager *nm, const char *ssid)
{
   struct NM_Access_Point *active;

   if (!nm->active_ap_path) return EINA_FALSE;
   active = enm_manager_find_ap(nm, nm->active_ap_path);
   if (!active || !active->ssid) return EINA_FALSE;
   return !strcmp(active->ssid, ssid);
}

/* Build the desired list of entries.  Returns count; caller frees labels/paths. */
struct _Popup_Entry
{
   enum {
      ENM_ENTRY_ETHERNET,
      ENM_ENTRY_WIFI,
      ENM_ENTRY_BLUETOOTH
   } kind;
   const char *label;   /* SSID or interface name — owned by AP/dev, do not free */
   const char *ap_path; /* stringshare AP path for wifi, NULL for ethernet */
   const char *connection_path; /* stringshare settings path for bluetooth */
   struct NM_Access_Point *ap; /* NULL for ethernet */
   struct NM_Device       *dev; /* only for ethernet */
   struct NM_Bluetooth_Connection *bt;
};

static int
_enm_popup_build_entries(struct NM_Manager *nm,
                         struct _Popup_Entry *out, int max)
{
   struct NM_Device *dev;
   Eina_Hash *seen_ssids;
   int n = 0;

   if (!nm) return 0;

   /* Ethernet entries first: prioritize active connections,
    * then include available idle devices. */
   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        if (dev->type != NM_DEVICE_TYPE_ETHERNET) continue;
        if (dev->state < NM_DEVICE_STATE_ACTIVATED) continue;
        if (n >= max) break;

        out[n].kind = ENM_ENTRY_ETHERNET;
        out[n].label = dev->interface ?: _("Wired");
        out[n].ap_path = NULL;
        out[n].connection_path = NULL;
        out[n].ap = NULL;
        out[n].dev = dev;
        out[n].bt = NULL;
        n++;
     }

   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        if (dev->type != NM_DEVICE_TYPE_ETHERNET) continue;
        if (dev->state >= NM_DEVICE_STATE_ACTIVATED) continue;
        if (dev->state < NM_DEVICE_STATE_UNAVAILABLE) continue;
        if (n >= max) break;

        out[n].kind = ENM_ENTRY_ETHERNET;
        out[n].label = dev->interface ?: _("Wired");
        out[n].ap_path = NULL;
        out[n].connection_path = NULL;
        out[n].ap = NULL;
        out[n].dev = dev;
        out[n].bt = NULL;
        n++;
     }

   /* Deduplicated WiFi entries */
   seen_ssids = eina_hash_string_superfast_new(NULL);

   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        struct NM_Access_Point *ap;

        if (dev->type != NM_DEVICE_TYPE_WIFI) continue;

        EINA_INLIST_FOREACH(dev->access_points, ap)
          {
             struct NM_Access_Point *best;

             if (!ap->ssid || !ap->ssid[0]) continue;
             if (eina_hash_find(seen_ssids, ap->ssid)) continue;

             best = _enm_best_ap_for_ssid(nm, ap->ssid);
             if (!best) continue;

             eina_hash_add(seen_ssids, ap->ssid, (void *)1);

             if (n >= max) break;
             out[n].kind = ENM_ENTRY_WIFI;
             out[n].label = best->ssid;
             out[n].ap_path = best->path;
             out[n].connection_path = NULL;
             out[n].ap = best;
             out[n].dev = NULL;
             out[n].bt = NULL;
             n++;
          }
     }

   eina_hash_free(seen_ssids);

   {
      struct NM_Bluetooth_Connection *bc;

      EINA_INLIST_FOREACH(nm->bluetooth_connections, bc)
        {
           struct NM_Device *bt_dev;

           if (n >= max) break;
           bt_dev = enm_bluetooth_connection_device_find(nm, bc);
           if (!bt_dev) continue;

           out[n].kind = ENM_ENTRY_BLUETOOTH;
           out[n].label = bc->name ?: _("Bluetooth");
           out[n].ap_path = NULL;
           out[n].connection_path = bc->path;
           out[n].ap = NULL;
           out[n].dev = bt_dev;
           out[n].bt = bc;
           n++;
        }
   }

   return n;
}

static Eina_Bool
_enm_has_wifi_device(struct NM_Manager *nm)
{
   struct NM_Device *dev;
   if (!nm) return EINA_FALSE;
   EINA_INLIST_FOREACH(nm->devices, dev)
     if (dev->type == NM_DEVICE_TYPE_WIFI) return EINA_TRUE;
   return EINA_FALSE;
}

static Eina_Bool
_enm_has_bt_entries(struct NM_Manager *nm)
{
   struct NM_Bluetooth_Connection *bc;

   if (!nm) return EINA_FALSE;
   EINA_INLIST_FOREACH(nm->bluetooth_connections, bc)
     if (enm_bluetooth_connection_device_find(nm, bc))
       return EINA_TRUE;
   return EINA_FALSE;
}

static Eina_Bool
_enm_popup_item_should_be_selected(E_NM_Instance *inst,
                                   Elm_Object_Item *it)
{
   const Elm_Genlist_Item_Class *icl;

   if (!it) return EINA_FALSE;

   icl = elm_genlist_item_item_class_get(it);
   if (icl == inst->ui.popup.itc_vpn)
     {
        struct NM_VPN_Connection *vc = elm_object_item_data_get(it);
        return !!(vc && vc->active_path);
     }

   if ((icl == inst->ui.popup.itc_group) ||
       (icl == inst->ui.popup.itc_group_wifi) ||
       (icl == inst->ui.popup.itc_group_vpn))
     return EINA_FALSE;

   {
      Enm_Item_Data *id = elm_object_item_data_get(it);

      if (!id || !id->nm) return EINA_FALSE;

      if (id->ap)
        return _enm_ssid_is_active(id->nm, id->ap->ssid);

      if (id->bt && id->dev)
        return (id->dev->state >= NM_DEVICE_STATE_ACTIVATED) &&
           (id->nm->active_conn_type == NM_DEVICE_TYPE_BLUETOOTH) &&
           id->nm->active_connection_path &&
           id->dev->active_conn_path &&
           !strcmp(id->nm->active_connection_path, id->dev->active_conn_path);

      if (id->dev)
        return (id->dev->type == NM_DEVICE_TYPE_ETHERNET) &&
           (id->dev->state >= NM_DEVICE_STATE_ACTIVATED);
   }

   return EINA_FALSE;
}

static void
_enm_popup_selection_sync(E_NM_Instance *inst)
{
   Elm_Object_Item *it, *next;

   if (!inst || !inst->ui.popup.genlist) return;

   inst->ui.popup.syncing_selection = EINA_TRUE;
   for (it = elm_genlist_first_item_get(inst->ui.popup.genlist);
        it; it = next)
     {
        Eina_Bool want_selected;

        next = elm_genlist_item_next_get(it);
        want_selected = _enm_popup_item_should_be_selected(inst, it);
        if (elm_genlist_item_selected_get(it) != want_selected)
          elm_genlist_item_selected_set(it, want_selected);
     }
   inst->ui.popup.syncing_selection = EINA_FALSE;
}

/* Drop a tracked group-header pointer if it matches.  Used in the orphan
 * cleanup pass so we don't leave stale Elm_Object_Item * pointers in
 * inst->ui.popup.group_{eth,wifi,vpn}. */
static void
_enm_drop_group_if(E_NM_Instance *inst, Elm_Object_Item *it)
{
   if (inst->ui.popup.group_eth == it)  inst->ui.popup.group_eth = NULL;
   if (inst->ui.popup.group_wifi == it) inst->ui.popup.group_wifi = NULL;
   if (inst->ui.popup.group_bt == it)   inst->ui.popup.group_bt = NULL;
   if (inst->ui.popup.group_vpn == it)  inst->ui.popup.group_vpn = NULL;
}

/* Diff-and-sync update of the popup genlist contents.
 *
 * The previous implementation called elm_genlist_clear() and re-appended
 * every row on each call, which produced a visible blink whenever any
 * NetworkManager state changed (new AP, signal strength tick, VPN
 * activate/deactivate, ...).  This walker keeps existing rows in place,
 * re-runs their text_get/content_get only when needed via
 * elm_genlist_item_update(), and only appends/removes items at the diff
 * boundary.
 *
 * Identity per row:
 *   - ethernet: Enm_Item_Data.dev pointer (stable per device)
 *   - wifi:     Enm_Item_Data.ssid stringshare (stable per SSID, while the
 *               concrete "best AP" struct backing it can change across
 *               scans — we refresh id->ap and id->ap_path on reuse)
 *   - VPN:      NM_VPN_Connection.path stringshare
 *   - groups:   the per-instance itc pointer (one of each per popup)
 */
static void
_enm_popup_update(struct NM_Manager *nm, E_NM_Instance *inst)
{
   Evas_Object *gl = inst->ui.popup.genlist;
   struct _Popup_Entry desired[256];
   int want_n, i, eth_count = 0, wifi_count = 0, bt_count = 0;
   Eina_Hash *eth_idx, *ap_idx, *bt_idx, *vpn_idx;
   Eina_List *orphans = NULL;
   Elm_Object_Item *it, *prev;
   Eina_Iterator *itr;

   EINA_SAFETY_ON_NULL_RETURN(nm);
   EINA_SAFETY_ON_NULL_RETURN(gl);

   /* NOTE: we intentionally do NOT kill the deselect timer here.  When the
    * user taps a row we schedule a 0.5s deselect so the highlight feels
    * deliberate.  The VPN active-changed path runs synchronously via an
    * Ecore_Job and would land in _enm_popup_update almost immediately —
    * killing the timer here meant the tapped row stayed selected, and
    * Elementary genlist doesn't refire "selected" on an already-selected
    * item, so the user couldn't tap again to deactivate.  Instead we
    * cancel only when the target item is actually deleted (orphan pass
    * below). */

   want_n = _enm_popup_build_entries(nm, desired, 256);
   for (i = 0; i < want_n; i++)
     {
        if (desired[i].kind == ENM_ENTRY_WIFI) wifi_count++;
        else if (desired[i].kind == ENM_ENTRY_BLUETOOTH) bt_count++;
        else eth_count++;
     }

   /* ------------------------------------------------------------------
    * Pass 1: index every existing row by its identity.  Group headers
    * already live on inst->ui.popup.group_*, so they're skipped here.
    * ------------------------------------------------------------------ */
   eth_idx = eina_hash_pointer_new(NULL);   /* key: NM_Device *      */
   ap_idx  = eina_hash_string_superfast_new(NULL); /* key: ssid stringshare */
   bt_idx  = eina_hash_string_superfast_new(NULL); /* key: settings path */
   vpn_idx = eina_hash_string_superfast_new(NULL); /* key: vc->path        */

   it = elm_genlist_first_item_get(gl);
   while (it)
     {
        const Elm_Genlist_Item_Class *icl = elm_genlist_item_item_class_get(it);

        if (icl == inst->ui.popup.itc_eth)
          {
             Enm_Item_Data *id = elm_object_item_data_get(it);
             if (id && id->dev) eina_hash_add(eth_idx, &id->dev, it);
          }
        else if (icl == inst->ui.popup.itc_ap)
          {
             Enm_Item_Data *id = elm_object_item_data_get(it);
             if (id && id->ssid) eina_hash_add(ap_idx, id->ssid, it);
          }
        else if (icl == inst->ui.popup.itc_bt)
          {
             Enm_Item_Data *id = elm_object_item_data_get(it);
             if (id && id->connection_path)
               eina_hash_add(bt_idx, id->connection_path, it);
          }
        else if (icl == inst->ui.popup.itc_vpn)
          {
             struct NM_VPN_Connection *vc = elm_object_item_data_get(it);
             if (vc && vc->path) eina_hash_add(vpn_idx, vc->path, it);
          }
        it = elm_genlist_item_next_get(it);
     }

   /* ------------------------------------------------------------------
    * Pass 2: sync ethernet section.
    * Keep prev pointing at the just-placed item so new rows insert
    * after their predecessor and ordering is preserved.
    * ------------------------------------------------------------------ */
   if (eth_count > 0)
     {
        if (!inst->ui.popup.group_eth)
          {
             inst->ui.popup.group_eth = elm_genlist_item_append(
                gl, inst->ui.popup.itc_group, (void *)_("Wired"), NULL,
                ELM_GENLIST_ITEM_GROUP, NULL, NULL);
             elm_genlist_item_select_mode_set(inst->ui.popup.group_eth,
                ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
          }
        /* prev tracks the last placed real row in this section.  While it
         * is NULL we have no real predecessor yet, so the first cache-miss
         * insert must use elm_genlist_item_append(parent=group) — passing
         * the group header itself as the "after" of insert_after misroutes
         * the new item (it lands as a sibling at top level instead of as a
         * child of the group), breaking section grouping and the pass-1
         * index next refresh. */
        prev = NULL;

        for (i = 0; i < want_n; i++)
          {
             if (desired[i].kind != ENM_ENTRY_ETHERNET) continue;
             it = eina_hash_find(eth_idx, &desired[i].dev);
             if (it)
               {
                  /* Reuse: refresh nm pointer (paranoia) and re-run cbs. */
                  Enm_Item_Data *id = elm_object_item_data_get(it);
                  if (id) id->nm = nm;
                  elm_genlist_item_update(it);
                  eina_hash_del(eth_idx, &desired[i].dev, it);
               }
             else
               {
                  Enm_Item_Data *id = calloc(1, sizeof(*id));
                  if (!id) continue;
                  id->nm = nm;
                  id->dev = desired[i].dev;
                  if (!prev)
                    it = elm_genlist_item_append(
                       gl, inst->ui.popup.itc_eth, id,
                       inst->ui.popup.group_eth,
                       ELM_GENLIST_ITEM_NONE, NULL, NULL);
                  else
                    it = elm_genlist_item_insert_after(
                       gl, inst->ui.popup.itc_eth, id,
                       inst->ui.popup.group_eth, prev,
                       ELM_GENLIST_ITEM_NONE, NULL, NULL);
               }
             prev = it;
          }
     }
   else if (inst->ui.popup.group_eth)
     {
        /* No ethernet rows desired and no group needed.  Any leftover
         * eth rows are picked up by the orphan pass below. */
        elm_object_item_del(inst->ui.popup.group_eth);
        inst->ui.popup.group_eth = NULL;
     }

   /* ------------------------------------------------------------------
    * Pass 3: sync wifi section.  Header is present whenever a wifi
    * adapter exists, regardless of AP count.
    * ------------------------------------------------------------------ */
   if (_enm_has_wifi_device(nm))
     {
        if (!inst->ui.popup.group_wifi)
          {
             inst->ui.popup.group_wifi = elm_genlist_item_append(
                gl, inst->ui.popup.itc_group_wifi, inst, NULL,
                ELM_GENLIST_ITEM_GROUP, NULL, NULL);
             elm_genlist_item_select_mode_set(inst->ui.popup.group_wifi,
                ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
          }
        else
          {
             /* Wireless-enabled toggle state may have flipped — refresh. */
             elm_genlist_item_update(inst->ui.popup.group_wifi);
          }
        prev = NULL;

        for (i = 0; i < want_n; i++)
          {
             if (desired[i].kind != ENM_ENTRY_WIFI) continue;
             it = eina_hash_find(ap_idx, desired[i].label);
             if (it)
               {
                  /* Reuse: the "best AP" pointer for this SSID may have
                   * rotated to a stronger AP, and signal strength/security
                   * almost certainly changed — refresh the Enm_Item_Data
                   * fields before triggering a content/text re-fetch. */
                  Enm_Item_Data *id = elm_object_item_data_get(it);
                  if (id)
                    {
                       id->nm = nm;
                       id->ap = desired[i].ap;
                       eina_stringshare_replace(&id->ap_path,
                                                desired[i].ap_path);
                    }
                  elm_genlist_item_update(it);
                  eina_hash_del(ap_idx, desired[i].label, it);
               }
             else
               {
                  Enm_Item_Data *id = calloc(1, sizeof(*id));
                  if (!id) continue;
                  id->nm = nm;
                  id->ap = desired[i].ap;
                  id->ap_path = eina_stringshare_add(desired[i].ap_path);
                  id->ssid = eina_stringshare_add(desired[i].label);
                  if (!prev)
                    it = elm_genlist_item_append(
                       gl, inst->ui.popup.itc_ap, id,
                       inst->ui.popup.group_wifi,
                       ELM_GENLIST_ITEM_NONE, NULL, NULL);
                  else
                    it = elm_genlist_item_insert_after(
                       gl, inst->ui.popup.itc_ap, id,
                       inst->ui.popup.group_wifi, prev,
                       ELM_GENLIST_ITEM_NONE, NULL, NULL);
               }
             prev = it;
          }
     }
   else if (inst->ui.popup.group_wifi)
     {
        elm_object_item_del(inst->ui.popup.group_wifi);
        inst->ui.popup.group_wifi = NULL;
     }

   /* ------------------------------------------------------------------
    * Pass 4: sync bluetooth section.
    * ------------------------------------------------------------------ */
   if (bt_count > 0 || _enm_has_bt_entries(nm))
     {
        if (!inst->ui.popup.group_bt)
          {
             inst->ui.popup.group_bt = elm_genlist_item_append(
                gl, inst->ui.popup.itc_group, (void *)_("Bluetooth"), NULL,
                ELM_GENLIST_ITEM_GROUP, NULL, NULL);
             elm_genlist_item_select_mode_set(inst->ui.popup.group_bt,
                ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
          }
        prev = NULL;

        for (i = 0; i < want_n; i++)
          {
             if (desired[i].kind != ENM_ENTRY_BLUETOOTH) continue;
             it = eina_hash_find(bt_idx, desired[i].connection_path);
             if (it)
               {
                  Enm_Item_Data *id = elm_object_item_data_get(it);
                  if (id)
                    {
                       id->nm = nm;
                       id->bt = desired[i].bt;
                       id->dev = desired[i].dev;
                    }
                  elm_genlist_item_update(it);
                  eina_hash_del(bt_idx, desired[i].connection_path, it);
               }
             else
               {
                  Enm_Item_Data *id = calloc(1, sizeof(*id));
                  if (!id) continue;
                  id->nm = nm;
                  id->bt = desired[i].bt;
                  id->dev = desired[i].dev;
                  id->connection_path =
                     eina_stringshare_add(desired[i].connection_path);
                  if (!prev)
                    it = elm_genlist_item_append(
                       gl, inst->ui.popup.itc_bt, id,
                       inst->ui.popup.group_bt,
                       ELM_GENLIST_ITEM_NONE, NULL, NULL);
                  else
                    it = elm_genlist_item_insert_after(
                       gl, inst->ui.popup.itc_bt, id,
                       inst->ui.popup.group_bt, prev,
                       ELM_GENLIST_ITEM_NONE, NULL, NULL);
               }
             prev = it;
          }
     }
   else if (inst->ui.popup.group_bt)
     {
        elm_object_item_del(inst->ui.popup.group_bt);
        inst->ui.popup.group_bt = NULL;
     }

   /* ------------------------------------------------------------------
    * Pass 5: sync VPN section.  Always present.
    * ------------------------------------------------------------------ */
   {
      struct NM_VPN_Connection *vc;

      if (!inst->ui.popup.group_vpn)
        {
           inst->ui.popup.group_vpn = elm_genlist_item_append(
              gl, inst->ui.popup.itc_group_vpn, inst, NULL,
              ELM_GENLIST_ITEM_GROUP, NULL, NULL);
           elm_genlist_item_select_mode_set(inst->ui.popup.group_vpn,
              ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
        }
      prev = NULL;

      EINA_INLIST_FOREACH(nm->vpn_connections, vc)
        {
           if (!vc->path) continue;
           it = eina_hash_find(vpn_idx, vc->path);
           if (it)
             {
                elm_genlist_item_update(it);
                eina_hash_del(vpn_idx, vc->path, it);
             }
           else
             {
                if (!prev)
                  it = elm_genlist_item_append(
                     gl, inst->ui.popup.itc_vpn, vc,
                     inst->ui.popup.group_vpn,
                     ELM_GENLIST_ITEM_NONE, NULL, NULL);
                else
                  it = elm_genlist_item_insert_after(
                     gl, inst->ui.popup.itc_vpn, vc,
                     inst->ui.popup.group_vpn, prev,
                     ELM_GENLIST_ITEM_NONE, NULL, NULL);
                /* New VPN row: the genlist's "realized" smart callback
                 * will fire when the row is rendered and emit
                 * "e,pill,show".  No extra wiring needed here. */
             }
           prev = it;
        }
   }

   /* ------------------------------------------------------------------
    * Pass 6: any items left in the indexes are orphans — desired set
    * no longer contains them, so delete.  Their itc->func.del takes
    * care of freeing Enm_Item_Data; VPN rows have del=NULL because the
    * module owns NM_VPN_Connection lifetime.
    * ------------------------------------------------------------------ */
   itr = eina_hash_iterator_data_new(eth_idx);
   EINA_ITERATOR_FOREACH(itr, it) orphans = eina_list_append(orphans, it);
   eina_iterator_free(itr);

   itr = eina_hash_iterator_data_new(ap_idx);
   EINA_ITERATOR_FOREACH(itr, it) orphans = eina_list_append(orphans, it);
   eina_iterator_free(itr);

   itr = eina_hash_iterator_data_new(bt_idx);
   EINA_ITERATOR_FOREACH(itr, it) orphans = eina_list_append(orphans, it);
   eina_iterator_free(itr);

   itr = eina_hash_iterator_data_new(vpn_idx);
   EINA_ITERATOR_FOREACH(itr, it) orphans = eina_list_append(orphans, it);
   eina_iterator_free(itr);

   EINA_LIST_FREE(orphans, it)
     {
        _enm_drop_group_if(inst, it); /* defensive — shouldn't be a group */
        /* If the row being deleted is the pending deselect target,
         * cancel the timer so it doesn't fire on a dangling pointer. */
        if (inst->ui.popup.deselect_item == it)
          {
             E_FREE_FUNC(inst->ui.popup.deselect_timer, ecore_timer_del);
             inst->ui.popup.deselect_item = NULL;
          }
        elm_object_item_del(it);
     }

   _enm_popup_selection_sync(inst);

   eina_hash_free(eth_idx);
   eina_hash_free(ap_idx);
   eina_hash_free(bt_idx);
   eina_hash_free(vpn_idx);
}

/* Wrap content in an elm_table with a transparent sizer rect so the whole
 * assembly has a floor of w x h (zone-relative, clamped by min/max) without
 * fighting the content's own min-size calculation. */
static Evas_Object *
_enm_widget_size_wrap(E_NM_Instance *inst, Evas_Object *content,
                      Evas_Coord percent_w, Evas_Coord percent_h,
                      Evas_Coord min_w, Evas_Coord min_h,
                      Evas_Coord max_w, Evas_Coord max_h)
{
   Evas_Object *tbl, *rect;
   Evas_Coord w, h, zw, zh;
   E_Zone *zone;

   zone = e_gadcon_client_zone_get(inst->gcc);
   e_zone_useful_geometry_get(zone, NULL, NULL, &zw, &zh);

   w = (zw * percent_w) / 100.0;
   h = (zh * percent_h) / 100.0;

   min_w *= elm_config_scale_get();
   max_w *= elm_config_scale_get();
   min_h *= elm_config_scale_get();
   max_h *= elm_config_scale_get();

   if      (w < min_w) w = min_w;
   else if (w > max_w) w = max_w;
   if      (h < min_h) h = min_h;
   else if (h > max_h) h = max_h;

   tbl = elm_table_add(e_comp->elm);

   rect = evas_object_rectangle_add(evas_object_evas_get(content));
   evas_object_color_set(rect, 0, 0, 0, 0);
   evas_object_size_hint_min_set(rect, w, h);
   elm_table_pack(tbl, rect, 0, 0, 1, 1);

   elm_table_pack(tbl, content, 0, 0, 1, 1);

   return tbl;
}

static void
_enm_popup_new(E_NM_Instance *inst)
{
   E_NM_Module_Context *ctxt = inst->ctxt;
   Evas_Object *box, *button, *gl, *icon;
   Elm_Genlist_Item_Class *itc;

   EINA_SAFETY_ON_FALSE_RETURN(inst->popup == NULL);

   if (!ctxt->nm) return;

   e_nm_scan(ctxt->nm);

   inst->popup = e_gadcon_popup_new(inst->gcc, 0);

   /* Outer elm box to stack genlist + IP label */
   box = elm_box_add(e_comp->elm);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   /* Genlist for AP/ethernet rows */
   gl = elm_genlist_add(box);
   evas_object_size_hint_weight_set(gl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_bounce_set(gl, EINA_FALSE, EINA_TRUE);
   elm_genlist_multi_select_set(gl, EINA_TRUE);
   elm_genlist_select_mode_set(gl, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_data_set(gl, "instance", inst);
   inst->ui.popup.genlist = gl;

   /* Item classes — created per-popup, freed in enm_popup_del */
   itc = elm_genlist_item_class_new();
   itc->item_style = "group_index";
   itc->func.text_get = _enm_itc_group_text_get;
   itc->func.content_get = NULL;
   itc->func.state_get = NULL;
   itc->func.del = NULL;
   inst->ui.popup.itc_group = itc;

   itc = elm_genlist_item_class_new();
   itc->item_style = "group_index";
   itc->func.text_get = _enm_itc_group_wifi_text_get;
   itc->func.content_get = _enm_itc_group_wifi_content_get;
   itc->func.state_get = NULL;
   itc->func.del = NULL;
   inst->ui.popup.itc_group_wifi = itc;

   itc = elm_genlist_item_class_new();
   /* double_label_blue: same layout as double_label but elm.text.sub is
    * rendered in wifi-band blue (51 153 255) so IPs stand out clearly. */
   itc->item_style = "double_label";
   itc->func.text_get = _enm_itc_ap_text_get;
   itc->func.content_get = _enm_itc_ap_content_get;
   itc->func.state_get = NULL;
   itc->func.del = _enm_itc_item_del;
   inst->ui.popup.itc_ap = itc;

   itc = elm_genlist_item_class_new();
   itc->item_style = "double_label";
   itc->func.text_get = _enm_itc_eth_text_get;
   itc->func.content_get = _enm_itc_eth_content_get;
   itc->func.state_get = NULL;
   itc->func.del = _enm_itc_item_del;
   inst->ui.popup.itc_eth = itc;

   itc = elm_genlist_item_class_new();
   itc->item_style = "double_label";
   itc->func.text_get = _enm_itc_bt_text_get;
   itc->func.content_get = _enm_itc_bt_content_get;
   itc->func.state_get = NULL;
   itc->func.del = _enm_itc_item_del;
   inst->ui.popup.itc_bt = itc;

   itc = elm_genlist_item_class_new();
   itc->item_style = "group_index";
   itc->func.text_get = _enm_itc_group_vpn_text_get;
   itc->func.content_get = _enm_itc_group_vpn_content_get;
   itc->func.state_get = NULL;
   itc->func.del = NULL;
   inst->ui.popup.itc_group_vpn = itc;

   itc = elm_genlist_item_class_new();
   itc->item_style = "double_label";
   itc->func.text_get = _enm_itc_vpn_text_get;
   itc->func.content_get = _enm_itc_vpn_content_get;
   itc->func.state_get = NULL;
   itc->func.del = NULL;
   inst->ui.popup.itc_vpn = itc;

   /* Click signal for row tap -> connect/disconnect. Avoid using selection
    * callbacks here so already-selected active rows still react to clicks. */
   evas_object_smart_callback_add(gl, "selected", _enm_item_clicked_cb, inst);
   evas_object_smart_callback_add(gl, "unselected", _enm_item_clicked_cb, inst);

   /* Right-click context menu for VPN rows: use genlist's per-item
    * "clicked,right" smart signal.  event_info is the Elm_Object_Item. */
   evas_object_smart_callback_add(gl, "clicked,right",
                                  _enm_vpn_genlist_clicked_right_cb, inst);

   /* Reveal the protocol pill on VPN rows once their view is realised. */
   evas_object_smart_callback_add(gl, "realized",
                                  _enm_vpn_genlist_realized_cb, inst);

   elm_box_pack_end(box, gl);
   evas_object_show(gl);

   /* Settings cog at the bottom of the popup.  IP label that used to sit
    * here was dropped — the assigned IP now appears under the wifi /
    * ethernet / VPN row name in wifi-band blue. */
   button = elm_button_add(box);
   elm_object_text_set(button, _("Settings"));
   evas_object_size_hint_align_set(button, 0.5, 0.5);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0.0);
   evas_object_smart_callback_add(button, "clicked",
                                  _enm_popup_settings_cb, inst);
   elm_box_pack_end(box, button);
   evas_object_show(button);

   icon = elm_icon_add(button);
   elm_icon_standard_set(icon, "preferences-system");
   elm_object_content_set(button, icon);
   evas_object_show(icon);

   evas_object_show(box);

   _enm_popup_update(ctxt->nm, inst);

   {
      Evas_Object *wrapper = _enm_widget_size_wrap(inst, box,
                                                    15, 35,
                                                    240, 240,
                                                    480, 600);
      evas_object_show(wrapper);
      e_gadcon_popup_content_set(inst->popup, wrapper);
   }
   e_comp_object_util_autoclose(inst->popup->comp_object, _enm_popup_del,
                                NULL, inst);
   e_gadcon_popup_show(inst->popup);
   e_object_data_set(E_OBJECT(inst->popup), inst);
   E_OBJECT_DEL_SET(inst->popup, _enm_popup_del_cb);
}

/* --- UI callbacks called from e_networkmanager.c -------------------------- */

static Eina_Bool
_enm_popup_update_timer_cb(void *data)
{
   E_NM_Module_Context *ctxt = data;

   ctxt->popup_update_timer = NULL;
   enm_mod_aps_update_now();
   return ECORE_CALLBACK_CANCEL;
}

static void
_enm_mod_aps_changed(struct NM_Manager *nm EINA_UNUSED)
{
   E_NM_Module_Context *ctxt = networkmanager_mod->data;

   /* Throttle popup rebuilds: coalesce rapid AP changes into a single
    * update after 0.5s.  Without this, NM's continuous AccessPointAdded/
    * Removed signals during scanning cause visible flicker. */
   if (ctxt->popup_update_timer) return;
   ctxt->popup_update_timer = ecore_timer_add(0.5,
                                               _enm_popup_update_timer_cb,
                                               ctxt);
}

void
enm_mod_aps_update_now(void)
{
   E_NM_Module_Context *ctxt;
   const Eina_List *l;
   E_NM_Instance *inst;

   if (!networkmanager_mod) return;
   ctxt = networkmanager_mod->data;

   /* Cancel any pending throttle timer — we are doing an immediate rebuild */
   E_FREE_FUNC(ctxt->popup_update_timer, ecore_timer_del);

   if (!ctxt->nm) return;

   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     {
        if (!inst->popup) continue;
        _enm_popup_update(ctxt->nm, inst);
     }
}

static void
_enm_gadget_setup(E_NM_Instance *inst)
{
   E_NM_Module_Context *ctxt = inst->ctxt;
   Evas_Object *o = inst->ui.gadget;

   DBG("has_manager=%d", ctxt->nm != NULL);

   if (!ctxt->nm)
     {
        edje_object_signal_emit(o, "e,unavailable", "e");
        edje_object_signal_emit(o, "e,changed,connected,no", "e");
     }
   else
     edje_object_signal_emit(o, "e,available", "e");
}

/* Map NM state to ConnMan theme state values:
 * OFFLINE=0, IDLE=1, ASSOCIATION=2, CONFIGURATION=3, READY=4, ONLINE=5 */
static int
_enm_state_to_connman(enum NM_State state)
{
   switch (state)
     {
      case NM_STATE_UNKNOWN:
      case NM_STATE_ASLEEP:           return 0; /* OFFLINE */
      case NM_STATE_DISCONNECTED:
      case NM_STATE_DISCONNECTING:    return 1; /* IDLE */
      case NM_STATE_CONNECTING:       return 2; /* ASSOCIATION */
      case NM_STATE_CONNECTED_LOCAL:  return 4; /* READY */
      case NM_STATE_CONNECTED_SITE:   return 4; /* READY */
      case NM_STATE_CONNECTED_GLOBAL: return 5; /* ONLINE */
      default:                        return 0;
     }
}

static void
_enm_mod_manager_update_inst(E_NM_Module_Context *ctxt EINA_UNUSED,
                              E_NM_Instance *inst,
                              struct NM_Manager *nm,
                              enum NM_State state)
{
   Evas_Object *o = inst->ui.gadget;
   Edje_Message_Int_Set *msg;
   struct NM_Access_Point *active_ap = NULL;
   const char *typestr;
   char buf[256];
   uint8_t strength;
   int theme_state;

   /* Determine connection technology type for gadget icon */
   if (nm && nm->active_conn_type == NM_DEVICE_TYPE_ETHERNET)
     typestr = "ethernet";
   else if (nm && nm->active_conn_type == NM_DEVICE_TYPE_BLUETOOTH)
     typestr = "bluetooth";
   else
     typestr = "wifi";

   /* Resolve active AP for real signal strength */
   if (nm && nm->active_ap_path)
     active_ap = enm_manager_find_ap(nm, nm->active_ap_path);

   /* Ethernet: full strength; WiFi: from active AP */
   if (nm && nm->active_conn_type == NM_DEVICE_TYPE_ETHERNET)
     strength = 100;
   else
     strength = active_ap ? active_ap->strength : 0;

   DBG("gadget_update: state=%d type=%s ap=%s strength=%d active_ap=%p",
       state, typestr, nm ? (nm->active_ap_path ?: "(null)") : "no-nm",
       strength, active_ap);
   theme_state = _enm_state_to_connman(state);

   msg = malloc(sizeof(*msg) + sizeof(int));
   if (!msg) return;
   msg->count = 2;
   msg->val[0] = theme_state;
   msg->val[1] = strength;

   edje_object_message_send(o, EDJE_MESSAGE_INT_SET, 1, msg);
   free(msg);

   snprintf(buf, sizeof(buf), "e,changed,technology,%s", typestr);
   edje_object_signal_emit(o, buf, "e");

   /* Set hover label text — shows SSID or interface name on mouse hover */
   if (nm && active_ap && active_ap->ssid)
     edje_object_part_text_set(o, "e.text.label", active_ap->ssid);
   else if (nm && nm->active_conn_type == NM_DEVICE_TYPE_ETHERNET)
     {
        struct NM_Device *dev;
        EINA_INLIST_FOREACH(nm->devices, dev)
          {
             if (dev->type == NM_DEVICE_TYPE_ETHERNET &&
                 dev->state >= NM_DEVICE_STATE_ACTIVATED)
               {
                  edje_object_part_text_set(o, "e.text.label",
                                            dev->interface ?: _("Wired"));
                  break;
               }
          }
     }
   else if (nm && nm->active_conn_type == NM_DEVICE_TYPE_BLUETOOTH)
     {
        struct NM_Bluetooth_Connection *bc;
        struct NM_Device *dev;
        Eina_Bool found = EINA_FALSE;

        EINA_INLIST_FOREACH(nm->bluetooth_connections, bc)
          {
             dev = enm_bluetooth_connection_device_find(nm, bc);
             if (dev && dev->state >= NM_DEVICE_STATE_ACTIVATED)
               {
                  edje_object_part_text_set(o, "e.text.label",
                                            bc->name ?: _("Bluetooth"));
                  found = EINA_TRUE;
                  break;
               }
          }
        if (!found)
          edje_object_part_text_set(o, "e.text.label", _("Bluetooth"));
     }
   else
     edje_object_part_text_set(o, "e.text.label", "");

   /* Set security overlay and frequency band on gadget icon */
   if (nm && active_ap)
     {
        const char *sec;

        sec = enm_ap_security_to_str(active_ap->wpa_flags, active_ap->rsn_flags);
        if (sec && strcmp(sec, "open"))
          {
             if (!strcmp(sec, "wpa") || !strcmp(sec, "wpa2") || !strcmp(sec, "sae"))
               edje_object_signal_emit(o, "e,security,psk", "e");
             else if (!strcmp(sec, "wep"))
               edje_object_signal_emit(o, "e,security,wep", "e");
             else if (!strcmp(sec, "802.1x"))
               edje_object_signal_emit(o, "e,security,ieee8021x", "e");
             else
               {
                  snprintf(buf, sizeof(buf), "e,security,%s", sec);
                  edje_object_signal_emit(o, buf, "e");
               }
          }
        else
          edje_object_signal_emit(o, "e,security,off", "e");

        if (active_ap->frequency > 0)
          {
             const char *band;

             if (active_ap->frequency >= 5925)
               band = "6G";
             else if (active_ap->frequency >= 3000)
               band = "5G";
             else
               band = "2.4G";

             edje_object_part_text_set(o, "e.text.band-label", band);
          }
        else
          edje_object_part_text_set(o, "e.text.band-label", "");
     }
   else
     {
        edje_object_signal_emit(o, "e,security,off", "e");
        edje_object_part_text_set(o, "e.text.band-label", "");
     }

   /* Build and set a tooltip on the gadget summarising connection + VPN. */
/* this is invalid as it sets it on th3e gaget.. and the gadget is not elm
   {
      Eina_Strbuf *tbuf = eina_strbuf_new();
      if (tbuf)
        {
           // Primary connection name (SSID or interface)
           if (nm && active_ap && active_ap->ssid)
             eina_strbuf_append(tbuf, active_ap->ssid);
           else if (nm && nm->active_conn_type == NM_DEVICE_TYPE_ETHERNET)
             {
                struct NM_Device *tdev;
                EINA_INLIST_FOREACH(nm->devices, tdev)
                  {
                     if (tdev->type == NM_DEVICE_TYPE_ETHERNET &&
                        tdev->state >= NM_DEVICE_STATE_ACTIVATED)
                       {
                          eina_strbuf_append(tbuf,
                              tdev->interface ?: _("Wired"));
                          break;
                       }
                  }
             }

           // Append VPN line if any VPN is active
           if (nm && enm_vpn_active_count(nm) > 0)
             {
                struct NM_VPN_Connection *vc;
                Eina_Bool first = EINA_TRUE;
                eina_strbuf_append(tbuf, "\nVPN: ");
                EINA_INLIST_FOREACH(nm->vpn_connections, vc)
                  {
                     if (vc->vpn_state != NM_VPN_STATE_ACTIVATED) continue;
                     if (!first) eina_strbuf_append(tbuf, ", ");
                     eina_strbuf_append(tbuf, vc->name ?: "VPN");
                     first = EINA_FALSE;
                  }
             }

           if (eina_strbuf_length_get(tbuf) > 0)
             elm_object_tooltip_text_set(o,
                 eina_strbuf_string_get(tbuf));
           else
             elm_object_tooltip_unset(o);

           eina_strbuf_free(tbuf);
        }
   }
 */
}

static void
_enm_mod_manager_update(struct NM_Manager *nm)
{
   E_NM_Module_Context *ctxt = networkmanager_mod->data;
   E_NM_Instance *inst;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN(nm);

   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     _enm_mod_manager_update_inst(ctxt, inst, nm, nm->state);

   /* Start or stop traffic monitor based on connection state */
   if (nm->state >= NM_STATE_CONNECTED_LOCAL)
     {
        _enm_traffic_timer_start(ctxt);
     }
   else
     _enm_traffic_timer_stop(ctxt);
}

static void
_enm_mod_manager_inout(struct NM_Manager *nm)
{
   E_NM_Module_Context *ctxt = networkmanager_mod->data;
   const Eina_List *l;
   E_NM_Instance *inst;

   DBG("Manager %s", nm ? "in" : "out");
   ctxt->nm = nm;

   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     _enm_gadget_setup(inst);

   if (ctxt->nm)
     {
        _enm_mod_manager_update(nm);
        /* Pre-fetch saved connections so the hash is warm when popup opens */
        enm_saved_connections_get(nm);
     }
   else
     _enm_traffic_timer_stop(ctxt);
}

static void
_enm_mod_vpn_changed_cb(struct NM_Manager *nm)
{
   E_NM_Module_Context *ctxt = networkmanager_mod ?
       networkmanager_mod->data : NULL;
   E_NM_Instance *inst;
   Eina_List *l;

   if (!ctxt) return;
   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     if (inst->popup) _enm_popup_update(nm, inst);
}

/* Emit e,vpn,active or e,vpn,inactive on every gadget instance based on
 * the current active VPN count.  Also refreshes the gadget tooltip so the
 * VPN name appears immediately after state changes. */
static void
_enm_vpn_emit_shield(E_NM_Module_Context *ctxt)
{
   if (!ctxt || !ctxt->nm) return;
   unsigned int n = enm_vpn_active_count(ctxt->nm);
   const char *sig = n > 0 ? "e,vpn,active" : "e,vpn,inactive";

   DBG("VPN shield emit: active_count=%u -> signal='%s' (%u instances)",
       n, sig, (unsigned int)eina_list_count(ctxt->instances));

   E_NM_Instance *inst; Eina_List *l;
   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     {
        if (!inst->ui.gadget) continue;
        edje_object_signal_emit(inst->ui.gadget, sig, "e");
     }
}

static const char *
_enm_vpn_reason_text(uint32_t reason)
{
   switch (reason)
     {
      case 0:  return _("Unknown reason.");
      case 1:  return _("Disconnected by user.");
      case 2:  return _("Device disconnected.");
      case 3:  return _("Service stopped.");
      case 4:  return _("IP configuration is invalid.");
      case 5:  return _("Connection attempt timed out.");
      case 6:  return _("Service start timeout.");
      case 7:  return _("Service failed to start.");
      case 8:  return _("No valid VPN secrets.");
      case 9:  return _("Invalid VPN secrets.");
      case 10: return _("Connection removed.");
      default: return _("Connection failed.");
     }
}

static void
_enm_vpn_check_failures(struct NM_Manager *nm)
{
   struct NM_VPN_Connection *vc;
   if (!nm) return;
   EINA_INLIST_FOREACH(nm->vpn_connections, vc)
     {
        if (vc->vpn_state == NM_VPN_STATE_FAILED &&
            vc->prev_state != NM_VPN_STATE_FAILED)
          {
             E_Notification_Notify n;
             char body[256];
             snprintf(body, sizeof(body), "%s: %s",
                      vc->name ?: "VPN",
                      _enm_vpn_reason_text(vc->fail_reason));
             memset(&n, 0, sizeof(E_Notification_Notify));
             n.app_name = "Enlightenment";
             n.summary  = _("VPN connection failed");
             n.body     = body;
             n.timeout  = 5000;
             e_notification_client_send(&n, NULL, NULL);
          }
     }
}

static void
_enm_mod_vpn_active_changed_cb(struct NM_Manager *nm)
{
   E_NM_Module_Context *ctxt = networkmanager_mod ?
       networkmanager_mod->data : NULL;
   if (!ctxt) return;
   _enm_vpn_check_failures(nm);
   _enm_vpn_emit_shield(ctxt);
   /* Refresh the gadget tooltip (built inside _enm_mod_manager_update_inst)
    * so the "VPN: <name>" line tracks active-state changes. */
   _enm_mod_manager_update(nm);
   /* And refresh the popup VPN rows (state pill, IP, disconnect button). */
   _enm_mod_vpn_changed_cb(nm);
}

/* --- deferred VPN-active-changed notification ------------------------------ */

static void
_enm_vpn_update_job_cb(void *data)
{
   struct NM_Manager *nm = data;
   E_NM_Module_Context *ctxt = networkmanager_mod ?
       networkmanager_mod->data : NULL;

   if (!ctxt) return;
   ctxt->vpn_update_job = NULL;   /* job has fired — clear the handle */
   _enm_mod_vpn_active_changed_cb(nm);
}

/* Public: called from e_networkmanager_vpn.c instead of the direct callback.
 * Multiple calls within the same D-Bus dispatch batch coalesce into one job. */
void
enm_vpn_active_changed_schedule(struct NM_Manager *nm)
{
   E_NM_Module_Context *ctxt = networkmanager_mod ?
       networkmanager_mod->data : NULL;

   if (!ctxt) return;
   if (ctxt->vpn_update_job) return;   /* already queued — coalesce */
   ctxt->vpn_update_job = ecore_job_add(_enm_vpn_update_job_cb, nm);
}

static const E_NM_Mod_Callbacks _enm_mod_cbs =
{
   .aps_changed        = _enm_mod_aps_changed,
   .manager_update     = _enm_mod_manager_update,
   .manager_inout      = _enm_mod_manager_inout,
   .vpn_changed        = _enm_mod_vpn_changed_cb,
   .vpn_active_changed = _enm_mod_vpn_active_changed_cb,
};

/* --- network activity indicator ------------------------------------------- */

/* Find the interface name for the active network connection */
static const char *
_enm_active_interface(struct NM_Manager *nm)
{
   struct NM_Device *dev;

   if (!nm) return NULL;

   /* WiFi: find first connected WiFi device (activated state) */
   if (nm->active_conn_type == NM_DEVICE_TYPE_WIFI)
     {
        EINA_INLIST_FOREACH(nm->devices, dev)
          {
             if (dev->type == NM_DEVICE_TYPE_WIFI &&
                 dev->state >= NM_DEVICE_STATE_ACTIVATED)
               return dev->interface;
          }
     }

   /* Ethernet: find first connected ethernet device */
   if (nm->active_conn_type == NM_DEVICE_TYPE_ETHERNET)
     {
        EINA_INLIST_FOREACH(nm->devices, dev)
          {
             if (dev->type == NM_DEVICE_TYPE_ETHERNET &&
                 dev->state >= NM_DEVICE_STATE_ACTIVATED)
               return dev->interface;
          }
     }

   if (nm->active_conn_type == NM_DEVICE_TYPE_BLUETOOTH)
     {
        EINA_INLIST_FOREACH(nm->devices, dev)
          {
             if (dev->type == NM_DEVICE_TYPE_BLUETOOTH &&
                 dev->state >= NM_DEVICE_STATE_ACTIVATED)
               return dev->interface;
          }
     }

   return NULL;
}

static Eina_Bool
_enm_read_sysfs_counter(const char *iface, const char *counter,
                         unsigned long long *val)
{
#ifdef __linux__
   char path[256];
   FILE *f;

   snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/%s", iface, counter);
   f = fopen(path, "r");
   if (!f) return EINA_FALSE;
   if (fscanf(f, "%llu", val) != 1)
     {
        fclose(f);
        return EINA_FALSE;
     }
   fclose(f);
   return EINA_TRUE;
#else
   (void)iface; (void)counter;
   *val = 0;
   return EINA_FALSE;
#endif
}

/* Classify bytes/sec into traffic level: 0=idle, 1=low, 2=medium, 3=high
 * Uses hysteresis: higher threshold to go up, lower threshold to go down,
 * preventing animation resets from boundary flickering. */
static int
_enm_traffic_level(unsigned long long bytes_per_sec, int current_level)
{
   /* Thresholds: up / down (with ~30% hysteresis) */
   static const unsigned long long thresh_up[]   = { 1, 10240, 512000 };
   static const unsigned long long thresh_down[] = { 1,  7168, 358400 };
   int level = current_level;

   if (level < 3 && bytes_per_sec >= thresh_up[level])
     {
        /* Move up to the highest matching level */
        while (level < 3 && bytes_per_sec >= thresh_up[level])
          level++;
     }
   else if (level > 0 && bytes_per_sec < thresh_down[level - 1])
     {
        /* Move down to the lowest matching level */
        while (level > 0 && bytes_per_sec < thresh_down[level - 1])
          level--;
     }
   return level;
}

static const char *_traffic_signals[] = {
   "e,traffic,rx,idle", "e,traffic,rx,low",
   "e,traffic,rx,medium", "e,traffic,rx,high",
   "e,traffic,tx,idle", "e,traffic,tx,low",
   "e,traffic,tx,medium", "e,traffic,tx,high",
};

static void
_enm_traffic_signal_emit(E_NM_Module_Context *ctxt, int rx_level, int tx_level)
{
   E_NM_Instance *inst;
   Eina_List *l;

   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     {
        if (!inst->ui.gadget) continue;
        if (rx_level != ctxt->rx_level)
          edje_object_signal_emit(inst->ui.gadget,
                                  _traffic_signals[rx_level], "e");
        if (tx_level != ctxt->tx_level)
          edje_object_signal_emit(inst->ui.gadget,
                                  _traffic_signals[4 + tx_level], "e");
     }
}

/*
 * Threaded traffic monitor.
 *
 * The worker struct holds a single-slot sample protected by a lock — the
 * worker writes rx/tx under the lock and signals main via ecore_thread_feedback;
 * the notify cb reads the slot under the lock.  No per-sample allocation, so
 * there is nothing to leak if ecore_thread_cancel drops in-flight feedback
 * messages.
 *
 * Cleanup is handled in the shared done path invoked from both the normal
 * end callback and the cancel callback.  The `ctxt->traffic_thread == thread`
 * check makes the done path robust against a stop+restart race: after
 * stop() cancels the old thread, ctxt->traffic_thread is set to NULL and
 * then reassigned to the new thread, so the old thread's delayed done cb
 * finds a pointer that is either NULL or the new thread and leaves it alone.
 */
static void
_enm_traffic_worker_heavy(void *data, Ecore_Thread *thread)
{
   Enm_Traffic_Worker *w = data;
   fd_set rfds, wfds, exfds;
   struct timeval tv;

   while (!ecore_thread_check(thread))
     {
        unsigned long long rx = 0, tx = 0;
        int ret;
        double tim = 0.0;

        eina_lock_take(&w->lock);
        if (w->ctxt) tim = w->ctxt->poll_time;
        eina_lock_release(&w->lock);
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&exfds);
        FD_SET(w->pipe_fd, &rfds);
        tv.tv_sec = tim;
        tv.tv_usec = (unsigned int)(tim * 1000000.0) % 1000000;
        ret = select(w->pipe_fd + 1, &rfds, &wfds, &exfds, &tv);
        if ((ret == 1) && (FD_ISSET(w->pipe_fd, &rfds)))
          {
            char buf[1];

            if (read(w->pipe_fd, buf, 1) < 0)
              fprintf(stderr, "%s: ERROR READING FROM FD\n", __func__);
            // our pipe fd was written to. we have been woken up by the
            // main loop so re-think thnigs.
            if (ecore_thread_check(thread)) break; // canceled thread
            // if we were not canceled, then poll time may have changed
            // so poll now and try again with new poll time
          }
        // XXX: open these files once and keep fd's and use pread()
        if (!_enm_read_sysfs_counter(w->iface, "rx_bytes", &rx) ||
            !_enm_read_sysfs_counter(w->iface, "tx_bytes", &tx))
          continue;

        eina_lock_take(&w->lock);
        w->rx = rx;
        w->tx = tx;
        w->have_sample = EINA_TRUE;
        eina_lock_release(&w->lock);

        /* msg_data is the worker itself — feedback carries no allocation */
        ecore_thread_feedback(thread, w);
     }
}

static void
_enm_traffic_worker_notify(void *data, Ecore_Thread *thread EINA_UNUSED,
                           void *msg_data EINA_UNUSED)
{
   Enm_Traffic_Worker *w = data;
   E_NM_Module_Context *ctxt = w->ctxt;
   unsigned long long rx, tx;
   int rx_level, tx_level;
   double divide;
   Eina_Bool have;

   eina_lock_take(&w->lock);
   have = w->have_sample;
   rx = w->rx;
   tx = w->tx;
   w->have_sample = EINA_FALSE;
   eina_lock_release(&w->lock);

   if (!have) return;

   if (!ctxt) return;
   /* First sample seeds the counters — no rate yet */
   if (ctxt->prev_rx == 0 && ctxt->prev_tx == 0)
     {
        ctxt->prev_rx = rx;
        ctxt->prev_tx = tx;
        return;
     }

   /* Poll interval ~0.5s, so bytes_per_sec = delta * 2 */
   divide = ctxt->poll_time;
   if (divide < 0.1) divide = 0.1;
   rx_level = _enm_traffic_level((rx - ctxt->prev_rx) / divide, ctxt->rx_level);
   tx_level = _enm_traffic_level((tx - ctxt->prev_tx) / divide, ctxt->tx_level);
   ctxt->prev_rx = rx;
   ctxt->prev_tx = tx;

   if (rx_level != ctxt->rx_level || tx_level != ctxt->tx_level)
     {
        _enm_traffic_signal_emit(ctxt, rx_level, tx_level);
        ctxt->rx_level = rx_level;
        ctxt->tx_level = tx_level;
     }
}

static void
_enm_traffic_worker_done(void *data, Ecore_Thread *thread)
{
   Enm_Traffic_Worker *w = data;

   /* Only clear ctxt->traffic_thread if it still points at us.  A stop()
    * immediately followed by start() will have NULL'd and then replaced the
    * pointer before this done cb runs for the cancelled predecessor. */
   if ((w->ctxt) && (w->ctxt->traffic_thread == thread))
     {
        w->ctxt->traffic_thread = NULL;
        w->ctxt->worker = NULL;
        if (w->ctxt->pipe) ecore_pipe_del(w->ctxt->pipe);
        w->ctxt->pipe = NULL;
        w->ctxt->pipe_fd = -1;
     }
   eina_lock_free(&w->lock);
   free(w->iface);
   free(w);
}

static void
_enm_traffic_worker_cancel(void *data, Ecore_Thread *thread)
{
   _enm_traffic_worker_done(data, thread);
}

static void
_cb_pipe_dummy(void *data EINA_UNUSED, void *buffer EINA_UNUSED, unsigned int bytes EINA_UNUSED)
{
}

static void
_enm_traffic_timer_start(E_NM_Module_Context *ctxt)
{
   const char *iface;
   Enm_Traffic_Worker *w;

   if (ctxt->powersave_high) return;
   if (!ctxt->nm) return;
   if (ctxt->nm->state < NM_STATE_CONNECTED_LOCAL) return;

   iface = _enm_active_interface(ctxt->nm);
   if (!iface) return;

   /* If a worker is already running for the same iface, nothing to do */
   if (ctxt->traffic_thread && ctxt->traffic_iface &&
       !strcmp(ctxt->traffic_iface, iface))
     return;

   /* Iface changed (or first start): tear down any previous worker */
   if (ctxt->traffic_thread)
     {
        ecore_thread_cancel(ctxt->traffic_thread);
        ctxt->traffic_thread = NULL;
        _enm_poll_wake(ctxt);
        if (ctxt->pipe) ecore_pipe_del(ctxt->pipe);
        ctxt->pipe = NULL;
        ctxt->pipe_fd = -1;
     }
   free(ctxt->traffic_iface);
   ctxt->traffic_iface = strdup(iface);

   ctxt->prev_rx = 0;
   ctxt->prev_tx = 0;
   ctxt->rx_level = 0;
   ctxt->tx_level = 0;

   w = E_NEW(Enm_Traffic_Worker, 1);
   w->ctxt  = ctxt;
   w->iface = strdup(iface);
   ctxt->worker = w;
   eina_lock_new(&w->lock);
   ctxt->pipe = ecore_pipe_add(_cb_pipe_dummy, NULL);
   if (ctxt->pipe) ecore_pipe_freeze(ctxt->pipe);
   if (ctxt->pipe) ctxt->pipe_fd = ecore_pipe_read_fd(ctxt->pipe);
   else ctxt->pipe_fd = -1;
   w->pipe_fd = ctxt->pipe_fd; // store fd local to worker thread
   ctxt->traffic_thread =
      ecore_thread_feedback_run(_enm_traffic_worker_heavy,
                                _enm_traffic_worker_notify,
                                _enm_traffic_worker_done,
                                _enm_traffic_worker_cancel,
                                w, EINA_TRUE);
}

static void
_enm_traffic_timer_stop(E_NM_Module_Context *ctxt)
{
   if (ctxt->traffic_thread)
     {
        ecore_thread_cancel(ctxt->traffic_thread);
        ctxt->traffic_thread = NULL;
        _enm_poll_wake(ctxt);
        if (ctxt->pipe) ecore_pipe_del(ctxt->pipe);
        ctxt->pipe = NULL;
        ctxt->pipe_fd = -1;
     }
   free(ctxt->traffic_iface);
   ctxt->traffic_iface = NULL;

   if (ctxt->rx_level || ctxt->tx_level)
     {
        _enm_traffic_signal_emit(ctxt, 0, 0);
        ctxt->rx_level = 0;
        ctxt->tx_level = 0;
     }
}

static Eina_Bool
_enm_powersave_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   E_NM_Module_Context *ctxt = data;
   E_Powersave_Mode mode = e_powersave_mode_get();

   if (mode >= E_POWERSAVE_MODE_EXTREME)
     {
        ctxt->powersave_high = EINA_TRUE;
        _enm_traffic_timer_stop(ctxt);
     }
   else
     {
        ctxt->powersave_high = EINA_FALSE;
        _enm_traffic_timer_start(ctxt);
     }
   return ECORE_CALLBACK_PASS_ON;
}

/* --- mouse / menu --------------------------------------------------------- */

static void
_enm_cb_menu_configure(void *data, E_Menu *m EINA_UNUSED,
                       E_Menu_Item *mi EINA_UNUSED)
{
   enm_config_dialog_show(data);
}

static void
_enm_menu_new(E_NM_Instance *inst, Evas_Event_Mouse_Down *ev)
{
   E_Menu *m;
   E_Menu_Item *mi;
   int x, y;

   m = e_menu_new();
   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Settings"));
   e_util_menu_item_theme_icon_set(mi, "configure");
   e_menu_item_callback_set(mi, _enm_cb_menu_configure, inst);
   m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);
   e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);
   e_menu_activate_mouse(m,
                         e_zone_current_get(),
                         x + ev->output.x, y + ev->output.y, 1, 1,
                         E_MENU_POP_DIRECTION_DOWN, ev->timestamp);
}

static void
_enm_cb_mouse_down(void *data, Evas *evas EINA_UNUSED,
                   Evas_Object *obj EINA_UNUSED, void *event)
{
   E_NM_Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (!inst) return;

   if (ev->button == 1)
     {
        if (!inst->popup)
          _enm_popup_new(inst);
     }
   else if (ev->button == 3)
     _enm_menu_new(inst, ev);
}

/* --- Gadcon API ------------------------------------------------------------ */

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   E_NM_Instance *inst;
   E_NM_Module_Context *ctxt;

   if (!networkmanager_mod) return NULL;

   ctxt = networkmanager_mod->data;

   inst = E_NEW(E_NM_Instance, 1);
   inst->ctxt = ctxt;
   inst->ui.gadget = edje_object_add(gc->evas);
   _enm_theme_edje_object_set(inst->ui.gadget, "main");

   inst->gcc = e_gadcon_client_new(gc, name, id, style, inst->ui.gadget);
   inst->gcc->data = inst;

   evas_object_event_callback_add(inst->ui.gadget, EVAS_CALLBACK_MOUSE_DOWN,
                                  _enm_cb_mouse_down, inst);

   _enm_gadget_setup(inst);

   /* The VPN badge in the connman/main edje group is an IMAGE part that
    * references elementary's i-shield-gold image set directly and is
    * toggled by e,vpn,active / e,vpn,inactive signals.  No swallow needed
    * here; the theme owns the asset and the geometry. */

   /* Append the instance BEFORE the initial shield emit so the loop in
    * _enm_vpn_emit_shield includes this new gadget. */
   ctxt->instances = eina_list_append(ctxt->instances, inst);

   if (ctxt->nm)
     {
        _enm_mod_manager_update_inst(ctxt, inst, ctxt->nm, ctxt->nm->state);
        /* Emit initial VPN shield state in case a VPN was already up */
        _enm_vpn_emit_shield(ctxt);
     }

   return inst->gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   E_NM_Module_Context *ctxt;
   E_NM_Instance *inst;

   if (!networkmanager_mod) return;

   ctxt = networkmanager_mod->data;
   if (!ctxt) return;

   inst = gcc->data;
   if (!inst) return;

   if (inst->popup) enm_popup_del(inst);

   evas_object_del(inst->ui.gadget);

   ctxt->instances = eina_list_remove(ctxt->instances, inst);

   E_FREE(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient EINA_UNUSED)
{
   e_gadcon_client_aspect_set(gcc, 16, 16);
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _(_e_nm_Name);
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;

   o = edje_object_add(evas);
   edje_object_file_set(o, e_nm_theme_path(), "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   static char idbuf[64]; /* returned string — use immediately */
   E_NM_Module_Context *ctxt;
   Eina_List *instances;

   if (!networkmanager_mod) return NULL;

   ctxt = networkmanager_mod->data;
   if (!ctxt) return NULL;

   instances = ctxt->instances;
   snprintf(idbuf, sizeof(idbuf), "networkmanager.%d",
            eina_list_count(instances));
   return idbuf;
}

static const E_Gadcon_Client_Class _gc_class =
{
   GADCON_CLIENT_CLASS_VERSION, _e_nm_name,
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new,
      NULL, e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, _e_nm_Name };

/* --- configure registry --------------------------------------------------- */

static const char _reg_cat[]  = "extensions";
static const char _reg_item[] = "extensions/networkmanager";

static void
_enm_configure_registry_register(void)
{
   e_configure_registry_category_add(_reg_cat, 90, _("Extensions"), NULL,
                                     "preferences-extensions");
   e_configure_registry_item_add(_reg_item, 111, _(_e_nm_Name), NULL,
                                 "preferences-network",
                                 e_int_config_networkmanager_module);
}

static void
_enm_configure_registry_unregister(void)
{
   e_configure_registry_item_del(_reg_item);
   e_configure_registry_category_del(_reg_cat);
}

/* --- module lifecycle ----------------------------------------------------- */

E_API void *
e_modapi_init(E_Module *m)
{
   E_NM_Module_Context *ctxt;

   conf_edd = E_CONFIG_DD_NEW("NetworkManager_Config", E_NM_Config);
#undef T
#undef D
#define T E_NM_Config
#define D conf_edd
   E_CONFIG_VAL(D, T, config_version, INT);
   E_CONFIG_VAL(D, T, poll_time, DOUBLE);

   networkmanager_config = e_config_domain_load("module.networkmanager",
                                                conf_edd);
   if ((networkmanager_config) &&
       (networkmanager_config->config_version != NETWORKMANAGER_CONFIG_VERSION))
     E_FREE(networkmanager_config);

   if (!networkmanager_config)
     {
        networkmanager_config = E_NEW(E_NM_Config, 1);
        if (!networkmanager_config) goto error_config;
        networkmanager_config->config_version = NETWORKMANAGER_CONFIG_VERSION;
        networkmanager_config->poll_time = 0.5;
     }
   E_CONFIG_LIMIT(networkmanager_config->poll_time, 0.1, 10.0);
   networkmanager_config->module = m;

   if (_e_nm_log_dom < 0)
     {
        _e_nm_log_dom = eina_log_domain_register("networkmanager",
                                                  EINA_COLOR_ORANGE);
        if (_e_nm_log_dom < 0)
          {
             EINA_LOG_CRIT("could not register logging domain networkmanager");
             goto error_log_domain;
          }
     }

   ctxt = E_NEW(E_NM_Module_Context, 1);
   if (!ctxt) goto error_nm_context;

   ctxt->pipe_fd = -1;
   ctxt->poll_time = networkmanager_config->poll_time;
   e_nm_module_callbacks_set(&_enm_mod_cbs);
   enm_agent_ui_register();

   if (!e_nm_system_init()) goto error_nm_system_init;

   ctxt->conf_dialog = NULL;
   networkmanager_mod = m;

   /* Initialize power-aware traffic monitoring state */
   ctxt->powersave_high = (e_powersave_mode_get() >= E_POWERSAVE_MODE_EXTREME);

   ctxt->powersave_handler =
     ecore_event_handler_add(E_EVENT_POWERSAVE_UPDATE,
                             _enm_powersave_cb, ctxt);

   _enm_configure_registry_register();
   e_gadcon_provider_register(&_gc_class);

   return ctxt;

error_nm_system_init:
   E_FREE(ctxt);
error_nm_context:
   eina_log_domain_unregister(_e_nm_log_dom);
error_log_domain:
   _e_nm_log_dom = -1;
   E_FREE(networkmanager_config);
error_config:
   E_CONFIG_DD_FREE(conf_edd);
   return NULL;
}

static void
_enm_instances_free(E_NM_Module_Context *ctxt)
{
   while (ctxt->instances)
     {
        E_NM_Instance *inst = ctxt->instances->data;
        ctxt->instances = eina_list_remove_list(ctxt->instances,
                                                ctxt->instances);
        e_object_del(E_OBJECT(inst->gcc));
     }
}

E_API int
e_modapi_shutdown(E_Module *m)
{
   E_NM_Module_Context *ctxt;

   ctxt = m->data;
   if (!ctxt) return 0;

   if (ctxt->worker)
     {
        Enm_Traffic_Worker *w = ctxt->worker;
        w->ctxt = NULL;
     }
   e_nm_system_shutdown();
   e_nm_module_callbacks_set(NULL);
   e_nm_agent_callbacks_set(NULL, NULL);

   _enm_instances_free(ctxt);
   _enm_configure_registry_unregister();
   e_gadcon_provider_unregister(&_gc_class);

   E_FREE_FUNC(ctxt->popup_update_timer, ecore_timer_del);
   E_FREE_FUNC(ctxt->vpn_update_job, ecore_job_del);
   _enm_traffic_timer_stop(ctxt);
   E_FREE_FUNC(ctxt->powersave_handler, ecore_event_handler_del);
   E_FREE(ctxt);
   networkmanager_mod = NULL;

   eina_stringshare_replace(&_theme_path, NULL);

   if (networkmanager_config && networkmanager_config->config_dialog)
     e_object_del(E_OBJECT(networkmanager_config->config_dialog));
   E_FREE(networkmanager_config);
   E_CONFIG_DD_FREE(conf_edd);

   eina_log_domain_unregister(_e_nm_log_dom);
   _e_nm_log_dom = -1;

   return 1;
}

E_API int
e_modapi_save(E_Module *m)
{
   E_NM_Module_Context *ctxt;

   ctxt = m->data;
   if (!ctxt) return 0;
   if (!networkmanager_config) return 0;
   e_config_domain_save("module.networkmanager", conf_edd,
                        networkmanager_config);
   return 1;
}
