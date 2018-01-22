#include "e.h"
#include <Efl_Wl.h>
#include "e-gadget-server-protocol.h"
#include "action_route-server-protocol.h"
#include <sched.h>

#ifdef __GNUC__
# pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#ifndef EFL_VERSION_1_21
static void efl_wl_aspect_set(Evas_Object *obj EINA_UNUSED, Eina_Bool set EINA_UNUSED)
{
}

static void efl_wl_minmax_set(Evas_Object *obj EINA_UNUSED, Eina_Bool set EINA_UNUSED)
{
}

static void *efl_wl_global_add(Evas_Object *obj EINA_UNUSED, const void *interface EINA_UNUSED, uint32_t version EINA_UNUSED, void *data EINA_UNUSED, void *bind_cb EINA_UNUSED)
{
   return NULL;
}

static Eina_Bool efl_wl_surface_extract(Evas_Object *surface EINA_UNUSED)
{
   return EINA_FALSE;
}
#endif

typedef enum
{
   EXIT_MODE_RESTART,
   EXIT_MODE_DELETE,
} Exit_Mode;

typedef struct Config_Item
{
   int id;
   int exit_mode;
   Eina_Stringshare *cmd;
   void *inst;
   Eina_Bool cmd_changed E_BITFIELD;
   Eina_Bool sandbox E_BITFIELD;
} Config_Item;

typedef struct Tooltip
{
   Evas_Object *obj;
   Evas_Object *content;
   Evas_Object *tooltip_content;
   struct wl_resource *tooltip_surface;
} Tooltip;

typedef struct Instance
{
   Evas_Object *box;
   Evas_Object *obj;
   Ecore_Exe *exe;
   Config_Item *ci;
   Eina_Hash *allowed_pids;
   Eina_List *tooltip_surfaces;
   void *gadget_resource;
   Tooltip popup, ctxpopup, base;
   Eina_List *extracted;
} Instance;

typedef struct RConfig
{
   Eina_List *items;
   Evas_Object *config_dialog;
} RConfig;

static Eina_Bool runner_enabled;

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;

static int ns_fd = -1;

static RConfig *rconfig;
static Eina_List *instances;
static Eina_List *wizards;

static Eina_Hash *sandbox_gadgets;

static Eina_List *handlers;
static Eio_Monitor *gadget_monitor;
static Eio_File *gadget_lister;

typedef struct Wizard_Item
{
   Evas_Object *site;
   Evas_Object *popup;
   E_Gadget_Wizard_End_Cb cb;
   void *data;
   int id;
   Eina_Bool sandbox E_BITFIELD;
} Wizard_Item;


static char *
sandbox_name(const char *filename)
{
   Efreet_Desktop *ed = eina_hash_find(sandbox_gadgets, filename);
   char buf[1024];

   snprintf(buf, sizeof(buf), "%s (v%s)", ed->name, (char*)eina_hash_find(ed->x, "X-Gadget-Version"));
   return strdup(buf);
}

static void
runner_run(Instance *inst)
{
   char *preload = eina_strdup(getenv("LD_PRELOAD"));
   char buf[PATH_MAX];
   int pid;

   snprintf(buf, sizeof(buf), "%s/enlightenment/gadgets/%s/loader.so", e_prefix_lib_get(), MODULE_ARCH);
   e_util_env_set("LD_PRELOAD", buf);

   snprintf(buf, sizeof(buf), "%d", inst->ci->id);
   e_util_env_set("E_GADGET_ID", buf);

   unshare(CLONE_NEWPID);

   inst->exe = efl_wl_run(inst->obj, inst->ci->cmd);

   setns(ns_fd, CLONE_NEWPID);

   e_util_env_set("E_GADGET_ID", NULL);
   e_util_env_set("LD_PRELOAD", preload);
   free(preload);
   eina_hash_free_buckets(inst->allowed_pids);
   pid = ecore_exe_pid_get(inst->exe);
   eina_hash_add(inst->allowed_pids, &pid, (void*)1);
}

static void
_config_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   Instance *inst = ci->inst;

   e_comp_ungrab_input(1, 1);
   rconfig->config_dialog = NULL;
   if (ci->cmd_changed)
     {
        char *cmd;

        cmd = elm_entry_markup_to_utf8(elm_entry_entry_get(evas_object_data_get(obj, "entry")));
        eina_stringshare_replace(&ci->cmd, cmd);
        free(cmd);
        e_config_save_queue();
     }
   if (!inst) ci->cmd_changed = 0;
   if (!ci->cmd_changed) return;
   ci->cmd_changed = 0;
   if (inst->exe) ecore_exe_quit(inst->exe);
   runner_run(inst);
}

static void
_config_label_add(Evas_Object *tb, const char *txt, int row)
{
   Evas_Object *o;

   o = elm_label_add(tb);
   E_ALIGN(o, 0, 0.5);
   elm_object_text_set(o, txt);
   evas_object_show(o);
   elm_table_pack(tb, o, 0, row, 1, 1);
}

static void
_config_cmd_changed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;

   ci->cmd_changed = 1;
}

static void
_config_cmd_activate(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Config_Item *ci = data;
   Instance *inst = ci->inst;
   char *cmd;

   ci->cmd_changed = 0;
   cmd = elm_entry_markup_to_utf8(elm_entry_entry_get(obj));
   eina_stringshare_replace(&ci->cmd, cmd);
   free(cmd);
   e_config_save_queue();
   if (!inst) return;
   if (inst->exe) ecore_exe_quit(inst->exe);
   runner_run(inst);
}

EINTERN Evas_Object *
config_runner(Config_Item *ci, E_Zone *zone)
{
   Evas_Object *popup, *tb, *o, *ent, *rg;
   int row = 0;

   if (!zone) zone = e_zone_current_get();
   popup = elm_popup_add(e_comp->elm);
   E_EXPAND(popup);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   tb = elm_table_add(popup);
   elm_table_align_set(tb, 0, 0.5);
   E_EXPAND(tb);
   evas_object_show(tb);
   elm_object_content_set(popup, tb);

   o = evas_object_rectangle_add(e_comp->evas);
   evas_object_size_hint_min_set(o, ELM_SCALE_SIZE(200), 1);
   elm_table_pack(tb, o, 0, row++, 2, 1);

   _config_label_add(tb, _("Command:"), row);
   ent = o = elm_entry_add(tb);
   E_FILL(o);
   evas_object_show(o);
   elm_entry_single_line_set(o, 1);
   elm_entry_entry_set(o, ci->cmd);
   evas_object_smart_callback_add(o, "changed,user", _config_cmd_changed, ci);
   evas_object_smart_callback_add(o, "activated", _config_cmd_activate, ci);
   elm_table_pack(tb, o, 1, row++, 1, 1);

   _config_label_add(tb, _("On Exit:"), row);
   o = rg = elm_radio_add(tb);
   E_FILL(o);
   evas_object_show(o);
   elm_object_text_set(o, _("Restart"));
   elm_radio_state_value_set(o, EXIT_MODE_RESTART);
   elm_radio_value_pointer_set(o, &ci->exit_mode);
   elm_table_pack(tb, o, 1, row++, 1, 1);

   o = elm_radio_add(tb);
   E_FILL(o);
   elm_radio_group_add(o, rg);
   evas_object_show(o);
   elm_object_text_set(o, _("Delete"));
   elm_radio_state_value_set(o, EXIT_MODE_DELETE);
   elm_table_pack(tb, o, 1, row++, 1, 1);


   popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_move(popup, zone->x, zone->y);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center(popup);
   evas_object_show(popup);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_priority_add(popup, EVAS_CALLBACK_DEL, EVAS_CALLBACK_PRIORITY_BEFORE, _config_close, ci);
   evas_object_data_set(popup, "entry", ent);
   e_comp_grab_input(1, 1);

   elm_object_focus_set(ent, 1);

   return rconfig->config_dialog = popup;
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(rconfig->items, l, ci)
          if (*id == ci->id) return ci;
     }

   ci = E_NEW(Config_Item, 1);
   if (!*id)
     *id = ci->id = rconfig->items ? eina_list_count(rconfig->items) + 1 : 1;
   else
     ci->id = *id;

   if (ci->id < 1) return ci;
   rconfig->items = eina_list_append(rconfig->items, ci);
   e_config_save_queue();

   return ci;
}

static void
wizard_site_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Wizard_Item *wi = data;
   wi->site = NULL;
   evas_object_hide(wi->popup);
   evas_object_del(wi->popup);
}

static void
_wizard_config_end(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Wizard_Item *wi = data;
   Eina_List *l;
   Config_Item *ci;

   EINA_LIST_FOREACH(rconfig->items, l, ci)
     {
        if (ci->id == wi->id)
          {
             if (ci->cmd) break;
             wi->id = 0;
             free(ci);
             rconfig->items = eina_list_remove_list(rconfig->items, l);
             break;
          }
     }

   if (wi->site)
     wi->cb(wi->data, wi->id);
   wizards = eina_list_remove(wizards, wi);
   if (wi->site)
     evas_object_event_callback_del_full(wi->site, EVAS_CALLBACK_DEL, wizard_site_del, wi);
   free(wi);
}

static Evas_Object *
runner_wizard(E_Gadget_Wizard_End_Cb cb, void *data, Evas_Object *site)
{
   int id = 0;
   Config_Item *ci;
   Wizard_Item *wi;

   wi = E_NEW(Wizard_Item, 1);
   wi->cb = cb;
   wi->data = data;
   wi->site = site;
   evas_object_event_callback_add(wi->site, EVAS_CALLBACK_DEL, wizard_site_del, wi);
   wizards = eina_list_append(wizards, wi);

   ci = _conf_item_get(&id);
   wi->id = ci->id;
   wi->popup = config_runner(ci, NULL);
   evas_object_event_callback_add(wi->popup, EVAS_CALLBACK_DEL, _wizard_config_end, wi);
   return wi->popup;
}

/////////////////////////////////////////

static void
mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   evas_object_focus_set(inst->obj, 1);
}

static void
runner_removed(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst = data;
   if (inst->box != event_info) return;
   rconfig->items = eina_list_remove(rconfig->items, inst->ci);
   eina_stringshare_del(inst->ci->cmd);
   E_FREE(inst->ci);
}

static void
runner_site_gravity(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   if (inst->gadget_resource)
     e_gadget_send_gadget_gravity(inst->gadget_resource, e_gadget_site_gravity_get(obj));
}

static void
runner_site_anchor(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   if (inst->gadget_resource)
     e_gadget_send_gadget_anchor(inst->gadget_resource, e_gadget_site_anchor_get(obj));
}

static void
runner_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Object *site = e_gadget_site_get(obj);

   evas_object_smart_callback_del_full(site, "gadget_removed", runner_removed, inst);
   evas_object_smart_callback_del_full(site, "gadget_site_anchor", runner_site_anchor, inst);
   evas_object_smart_callback_del_full(site, "gadget_site_gravity", runner_site_gravity, inst);
   if (inst->ci)
     inst->ci->inst = NULL;
   else
     ecore_exe_signal(inst->exe, 2);
   E_FREE_FUNC(inst->exe, ecore_exe_terminate);
   instances = eina_list_remove(instances, inst);
   eina_hash_free(inst->allowed_pids);
   eina_list_free(inst->tooltip_surfaces);
   free(inst);
}

static Evas_Object *
runner_gadget_configure(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "runner");
   if (inst->ci->sandbox)
     {
        if (inst->gadget_resource)
          e_gadget_send_gadget_configure(inst->gadget_resource);
        return NULL;
     }
   else
     return config_runner(inst->ci, e_comp_object_util_zone_get(g));
}

static void
runner_created(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   if (inst->box != event_info) return;
   e_gadget_configure_cb_set(inst->box, runner_gadget_configure);
   evas_object_smart_callback_del_full(obj, "gadget_created", runner_created, data);
}


static void
gadget_unbind(struct wl_resource *resource)
{
   Instance *inst = wl_resource_get_user_data(resource);
   inst->gadget_resource = NULL;
}

static void
gadget_open_uri(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, const char *uri)
{
   //Instance *inst = wl_resource_get_user_data(resource);

   /* FIXME: rate limit? */
   e_util_open(uri, NULL);
}

static void
gadget_set_tooltip(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *surface)
{
   Instance *inst = wl_resource_get_user_data(resource);

   inst->tooltip_surfaces = eina_list_append(inst->tooltip_surfaces, surface);
}

static const struct e_gadget_interface _gadget_interface =
{
   .open_uri = gadget_open_uri,
   .set_tooltip = gadget_set_tooltip,
};

static void
gadget_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   struct wl_resource *res;
   Instance *inst = data;
   pid_t pid;
   Evas_Object *site;

   wl_client_get_credentials(client, &pid, NULL, NULL);
   if (pid != ecore_exe_pid_get(inst->exe))
     {
        wl_client_post_no_memory(client);
        return;
     }

   res = wl_resource_create(client, &e_gadget_interface, version, id);
   wl_resource_set_implementation(res, &_gadget_interface, data, gadget_unbind);
   inst->gadget_resource = res;
   site = e_gadget_site_get(inst->box);
   e_gadget_send_gadget_orient(res, e_gadget_site_orient_get(site));
   e_gadget_send_gadget_gravity(res, e_gadget_site_gravity_get(site));
   e_gadget_send_gadget_anchor(res, e_gadget_site_anchor_get(site));
}

static void
ar_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   struct wl_resource *res;
   Instance *inst = data;
   int v;
   const void *ar_interface;
   pid_t pid;

   wl_client_get_credentials(client, &pid, NULL, NULL);
   if (pid != ecore_exe_pid_get(inst->exe))
     {
        wl_client_post_no_memory(client);
        return;
     }
   ar_interface = e_comp_wl_extension_action_route_interface_get(&v);

   if (!(res = wl_resource_create(client, &action_route_interface, MIN(v, version), id)))
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, ar_interface, inst->allowed_pids, NULL);
}

static void
child_close(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Object *ext;

   inst->popup.obj = NULL;
   inst->popup.content = NULL;
   ext = evas_object_data_get(obj, "extracted");
   elm_box_unpack_all(obj);
   inst->extracted = eina_list_remove(inst->extracted, ext);
   evas_object_hide(ext);
}

static void
child_added(void *data, Evas_Object *obj, void *event_info)
{
   Evas_Object *popup, *bx;
   E_Zone *zone = e_comp_object_util_zone_get(obj);
   Instance *inst = data;

   if (!efl_wl_surface_extract(event_info)) return;
   inst->extracted = eina_list_append(inst->extracted, event_info);
   inst->popup.content = event_info;

   popup = elm_popup_add(e_comp->elm);
   e_comp_object_util_del_list_append(event_info, popup);
   E_EXPAND(popup);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   elm_popup_allow_events_set(popup, 1);
   elm_popup_scrollable_set(popup, 1);

   bx = elm_box_add(popup);
   E_EXPAND(event_info);
   E_FILL(event_info);
   elm_box_homogeneous_set(bx, 1);
   evas_object_show(bx);
   evas_object_show(event_info);
   elm_box_pack_end(bx, event_info);
   elm_object_content_set(popup, bx);

   inst->popup.obj = popup = e_comp_object_util_add(popup, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(popup, E_LAYER_POPUP);
   evas_object_move(popup, zone->x, zone->y);
   evas_object_resize(popup, zone->w / 4, zone->h / 3);
   e_comp_object_util_center(popup);
   evas_object_show(popup);
   e_comp_canvas_feed_mouse_up(0);
   e_comp_object_util_autoclose(popup, NULL, e_comp_object_util_autoclose_on_escape, NULL);
   evas_object_event_callback_add(bx, EVAS_CALLBACK_DEL, child_close, inst);
   evas_object_data_set(bx, "extracted", event_info);
   evas_object_focus_set(event_info, 1);
}

static void
extracted_del(Instance *inst, Evas_Object *ext)
{
   inst->extracted = eina_list_remove(inst->extracted, ext);
   evas_object_hide(ext);
}

static void
popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Object *ext;

   inst->ctxpopup.obj = NULL;
   inst->ctxpopup.content = NULL;
   ext = evas_object_data_get(obj, "extracted");
   elm_box_unpack_all(obj);
   extracted_del(inst, ext);
}

static void
popup_dismissed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_del(obj);
}

static void
popup_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   elm_ctxpopup_dismiss(inst->ctxpopup.obj);
   evas_object_del(elm_object_content_get(inst->ctxpopup.obj));
}

static void
popup_hints_update(Evas_Object *obj)
{
   double w, h;
   E_Zone *zone = e_comp_object_util_zone_get(obj);

   evas_object_size_hint_weight_get(obj, &w, &h);
   w = E_CLAMP(w, 0, 0.5);
   h = E_CLAMP(h, 0, 0.5);

   if ((w > 0) && (h > 0))
     {
        evas_object_size_hint_min_set(obj, w * zone->w, h * zone->h);
        evas_object_size_hint_max_set(obj, w * zone->w, h * zone->h);
     }
   if ((!EINA_DBL_NONZERO(w)) && (!EINA_DBL_NONZERO(h)))
     {
        int ww, hh;
        evas_object_geometry_get(obj, NULL, NULL, &ww, &hh);
        evas_object_size_hint_min_set(obj, ww, hh);
     }
   E_WEIGHT(obj, 0, 0);
}

static void
popup_hints(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_event_callback_del_full(obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS, popup_hints, data);
   popup_hints_update(obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS, popup_hints, data);
}

static void
tooltip_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Tooltip *tt = data;
   Instance *inst = evas_object_data_get(tt->tooltip_content, "instance");

   tt->tooltip_content = NULL;
   inst->tooltip_surfaces = eina_list_remove(inst->tooltip_surfaces, tt->tooltip_surface);
   tt->tooltip_surface = NULL;
   elm_object_tooltip_unset(tt->obj);
   extracted_del(inst, obj);
}

static void
tooltip_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Tooltip *tt = data;

   elm_box_unpack_all(obj);
   if (tt->tooltip_content) evas_object_hide(tt->tooltip_content);
   tt->tooltip_content = NULL;
   tt->tooltip_surface = NULL;
}

static void
tooltip_hints(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int w, h;
   evas_object_size_hint_aspect_get(obj, NULL, &w, &h);
   evas_object_size_hint_min_set(obj, w, h);
}

static Evas_Object *
tooltip_content_cb(void *data, Evas_Object *obj EINA_UNUSED, Evas_Object *tooltip)
{
   Evas_Object *bx;
   Tooltip *tt = data;

   bx = elm_box_add(tooltip);
   evas_object_pass_events_set(bx, 1);
   evas_object_event_callback_add(bx, EVAS_CALLBACK_DEL, tooltip_hide, tt);
   elm_box_pack_end(bx, tt->tooltip_content);
   evas_object_show(tt->tooltip_content);
   elm_box_recalculate(bx);
   evas_object_show(bx);
   return bx;
}

static void
popup_added(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst = data;
   Evas_Object *bx;

   if (!efl_wl_surface_extract(event_info)) return;
   inst->extracted = eina_list_append(inst->extracted, event_info);
   if (inst->tooltip_surfaces)
     {
        Eina_List *l;
        struct wl_resource *surface;

        EINA_LIST_FOREACH(inst->tooltip_surfaces, l, surface)
          if (event_info == efl_wl_extracted_surface_object_find(surface))
            {
               Evas_Object *base = efl_wl_extracted_surface_extracted_parent_get(event_info);
               Tooltip *tt;

               //FIXME: if (inst->tooltip_content) error
               if (base)
                 {
                    if (base == inst->popup.content)
                      tt = &inst->popup;
                    else if (base == inst->ctxpopup.content)
                      tt = &inst->ctxpopup;
                 }
               else // base tooltip
                 tt = &inst->base;
               if (tt)
                 {
                    tt->tooltip_surface = surface;
                    tt->tooltip_content = event_info;
                    evas_object_data_set(event_info, "instance", inst);
                    evas_object_pass_events_set(event_info, 1);
                    E_FILL(event_info);
                    tooltip_hints(NULL, NULL, event_info, NULL);
                    evas_object_event_callback_add(event_info, EVAS_CALLBACK_DEL, tooltip_del, tt);
                    evas_object_event_callback_add(event_info, EVAS_CALLBACK_CHANGED_SIZE_HINTS, tooltip_hints, tt);
                    elm_object_tooltip_content_cb_set(tt->obj, tooltip_content_cb, tt, NULL);
                 }
               else
                 {
                    inst->extracted = eina_list_remove(inst->extracted, event_info);
                    evas_object_hide(event_info);
                 }
               return;
            }
     }

   //FIXME: if (inst->ctxpopup.obj) error

   inst->ctxpopup.obj = elm_ctxpopup_add(inst->box);
   inst->ctxpopup.content = event_info;
   elm_object_style_set(inst->ctxpopup.obj, "noblock");
   evas_object_smart_callback_add(inst->ctxpopup.obj, "dismissed", popup_dismissed, inst);
   evas_object_event_callback_add(event_info, EVAS_CALLBACK_DEL, popup_hide, inst);

   bx = elm_box_add(inst->ctxpopup.obj);
   popup_hints_update(event_info);
   E_FILL(event_info);
   evas_object_event_callback_add(event_info, EVAS_CALLBACK_CHANGED_SIZE_HINTS, popup_hints, inst);
   evas_object_show(bx);
   elm_box_pack_end(bx, event_info);
   elm_box_recalculate(bx);
   evas_object_show(event_info);
   evas_object_data_set(bx, "extracted", event_info);
   evas_object_event_callback_add(bx, EVAS_CALLBACK_DEL, popup_del, inst);
   elm_object_content_set(inst->ctxpopup.obj, bx);

   e_gadget_util_ctxpopup_place(inst->box, inst->ctxpopup.obj, NULL);
   evas_object_show(inst->ctxpopup.obj);
   evas_object_focus_set(event_info, 1);
}

static void
runner_hints(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   int w, h;
   Evas_Aspect_Control aspect;

   evas_object_size_hint_min_get(obj, &w, &h);
   evas_object_size_hint_min_set(inst->box, w, h);
   evas_object_size_hint_max_get(obj, &w, &h);
   evas_object_size_hint_max_set(inst->box, w, h);
   evas_object_size_hint_aspect_get(obj, &aspect, &w, &h);
   evas_object_size_hint_aspect_set(inst->box, aspect, w, h);
}

static void
runner_menu_bugreport(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   char *url = data;
   e_util_open(url, NULL);
   free(url);
}

static void
runner_menu(void *data, Evas_Object *obj, void *event_info)
{
   E_Menu *m = event_info;
   Instance *inst = data;

   if (inst->ci->sandbox)
     {
        E_Menu_Item *mi;
        E_Menu *subm;
        Efreet_Desktop *ed = eina_hash_find(sandbox_gadgets, e_gadget_type_get(obj));
        char buf[1024];

        e_menu_title_set(m, ed->name);

        subm = e_menu_new();
        snprintf(buf, sizeof(buf), _("Version: %s"), (char*)eina_hash_find(ed->x, "X-Gadget-Version"));
        e_menu_title_set(subm, buf);

        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, _("Details"));
        e_menu_item_submenu_set(mi, subm);
        e_object_unref(E_OBJECT(subm));

        mi = e_menu_item_new(subm);
        snprintf(buf, sizeof(buf), "PID: %u", ecore_exe_pid_get(inst->exe));
        e_menu_item_label_set(mi, buf);
        e_menu_item_disabled_set(mi, 1);

        mi = e_menu_item_new(subm);
        e_menu_item_label_set(mi, _("Report bug"));
        e_util_menu_item_theme_icon_set(mi, "dialog-error");
        e_menu_item_callback_set(mi, runner_menu_bugreport, eina_strdup(eina_hash_find(ed->x, "X-Gadget-Bugreport")));
     }
   else
     {
        char buf[1024], *p;

        strncpy(buf, inst->ci->cmd, sizeof(buf) - 1);
        p = strchr(buf, ' ');
        if (p) p[0] = 0;
        e_menu_title_set(m, ecore_file_file_get(buf));
     }
}

static Evas_Object *
gadget_create(Evas_Object *parent, Config_Item *ci, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;
   int ar_version;

   inst = E_NEW(Instance, 1);
   instances = eina_list_append(instances, inst);
   inst->ci = ci;
   if (!inst->ci)
     inst->ci = _conf_item_get(id);
   inst->ci->inst = inst;
   inst->allowed_pids = eina_hash_int32_new(NULL);
   inst->obj = efl_wl_add(e_comp->evas);
   E_EXPAND(inst->obj);
   E_FILL(inst->obj);
   evas_object_show(inst->obj);
   efl_wl_aspect_set(inst->obj, 1);
   efl_wl_minmax_set(inst->obj, 1);
   efl_wl_global_add(inst->obj, &e_gadget_interface, 1, inst, gadget_bind);
   evas_object_smart_callback_add(inst->obj, "child_added", child_added, inst);
   evas_object_smart_callback_add(inst->obj, "popup_added", popup_added, inst);
   e_comp_wl_extension_action_route_interface_get(&ar_version);
   efl_wl_global_add(inst->obj, &action_route_interface, ar_version, inst, ar_bind);
   evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_MOUSE_DOWN, mouse_down, inst);
   evas_object_smart_callback_add(parent, "gadget_created", runner_created, inst);
   evas_object_smart_callback_add(parent, "gadget_removed", runner_removed, inst);
   evas_object_smart_callback_add(parent, "gadget_site_anchor", runner_site_anchor, inst);
   evas_object_smart_callback_add(parent, "gadget_site_gravity", runner_site_gravity, inst);
   runner_run(inst);
   ecore_exe_data_set(inst->exe, inst);
   inst->base.obj = inst->box = elm_box_add(e_comp->elm);
   evas_object_data_set(inst->box, "runner", inst);
   evas_object_smart_callback_add(inst->box, "gadget_menu", runner_menu, inst);
   evas_object_event_callback_add(inst->box, EVAS_CALLBACK_DEL, runner_del, inst);
   evas_object_event_callback_add(inst->obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS, runner_hints, inst);
   elm_box_homogeneous_set(inst->box, 1);
   elm_box_pack_end(inst->box, inst->obj);
   return inst->box;
}

static Evas_Object *
runner_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Evas_Object *obj;
   Config_Item *ci = NULL;

   if (orient) return NULL;
   if (*id > 0) ci = _conf_item_get(id);
   if ((*id < 0) || ci->inst)
     {
        obj = elm_image_add(parent);
        elm_image_file_set(obj, e_theme_edje_file_get(NULL, "e/icons/modules-launcher"), "e/icons/modules-launcher");
        evas_object_size_hint_aspect_set(obj, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
        return obj;
     }
   return gadget_create(parent, ci, id, orient);
}

static Eina_Bool
runner_exe_del(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Exe_Event_Del *ev)
{
   Instance *inst = ecore_exe_data_get(ev->exe);

   if ((!inst) || (!instances) || (!eina_list_data_find(instances, inst))) return ECORE_CALLBACK_RENEW;
   switch (inst->ci->exit_mode)
     {
      case EXIT_MODE_RESTART:
        /* FIXME: probably notify? */
        if (ev->exit_code == 255) //exec error
          e_gadget_del(inst->box);
        else
          {
             runner_run(inst);
             ecore_exe_data_set(inst->exe, inst);
          }
        break;
      case EXIT_MODE_DELETE:
        e_gadget_del(inst->box);
        break;
     }
   return ECORE_CALLBACK_RENEW;
}

///////////////////////////////

static Evas_Object *
sandbox_create(Evas_Object *parent, const char *type, int *id, E_Gadget_Site_Orient orient)
{
   Efreet_Desktop *ed = eina_hash_find(sandbox_gadgets, type);
   Config_Item *ci = NULL;

   if (*id > 0) ci = _conf_item_get(id);
   if ((*id < 0) || (ci && ci->inst))
     {
        if (ed->x)
          {
             const char *orients = eina_hash_find(ed->x, "X-Gadget-Orientations");

             if (orients)
               {
                  const char *ostring[] =
                  {
                     "None",
                     "Horizontal",
                     "Vertical",
                  };
                  const char *v, *val = orients;
                  Eina_Bool found = EINA_FALSE;

                  v = strchr(val, ';');
                  do
                    {
                       if (v)
                         {
                            if (!memcmp(val, ostring[orient], v - val))
                              {
                                 found = EINA_TRUE;
                                 break;
                              }
                            val = v + 1;
                            v = strchr(val, ';');
                         }
                       else
                         {
                            if (!strcmp(val, ostring[orient]))
                              found = EINA_TRUE;
                            break;
                         }
                    } while (val[0]);
                  if (!found) return NULL;
               }
          }
        if (ed->icon)
          {
             int w, h;
             Eina_Bool fail = EINA_FALSE;
             Evas_Object *obj;

             obj = elm_image_add(parent);
             if (ed->icon[0] == '/')
               {
                  if (eina_str_has_extension(ed->icon, ".edj"))
                    fail = !elm_image_file_set(obj, ed->icon, "icon");
                  else
                    fail = !elm_image_file_set(obj, ed->icon, NULL);
               }
             else
               {
                  if (!elm_image_file_set(obj, e_theme_edje_file_get(NULL, ed->icon), ed->icon))
                    fail = !elm_icon_standard_set(obj, ed->icon);
               }
             if (!fail)
               {
                  elm_image_object_size_get(obj, &w, &h);
                  if (w && h)
                    evas_object_size_hint_aspect_set(obj, EVAS_ASPECT_CONTROL_BOTH, w, h);
                  return obj;
               }
             evas_object_del(obj);
          }
     }
   if (!ci)
     {
        ci = _conf_item_get(id);
        ci->cmd = eina_stringshare_add(ed->exec);
        ci->exit_mode = EXIT_MODE_RESTART;
     }
   ci->sandbox = 1;
   return gadget_create(parent, ci, id, orient);
}

///////////////////////////////

static void
gadget_dir_add(const char *filename)
{
   const char *file;
   char buf[PATH_MAX];
   Efreet_Desktop *ed;

   file = ecore_file_file_get(filename);
   snprintf(buf, sizeof(buf), "%s/%s.desktop", filename, file);
   ed = efreet_desktop_new(buf);
   EINA_SAFETY_ON_NULL_RETURN(ed);
   if (!ed->name)
     {
        char str[4096];
        snprintf(str, sizeof(str), _("A gadget .desktop file was found,</ps>"
                                     "but no [Name] entry was specified!</ps>"
                                     "%s"), buf);
        /* FIXME: maybe don't use notification here? T6630 */
        e_notification_util_send(_("Gadget Error"), str);
        efreet_desktop_free(ed);
        return;
     }
   if (ed->type != EFREET_DESKTOP_TYPE_APPLICATION)
     {
        char str[4096];
        snprintf(str, sizeof(str), _("A gadget .desktop file was found,</ps>"
                                     "but [Type] is not set to Application!</ps>"
                                     "%s"), buf);
        /* FIXME: maybe don't use notification here? T6630 */
        e_notification_util_send(_("Gadget Error"), str);
        efreet_desktop_free(ed);
        return;
     }
   if ((!ed->x) || (!eina_hash_find(ed->x, "X-Gadget-Version")))
     {
        char str[4096];
        snprintf(str, sizeof(str), _("A gadget .desktop file was found,</ps>"
                                     "but [X-Gadget-Version] is missing!</ps>"
                                     "%s"), buf);
        /* FIXME: maybe don't use notification here? T6630 */
        e_notification_util_send(_("Gadget Error"), str);
        efreet_desktop_free(ed);
        return;
     }
   if (!eina_hash_find(ed->x, "X-Gadget-Bugreport"))
     {
        char str[4096];
        snprintf(str, sizeof(str), _("A gadget .desktop file was found,</ps>"
                                     "but [X-Gadget-Bugreport] is missing!</ps>"
                                     "%s"), buf);
        /* FIXME: maybe don't use notification here? T6630 */
        e_notification_util_send(_("Gadget Error"), str);
        efreet_desktop_free(ed);
        return;
     }
   eina_hash_add(sandbox_gadgets, filename, ed);
   e_gadget_external_type_add("runner_sandbox", filename, sandbox_create, NULL);
   e_gadget_external_type_name_cb_set("runner_sandbox", filename, sandbox_name);
}

static Eina_Bool
monitor_dir_create(void *d EINA_UNUSED, int t EINA_UNUSED, Eio_Monitor_Event *ev)
{
   if (!eina_hash_find(sandbox_gadgets, ev->filename))
     gadget_dir_add(ev->filename);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
monitor_dir_del(void *d EINA_UNUSED, int t EINA_UNUSED, Eio_Monitor_Event *ev)
{
   eina_hash_del_by_key(sandbox_gadgets, ev->filename);
   e_gadget_external_type_del("runner_sandbox", ev->filename);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
monitor_error(void *d EINA_UNUSED, int t EINA_UNUSED, Eio_Monitor_Error *ev EINA_UNUSED)
{
   /* panic? */
   return ECORE_CALLBACK_RENEW;
}


static Eina_Bool
list_filter_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED, const Eina_File_Direct_Info *info)
{
   struct stat st;
   char buf[PATH_MAX];

   if (info->type != EINA_FILE_DIR) return EINA_FALSE;
   if (info->path[info->name_start] == '.') return EINA_FALSE;
   snprintf(buf, sizeof(buf), "%s/%s.desktop", info->path, info->path + info->name_start);
   return !stat(info->path, &st);
}

static void
list_main_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED, const Eina_File_Direct_Info *info)
{
   gadget_dir_add(info->path);
}

static void
list_done_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED)
{
   gadget_lister = NULL;
}

static void
list_error_cb(void *d EINA_UNUSED, Eio_File *ls EINA_UNUSED, int error EINA_UNUSED)
{
   gadget_lister = NULL;
}

EINTERN void
e_gadget_runner_init(void)
{
   char buf[PATH_MAX];

   snprintf(buf, sizeof(buf), "%s/enlightenment/gadgets/%s/loader.so", e_prefix_lib_get(), MODULE_ARCH);
   if (!ecore_file_exists(buf)) return;
   runner_enabled = EINA_TRUE;
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, exit_mode, INT);
   E_CONFIG_VAL(D, T, cmd, STR);

   conf_edd = E_CONFIG_DD_NEW("RConfig", RConfig);
#undef T
#undef D
#define T RConfig
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   rconfig = e_config_domain_load("e_gadget_runner", conf_edd);
   if (!rconfig) rconfig = E_NEW(RConfig, 1);

   e_gadget_type_add("runner", runner_create, runner_wizard);
   {
      snprintf(buf, sizeof(buf), "%s/enlightenment/gadgets/%s", e_prefix_lib_get(), MODULE_ARCH);
      gadget_monitor = eio_monitor_add(buf);
      gadget_lister = eio_file_direct_ls(buf, list_filter_cb, list_main_cb, list_done_cb, list_error_cb, NULL);
   }
   E_LIST_HANDLER_APPEND(handlers, ECORE_EXE_EVENT_DEL, runner_exe_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_DIRECTORY_CREATED, monitor_dir_create, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_DIRECTORY_DELETED, monitor_dir_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, EIO_MONITOR_ERROR, monitor_error, NULL);

   sandbox_gadgets = eina_hash_string_superfast_new((Eina_Free_Cb)efreet_desktop_free);
   {
      snprintf(buf, sizeof(buf), "/proc/%d/ns/pid", getpid());
      ns_fd = open(buf, O_RDONLY);
   }
}

EINTERN void
e_gadget_runner_shutdown(void)
{
   if (!runner_enabled) return;
   e_gadget_type_del("runner");
   e_gadget_external_type_del("runner_sandbox", NULL);

   if (rconfig)
     {
        Config_Item *ci;

        if (rconfig->config_dialog)
          {
             evas_object_hide(rconfig->config_dialog);
             evas_object_del(rconfig->config_dialog);
          }

        EINA_LIST_FREE(rconfig->items, ci)
          {
             eina_stringshare_del(ci->cmd);
             free(ci);
          }

     }
   E_FREE(rconfig);
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FREE_FUNC(sandbox_gadgets, eina_hash_free);
   E_FREE_FUNC(gadget_lister, eio_file_cancel);
   close(ns_fd);
   ns_fd = -1;
}

EINTERN void
e_gadget_runner_save(void)
{
   if (!runner_enabled) return;
   e_config_domain_save("e_gadget_runner", conf_edd, rconfig);
}
