#include "e.h"
#include "e_mod_main.h"
#include "e_networkmanager.h"

E_Module *networkmanager_mod = NULL;
static Eina_Stringshare *_theme_path = NULL;

const char _e_nm_name[] = "networkmanager";
const char _e_nm_Name[] = N_("NetworkManager");
int _e_nm_log_dom = -1;

/* Forward declarations for traffic monitor */
static void _enm_traffic_timer_start(E_NM_Module_Context *ctxt);
static void _enm_traffic_timer_stop(E_NM_Module_Context *ctxt);

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
   inst->ui.popup.genlist = inst->ui.popup.ip_label = NULL;
   E_FREE_FUNC(inst->ui.popup.itc_group, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_group_wifi, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_ap, elm_genlist_item_class_free);
   E_FREE_FUNC(inst->ui.popup.itc_eth, elm_genlist_item_class_free);
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


static Eina_Bool _enm_ssid_is_active(struct NM_Manager *nm, const char *ssid);

/* Forward declarations for icon/forget-button factories used in item class
 * callbacks defined before the factory implementations. */
static Evas_Object *_enm_ap_icon_new(struct NM_Manager *nm,
                                      struct NM_Access_Point *ap, Evas *evas);
static Evas_Object *_enm_ap_end_new(struct NM_Manager *nm,
                                     struct NM_Access_Point *ap,
                                     Evas_Object *parent);
static Evas_Object *_enm_eth_icon_new(struct NM_Device *dev, Evas *evas);

/* Per-item data for genlist AP and ethernet rows */
typedef struct _Enm_Item_Data
{
   struct NM_Manager      *nm;
   struct NM_Access_Point *ap;   /* NULL for ethernet */
   struct NM_Device       *dev;
   const char             *ap_path;   /* stringshare — AP D-Bus object path */
   const char             *ssid;      /* stringshare */
} Enm_Item_Data;

static void
_enm_item_data_free(Enm_Item_Data *id)
{
   if (!id) return;
   eina_stringshare_del(id->ap_path);
   eina_stringshare_del(id->ssid);
   free(id);
}

/* Genlist item class del callback — frees per-item data */
static void
_enm_itc_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   _enm_item_data_free(data);
}

/* Genlist text_get for AP rows: returns SSID */
static char *
_enm_itc_ap_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                      const char *part)
{
   Enm_Item_Data *id = data;

   if (!strcmp(part, "elm.text"))
     return id->ssid ? strdup(id->ssid) : NULL;
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

        ic = _enm_ap_icon_new(id->nm, id->ap, evas_object_evas_get(obj));
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

/* Genlist text_get for ethernet rows: returns interface name */
static char *
_enm_itc_eth_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                       const char *part)
{
   Enm_Item_Data *id = data;

   if (!strcmp(part, "elm.text"))
     return id->dev ? strdup(id->dev->interface ?: _("Wired")) : NULL;
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

        ic = _enm_eth_icon_new(id->dev, evas_object_evas_get(obj));
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
     elm_genlist_item_selected_set(inst->ui.popup.deselect_item, EINA_FALSE);
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

/* Activated smart callback — handles connect/disconnect on row tap */
static void
_enm_item_activated_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *event_info)
{
   E_NM_Instance *inst = data;
   Elm_Object_Item *it = event_info;
   Enm_Item_Data *id;
   struct NM_Manager *nm;
   struct NM_Device *dev;

   if (!it) return;
   /* Delay deselect so the user sees their click landed. */
   _enm_deselect_timer_schedule(inst, it);
   id = elm_object_item_data_get(it);
   if (!id) return;

   nm = inst->ctxt->nm;
   if (!nm) return;

   /* Ethernet row: no connect action from list tap */
   if (!id->ap) return;

   /* If this AP's SSID is currently active, disconnect */
   if (nm->active_ap_path && _enm_ssid_is_active(nm, id->ap->ssid))
     {
        INF("Disconnect from %s", id->ap->ssid ?: id->ap_path);
        enm_ap_disconnect(nm);
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
                  INF("Connect to %s on device %s",
                      id->ap->ssid ?: id->ap_path,
                      dev->interface ?: dev->path);
                  enm_ap_connect(nm, dev, id->ap);
                  return;
               }
          }
     }
}

static Evas_Object *
_enm_ap_icon_new(struct NM_Manager *nm, struct NM_Access_Point *ap, Evas *evas)
{
   Edje_Message_Int_Set *msg;
   Evas_Object *icon;
   int state_val;

   icon = edje_object_add(evas);
   _enm_theme_edje_object_set(icon, "icon/wifi");

   /* Map active AP to ONLINE(5), otherwise IDLE(1) — ConnMan theme values */
   state_val = (ap->ssid && _enm_ssid_is_active(nm, ap->ssid)) ? 5 : 1;

   msg = malloc(sizeof(*msg) + sizeof(int));
   if (msg)
     {
        msg->count = 2;
        msg->val[0] = state_val;
        msg->val[1] = ap->strength;
        edje_object_message_send(icon, EDJE_MESSAGE_INT_SET, 1, msg);
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
           edje_object_signal_emit(icon, secbuf, "e");
        }
      else
        edje_object_signal_emit(icon, "e,security,off", "e");
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

        edje_object_part_text_set(icon, "e.text.band-label", band);
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
_enm_eth_icon_new(struct NM_Device *dev, Evas *evas)
{
   Edje_Message_Int_Set *msg;
   Evas_Object *icon;
   int state_val;

   icon = edje_object_add(evas);
   _enm_theme_edje_object_set(icon, "icon/ethernet");

   /* NM device state 100 = activated → ONLINE(5), otherwise IDLE(1) */
   state_val = (dev->state >= 100) ? 5 : 1;

   msg = malloc(sizeof(*msg) + sizeof(int));
   if (msg)
     {
        msg->count = 2;
        msg->val[0] = state_val;
        msg->val[1] = 100; /* ethernet has no signal strength concept */
        edje_object_message_send(icon, EDJE_MESSAGE_INT_SET, 1, msg);
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
   const char *label;   /* SSID or interface name — owned by AP/dev, do not free */
   const char *ap_path; /* stringshare AP path for wifi, NULL for ethernet */
   struct NM_Access_Point *ap; /* NULL for ethernet */
   struct NM_Device       *dev; /* only for ethernet */
};

static int
_enm_popup_build_entries(struct NM_Manager *nm,
                         struct _Popup_Entry *out, int max)
{
   struct NM_Device *dev;
   Eina_Hash *seen_ssids;
   int n = 0;

   if (!nm) return 0;

   /* Ethernet entries first */
   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        if (dev->type != NM_DEVICE_TYPE_ETHERNET) continue;
        if (dev->state < 100) continue;
        if (n >= max) break;

        out[n].label = dev->interface ?: _("Wired");
        out[n].ap_path = NULL;
        out[n].ap = NULL;
        out[n].dev = dev;
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
             out[n].label = best->ssid;
             out[n].ap_path = best->path;
             out[n].ap = best;
             out[n].dev = NULL;
             n++;
          }
     }

   eina_hash_free(seen_ssids);
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

static void
_enm_popup_update(struct NM_Manager *nm, E_NM_Instance *inst)
{
   Evas_Object *gl = inst->ui.popup.genlist;
   struct _Popup_Entry desired[256];
   int want_n, i;
   Elm_Object_Item *wifi_group = NULL, *eth_group = NULL;
   int wifi_count = 0, eth_count = 0;

   EINA_SAFETY_ON_NULL_RETURN(nm);
   EINA_SAFETY_ON_NULL_RETURN(gl);

   /* elm_genlist_clear() below frees all items — drop any pending
    * deselect targeting an item about to be destroyed. */
   E_FREE_FUNC(inst->ui.popup.deselect_timer, ecore_timer_del);
   inst->ui.popup.deselect_item = NULL;

   want_n = _enm_popup_build_entries(nm, desired, 256);

   /* Count per-type so we know whether to insert group headers */
   for (i = 0; i < want_n; i++)
     {
        if (desired[i].ap)
          wifi_count++;
        else
          eth_count++;
     }

   /* Clear and rebuild — simpler than incremental diff with grouped headers */
   elm_genlist_clear(gl);

   /* Insert ethernet group + items */
   if (eth_count > 0)
     {
        eth_group = elm_genlist_item_append(gl, inst->ui.popup.itc_group,
                                            (void *)_("Wired"), NULL,
                                            ELM_GENLIST_ITEM_GROUP,
                                            NULL, NULL);
        elm_genlist_item_select_mode_set(eth_group,
                                         ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);

        for (i = 0; i < want_n; i++)
          {
             Enm_Item_Data *id;

             if (desired[i].ap) continue; /* skip wifi entries here */

             id = calloc(1, sizeof(*id));
             if (!id) continue;
             id->nm = nm;
             id->ap = NULL;
             id->dev = desired[i].dev;
             id->ap_path = NULL;
             id->ssid = NULL;

             elm_genlist_item_append(gl, inst->ui.popup.itc_eth, id,
                                     eth_group, ELM_GENLIST_ITEM_NONE,
                                     NULL, NULL);
          }
     }

   /* Insert wifi group header (always when adapter present) + AP items */
   if (_enm_has_wifi_device(nm))
     {
        wifi_group = elm_genlist_item_append(gl, inst->ui.popup.itc_group_wifi,
                                             inst, NULL,
                                             ELM_GENLIST_ITEM_GROUP,
                                             NULL, NULL);
        elm_genlist_item_select_mode_set(wifi_group,
                                          ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);

        if (wifi_count > 0)
          {
             for (i = 0; i < want_n; i++)
               {
                  Enm_Item_Data *id;

                  if (!desired[i].ap) continue;

                  id = calloc(1, sizeof(*id));
                  if (!id) continue;
                  id->nm = nm;
                  id->ap = desired[i].ap;
                  id->dev = NULL;
                  id->ap_path = eina_stringshare_add(desired[i].ap_path);
                  id->ssid = eina_stringshare_add(desired[i].label);

                  elm_genlist_item_append(gl, inst->ui.popup.itc_ap, id,
                                          wifi_group, ELM_GENLIST_ITEM_NONE,
                                          NULL, NULL);
               }
          }
     }

   /* Update IP label */
   if (nm->ip_address)
     {
        char ipbuf[128];
        snprintf(ipbuf, sizeof(ipbuf), "IP: %s", nm->ip_address);
        elm_object_text_set(inst->ui.popup.ip_label, ipbuf);
     }
   else
     elm_object_text_set(inst->ui.popup.ip_label, "");

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
   Evas_Object *box, *gl;
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
   itc->item_style = "default";
   itc->func.text_get = _enm_itc_ap_text_get;
   itc->func.content_get = _enm_itc_ap_content_get;
   itc->func.state_get = NULL;
   itc->func.del = _enm_itc_item_del;
   inst->ui.popup.itc_ap = itc;

   itc = elm_genlist_item_class_new();
   itc->item_style = "default";
   itc->func.text_get = _enm_itc_eth_text_get;
   itc->func.content_get = _enm_itc_eth_content_get;
   itc->func.state_get = NULL;
   itc->func.del = _enm_itc_item_del;
   inst->ui.popup.itc_eth = itc;

   /* Selected signal for row tap → connect/disconnect (single-click) */
   evas_object_smart_callback_add(gl, "selected", _enm_item_activated_cb,
                                   inst);

   elm_box_pack_end(box, gl);
   evas_object_show(gl);

   /* IP address label */
   inst->ui.popup.ip_label = elm_label_add(box);
   evas_object_size_hint_align_set(inst->ui.popup.ip_label,
                                   EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(inst->ui.popup.ip_label,
                                    EVAS_HINT_EXPAND, 0);
   elm_object_text_set(inst->ui.popup.ip_label, "");
   elm_box_pack_end(box, inst->ui.popup.ip_label);
   evas_object_show(inst->ui.popup.ip_label);

   evas_object_show(box);

   _enm_popup_update(ctxt->nm, inst);

   {
      Evas_Object *wrapper = _enm_widget_size_wrap(inst, box,
                                                    10, 30,
                                                    192, 240,
                                                    360, 400);
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
   typestr = (nm && nm->active_conn_type == NM_DEVICE_TYPE_ETHERNET)
     ? "ethernet" : "wifi";

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
             if (dev->type == NM_DEVICE_TYPE_ETHERNET && dev->state >= 100)
               {
                  edje_object_part_text_set(o, "e.text.label",
                                            dev->interface ?: _("Wired"));
                  break;
               }
          }
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

static const E_NM_Mod_Callbacks _enm_mod_cbs =
{
   .aps_changed    = _enm_mod_aps_changed,
   .manager_update = _enm_mod_manager_update,
   .manager_inout  = _enm_mod_manager_inout,
};

/* --- network activity indicator ------------------------------------------- */

/* Find the interface name for the active network connection */
static const char *
_enm_active_interface(struct NM_Manager *nm)
{
   struct NM_Device *dev;

   if (!nm) return NULL;

   /* WiFi: find first connected WiFi device (state >= 100 = activated) */
   if (nm->active_conn_type == NM_DEVICE_TYPE_WIFI)
     {
        EINA_INLIST_FOREACH(nm->devices, dev)
          {
             if (dev->type == NM_DEVICE_TYPE_WIFI && dev->state >= 100)
               return dev->interface;
          }
     }

   /* Ethernet: find first connected ethernet device */
   if (nm->active_conn_type == NM_DEVICE_TYPE_ETHERNET)
     {
        EINA_INLIST_FOREACH(nm->devices, dev)
          {
             if (dev->type == NM_DEVICE_TYPE_ETHERNET && dev->state >= 100)
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
typedef struct _Enm_Traffic_Worker
{
   E_NM_Module_Context *ctxt;
   char                *iface;   /* strdup; worker-only read */
   Eina_Lock            lock;
   unsigned long long   rx;
   unsigned long long   tx;
   Eina_Bool            have_sample;
} Enm_Traffic_Worker;

static void
_enm_traffic_worker_heavy(void *data, Ecore_Thread *thread)
{
   Enm_Traffic_Worker *w = data;

   while (!ecore_thread_check(thread))
     {
        unsigned long long rx = 0, tx = 0;
        int i;

        /* ~0.5s tick split into 100ms chunks so cancel is responsive */
        for (i = 0; i < 5; i++)
          {
             if (ecore_thread_check(thread)) return;
             usleep(100 * 1000);
          }

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
   Eina_Bool have;

   eina_lock_take(&w->lock);
   have = w->have_sample;
   rx = w->rx;
   tx = w->tx;
   w->have_sample = EINA_FALSE;
   eina_lock_release(&w->lock);

   if (!have) return;

   /* First sample seeds the counters — no rate yet */
   if (ctxt->prev_rx == 0 && ctxt->prev_tx == 0)
     {
        ctxt->prev_rx = rx;
        ctxt->prev_tx = tx;
        return;
     }

   /* Poll interval ~0.5s, so bytes_per_sec = delta * 2 */
   rx_level = _enm_traffic_level((rx - ctxt->prev_rx) * 2, ctxt->rx_level);
   tx_level = _enm_traffic_level((tx - ctxt->prev_tx) * 2, ctxt->tx_level);
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
   if (w->ctxt->traffic_thread == thread) w->ctxt->traffic_thread = NULL;
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
   eina_lock_new(&w->lock);
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
_enm_menu_new(E_NM_Instance *inst, Evas_Event_Mouse_Down *ev)
{
   E_Menu *m;
   int x, y;

   m = e_menu_new();
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

   if (ctxt->nm)
     _enm_mod_manager_update_inst(ctxt, inst, ctxt->nm, ctxt->nm->state);

   ctxt->instances = eina_list_append(ctxt->instances, inst);

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
                                 "preferences-network", NULL);
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

   e_nm_system_shutdown();
   e_nm_module_callbacks_set(NULL);
   e_nm_agent_callbacks_set(NULL, NULL);

   _enm_instances_free(ctxt);
   _enm_configure_registry_unregister();
   e_gadcon_provider_unregister(&_gc_class);

   E_FREE_FUNC(ctxt->popup_update_timer, ecore_timer_del);
   _enm_traffic_timer_stop(ctxt);
   E_FREE_FUNC(ctxt->powersave_handler, ecore_event_handler_del);
   E_FREE(ctxt);
   networkmanager_mod = NULL;

   eina_stringshare_replace(&_theme_path, NULL);

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
   return 1;
}
