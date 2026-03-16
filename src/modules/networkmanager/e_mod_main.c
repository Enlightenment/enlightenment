#include "e.h"
#include "e_mod_main.h"
#include "e_networkmanager.h"

E_Module *networkmanager_mod = NULL;
static char tmpbuf[4096]; /* general purpose buffer, use immediately */

const char _e_nm_name[] = "networkmanager";
const char _e_nm_Name[] = N_("NetworkManager");
int _e_nm_log_dom = -1;

/* Forward declarations for traffic monitor */
static void _enm_traffic_timer_start(E_NM_Module_Context *ctxt);
static void _enm_traffic_timer_stop(E_NM_Module_Context *ctxt);

const char *
e_nm_theme_path(void)
{
#define TF "/e-module-connman.edj"
   size_t dirlen;

   dirlen = strlen(networkmanager_mod->dir);
   if (dirlen >= sizeof(tmpbuf) - sizeof(TF))
     return NULL;

   memcpy(tmpbuf, networkmanager_mod->dir, dirlen);
   memcpy(tmpbuf + dirlen, TF, sizeof(TF));

   return tmpbuf;
#undef TF
}

/* --- popup ---------------------------------------------------------------- */

void
enm_popup_del(E_NM_Instance *inst)
{
   E_FREE_FUNC(inst->popup, e_object_del);
   E_FREE_FUNC(inst->ctxt->popup_update_timer, ecore_timer_del);
   inst->ui.popup.genlist = inst->ui.popup.ip_label = NULL;
   /* Item classes are freed with the popup — clear pointers so update guard works */
   if (inst->ui.popup.itc_group)
     {
        elm_genlist_item_class_free(inst->ui.popup.itc_group);
        inst->ui.popup.itc_group = NULL;
     }
   if (inst->ui.popup.itc_group_wifi)
     {
        elm_genlist_item_class_free(inst->ui.popup.itc_group_wifi);
        inst->ui.popup.itc_group_wifi = NULL;
     }
   if (inst->ui.popup.itc_ap)
     {
        elm_genlist_item_class_free(inst->ui.popup.itc_ap);
        inst->ui.popup.itc_ap = NULL;
     }
   if (inst->ui.popup.itc_eth)
     {
        elm_genlist_item_class_free(inst->ui.popup.itc_eth);
        inst->ui.popup.itc_eth = NULL;
     }
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
        Evas_Object *ic = _enm_ap_icon_new(id->nm, id->ap, evas_object_evas_get(obj));
        if (ic) evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(32), ELM_SCALE_SIZE(32));
        return ic;
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
        Evas_Object *ic = _enm_eth_icon_new(id->dev, evas_object_evas_get(obj));
        if (ic) evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(32), ELM_SCALE_SIZE(32));
        return ic;
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
   E_NM_Module_Context *ctxt;

   if (!inst) return;
   ctxt = inst->ctxt;
   if (!ctxt || !ctxt->nm) return;

   enm_wireless_enabled_set(ctxt->nm, elm_check_state_get(obj));
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
   evas_object_show(ck);
   evas_object_propagate_events_set(ck, EINA_FALSE);
   return ck;
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
   /* Deselect immediately so repeat clicks always fire "selected" */
   elm_genlist_item_selected_set(it, EINA_FALSE);
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
   if (!e_theme_edje_object_set(icon, "base/theme/modules/networkmanager",
                                "e/modules/networkmanager/icon/wifi"))
     e_theme_edje_object_set(icon, "base/theme/modules/connman",
                             "e/modules/connman/icon/wifi");

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

        edje_object_part_text_set(icon, "band_label", band);
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
   evas_object_size_hint_min_set(end, ELM_SCALE_SIZE(32), ELM_SCALE_SIZE(32));

   return end;
}

static Evas_Object *
_enm_eth_icon_new(struct NM_Device *dev, Evas *evas)
{
   Edje_Message_Int_Set *msg;
   Evas_Object *icon;
   int state_val;

   icon = edje_object_add(evas);
   if (!e_theme_edje_object_set(icon, "base/theme/modules/networkmanager",
                                "e/modules/networkmanager/icon/ethernet"))
     e_theme_edje_object_set(icon, "base/theme/modules/connman",
                             "e/modules/connman/icon/ethernet");

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
        e_widget_label_text_set(inst->ui.popup.ip_label, ipbuf);
     }
   else
     e_widget_label_text_set(inst->ui.popup.ip_label, "");

}

static void
_enm_widget_size_set(E_NM_Instance *inst, Evas_Object *widget,
                     Evas_Coord percent_w, Evas_Coord percent_h,
                     Evas_Coord min_w, Evas_Coord min_h,
                     Evas_Coord max_w, Evas_Coord max_h)
{
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

   evas_object_size_hint_min_set(widget, w, h);
}

static void
_enm_popup_new(E_NM_Instance *inst)
{
   E_NM_Module_Context *ctxt = inst->ctxt;
   Evas_Object *box, *gl;
   Evas *evas;
   Elm_Genlist_Item_Class *itc;

   EINA_SAFETY_ON_FALSE_RETURN(inst->popup == NULL);

   if (!ctxt->nm) return;

   e_nm_scan(ctxt->nm);

   inst->popup = e_gadcon_popup_new(inst->gcc, 0);
   evas = e_comp->evas;

   /* Outer elm box to stack genlist + IP label */
   box = elm_box_add(e_comp->elm);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   /* Genlist for AP/ethernet rows */
   gl = elm_genlist_add(box);
   evas_object_size_hint_weight_set(gl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gl, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(gl, ELM_SCALE_SIZE(192), ELM_SCALE_SIZE(100));
   elm_scroller_bounce_set(gl, EINA_FALSE, EINA_TRUE);
   evas_object_show(gl);
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

   /* IP address label */
   inst->ui.popup.ip_label = e_widget_label_add(evas, "");
   elm_box_pack_end(box, inst->ui.popup.ip_label);
   evas_object_show(inst->ui.popup.ip_label);

   evas_object_show(box);

   _enm_popup_update(ctxt->nm, inst);

   _enm_widget_size_set(inst, box, 10, 30, 192, 240, 360, 400);
   e_gadcon_popup_content_set(inst->popup, box);
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
   const Eina_List *l;
   E_NM_Instance *inst;

   ctxt->popup_update_timer = NULL;

   if (!ctxt->nm) return ECORE_CALLBACK_CANCEL;

   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     {
        if (!inst->popup) continue;
        _enm_popup_update(ctxt->nm, inst);
     }

   return ECORE_CALLBACK_CANCEL;
}

void
enm_mod_aps_changed(struct NM_Manager *nm EINA_UNUSED)
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
   if (!networkmanager_mod) return;
   ctxt = networkmanager_mod->data;
   const Eina_List *l;
   E_NM_Instance *inst;

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

             edje_object_part_text_set(o, "band_label", band);
          }
        else
          edje_object_part_text_set(o, "band_label", "");
     }
   else
     {
        edje_object_signal_emit(o, "e,security,off", "e");
        edje_object_part_text_set(o, "band_label", "");
     }
}

void
enm_mod_manager_update(struct NM_Manager *nm)
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

void
enm_mod_manager_inout(struct NM_Manager *nm)
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
        enm_mod_manager_update(nm);
        /* Pre-fetch saved connections so the hash is warm when popup opens */
        enm_saved_connections_get(nm);
     }
   else
     _enm_traffic_timer_stop(ctxt);
}

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

static Eina_Bool
_enm_traffic_poll_cb(void *data)
{
   E_NM_Module_Context *ctxt = data;
   const char *iface;
   unsigned long long rx = 0, tx = 0;
   int rx_level, tx_level;

   if (!ctxt->nm) return ECORE_CALLBACK_RENEW;
   if (ctxt->nm->state < NM_STATE_CONNECTED_LOCAL) return ECORE_CALLBACK_RENEW;

   iface = _enm_active_interface(ctxt->nm);
   if (!iface) return ECORE_CALLBACK_RENEW;

   if (!_enm_read_sysfs_counter(iface, "rx_bytes", &rx) ||
       !_enm_read_sysfs_counter(iface, "tx_bytes", &tx))
     return ECORE_CALLBACK_RENEW;

   /* First poll: seed the counters */
   if (ctxt->prev_rx == 0 && ctxt->prev_tx == 0)
     {
        ctxt->prev_rx = rx;
        ctxt->prev_tx = tx;
        return ECORE_CALLBACK_RENEW;
     }

   /* Poll interval is 0.5s, so bytes_per_sec = delta * 2 */
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

   return ECORE_CALLBACK_RENEW;
}

static void
_enm_traffic_timer_start(E_NM_Module_Context *ctxt)
{
   if (ctxt->traffic_timer) return;
   if (ctxt->screen_off || ctxt->powersave_high) return;
   if (!ctxt->nm) return;
   if (ctxt->nm->state < NM_STATE_CONNECTED_LOCAL) return;

   ctxt->prev_rx = 0;
   ctxt->prev_tx = 0;
   ctxt->rx_level = 0;
   ctxt->tx_level = 0;
   ctxt->traffic_timer = ecore_timer_add(0.5, _enm_traffic_poll_cb, ctxt);
}

static void
_enm_traffic_timer_stop(E_NM_Module_Context *ctxt)
{
   E_FREE_FUNC(ctxt->traffic_timer, ecore_timer_del);
   if (ctxt->rx_level || ctxt->tx_level)
     {
        _enm_traffic_signal_emit(ctxt, 0, 0);
        ctxt->rx_level = 0;
        ctxt->tx_level = 0;
     }
}

static Eina_Bool
_enm_screensaver_on_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   E_NM_Module_Context *ctxt = data;

   ctxt->screen_off = EINA_TRUE;
   _enm_traffic_timer_stop(ctxt);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_enm_screensaver_off_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   E_NM_Module_Context *ctxt = data;

   ctxt->screen_off = EINA_FALSE;
   _enm_traffic_timer_start(ctxt);
   return ECORE_CALLBACK_PASS_ON;
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

   /* Try networkmanager theme first, fall back to connman */
   if (!e_theme_edje_object_set(inst->ui.gadget,
                                "base/theme/modules/networkmanager",
                                "e/modules/networkmanager/main"))
     e_theme_edje_object_set(inst->ui.gadget,
                             "base/theme/modules/connman",
                             "e/modules/connman/main");

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
   E_NM_Module_Context *ctxt;
   Eina_List *instances;

   if (!networkmanager_mod) return NULL;

   ctxt = networkmanager_mod->data;
   if (!ctxt) return NULL;

   instances = ctxt->instances;
   snprintf(tmpbuf, sizeof(tmpbuf), "networkmanager.%d",
            eina_list_count(instances));
   return tmpbuf;
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
   Eldbus_Connection *c;

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

   c = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   if (!c) goto error_dbus_bus_get;

   if (!e_nm_system_init(c)) goto error_nm_system_init;

   ctxt->conf_dialog = NULL;
   networkmanager_mod = m;

   /* Initialize power-aware traffic monitoring state */
   ctxt->powersave_high = (e_powersave_mode_get() >= E_POWERSAVE_MODE_EXTREME);

   /* Register event handlers for power-aware traffic monitoring */
   ctxt->screensaver_on_handler =
     ecore_event_handler_add(E_EVENT_SCREENSAVER_ON,
                             _enm_screensaver_on_cb, ctxt);
   ctxt->screensaver_off_handler =
     ecore_event_handler_add(E_EVENT_SCREENSAVER_OFF,
                             _enm_screensaver_off_cb, ctxt);
   ctxt->powersave_handler =
     ecore_event_handler_add(E_EVENT_POWERSAVE_UPDATE,
                             _enm_powersave_cb, ctxt);

   _enm_configure_registry_register();
   e_gadcon_provider_register(&_gc_class);

   return ctxt;

error_nm_system_init:
   eldbus_connection_unref(c);
error_dbus_bus_get:
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

   _enm_instances_free(ctxt);
   _enm_configure_registry_unregister();
   e_gadcon_provider_unregister(&_gc_class);

   E_FREE_FUNC(ctxt->popup_update_timer, ecore_timer_del);
   E_FREE_FUNC(ctxt->traffic_timer, ecore_timer_del);
   E_FREE_FUNC(ctxt->screensaver_on_handler, ecore_event_handler_del);
   E_FREE_FUNC(ctxt->screensaver_off_handler, ecore_event_handler_del);
   E_FREE_FUNC(ctxt->powersave_handler, ecore_event_handler_del);
   E_FREE(ctxt);
   networkmanager_mod = NULL;

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
