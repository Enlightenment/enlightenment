#include "e_mod_main.h"

static Elm_Genlist_Item_Class *adapt_itc = NULL;
static Elm_Genlist_Item_Class *dev_itc = NULL;
static Elm_Genlist_Item_Class *group_itc = NULL;
static Eina_List *lists = NULL;
static Eina_List *adapters = NULL;
static Eina_List *devices = NULL;

static void
_adapter_add(Evas_Object *gl, Obj *o)
{
   Eina_List *l;
   Config_Adapter *ad;
   Elm_Object_Item *it = evas_object_data_get(gl, "adapters_item");;
   elm_genlist_item_append(gl, adapt_itc, o, it, ELM_GENLIST_ITEM_NONE,
                            NULL, NULL);
   if ((ebluez5_config) && (o->address))
     {
        EINA_LIST_FOREACH(ebluez5_config->adapters, l, ad)
          {
             if (!ad->addr) continue;
             if (!strcmp(ad->addr, o->address))
               {
                  if (ad->powered) bz_obj_power_on(o);
                  else bz_obj_power_off(o);
                  if (ad->pairable) bz_obj_pairable(o);
                  else bz_obj_unpairable(o);
               }
          }
     }
}

static int
_cb_insert_cmp(const void *ai, const void *bi)
{
   Obj *a = elm_object_item_data_get(ai);
   Obj *b = elm_object_item_data_get(bi);
   Eina_Bool apub = EINA_FALSE, bpub = EINA_FALSE;

   if ((!a) || (!a->address)) return -1;
   if ((!b) || (!b->address)) return 1;

   // prefer paired at top
   if ((a->paired) && (!b->paired)) return -1;
   if ((!a->paired) && (b->paired)) return 1;
   // prefer public addresses next after being paired
   if ((a->address_type) && (!strcmp(a->address_type, "public")))
     apub = EINA_TRUE;
   if ((b->address_type) && (!strcmp(b->address_type, "public")))
     bpub = EINA_TRUE;
   if ((apub) && (!bpub)) return -1;
   if ((!apub) && (bpub)) return 1;
   // and sort by address
   return strcmp(a->address, b->address);
}

static void
_device_add(Evas_Object *gl, Obj *o)
{
   Elm_Object_Item *it = evas_object_data_get(gl, "devices_item");;

   elm_genlist_item_sorted_insert(gl, dev_itc, o, it, ELM_GENLIST_ITEM_NONE,
                                  _cb_insert_cmp, NULL, NULL);
}

static void
_cb_power(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   if (elm_check_state_get(obj))
     {
        if (o->path)
          {
             const char *s = strrchr(o->path, '/');

             if (s) ebluez5_rfkill_unblock(s + 1);
          }
        bz_obj_power_on(o);
     }
   else bz_obj_power_off(o);
}

static void
_cb_scan(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   if (elm_check_state_get(obj)) bz_obj_discover_start(o);
   else bz_obj_discover_stop(o);
}

static void
_cb_visible(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   if (elm_check_state_get(obj)) bz_obj_discoverable(o);
   else bz_obj_undiscoverable(o);
}

static void
_cb_pairable(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   if (elm_check_state_get(obj)) bz_obj_pairable(o);
   else bz_obj_unpairable(o);
}

static void
_cb_connect(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   bz_obj_connect(o);
}

static void
_cb_disconnect(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   bz_obj_disconnect(o);
}

static void
_cb_trust(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   bz_obj_trust(o);
}

static void
_cb_distrust(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   bz_obj_distrust(o);
}

static void
_cb_pair(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   bz_obj_pair(o);
}

static void
_cb_unpair(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   bz_obj_remove(o);
}

static void
_agent_done(Obj *o)
{
   Eina_List *l;
   Evas_Object *gl;
   Elm_Object_Item *it;

   if (o->agent_request)
     {
        eina_stringshare_del(o->agent_request);
        o->agent_request = NULL;
     }
   EINA_LIST_FOREACH(lists, l, gl)
     {
        for (it = elm_genlist_first_item_get(gl); it;
             it = elm_genlist_item_next_get(it))
          {
             if (o == elm_object_item_data_get(it))
               {
                  elm_genlist_item_update(it);
                  break;
               }
          }
     }
}

static void
_cb_agent_ok(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Obj *o = data;

   if ((o->agent_entry_fn) && (o->agent_msg_ok))
     {
        Evas_Object *en = evas_object_data_get(obj, "entry");

        if (en)
          {
             const char *s = elm_object_text_get(en);

             if (s) o->agent_entry_fn(o->agent_msg_ok, s);
          }
     }
   if (o->agent_msg_err)
     {
        bz_agent_msg_drop(o->agent_msg_err);
        o->agent_msg_err = NULL;
     }
   if (o->agent_msg_ok)
     {
        bz_agent_msg_reply(o->agent_msg_ok);
        o->agent_msg_ok = NULL;
     }
   _agent_done(o);
}

static void
_cb_agent_cancel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Obj *o = data;
   if (o->agent_msg_ok)
     {
        bz_agent_msg_drop(o->agent_msg_ok);
        o->agent_msg_ok = NULL;
     }
   if (o->agent_msg_err)
     {
        bz_agent_msg_reply(o->agent_msg_err);
        o->agent_msg_err = NULL;
     }
   _agent_done(o);
}

static char *
_cb_group_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                   const char *part EINA_UNUSED)
{
   if (!data) return strdup(_("Adapters"));
   return strdup(_("Devices"));
}

static Evas_Object *
_cb_group_content_get(void *data EINA_UNUSED, Evas_Object *obj,
                      const char *part EINA_UNUSED)
{
   if (!strcmp(part, "elm.swallow.icon"))
     {
        Evas_Object *ic = elm_icon_add(obj);
        if (!data)
          elm_icon_standard_set(ic, "computer");
        else
          elm_icon_standard_set(ic, "system-run");
        evas_object_size_hint_min_set(ic,
                                      ELM_SCALE_SIZE(16),
                                      ELM_SCALE_SIZE(16));
        return ic;
     }
   return NULL;
}

static char *
_cb_adapt_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                   const char *part EINA_UNUSED)
{
   Obj *o = data;

   if (!strcmp(part, "elm.text"))
     {
        return strdup(util_obj_name_get(o));
     }
   else if (!strcmp(part, "elm.text.sub"))
     {
        if (o->address) return strdup(o->address);
        return strdup(_("Unknown Address"));
     }
   return NULL;
}

static Evas_Object *
_cb_adapt_content_get(void *data EINA_UNUSED, Evas_Object *obj,
                      const char *part EINA_UNUSED)
{
   Obj *o = data;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        return util_obj_icon_add(obj, o, 48);
     }
   else if (!strcmp(part, "elm.swallow.end"))
     {
        Evas_Object *tab, *ck;

        tab = elm_table_add(obj);
        evas_object_size_hint_weight_set(tab, EVAS_HINT_EXPAND, 0);
        evas_object_size_hint_align_set(tab, EVAS_HINT_FILL, 0.0);

        ck = util_check_add(obj, _("Power"), _("Enable power for this adapter"),
                            o->powered);
        evas_object_smart_callback_add(ck, "changed", _cb_power, o);
        elm_table_pack(tab, ck, 0, 0, 1, 1);
        evas_object_show(ck);

        ck = util_check_add(obj, _("Visible"), _("Make this adapter visible to other devices"),
                            o->discoverable);
        evas_object_smart_callback_add(ck, "changed", _cb_visible, o);
        elm_table_pack(tab, ck, 1, 0, 1, 1);
        evas_object_show(ck);

        ck = util_check_add(obj, _("Scan"), _("Scan for other devices"),
                            o->discovering);
        evas_object_smart_callback_add(ck, "changed", _cb_scan, o);
        elm_table_pack(tab, ck, 0, 1, 1, 1);
        evas_object_show(ck);

        ck = util_check_add(obj, _("Pairable"), _("Allow this adapter to have other devices request to pair with it"),
                            o->pairable);
        evas_object_smart_callback_add(ck, "changed", _cb_pairable, o);
        elm_table_pack(tab, ck, 1, 1, 1, 1);
        evas_object_show(ck);

        return tab;
     }
   return NULL;
}

static char *
_cb_dev_text_get(void *data, Evas_Object *obj EINA_UNUSED,
                 const char *part EINA_UNUSED)
{
   Obj *o = data;
   return strdup(util_obj_name_get(o));
}

static Evas_Object *
_cb_dev_content_get(void *data EINA_UNUSED, Evas_Object *obj,
                    const char *part EINA_UNUSED)
{
   Obj *o = data;
   char buf[512];

   if (!strcmp(part, "elm.swallow.icon"))
     {
        Evas_Object *ic, *bx;

        bx = elm_box_add(obj);
        ic = util_obj_icon_add(obj, o, 24);
        snprintf(buf, sizeof(buf),
                 _("Address: %s (%s)<br>"
                   "Services: %s%s%s%s%s%s%s%s%s<br>"
                   "Trusted: %s<br>"
                   "Blocked: %s<br>"
                     )
                 ,
                 (o->address) ? o->address : _("Unknown"),
                 (o->address_type) ? o->address_type : _("Unknown"),
                 (o->klass & BZ_OBJ_CLASS_SERVICE_LIMITED_DISCOVERABLE) ? _("Limited-Discoverable ") : "",
                 (o->klass & BZ_OBJ_CLASS_SERVICE_POSITIONING_BIT) ? _("Positioning ") : "",
                 (o->klass & BZ_OBJ_CLASS_SERVICE_NETWORKING_BIT) ? _("Networking ") : "",
                 (o->klass & BZ_OBJ_CLASS_SERVICE_RENDERING_BIT) ? _("Rendering ") : "",
                 (o->klass & BZ_OBJ_CLASS_SERVICE_CAPTURING_BIT) ? _("Capture ") : "",
                 (o->klass & BZ_OBJ_CLASS_SERVICE_OBJECT_TRANSFER_BIT) ? _("OBEX ") : "",
                 (o->klass & BZ_OBJ_CLASS_SERVICE_AUDIO_BIT) ? _("Audio ") : "",
                 (o->klass & BZ_OBJ_CLASS_SERVICE_TELEPHONY_BIT) ? _("Telephony ") : "",
                 (o->klass & BZ_OBJ_CLASS_SERVICE_INFORMATION_BIT) ? _("Information ") : "",
                 (o->trusted) ? _("Yes") : _("No"),
                 (o->blocked) ? _("Yes") : _("No")
                );
        elm_object_tooltip_text_set(ic, buf);
        elm_box_pack_end(bx, ic);
        evas_object_show(ic);
        return bx;
     }
   else if (!strcmp(part, "elm.swallow.end"))
     {
        Evas_Object *bx, *ic, *bt, *lb, *tb, *en, *rec;

        bx = elm_box_add(obj);
        elm_box_horizontal_set(bx, EINA_TRUE);
        if (o->paired)
          {
             if (o->connected)
               {
                  bt = util_button_icon_add(obj, "network-offline",
                                            _("Disconnect this device"));
                  evas_object_smart_callback_add(bt, "clicked", _cb_disconnect, o);
               }
             else
               {
                  bt = util_button_icon_add(obj, "network-transmit-receive",
                                            _("Connect this device"));
                  evas_object_smart_callback_add(bt, "clicked", _cb_connect, o);
               }
             elm_box_pack_end(bx, bt);
             evas_object_show(bt);

             if (o->trusted)
               {
                  bt = util_button_icon_add(obj, "security-low",
                                            _("Disrust this device"));
                  evas_object_smart_callback_add(bt, "clicked", _cb_distrust, o);
               }
             else
               {
                  bt = util_button_icon_add(obj, "security-high",
                                            _("Trust this device"));
                  evas_object_smart_callback_add(bt, "clicked", _cb_trust, o);
               }
             elm_box_pack_end(bx, bt);
             evas_object_show(bt);
          }
        if (!o->paired)
          {
             if (o->agent_request)
               {
                  if (o->agent_entry_fn)
                    {
                       tb = elm_table_add(obj);

                       rec = evas_object_rectangle_add(evas_object_evas_get(obj));
                       evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(80), ELM_SCALE_SIZE(1));
                       elm_table_pack(tb, rec, 0, 0, 1, 1);

                       en = elm_entry_add(obj);
                       elm_entry_single_line_set(en, EINA_TRUE);
                       elm_entry_scrollable_set(en, EINA_TRUE);
                       elm_scroller_policy_set(en, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
                       elm_object_part_text_set(en, "guide", o->agent_request);
//                       elm_entry_password_set(en, EINA_TRUE);
                       evas_object_smart_callback_add(en, "activated", _cb_agent_ok, o);
                       evas_object_smart_callback_add(en, "aborted", _cb_agent_cancel, o);
                       elm_table_pack(tb, en, 0, 0, 1, 1);
                       evas_object_show(en);

                       elm_box_pack_end(bx, tb);
                       evas_object_show(tb);

                       bt = util_button_icon_add(obj, "list-add",
                                                 _("Pair with this device"));
                       evas_object_data_set(bt, "entry", en);
                       evas_object_smart_callback_add(bt, "clicked", _cb_agent_ok, o);
                       elm_box_pack_end(bx, bt);
                       evas_object_show(bt);

                       bt = util_button_icon_add(obj, "list-remove",
                                                 _("Reject pairing"));
                       evas_object_smart_callback_add(bt, "clicked", _cb_agent_cancel, o);
                       elm_box_pack_end(bx, bt);
                       evas_object_show(bt);
                    }
                  else
                    {
                       lb = elm_label_add(obj);
                       elm_layout_text_set(lb, NULL, o->agent_request);
                       elm_box_pack_end(bx, lb);
                       evas_object_show(lb);

                       bt = util_button_icon_add(obj, "list-add",
                                                 _("Pair with this device"));
                       evas_object_smart_callback_add(bt, "clicked", _cb_agent_ok, o);
                       elm_box_pack_end(bx, bt);
                       evas_object_show(bt);

                       bt = util_button_icon_add(obj, "list-remove",
                                                 _("Reject pairing"));
                       evas_object_smart_callback_add(bt, "clicked", _cb_agent_cancel, o);
                       elm_box_pack_end(bx, bt);
                       evas_object_show(bt);
                    }
               }
             else
               {
                  bt = util_button_icon_add(obj, "list-add",
                                            _("Pair with this device"));
                  evas_object_smart_callback_add(bt, "clicked", _cb_pair, o);
                  elm_box_pack_end(bx, bt);
                  evas_object_show(bt);
               }
          }
        else
          {
             bt = util_button_icon_add(obj, "list-remove",
                                        _("Unpair with this device"));
             evas_object_smart_callback_add(bt, "clicked", _cb_unpair, o);
             elm_box_pack_end(bx, bt);
             evas_object_show(bt);
          }

        ic = util_obj_icon_rssi_add(obj, o, 24);
        elm_box_pack_end(bx, ic);
        evas_object_show(ic);
        return bx;
     }
   return NULL;
}

static void
_cb_list_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   lists = eina_list_remove(lists, obj);
}

/*
static void
_cb_settings(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   ebluez5_popup_hide(inst);
   if (e_configure_registry_exists("extensions/bluez5"))
      e_configure_registry_call("extensions/bluez5", NULL, NULL);
}
*/

Evas_Object *
ebluez5_popup_content_add(Evas_Object *base, Instance *inst)
{
   Evas_Object *o, *box, *tab, *gl;
   Eina_List *l;
   Elm_Object_Item *it;
   Obj *oo;

   o = box = elm_box_add(base);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   tab = o = elm_table_add(base);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);

   o = evas_object_rectangle_add(evas_object_evas_get(base));
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(320), ELM_SCALE_SIZE(240));
   evas_object_size_hint_max_set(o, ELM_SCALE_SIZE(560), ELM_SCALE_SIZE(400));
   elm_table_pack(tab, o, 0, 0, 1, 1);

   o = gl = elm_genlist_add(base);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_genlist_mode_set(o, ELM_LIST_LIMIT);
   elm_genlist_select_mode_set(o, ELM_OBJECT_SELECT_MODE_NONE);

   lists = eina_list_append(lists, gl);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _cb_list_del, inst);

   it = elm_genlist_item_append(gl, group_itc, NULL, NULL,
                                ELM_GENLIST_ITEM_GROUP, NULL, NULL);
   evas_object_data_set(gl, "adapters_item", it);
   it = elm_genlist_item_append(gl, group_itc, gl, NULL,
                                ELM_GENLIST_ITEM_GROUP, NULL, NULL);
   evas_object_data_set(gl, "devices_item", it);

   EINA_LIST_FOREACH(adapters, l, oo)
     {
        _adapter_add(gl, oo);
     }
   EINA_LIST_FOREACH(devices, l, oo)
     {
        _device_add(gl, oo);
     }

   elm_table_pack(tab, o, 0, 0, 1, 1);
   evas_object_show(o);

   elm_box_pack_end(box, tab);
   evas_object_show(tab);
/*
   o = elm_separator_add(base);
   elm_separator_horizontal_set(o, EINA_TRUE);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, o);
   evas_object_show(o);

   o = elm_button_add(base);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_layout_text_set(o, NULL, _("Settings"));
   elm_object_tooltip_text_set(o, _("Bring up more detailed Bluetooth settings"));
   evas_object_smart_callback_add(o, "clicked", _cb_settings, inst);
   elm_box_pack_end(box, o);
   evas_object_show(o);
 */
   return box;
}

void
ebluze5_popup_init(void)
{
   adapt_itc = elm_genlist_item_class_new();
   adapt_itc->item_style = "double_label";
   adapt_itc->func.text_get = _cb_adapt_text_get;
   adapt_itc->func.content_get = _cb_adapt_content_get;
   adapt_itc->func.state_get = NULL;
   adapt_itc->func.del = NULL;

   dev_itc = elm_genlist_item_class_new();
   dev_itc->item_style = "default";
   dev_itc->func.text_get = _cb_dev_text_get;
   dev_itc->func.content_get = _cb_dev_content_get;
   dev_itc->func.state_get = NULL;
   dev_itc->func.del = NULL;

   group_itc = elm_genlist_item_class_new();
   group_itc->item_style = "group_index";
   group_itc->func.text_get = _cb_group_text_get;
   group_itc->func.content_get = _cb_group_content_get;
   group_itc->func.state_get = NULL;
   group_itc->func.del = NULL;
}

void
ebluze5_popup_shutdown(void)
{
   ebluze5_popup_clear();
   elm_genlist_item_class_free(group_itc);
   elm_genlist_item_class_free(dev_itc);
   elm_genlist_item_class_free(adapt_itc);
   group_itc = NULL;
   dev_itc = NULL;
   adapt_itc = NULL;
}

void
ebluze5_popup_clear(void)
{
   Eina_List *l;
   Evas_Object *gl;

   adapters = eina_list_free(adapters);
   devices = eina_list_free(devices);
   EINA_LIST_FOREACH(lists, l, gl)
     {
        elm_genlist_clear(gl);
     }
}

void
ebluez5_popup_adapter_add(Obj *o)
{
   Eina_List *l;
   Evas_Object *gl;

   adapters = eina_list_append(adapters, o);
   EINA_LIST_FOREACH(lists, l, gl)
     {
        _adapter_add(gl, o);
     }
}

void
ebluez5_popup_adapter_del(Obj *o)
{
   Eina_List *l;
   Evas_Object *gl;
   Elm_Object_Item *it;

   EINA_LIST_FOREACH(lists, l, gl)
     {
        for (it = elm_genlist_first_item_get(gl); it;
             it = elm_genlist_item_next_get(it))
          {
             if (o == elm_object_item_data_get(it))
               {
                  elm_object_item_del(it);
                  break;
               }
          }
     }
   adapters = eina_list_remove(adapters, o);
}

void
ebluez5_popup_adapter_change(Obj *o)
{
   Eina_List *l;
   Evas_Object *gl;
   Elm_Object_Item *it;

   EINA_LIST_FOREACH(lists, l, gl)
     {
        for (it = elm_genlist_first_item_get(gl); it;
             it = elm_genlist_item_next_get(it))
          {
             if (o == elm_object_item_data_get(it))
               {
                  elm_genlist_item_update(it);
                  if (o->address)
                    ebluez5_conf_adapter_add(o->address, o->powered,
                                             o->pairable);
                  break;
               }
          }
     }
}

void
ebluez5_popup_device_add(Obj *o)
{
   Eina_List *l;
   Evas_Object *gl;

   devices = eina_list_append(devices, o);
   EINA_LIST_FOREACH(lists, l, gl)
     {
        _device_add(gl, o);
     }
}

void
ebluez5_popup_device_del(Obj *o)
{
   Eina_List *l;
   Evas_Object *gl;
   Elm_Object_Item *it;

   EINA_LIST_FOREACH(lists, l, gl)
     {
        for (it = elm_genlist_first_item_get(gl); it;
             it = elm_genlist_item_next_get(it))
          {
             if (o == elm_object_item_data_get(it))
               {
                  elm_object_item_del(it);
                  break;
               }
          }
     }
   devices = eina_list_remove(devices, o);
}

void
ebluez5_popup_device_change(Obj *o)
{
   Eina_List *l;
   Evas_Object *gl;
   Elm_Object_Item *it;
   Eina_Bool alert = EINA_FALSE;

   if (o->agent_alert)
     {
        alert = EINA_TRUE;
        o->agent_alert = EINA_FALSE;
        if (!lists) ebluez5_popups_show();
     }
   EINA_LIST_FOREACH(lists, l, gl)
     {
        for (it = elm_genlist_first_item_get(gl); it;
             it = elm_genlist_item_next_get(it))
          {
             if (o == elm_object_item_data_get(it))
               {
                  // if this is a change for the obj for an "initial"
                  // request to the user for ok/pin etc. then change it
                  // and do the updates
                  if (alert)
                    {
                       elm_genlist_item_update(it);
                       elm_genlist_item_show(it, ELM_GENLIST_ITEM_SCROLLTO_MIDDLE);
                    }
                  // otherwise only do changes to items with no agent
                  // requests pending so we dont delete focused widgets
                  // or entry widgets etc.
                  else if (!o->agent_request)
                    elm_genlist_item_update(it);
                  break;
               }
          }
     }
}
