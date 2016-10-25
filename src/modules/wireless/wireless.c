#include "wireless.h"


/* FIXME */
void connman_technology_enabled_set(Wireless_Service_Type type, Eina_Bool state);

static const char *wireless_theme_groups[] =
{
  [WIRELESS_SERVICE_TYPE_ETHERNET] = "e/gadget/wireless/ethernet",
  [WIRELESS_SERVICE_TYPE_WIFI] = "e/gadget/wireless/wifi",
  [WIRELESS_SERVICE_TYPE_BLUETOOTH] = "e/gadget/wireless/bluetooth",
  [WIRELESS_SERVICE_TYPE_CELLULAR] = "e/gadget/wireless/cellular",
};

static const char *wireless_ipv4_methods[] =
{
   N_("Disabled"),
   N_("Manual"),
   "DHCP",
   N_("Fixed"),
};

static const char *wireless_ipv6_methods[] =
{
   N_("Off"),
   N_("Manual"),
   N_("Auto"),
   "6to4",
   N_("Fixed"),
};

static const char *wireless_proxy_methods[] =
{
   N_("Direct"),
   N_("Manual"),
   N_("Auto"),
};

typedef struct Instance
{
   int id;
   E_Gadget_Site_Orient orient;
   Evas_Object *box;
   Evas_Object *icon[WIRELESS_SERVICE_TYPE_LAST];

   Eina_Bool popup;

   struct
   {
      Evas_Object *error;
      Evas_Object *address;
      Evas_Object *method;
      Evas_Object *signal;
      Wireless_Service_Type type;
   } tooltip;
} Instance;

typedef struct Wireless_Auth_Popup
{
   Evas_Object *popup;
   Wireless_Auth_Cb cb;
   void *data;
   Eina_Bool sent : 1;
} Wireless_Auth_Popup;

static Eina_Array *wireless_networks;
static Wireless_Connection *wireless_current[WIRELESS_SERVICE_TYPE_LAST];
static Eina_Bool wireless_type_enabled[WIRELESS_SERVICE_TYPE_LAST];
static Eina_Bool wireless_type_available[WIRELESS_SERVICE_TYPE_LAST];
static Eina_List *instances;
static Eina_List *wireless_auth_pending;
static Wireless_Auth_Popup *wireless_auth_popup;
static Eina_Bool wireless_offline;
static Evas_Object *wireless_edit_popup;
static Wireless_Connection *wireless_edit[2];
static unsigned int wireless_network_count[WIRELESS_SERVICE_TYPE_LAST];

static struct
{
   Evas_Object *popup;
   Evas_Object *box;
   Evas_Object *content;
   Eina_Stringshare *name_servers;
   Eina_Stringshare *time_servers;
   Eina_Stringshare *domain_servers;
   Eina_Stringshare *proxy_servers;
   Eina_Stringshare *proxy_excludes;
   Eina_Hash *items;
   Eina_List *entries;
   Wireless_Service_Type type;
} wireless_popup;

static Eina_Bool auth_popup;

#undef DBG
#undef INF
#undef WRN
#undef ERR

#define DBG(...) EINA_LOG_DOM_DBG(_wireless_gadget_log_dom, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_wireless_gadget_log_dom, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_wireless_gadget_log_dom, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_wireless_gadget_log_dom, __VA_ARGS__)
static int _wireless_gadget_log_dom = -1;

static void
_wifi_icon_signal(Evas_Object *icon, int state, int strength)
{
   Edje_Message_Int_Set *msg;

   DBG("icon msg: %d %d%%", state, strength);
   msg = alloca(sizeof(Edje_Message_Int_Set) + sizeof(int));
   msg->count = 2;
   msg->val[0] = state;
   msg->val[1] = strength;
   edje_object_message_send(elm_layout_edje_get(icon), EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_wifi_icon_init(Evas_Object *icon, Wireless_Network *wn, int type)
{
   int state = 0, strength = 0;

   if (wn)
     {
        state = wn->state;
        strength = wn->strength;
     }
   _wifi_icon_signal(icon, state, strength);

   if (!wn)
     {
        if (wireless_type_available[type])
          elm_object_signal_emit(icon, "e,state,default", "e");
        else
          elm_object_signal_emit(icon, "e,state,error", "e");
        elm_object_signal_emit(icon, "e,state,unsecured", "e");
        return;
     }
   if (wn->state == WIRELESS_NETWORK_STATE_FAILURE)
     {
        elm_object_signal_emit(icon, "e,state,error", "e");
        return;
     }
   elm_object_signal_emit(icon, "e,state,default", "e");
   switch (wn->type)
     {
      case WIRELESS_SERVICE_TYPE_WIFI:
        if (wn->security > WIRELESS_NETWORK_SECURITY_WEP)
          elm_object_signal_emit(icon, "e,state,secure", "e");
        else if (wn->security == WIRELESS_NETWORK_SECURITY_WEP)
          elm_object_signal_emit(icon, "e,state,insecure", "e");
        else if (!wn->security)
          elm_object_signal_emit(icon, "e,state,unsecured", "e");
        break;
      default: break;
     }
}

static void
_wireless_popup_toggle(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   connman_technology_enabled_set(wireless_popup.type, elm_check_state_get(obj));
}

static void
_wireless_popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->popup = 0;
   E_FREE_FUNC(wireless_popup.items, eina_hash_free);
   E_FREE_FUNC(wireless_popup.entries, eina_list_free);
   eina_stringshare_replace(&wireless_popup.proxy_servers, NULL);
   eina_stringshare_replace(&wireless_popup.proxy_excludes, NULL);
   eina_stringshare_replace(&wireless_popup.name_servers, NULL);
   eina_stringshare_replace(&wireless_popup.time_servers, NULL);
   eina_stringshare_replace(&wireless_popup.domain_servers, NULL);
   wireless_popup.box = NULL;
   wireless_popup.content = NULL;
   wireless_popup.popup = NULL;
   wireless_popup.type = -1;
}

static void
_wireless_edit_basic_entries_update(void)
{
   Eina_List *l;
   Evas_Object *ent;
   Eina_Bool disabled;

   if (wireless_edit[1]->ipv6)
     disabled = wireless_edit[1]->method != WIRELESS_NETWORK_IPV6_METHOD_MANUAL;
   else
     disabled = wireless_edit[1]->method != WIRELESS_NETWORK_IPV4_METHOD_MANUAL;
   EINA_LIST_FOREACH(wireless_popup.entries, l, ent)
     elm_object_disabled_set(ent, disabled);
}

static Evas_Object *
_wireless_popup_table_entry_row(Evas_Object *tb, const char *name, Evas_Smart_Cb cb, void *data, int *row)
{
   Evas_Object *fr, *entry;

   fr = elm_frame_add(tb);
   evas_object_show(fr);
   E_EXPAND(fr);
   E_FILL(fr);
   elm_object_text_set(fr, name);
   elm_table_pack(tb, fr, 0, *row, 2, 2);
   *row += 2;

   entry = elm_entry_add(tb);
   evas_object_show(entry);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   evas_object_data_set(entry, "table", tb);
   evas_object_smart_callback_add(entry, "activated", cb, data);
   elm_object_content_set(fr, entry);
   return entry;
}

static void
_wireless_edit_entry_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Eina_Stringshare **str = data;

   eina_stringshare_replace(str, elm_entry_entry_get(obj));
}

static void
_wireless_gadget_edit_array_entry(Eina_Array *arr, Eina_Stringshare **ptr)
{
   Eina_Stringshare *str;
   unsigned int i;
   Eina_Array_Iterator it;
   Eina_Strbuf *buf;

   if (!arr) return;
   buf = eina_strbuf_new();
   EINA_ARRAY_ITER_NEXT(arr, i, str, it)
     {
        if (eina_strbuf_length_get(buf)) eina_strbuf_append(buf, ", ");
        eina_strbuf_append(buf, str);
     }
   eina_stringshare_replace(ptr, eina_strbuf_string_get(buf));
   eina_strbuf_free(buf);
}

static void
_wireless_gadget_edit_proxy_method_update(void)
{
   Evas_Object *ent, *tb = wireless_popup.content;
   int row = 1;
   Wireless_Connection *wc = wireless_edit[1];

   evas_object_del(elm_table_child_get(wireless_popup.content, 0, 1));
   evas_object_del(elm_table_child_get(wireless_popup.content, 0, 3));
   evas_object_del(elm_table_child_get(wireless_popup.content, 0, 5));
   switch (wc->proxy_type)
     {
      case WIRELESS_PROXY_TYPE_DIRECT:
        _wireless_popup_table_entry_row(tb, NULL, NULL, NULL, &row);
        evas_object_hide(elm_table_child_get(wireless_popup.content, 0, 1));
        _wireless_popup_table_entry_row(tb, NULL, NULL, NULL, &row);
        evas_object_hide(elm_table_child_get(wireless_popup.content, 0, 3));
        _wireless_popup_table_entry_row(tb, NULL, NULL, NULL, &row);
        evas_object_hide(elm_table_child_get(wireless_popup.content, 0, 5));
        break;
      case WIRELESS_PROXY_TYPE_MANUAL:
        ent = _wireless_popup_table_entry_row(tb, _("Proxy Servers"), NULL, NULL, &row);
        elm_entry_entry_set(ent, wireless_popup.proxy_servers);
        evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_popup.proxy_servers);
        ent = _wireless_popup_table_entry_row(tb, _("Proxy Excludes"), NULL, NULL, &row);
        elm_entry_entry_set(ent, wireless_popup.proxy_excludes);
        evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_popup.proxy_excludes);
        _wireless_popup_table_entry_row(tb, NULL, NULL, NULL, &row);
        evas_object_hide(elm_table_child_get(wireless_popup.content, 0, 5));
        break;
      case WIRELESS_PROXY_TYPE_AUTO:
        ent = _wireless_popup_table_entry_row(tb, _("Proxy Address"), NULL, NULL, &row);
        elm_entry_entry_set(ent, wc->proxy_url);
        evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_edit[1]->address);
        _wireless_popup_table_entry_row(tb, NULL, NULL, NULL, &row);
        evas_object_hide(elm_table_child_get(wireless_popup.content, 0, 3));
        _wireless_popup_table_entry_row(tb, NULL, NULL, NULL, &row);
        evas_object_hide(elm_table_child_get(wireless_popup.content, 0, 5));
        break;
     }
}

static void
_wireless_gadget_edit_proxy_method(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   wireless_edit[1]->proxy_type = (intptr_t)elm_object_item_data_get(event_info);
   _wireless_gadget_edit_proxy_method_update();
}

static void
_wireless_gadget_edit_method(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   wireless_edit[1]->method = (intptr_t)elm_object_item_data_get(event_info);
   _wireless_edit_basic_entries_update();
}

static void
_wireless_gadget_edit_method_open(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int i, fixed;
   const char **methods;

   elm_hoversel_clear(obj);
   evas_object_layer_set(obj, E_LAYER_MENU);
   if (wireless_edit[1]->ipv6)
     {
        fixed = WIRELESS_NETWORK_IPV6_METHOD_FIXED;
        methods = wireless_ipv6_methods;
     }
   else
     {
        fixed = WIRELESS_NETWORK_IPV6_METHOD_FIXED;
        methods = wireless_ipv4_methods;
     }
   for (i = 0; i < fixed; i++)
     {
        if ((int)wireless_edit[1]->method != i)
          elm_hoversel_item_add(obj, methods[i], NULL, ELM_ICON_NONE, NULL, (intptr_t*)(long)i);
     }
}

static void
_wireless_edit_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   int i;

   wireless_popup.entries = eina_list_free(wireless_popup.entries);
   eina_stringshare_del(wireless_edit[0]->wn->path);
   free(wireless_edit[0]->wn);
   for (i = 0; i <= 1; i++)
     {
        eina_stringshare_del(wireless_edit[i]->address);
        eina_stringshare_del(wireless_edit[i]->gateway);
        if (wireless_edit[i]->ipv6)
          eina_stringshare_del(wireless_edit[i]->ip.v6.prefixlength);
        else
          eina_stringshare_del(wireless_edit[i]->ip.v4.netmask);
        eina_stringshare_del(wireless_edit[i]->proxy_url);
        array_clear(wireless_edit[i]->proxy_excludes);
        array_clear(wireless_edit[i]->proxy_servers);
        E_FREE(wireless_edit[i]);
     }
   wireless_edit_popup = NULL;
}

static Eina_Array *
string_to_array(const char *str)
{
   const char *p = str;
   Eina_Array *arr;

   arr = eina_array_new(1);
   do
     {
        const char *start, *end;

        start = p;
        p = strchr(p, ',');
        if (!p) break;
        end = p - 1;
        while (isspace(start[0])) start++;
        while (isspace(end[0])) end--;
        end++;

        if (start == end) break;
        eina_array_push(arr, eina_stringshare_add_length(start, end - start));
        p++;
     } while (p[0]);
   return arr;
}

static Eina_Bool
_wireless_array_notequal(Eina_Array *a, Eina_Array *b)
{
   unsigned int i;

   if ((!!a) != (!!b)) return EINA_TRUE;
   if ((!a) && (!b)) return EINA_FALSE;
   if (eina_array_count(a) != eina_array_count(b)) return EINA_TRUE;
   for (i = 0; i < eina_array_count(a); i++)
     if (eina_array_data_get(a, i) != eina_array_data_get(b, i)) return EINA_TRUE;
   return EINA_FALSE;
}

static void
_wireless_edit_send()
{
   Eina_Bool basic = EINA_FALSE, proxy = EINA_FALSE;

   EINTERN void connman_service_edit(const char *path, Wireless_Connection *wc);
   EINTERN void connman_service_edit_proxy(const char *path, Wireless_Connection *wc);
   EINTERN void connman_service_edit_domains(const char *path, Wireless_Connection *wc);
   EINTERN void connman_service_edit_nameservers(const char *path, Wireless_Connection *wc);
   EINTERN void connman_service_edit_timeservers(const char *path, Wireless_Connection *wc);

   if (wireless_edit[0]->method == wireless_edit[1]->method)
     {
        if (wireless_edit[1]->ipv6)
          switch (wireless_edit[1]->method)
            {
             case WIRELESS_NETWORK_IPV6_METHOD_AUTO:
               basic = wireless_edit[0]->ip.v6.privacy != wireless_edit[1]->ip.v6.privacy;
               break;
             case WIRELESS_NETWORK_IPV6_METHOD_MANUAL:
               basic = wireless_edit[0]->address != wireless_edit[1]->address;
               if (basic) break;
               basic = wireless_edit[0]->gateway != wireless_edit[1]->gateway;
               if (basic) break;
               basic = wireless_edit[0]->ip.v6.prefixlength != wireless_edit[1]->ip.v6.prefixlength;
               break;
             default: break;
            }
        else
          switch (wireless_edit[1]->method)
            {
             case WIRELESS_NETWORK_IPV4_METHOD_MANUAL:
               basic = wireless_edit[0]->address != wireless_edit[1]->address;
               if (basic) break;
               basic = wireless_edit[0]->gateway != wireless_edit[1]->gateway;
               if (basic) break;
               basic = wireless_edit[0]->ip.v4.netmask != wireless_edit[1]->ip.v4.netmask;
               break;
             default: break;
            }
     }
   else
     basic = EINA_TRUE;

   if (basic)
     connman_service_edit(wireless_edit[1]->wn->path, wireless_edit[1]);

   if (wireless_edit[1]->proxy_type == WIRELESS_PROXY_TYPE_MANUAL)
     {
        array_clear(wireless_edit[1]->proxy_servers);
        array_clear(wireless_edit[1]->proxy_excludes);
        if (wireless_popup.proxy_servers)
          wireless_edit[1]->proxy_servers = string_to_array(wireless_popup.proxy_servers);
        if (wireless_popup.proxy_excludes)
          wireless_edit[1]->proxy_excludes = string_to_array(wireless_popup.proxy_excludes);
     }
   if (wireless_edit[0]->proxy_type == wireless_edit[1]->proxy_type)
     {
        switch (wireless_edit[0]->proxy_type)
          {
            case WIRELESS_PROXY_TYPE_MANUAL:
              proxy = _wireless_array_notequal(wireless_edit[0]->proxy_servers,
                wireless_edit[1]->proxy_servers);
              if (proxy) break;
              proxy = _wireless_array_notequal(wireless_edit[0]->proxy_excludes,
                wireless_edit[1]->proxy_excludes);
              break;
            case WIRELESS_PROXY_TYPE_AUTO:
              proxy = wireless_edit[0]->proxy_url != wireless_edit[1]->proxy_url;
              break;
            break;
           default: break;
          }
        
     }
   else
     proxy = EINA_TRUE;
   if (proxy)
     connman_service_edit_proxy(wireless_edit[1]->wn->path, wireless_edit[1]);

   array_clear(wireless_edit[1]->domain_servers);
   if (wireless_popup.domain_servers)
     wireless_edit[1]->domain_servers = string_to_array(wireless_popup.domain_servers);
   array_clear(wireless_edit[1]->name_servers);
   if (wireless_popup.name_servers)
     wireless_edit[1]->name_servers = string_to_array(wireless_popup.name_servers);
   array_clear(wireless_edit[1]->name_servers);
   if (wireless_popup.name_servers)
     wireless_edit[1]->name_servers = string_to_array(wireless_popup.name_servers);

   if (_wireless_array_notequal(wireless_edit[0]->domain_servers, wireless_edit[1]->domain_servers))
     connman_service_edit_domains(wireless_edit[1]->wn->path, wireless_edit[1]);
   if (_wireless_array_notequal(wireless_edit[0]->name_servers, wireless_edit[1]->name_servers))
     connman_service_edit_nameservers(wireless_edit[1]->wn->path, wireless_edit[1]);
   if (_wireless_array_notequal(wireless_edit[0]->time_servers, wireless_edit[1]->time_servers))
     connman_service_edit_timeservers(wireless_edit[1]->wn->path, wireless_edit[1]);
}

static void
_wireless_edit_send_button()
{
   e_comp_object_util_autoclose(NULL, NULL, NULL, NULL);
   _wireless_edit_send();
}

static void
_wireless_edit_remove()
{
   EINTERN void connman_service_remove(const char *path);
   e_comp_object_util_autoclose(NULL, NULL, NULL, NULL);
   connman_service_remove(wireless_edit[1]->wn->path);
}

static Eina_Bool
_wireless_edit_key(void *d EINA_UNUSED, Ecore_Event_Key *ev)
{
   if ((!strcmp(ev->key, "Return")) || (!strcmp(ev->key, "KP_Enter")))
     {
        _wireless_edit_send();
        return EINA_FALSE;
     }
   return !!strcmp(ev->key, "Escape");
}

static void
_wireless_gadget_edit_proxy_method_open(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int i;

   elm_hoversel_clear(obj);
   for (i = 0; i <= WIRELESS_PROXY_TYPE_AUTO; i++)
     {
        if ((int)wireless_edit[1]->proxy_type != i)
          elm_hoversel_item_add(obj, wireless_proxy_methods[i], NULL, ELM_ICON_NONE, NULL, (intptr_t*)(long)i);
     }
}

static void
_wireless_gadget_edit_proxy(void)
{
   Evas_Object *tb, *fr, *hoversel;
   int row = 0;
   Wireless_Connection *wc = wireless_edit[1];

   wireless_popup.content = tb = elm_table_add(wireless_popup.popup);
   E_FILL(tb);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_box_pack_end(wireless_popup.box, tb);

   fr = elm_frame_add(tb);
   E_EXPAND(fr);
   E_FILL(fr);
   evas_object_show(fr);
   elm_object_text_set(fr, _("Proxy Type"));
   elm_table_pack(tb, fr, 0, row++, 2, 1);

   hoversel = elm_hoversel_add(tb);
   elm_hoversel_hover_parent_set(hoversel, wireless_popup.popup);
   elm_hoversel_auto_update_set(hoversel, 1);
   evas_object_show(hoversel);
   elm_object_content_set(fr, hoversel);
   evas_object_smart_callback_add(hoversel, "selected", _wireless_gadget_edit_proxy_method, NULL);
   evas_object_smart_callback_add(hoversel, "clicked", _wireless_gadget_edit_proxy_method_open, NULL);
   elm_object_text_set(hoversel, wireless_proxy_methods[wc->proxy_type]);
   _wireless_gadget_edit_proxy_method_update();
}

static void
_wireless_gadget_edit_dnstime(void)
{
   Evas_Object *tb, *ent;
   int row = 0;

   wireless_popup.content = tb = elm_table_add(wireless_popup.popup);
   E_FILL(tb);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_box_pack_end(wireless_popup.box, tb);

   ent = _wireless_popup_table_entry_row(tb, _("Nameservers"), NULL, NULL, &row);
   elm_entry_entry_set(ent, wireless_popup.name_servers);
   evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_popup.name_servers);

   ent = _wireless_popup_table_entry_row(tb, _("Timeservers"), NULL, NULL, &row);
   elm_entry_entry_set(ent, wireless_popup.time_servers);
   evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_popup.time_servers);

   ent = _wireless_popup_table_entry_row(tb, _("Search Domains"), NULL, NULL, &row);
   elm_entry_entry_set(ent, wireless_popup.domain_servers);
   evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_popup.domain_servers);

   _wireless_popup_table_entry_row(tb, NULL, NULL, NULL, &row);
   evas_object_hide(elm_table_child_get(tb, 0, 6));
}

static Evas_Object *
_wireless_gadget_edit_basic(void)
{
   Evas_Object *tb, *fr, *hoversel, *ent, *entry;
   Eina_Bool disabled;
   int row = 0, fixed;
   const char **methods;
   Wireless_Connection *wc = wireless_edit[1];

   wireless_popup.content = tb = elm_table_add(wireless_popup.box);
   E_FILL(tb);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_box_pack_end(wireless_popup.box, tb);

   fr = elm_frame_add(tb);
   E_EXPAND(fr);
   E_FILL(fr);
   evas_object_show(fr);
   elm_object_text_set(fr, _("Method"));
   elm_table_pack(tb, fr, 0, row++, 2, 1);

   hoversel = elm_hoversel_add(tb);
   elm_hoversel_hover_parent_set(hoversel, wireless_popup.popup);
   elm_hoversel_auto_update_set(hoversel, 1);
   evas_object_show(hoversel);
   elm_object_content_set(fr, hoversel);
   evas_object_smart_callback_add(hoversel, "selected", _wireless_gadget_edit_method, NULL);
   if (wc->ipv6)
     {
        fixed = WIRELESS_NETWORK_IPV6_METHOD_FIXED;
        methods = wireless_ipv6_methods;
     }
   else
     {
        fixed = WIRELESS_NETWORK_IPV6_METHOD_FIXED;
        methods = wireless_ipv4_methods;
     }
   disabled = (int)wc->method == fixed;
   elm_object_disabled_set(hoversel, disabled);
   if (disabled)
     elm_hoversel_item_add(hoversel, _("Fixed"), NULL, ELM_ICON_NONE, NULL, NULL);
   else
     {
        elm_object_text_set(hoversel, methods[wc->method]);
        evas_object_smart_callback_add(hoversel, "clicked", _wireless_gadget_edit_method_open, NULL);
     }
   
   ent = entry = _wireless_popup_table_entry_row(tb, _("Address"), NULL, NULL, &row);
   elm_object_disabled_set(ent, disabled);
   wireless_popup.entries = eina_list_append(wireless_popup.entries, ent);
   elm_entry_entry_set(ent, wc->address);
   evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_edit[1]->address);
   if (wc->ipv6)
     {
        ent = _wireless_popup_table_entry_row(tb, _("PrefixLength"), NULL, NULL, &row);
        elm_entry_entry_set(ent, wc->ip.v6.prefixlength);
        evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_edit[1]->ip.v6.prefixlength);
     }
   else
     {
        ent = _wireless_popup_table_entry_row(tb, _("Netmask"), NULL, NULL, &row);
        elm_entry_entry_set(ent, wc->ip.v4.netmask);
        evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_edit[1]->ip.v4.netmask);
     }
   elm_object_disabled_set(ent, disabled);
   wireless_popup.entries = eina_list_append(wireless_popup.entries, ent);
   ent = _wireless_popup_table_entry_row(tb, _("Gateway"), NULL, NULL, &row);
   elm_entry_entry_set(ent, wc->gateway);
   elm_object_disabled_set(ent, disabled);
   evas_object_smart_callback_add(ent, "changed,user", _wireless_edit_entry_changed, &wireless_edit[1]->gateway);
   wireless_popup.entries = eina_list_append(wireless_popup.entries, ent);
   _wireless_edit_basic_entries_update();

   return entry;
}

static void
_wireless_gadget_edit_select_pre(void)
{
   elm_box_unpack(wireless_popup.box, wireless_popup.content);
   evas_object_del(wireless_popup.content);
   wireless_popup.entries = eina_list_free(wireless_popup.entries);
}

static void
_wireless_gadget_edit_select_basic(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _wireless_gadget_edit_select_pre();
   _wireless_gadget_edit_basic();
}

static void
_wireless_gadget_edit_select_proxy(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _wireless_gadget_edit_select_pre();
   _wireless_gadget_edit_proxy();
}

static void
_wireless_gadget_edit_select_dnstime(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   _wireless_gadget_edit_select_pre();
   _wireless_gadget_edit_dnstime();
}

static void
_wireless_gadget_edit(int type)
{
   Evas_Object *popup, *entry, *box1, *box, *list, *lbl, *bt;
   Elm_Object_Item *it;
   Eina_Bool disabled;
   int i;
   char buf[1024] = {0};
   Wireless_Connection *wc = wireless_current[type];
   Wireless_Network *wn;

   if (!wc) return;
   if (wireless_popup.popup)
     {
        evas_object_hide(wireless_popup.popup);
        evas_object_del(wireless_popup.popup);
     }
   wireless_edit[0] = E_NEW(Wireless_Connection, 1);
   wireless_edit[1] = E_NEW(Wireless_Connection, 1);
   wn = E_NEW(Wireless_Network, 1);
   wn->path = eina_stringshare_ref(wc->wn->path);
   for (i = 0; i <= 1; i++)
     {
        Eina_Array *arrays[] =
          { wc->domain_servers, wc->name_servers, wc->time_servers, wc->proxy_servers,
            wc->proxy_excludes, NULL };
        Eina_Array **arrays2[] =
          { &wireless_edit[i]->domain_servers, &wireless_edit[i]->name_servers,
            &wireless_edit[i]->time_servers, &wireless_edit[i]->proxy_servers,
            &wireless_edit[i]->proxy_excludes, NULL };
        unsigned int ii;

        wireless_edit[i]->wn = wn;
        wireless_edit[i]->method = wc->method;
        wireless_edit[i]->address = eina_stringshare_ref(wc->address);
        wireless_edit[i]->gateway = eina_stringshare_ref(wc->gateway);
        wireless_edit[i]->ipv6 = wc->ipv6;
        if (wc->ipv6)
          {
             wireless_edit[i]->ip.v6.prefixlength = eina_stringshare_ref(wc->ip.v6.prefixlength);
             wireless_edit[i]->ip.v6.privacy = wc->ip.v6.privacy;
          }
        else
          wireless_edit[i]->ip.v4.netmask = eina_stringshare_ref(wc->ip.v4.netmask);
        wireless_edit[i]->proxy_type = wc->proxy_type;
        wireless_edit[i]->proxy_url = eina_stringshare_ref(wc->proxy_url);
        /* fuuuuck thiiiiiiis */
        for (ii = 0; ii < EINA_C_ARRAY_LENGTH(arrays); ii++)
          {
             unsigned int iii;
             Eina_Stringshare *str;
             Eina_Array_Iterator itr;

             if (!arrays[ii]) continue;
             *arrays2[ii] = eina_array_new(eina_array_count(arrays[ii]));
             EINA_ARRAY_ITER_NEXT(arrays[ii], iii, str, itr)
               eina_array_push(*arrays2[ii], eina_stringshare_ref(str));
          }
     }
   _wireless_gadget_edit_array_entry(wc->domain_servers, &wireless_popup.domain_servers);
   _wireless_gadget_edit_array_entry(wc->name_servers, &wireless_popup.name_servers);
   _wireless_gadget_edit_array_entry(wc->time_servers, &wireless_popup.time_servers);
   _wireless_gadget_edit_array_entry(wc->proxy_servers, &wireless_popup.proxy_servers);
   _wireless_gadget_edit_array_entry(wc->proxy_excludes, &wireless_popup.proxy_excludes);

   wireless_popup.popup = popup = elm_popup_add(e_comp->elm);
   evas_object_layer_set(popup, E_LAYER_MENU);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   box = elm_box_add(popup);
   E_EXPAND(box);
   E_FILL(box);
   evas_object_show(box);
   elm_object_content_set(popup, box);

   lbl = elm_label_add(box);
   elm_object_style_set(lbl, "marker");
   evas_object_show(lbl);
   if (wireless_popup.type == WIRELESS_SERVICE_TYPE_ETHERNET)
     strncpy(buf, _("Edit Connection Details: <b>Ethernet</b>"), sizeof(buf) - 1);
   else
     snprintf(buf, sizeof(buf), "%s: <hilight>%s</hilight>", _("Edit Connection Details"), wc->wn->name);
   elm_object_text_set(lbl, buf);
   elm_box_pack_end(box, lbl);

   wireless_popup.box = box1 = elm_box_add(popup);
   E_EXPAND(box1);
   E_FILL(box1);
   elm_box_horizontal_set(box1, 1);
   evas_object_show(box1);
   elm_box_pack_end(box, box1);

   list = elm_list_add(box1);
   E_ALIGN(list, 0, EVAS_HINT_FILL);
   E_WEIGHT(list, 0, EVAS_HINT_EXPAND);
   elm_box_pack_end(box1, list);
   elm_list_select_mode_set(list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_scroller_content_min_limit(list, 1, 1);

   entry = _wireless_gadget_edit_basic();
   it = elm_list_item_append(list, _("Basic"), NULL, NULL, _wireless_gadget_edit_select_basic, NULL);
   elm_list_item_selected_set(it, 1);
   elm_list_item_append(list, _("Proxy"), NULL, NULL, _wireless_gadget_edit_select_proxy, NULL);
   elm_list_item_append(list, _("DNS/Time"), NULL, NULL, _wireless_gadget_edit_select_dnstime, NULL);
   elm_list_go(list);
   evas_object_show(list);

   if (wc->ipv6)
     disabled = wc->method == WIRELESS_NETWORK_IPV4_METHOD_FIXED;
   else
     disabled = wc->method == WIRELESS_NETWORK_IPV6_METHOD_FIXED;
   if (!disabled)
     {
        bt = elm_button_add(box);
        E_EXPAND(bt);
        E_FILL(bt);
        evas_object_show(bt);
        elm_object_text_set(bt, _("Deal with it"));
        evas_object_smart_callback_add(bt, "clicked", _wireless_edit_send_button, NULL);
        elm_box_pack_end(box, bt);

        bt = elm_button_add(box);
        E_EXPAND(bt);
        E_FILL(bt);
        evas_object_show(bt);
        elm_object_text_set(bt, _("Forget Network"));
        evas_object_smart_callback_add(bt, "clicked", _wireless_edit_remove, NULL);
        elm_box_pack_end(box, bt);
     }
   wireless_edit_popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(wireless_edit_popup, E_LAYER_POPUP);
   evas_object_resize(wireless_edit_popup, e_zone_current_get()->w / 3, e_zone_current_get()->h / 2);
   e_comp_object_util_center(wireless_edit_popup);
   evas_object_show(wireless_edit_popup);
   e_comp_object_util_autoclose(wireless_edit_popup, NULL, _wireless_edit_key, NULL);
   evas_object_event_callback_add(wireless_edit_popup, EVAS_CALLBACK_DEL, _wireless_edit_del, NULL);
   elm_object_focus_set(entry, 1);
}

static void
_wireless_popup_network_click(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Wireless_Network *wn = data;

   if ((wn->state == WIRELESS_NETWORK_STATE_CONNECTED) || (wn->state == WIRELESS_NETWORK_STATE_ONLINE))
     {
        int type = wireless_popup.type;

        evas_object_hide(wireless_popup.popup);
        evas_object_del(wireless_popup.popup);
        _wireless_gadget_edit(type);
     }
   else
     {
        /* FIXME */
        if (!wn->connect_cb(wn))
          {}
     }
}

static void
_wireless_popup_list_populate(void)
{
   Eina_Iterator *it;
   Wireless_Network *wn;

   if (!wireless_networks) return;
   it = eina_array_iterator_new(wireless_networks);
   EINA_ITERATOR_FOREACH(it, wn)
     {
        Evas_Object *icon;
        Elm_Object_Item *item;
        const char *name = wn->name;

        if (wn->type != wireless_popup.type) continue;
        icon = elm_layout_add(wireless_popup.content);
        e_theme_edje_object_set(icon, NULL, wireless_theme_groups[wireless_popup.type]);
        _wifi_icon_init(icon, wn, wn->type);
        if (!name)
          name = _("<SSID hidden>");
        item = elm_list_item_append(wireless_popup.content, name, icon, NULL, _wireless_popup_network_click, wn);
        eina_hash_add(wireless_popup.items, &wn, item);
     }
   eina_iterator_free(it);
}

static void
_wireless_gadget_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Instance *inst = data;
   Evas_Object *ctx, *box, *list, *toggle;
   int type;
   E_Zone *zone;
   const char *names[] =
   {
      _("Ethernet"),
      _("Wifi"),
      _("Bluetooth"),
      _("Cellular"),
   };

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (e_desklock_state_get()) return;
   if (auth_popup) return;
   for (type = 0; type < WIRELESS_SERVICE_TYPE_LAST; type++)
     if (obj == inst->icon[type])
       break;
   if (ev->button == 2) connman_technology_enabled_set(type, !wireless_type_enabled[type]);
   if (ev->button == 3) _wireless_gadget_edit(type);
   if (ev->button != 1) return;
   if (wireless_popup.popup)
     {
        evas_object_hide(wireless_popup.popup);
        evas_object_del(wireless_popup.popup);
        if (wireless_popup.type == type)
          return;
     }
   inst->popup = 1;
   wireless_popup.type = type;
   wireless_popup.items = eina_hash_pointer_new(NULL);
   ctx = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(ctx, "noblock");

   box = elm_box_add(ctx);
   E_EXPAND(box);
   E_FILL(box);

   wireless_popup.content = list = elm_list_add(ctx);
   elm_list_mode_set(list, ELM_LIST_LIMIT);
   E_EXPAND(list);
   E_FILL(list);
   _wireless_popup_list_populate();
   elm_list_go(list);
   evas_object_show(list);
   elm_box_pack_end(box, list);
   toggle = elm_check_add(ctx);
   evas_object_show(toggle);
   elm_object_style_set(toggle, "toggle");
   elm_object_text_set(toggle, names[type]);
   elm_object_part_text_set(toggle, "on", _("On"));
   elm_object_part_text_set(toggle, "off", _("Off"));
   elm_check_state_set(toggle, wireless_type_enabled[type]);
   evas_object_smart_callback_add(toggle, "changed", _wireless_popup_toggle, inst);
   elm_box_pack_end(box, toggle);
   elm_object_content_set(ctx, box);
   e_gadget_util_ctxpopup_place(inst->box, ctx, inst->icon[type]);
   wireless_popup.popup = e_comp_object_util_add(ctx, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(wireless_popup.popup, E_LAYER_POPUP);
   e_comp_object_util_autoclose(wireless_popup.popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);

   zone = e_zone_current_get();
   evas_object_resize(wireless_popup.popup, zone->w / 5, zone->h / 3);
   evas_object_size_hint_min_set(box, zone->w / 5, zone->h / 3);
   evas_object_show(wireless_popup.popup);
   evas_object_event_callback_add(wireless_popup.popup, EVAS_CALLBACK_DEL, _wireless_popup_del, inst);
}

static Evas_Object *
_wireless_tooltip_row(Evas_Object *tb, const char *label, const char *value, int row)
{
   Evas_Object *lbl;

   lbl = elm_label_add(tb);
   evas_object_show(lbl);
   E_ALIGN(lbl, 0, 0.5);
   elm_object_text_set(lbl, label);
   elm_table_pack(tb, lbl, 0, row, 1, 1);

   lbl = elm_label_add(tb);
   evas_object_show(lbl);
   E_ALIGN(lbl, 0, 0.5);
   elm_object_text_set(lbl, value);
   elm_table_pack(tb, lbl, 1, row, 1, 1);
   return lbl;
}

static const char *
_wireless_tooltip_method_name(void)
{
   if (wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->ipv6)
     return wireless_ipv6_methods[wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->method];
   return wireless_ipv4_methods[wireless_current[WIRELESS_SERVICE_TYPE_WIFI]->method];
}

static void
_wireless_tooltip_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   inst->tooltip.error = inst->tooltip.address =
     inst->tooltip.method = inst->tooltip.signal = NULL;
   inst->tooltip.type = -1;
}

static Evas_Object *
_wireless_tooltip(void *data, Evas_Object *obj EINA_UNUSED, Evas_Object *tooltip)
{
   Instance *inst = data;
   Evas_Object *tb;
   int row = 0;
   char buf[1024];
   int type = WIRELESS_SERVICE_TYPE_WIFI;

   if (!wireless_current[type])
     {
        if (!wireless_type_available[type])//connman not found
          {
             inst->tooltip.error = elm_label_add(tooltip);
             elm_object_text_set(inst->tooltip.error, _("Error: Connman not detected!"));
             evas_object_event_callback_add(inst->tooltip.error, EVAS_CALLBACK_DEL, _wireless_tooltip_del, inst);
             return inst->tooltip.error;
          }
        return NULL;
     }
   tb = elm_table_add(tooltip);
   elm_table_padding_set(tb, 5, 1);

   _wireless_tooltip_row(tb, _("Name:"), wireless_current[type]->wn->name, row++);
   inst->tooltip.method = _wireless_tooltip_row(tb, _("Method:"), _wireless_tooltip_method_name(), row++);

   inst->tooltip.address = _wireless_tooltip_row(tb, _("Address:"), wireless_current[type]->address, row++);
   snprintf(buf, sizeof(buf), "%u%%", wireless_current[type]->wn->strength);
   inst->tooltip.signal = _wireless_tooltip_row(tb, _("Signal:"), buf, row++);

   evas_object_event_callback_add(tb, EVAS_CALLBACK_DEL, _wireless_tooltip_del, inst);
   return tb;
}

static void
wireless_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   if (inst->popup)
     {
        evas_object_hide(wireless_popup.popup);
        evas_object_del(wireless_popup.popup);
     }

   instances = eina_list_remove(instances, inst);
   evas_object_hide(wireless_popup.popup);
   evas_object_del(wireless_popup.popup);
   free(inst);

   if (instances) return;
   eina_log_domain_unregister(_wireless_gadget_log_dom);
   _wireless_gadget_log_dom = -1;
}

static void
_wireless_gadget_icon_add(Instance *inst, int type)
{
   if (!inst->icon[type])
     {
        Evas_Object *g;

        inst->icon[type] = g = elm_layout_add(inst->box);
        E_EXPAND(g);
        E_FILL(g);
        e_theme_edje_object_set(g, NULL, wireless_theme_groups[type]);
        elm_object_tooltip_content_cb_set(g, _wireless_tooltip, inst, NULL);
        evas_object_event_callback_add(g, EVAS_CALLBACK_MOUSE_DOWN, _wireless_gadget_mouse_down, inst);
     }
   DBG("Updating icon for %d", type);
   _wifi_icon_init(inst->icon[type], wireless_current[type] ? wireless_current[type]->wn : NULL, type);
   evas_object_hide(inst->icon[type]);
}

static void
_wireless_gadget_refresh(Instance *inst)
{
   int type;
   int avail = 0;

   if (inst->id < 0) return;
   for (type = 0; type < WIRELESS_SERVICE_TYPE_LAST; type++)
     {
        if (wireless_type_available[type])
          _wireless_gadget_icon_add(inst, type);
        else
          {
             if (inst->tooltip.type == type)
               elm_object_tooltip_hide(inst->icon[type]);
             if (wireless_popup.type == type)
               {
                  evas_object_hide(wireless_popup.popup);
                  evas_object_del(wireless_popup.popup);
               }
             E_FREE_FUNC(inst->icon[type], evas_object_del);
          }
     }
   elm_box_unpack_all(inst->box);
   type = WIRELESS_SERVICE_TYPE_ETHERNET;
   if (inst->icon[type])
     {
        /* only show ethernet if it's connected or there's no wifi available */
        if ((!inst->icon[WIRELESS_SERVICE_TYPE_WIFI]) ||
            wireless_network_count[WIRELESS_SERVICE_TYPE_ETHERNET] ||
            (wireless_current[type] &&
             wireless_current[type]->wn &&
            (wireless_current[type]->wn->state == WIRELESS_NETWORK_STATE_ONLINE)))
          {
             elm_box_pack_end(inst->box, inst->icon[type]);
             evas_object_show(inst->icon[type]);
             avail++;
          }
     }
   for (type = WIRELESS_SERVICE_TYPE_WIFI; type < WIRELESS_SERVICE_TYPE_LAST; type++)
     {
        if (!inst->icon[type]) continue;
        if (wireless_type_enabled[type] && (!wireless_network_count[type])) continue;
        
        elm_box_pack_end(inst->box, inst->icon[type]);
        evas_object_show(inst->icon[type]);
        avail++;
     }
   if (!avail)
     {
        type = WIRELESS_SERVICE_TYPE_ETHERNET;
        _wireless_gadget_icon_add(inst, type);
        elm_box_pack_end(inst->box, inst->icon[type]);
        evas_object_show(inst->icon[type]);
        avail++;
     }
   if (inst->orient == E_GADGET_SITE_ORIENT_VERTICAL)
     evas_object_size_hint_aspect_set(inst->box, EVAS_ASPECT_CONTROL_BOTH, 1, avail);
   else
     evas_object_size_hint_aspect_set(inst->box, EVAS_ASPECT_CONTROL_BOTH, avail, 1);
}

static Evas_Object *
wireless_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Evas_Object *g;
   Instance *inst;

   if (!instances)
     _wireless_gadget_log_dom = eina_log_domain_register("wireless", EINA_COLOR_CYAN);
   inst = E_NEW(Instance, 1);
   inst->id = *id;
   inst->orient = orient;
   wireless_popup.type = inst->tooltip.type = -1;
   inst->box = elm_box_add(parent);
   elm_box_horizontal_set(inst->box, orient != E_GADGET_SITE_ORIENT_VERTICAL);
   elm_box_homogeneous_set(inst->box, 1);
   evas_object_event_callback_add(inst->box, EVAS_CALLBACK_DEL, wireless_del, inst);

   if (*id < 0)
     {
        inst->icon[WIRELESS_SERVICE_TYPE_WIFI] = g = elm_layout_add(inst->box);
        E_EXPAND(g);
        E_FILL(g);
        e_theme_edje_object_set(g, NULL, "e/gadget/wireless/wifi");
        elm_object_signal_emit(g, "e,state,default", "e");
        _wifi_icon_signal(g, WIRELESS_NETWORK_STATE_ONLINE, 100);
        evas_object_show(g);
        elm_box_pack_end(inst->box, g);
        evas_object_size_hint_aspect_set(inst->box, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
     }
   else
     _wireless_gadget_refresh(inst);
   instances = eina_list_append(instances, inst);

   return inst->box;
}

static Ecore_Event_Handler *handler;

static Eina_Bool
_wireless_mode_change()
{
   EINTERN void connman_airplane_mode_set(Eina_Bool set);

   if (wireless_offline != e_config->mode.offline)
     connman_airplane_mode_set(e_config->mode.offline);
   return ECORE_CALLBACK_RENEW;
}

EINTERN void
wireless_gadget_init(void)
{
   e_gadget_type_add("Wireless", wireless_create, NULL);
   handler = ecore_event_handler_add(E_EVENT_CONFIG_MODE_CHANGED, _wireless_mode_change, NULL);
}

EINTERN void
wireless_gadget_shutdown(void)
{
   e_gadget_type_del("Wireless");
   E_FREE_FUNC(handler, ecore_event_handler_del);
}

EINTERN void
wireless_service_type_available_set(Eina_Bool *avail)
{
   if (!memcmp(avail, &wireless_type_available, sizeof(wireless_type_available))) return;
   memcpy(&wireless_type_available, avail, WIRELESS_SERVICE_TYPE_LAST * sizeof(Eina_Bool));
   E_LIST_FOREACH(instances, _wireless_gadget_refresh);
}

EINTERN void
wireless_service_type_enabled_set(Eina_Bool *avail)
{
   if (!memcmp(avail, &wireless_type_enabled, sizeof(wireless_type_enabled))) return;
   memcpy(&wireless_type_enabled, avail, WIRELESS_SERVICE_TYPE_LAST * sizeof(Eina_Bool));
   E_LIST_FOREACH(instances, _wireless_gadget_refresh);
}

EINTERN void
wireless_wifi_current_networks_set(Wireless_Connection **current)
{
   Eina_List *l;
   Instance *inst;
   Wireless_Connection *prev[WIRELESS_SERVICE_TYPE_LAST] = {NULL};
   int type;

   memcpy(&prev, &wireless_current, WIRELESS_SERVICE_TYPE_LAST * sizeof(void*));
   memcpy(&wireless_current, current, WIRELESS_SERVICE_TYPE_LAST * sizeof(void*));
   type = wireless_popup.type;
   if ((type > -1) && wireless_popup.items)
     {
        Elm_Object_Item *it;
        Evas_Object *icon;

        if (wireless_current[type])
          {
             it = eina_hash_find(wireless_popup.items, &wireless_current[type]->wn);
             icon = elm_object_item_content_get(it);
             _wifi_icon_init(icon, wireless_current[type]->wn, type);
          }
        if (prev[type])
          {
             it = eina_hash_find(wireless_popup.items, &prev[type]->wn);
             if (it)
               {
                  icon = elm_object_item_content_get(it);
                  _wifi_icon_init(icon, prev[type]->wn, type);
               }
          }
     }
   else if ((type > -1) && wireless_popup.popup && (!wireless_current[type]))
     {
        evas_object_hide(wireless_popup.popup);
        evas_object_del(wireless_popup.popup);
     }
   EINA_LIST_FOREACH(instances, l, inst)
     {
        _wireless_gadget_refresh(inst);
        type = inst->tooltip.type;
        if (type < 0) continue;
        if (prev[type] &&
          ((!wireless_current[type]) ||
            ((wireless_current[type] != prev[type]) && (!eina_streq(wireless_current[type]->wn->name, prev[type]->wn->name)))))
          {
             elm_object_tooltip_hide(inst->icon[type]);
             continue;
          }
        if (inst->tooltip.method)
          elm_object_text_set(inst->tooltip.method, _wireless_tooltip_method_name());
        if (inst->tooltip.address)
          elm_object_text_set(inst->tooltip.address, wireless_current[type]->address);
        if (inst->tooltip.signal)
          {
             char buf[32];

             snprintf(buf, sizeof(buf), "%u%%", wireless_current[type]->wn->strength);
             elm_object_text_set(inst->tooltip.signal, buf);
          }
     }
}

static Eina_Bool
_wireless_networks_count(const void *cont EINA_UNUSED, void *data, void *fdata EINA_UNUSED)
{
   Wireless_Network *wn = data;

   wireless_network_count[wn->type]++;
   return EINA_TRUE;
}

EINTERN Eina_Array *
wireless_networks_set(Eina_Array *networks)
{
   Eina_Array *prev = wireless_networks;

   wireless_networks = networks;
   memset(&wireless_network_count, 0, sizeof(wireless_network_count));
   eina_array_foreach(networks, _wireless_networks_count, NULL);
   if (wireless_popup.popup && wireless_popup.items)
     {
        elm_list_clear(wireless_popup.content);
        eina_hash_free_buckets(wireless_popup.items);
        _wireless_popup_list_populate();
     }

   return prev;
}

EINTERN void
wireless_airplane_mode_set(Eina_Bool enabled)
{
   wireless_offline = enabled;
   if (enabled == e_config->mode.offline) return;
   e_config->mode.offline = !!enabled;
   e_config_mode_changed();
   e_config_save_queue();
}

static void
_wireless_auth_del(void *data, Evas_Object *popup)
{
   Wireless_Auth_Popup *p = data;

   if (!p->sent)
     p->cb(p->data, NULL);
   free(p);
   wireless_auth_popup = NULL;
   evas_object_hide(popup);
   evas_object_del(popup);
   if (!wireless_auth_pending) return;
   wireless_auth_popup = eina_list_data_get(wireless_auth_pending);
   wireless_auth_pending = eina_list_remove_list(wireless_auth_pending, wireless_auth_pending);
   evas_object_show(wireless_auth_popup->popup);
   e_comp_object_util_autoclose(wireless_auth_popup->popup,
     _wireless_auth_del, e_comp_object_util_autoclose_on_escape, wireless_auth_popup);
}

static void
_wireless_auth_send(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Wireless_Auth_Popup *p = data;
   Eina_Array *arr = NULL;
   Evas_Object *tb, *o;
   unsigned int row = 1;

   tb = evas_object_data_get(obj, "table");
   do
     {
        const char *txt;

        o = elm_table_child_get(tb, 0, row);
        if (!o) break;
        if (!arr) arr = eina_array_new(2);
        txt = elm_object_text_get(o);
        eina_array_push(arr, txt);
        o = elm_object_content_get(o);
        /* skip checkboxes */
        if (!strncmp(txt, "Pass", 4)) row++;
        eina_array_push(arr, elm_object_text_get(o));
        row += 2;
     } while (1);
   p->cb(p->data, arr);
   p->sent = 1;
   eina_array_free(arr);
   e_comp_object_util_autoclose(NULL, NULL, NULL, NULL);
}

static void
_wireless_auth_password_toggle(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   elm_entry_password_set(data, !elm_entry_password_get(data));
}

EINTERN void
wireless_authenticate(const Eina_Array *fields, Wireless_Auth_Cb cb, void *data)
{
   Evas_Object *popup, *tb, *lbl, *entry = NULL;
   Eina_Iterator *it;
   const char *f;
   Wireless_Auth_Popup *p;
   int row = 0;

   p = E_NEW(Wireless_Auth_Popup, 1);
   p->cb = cb;
   p->data = data;
   if (wireless_popup.popup)
     {
        evas_object_hide(wireless_popup.popup);
        evas_object_del(wireless_popup.popup);
     }

   popup = elm_popup_add(e_comp->elm);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   lbl = elm_label_add(popup);
   evas_object_show(lbl);
   elm_object_text_set(lbl, _("Authentication Required"));
   elm_table_pack(tb, lbl, 0, row++, 2, 1);

   it = eina_array_iterator_new(fields);
   EINA_ITERATOR_FOREACH(it, f)
     {
        Evas_Object *o;
        char buf[1024];
        Evas_Object *ck;

        o = _wireless_popup_table_entry_row(tb, f, _wireless_auth_send, p, &row);
        if (strncmp(f, "Pass", 4)) continue;
        if (!entry) entry = o;
        elm_entry_password_set(o, 1);

        ck = elm_check_add(tb);
        evas_object_show(ck);
        E_ALIGN(ck, 0, -1);
        snprintf(buf, sizeof(buf), _("Show %s"), f);
        evas_object_smart_callback_add(ck, "changed", _wireless_auth_password_toggle, o);
        elm_object_text_set(ck, buf);
        elm_table_pack(tb, ck, 0, row++, 2, 1);
     }
   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   p->popup = popup;
   evas_object_resize(popup, e_zone_current_get()->w / 4, e_zone_current_get()->h / 3);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   e_comp_object_util_center(popup);
   if (wireless_auth_popup)
     wireless_auth_pending = eina_list_append(wireless_auth_pending, p);
   else
     {
        wireless_auth_popup = p;
        evas_object_show(popup);
        e_comp_object_util_autoclose(popup, _wireless_auth_del,
          e_comp_object_util_autoclose_on_escape, p);
        elm_object_focus_set(entry, 1);
     }
}

EINTERN void
wireless_authenticate_cancel(void)
{
   if (!wireless_auth_popup) return;
   evas_object_hide(wireless_auth_popup->popup);
   evas_object_del(wireless_auth_popup->popup);
}

static void
_wireless_auth_external_deny(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   /* FIXME */
   free(data);
   auth_popup = 0;
}

static void
_wireless_auth_external_allow(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   char *uri = data;

   e_util_open(uri, NULL);
   DBG("launched uri: %s", uri);
   free(uri);
   auth_popup = 0;
}

EINTERN void
wireless_authenticate_external(Wireless_Network *wn, const char *url)
{
   char buf[1024];
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(instances, l, inst)
     if (wireless_popup.popup)
       {
          evas_object_hide(wireless_popup.popup);
          evas_object_del(wireless_popup.popup);
       }
   if (wn->type == WIRELESS_SERVICE_TYPE_ETHERNET)
     snprintf(buf, sizeof(buf), _("Ethernet connection wants to open a url:<br>%s"), url);
   else
     snprintf(buf, sizeof(buf), _("Network '%s' wants to open a url:<br>%s"), wn->name, url);
   EINA_LIST_FOREACH(instances, l, inst)
     {
        if (!inst->icon[wn->type]) continue;
        e_gadget_util_allow_deny_ctxpopup(inst->box, buf, _wireless_auth_external_allow, _wireless_auth_external_deny, strdup(url));
        auth_popup = 1;
        break;
     }
}
