#include "e.h"
#include "e_mod_main.h"
#include "e_networkmanager.h"

E_Module *networkmanager_mod = NULL;
static char tmpbuf[4096]; /* general purpose buffer, use immediately */

const char _e_nm_name[] = "networkmanager";
const char _e_nm_Name[] = N_("NetworkManager");
int _e_nm_log_dom = -1;

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
   inst->ui.popup.enabled = inst->ui.popup.list = inst->ui.popup.ip_label = NULL;
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

   E_FREE_FUNC(inst->popup, e_object_del);
}

static void
_enm_wireless_changed(void *data, Evas_Object *obj EINA_UNUSED,
                      void *info EINA_UNUSED)
{
   E_NM_Instance *inst = data;
   E_NM_Module_Context *ctxt = inst->ctxt;

   if (!ctxt) return;
   if (!ctxt->nm) return;
   enm_wireless_enabled_set(ctxt->nm, !!ctxt->wireless_enabled);
}

static Eina_Bool _enm_ssid_is_active(struct NM_Manager *nm, const char *ssid);

static void
_enm_popup_selected_cb(void *data)
{
   E_NM_Instance *inst = data;
   const char *path;
   struct NM_Access_Point *ap;
   struct NM_Manager *nm;
   struct NM_Device *dev;

   nm = inst->ctxt->nm;
   if (!nm) return;

   path = e_widget_ilist_selected_value_get(inst->ui.popup.list);
   if (!path) return;

   ap = enm_manager_find_ap(nm, path);
   if (!ap) return;

   /* If this AP is currently active, disconnect instead */
   if (nm->active_ap_path && _enm_ssid_is_active(nm, ap->ssid))
     {
        INF("Disconnect from %s", ap->ssid ?: path);
        enm_ap_disconnect(nm);
        return;
     }

   /* Walk devices to find which owns this AP */
   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        struct NM_Access_Point *a;
        EINA_INLIST_FOREACH(dev->access_points, a)
          {
             if (a == ap)
               {
                  INF("Connect to %s on device %s", ap->ssid ?: path,
                      dev->interface ?: dev->path);
                  enm_ap_connect(nm, dev, ap);
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
   }

   return icon;
}

struct _Enm_Forget_Data
{
   struct NM_Manager *nm;
   const char *connection_path; /* stringshare */
};

static void
_enm_forget_mouse_up_cb(void *data, Evas *e EINA_UNUSED,
                         Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   struct _Enm_Forget_Data *fd = data;

   INF("Forget connection: %s", fd->connection_path);
   enm_connection_delete(fd->nm, fd->connection_path);
}

static void
_enm_forget_data_free_cb(void *data, Evas *e EINA_UNUSED,
                          Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   struct _Enm_Forget_Data *fd = data;
   eina_stringshare_del(fd->connection_path);
   free(fd);
}

static Evas_Object *
_enm_ap_end_new(struct NM_Manager *nm, struct NM_Access_Point *ap, Evas *evas)
{
   Evas_Object *end;
   const char *conn_path;
   struct _Enm_Forget_Data *fd;

   /* Only show forget for saved (known) networks */
   if (!nm->saved_connections || !ap->ssid)
     return NULL;

   conn_path = eina_hash_find(nm->saved_connections, ap->ssid);
   if (!conn_path)
     return NULL;

   end = edje_object_add(evas);
   if (!e_theme_edje_object_set(end, "base/theme/modules/networkmanager",
                                "e/modules/networkmanager/forget"))
     {
        if (!e_theme_edje_object_set(end, "base/theme/modules/connman",
                                     "e/modules/connman/forget"))
          {
             evas_object_del(end);
             return NULL;
          }
     }

   fd = malloc(sizeof(*fd));
   if (!fd)
     {
        evas_object_del(end);
        return NULL;
     }
   fd->nm = nm;
   fd->connection_path = eina_stringshare_add(conn_path);

   evas_object_propagate_events_set(end, EINA_FALSE);
   evas_object_event_callback_add(end, EVAS_CALLBACK_MOUSE_UP,
                                  _enm_forget_mouse_up_cb, fd);
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

static void
_enm_popup_update(struct NM_Manager *nm, E_NM_Instance *inst)
{
   Evas_Object *list = inst->ui.popup.list;
   Evas_Object *enabled = inst->ui.popup.enabled;
   Evas *evas = evas_object_evas_get(list);
   struct NM_Device *dev;
   Eina_Hash *seen_ssids;

   EINA_SAFETY_ON_NULL_RETURN(nm);

   /* Refresh saved connections for forget button visibility.
    * This is async — the hash populates as D-Bus replies arrive,
    * then enm_mod_aps_changed() triggers a popup re-render. */
   enm_saved_connections_get(nm);

   e_widget_ilist_freeze(list);
   e_widget_ilist_clear(list);

   /* Show connected ethernet devices first */
   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        Evas_Object *icon;

        if (dev->type != NM_DEVICE_TYPE_ETHERNET) continue;
        if (dev->state < 100) continue; /* not activated */

        icon = _enm_eth_icon_new(dev, evas);
        e_widget_ilist_append_full(list, icon, NULL,
                                   dev->interface ?: _("Wired"),
                                   NULL, NULL, NULL);
     }

   /* Deduplicate APs by SSID — show only the strongest station per SSID */
   seen_ssids = eina_hash_string_superfast_new(NULL);

   EINA_INLIST_FOREACH(nm->devices, dev)
     {
        struct NM_Access_Point *ap;

        if (dev->type != NM_DEVICE_TYPE_WIFI) continue;

        EINA_INLIST_FOREACH(dev->access_points, ap)
          {
             struct NM_Access_Point *best;
             Evas_Object *icon, *end;

             /* Skip hidden networks (empty or NULL SSID) */
             if (!ap->ssid || !ap->ssid[0]) continue;

             /* Skip if we already showed this SSID */
             if (eina_hash_find(seen_ssids, ap->ssid)) continue;

             /* Find the best AP for this SSID and only show that one */
             best = _enm_best_ap_for_ssid(nm, ap->ssid);
             if (!best) continue;

             eina_hash_add(seen_ssids, ap->ssid, (void *)1);

             icon = _enm_ap_icon_new(nm, best, evas);
             end = _enm_ap_end_new(nm, best, evas);

             e_widget_ilist_append_full(list, icon, end,
                                        best->ssid,
                                        _enm_popup_selected_cb,
                                        inst, best->path);
          }
     }

   eina_hash_free(seen_ssids);

   e_widget_ilist_thaw(list);
   e_widget_ilist_go(list);

   /* Update IP label */
   if (nm->ip_address)
     {
        char ipbuf[128];
        snprintf(ipbuf, sizeof(ipbuf), "IP: %s", nm->ip_address);
        e_widget_label_text_set(inst->ui.popup.ip_label, ipbuf);
     }
   else
     e_widget_label_text_set(inst->ui.popup.ip_label, "");

   if (inst->ctxt)
     {
        inst->ctxt->wireless_enabled = nm->wireless_enabled ? 1 : 0;
        e_widget_check_checked_set(enabled, inst->ctxt->wireless_enabled);
     }

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

   e_widget_size_min_set(widget, w, h);
}

static void
_enm_popup_new(E_NM_Instance *inst)
{
   E_NM_Module_Context *ctxt = inst->ctxt;
   Evas_Object *list, *ck;
   Evas *evas;

   EINA_SAFETY_ON_FALSE_RETURN(inst->popup == NULL);

   if (!ctxt->nm) return;

   e_nm_scan(ctxt->nm);

   inst->popup = e_gadcon_popup_new(inst->gcc, 0);
   evas = e_comp->evas;

   list = e_widget_list_add(evas, 0, 0);
   inst->ui.popup.list = e_widget_ilist_add(evas, 24, 24, NULL);
   e_widget_size_min_set(inst->ui.popup.list, 60, 100);
   e_widget_list_object_append(list, inst->ui.popup.list, 1, 1, 0.5);

   inst->ui.popup.ip_label = e_widget_label_add(evas, "");
   e_widget_list_object_append(list, inst->ui.popup.ip_label, 1, 0, 0.5);

   ck = e_widget_check_add(evas, _("Wifi On"), &(ctxt->wireless_enabled));
   inst->ui.popup.enabled = ck;
   e_widget_list_object_append(list, ck, 1, 0, 0.5);
   evas_object_smart_callback_add(ck, "changed",
                                  _enm_wireless_changed, inst);

   _enm_popup_update(ctxt->nm, inst);

   _enm_widget_size_set(inst, list, 10, 30, 192, 240, 360, 400);
   e_gadcon_popup_content_set(inst->popup, list);
   e_comp_object_util_autoclose(inst->popup->comp_object, _enm_popup_del,
                                NULL, inst);
   e_gadcon_popup_show(inst->popup);
   e_object_data_set(E_OBJECT(inst->popup), inst);
   E_OBJECT_DEL_SET(inst->popup, _enm_popup_del_cb);
}

/* --- UI callbacks called from e_networkmanager.c -------------------------- */

void
enm_mod_aps_changed(struct NM_Manager *nm)
{
   E_NM_Module_Context *ctxt = networkmanager_mod->data;
   const Eina_List *l;
   E_NM_Instance *inst;

   EINA_LIST_FOREACH(ctxt->instances, l, inst)
     {
        if (!inst->popup) continue;
        _enm_popup_update(nm, inst);
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
     enm_mod_manager_update(nm);
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
