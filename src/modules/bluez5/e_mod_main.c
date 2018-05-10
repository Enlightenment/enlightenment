#include "e_mod_main.h"

static Eina_List *instances = NULL;
static E_Module *mod = NULL;

/* Local config */
static E_Config_DD *conf_edd = NULL;
Config *ebluez5_config = NULL;

E_API E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Bluez5"};

static void
_mod_icon_set(Evas_Object *base, Eina_Bool gadget)
{
   char edj_path[4096], *group;

   // XXX: hack for now until we make the icon do things and have it
   // in theme in efl
   snprintf(edj_path, sizeof(edj_path), "%s/e-module-bluez5.edj", mod->dir);
   if (1) group = "e/modules/bluez5/main";
   else group = "e/modules/bluez5/inactive";

   if (!e_theme_edje_object_set(base, "base/theme/modules/bluez5", group))
     {
        if (gadget)
          elm_layout_file_set(base, edj_path, group);
        else
          edje_object_file_set(base, edj_path, group);
     }
}

/////////////////////////////////////////////////////////////////////////////

static void
_gad_popup_dismiss(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);
   inst->pop = NULL;
}

static void
_gad_popup_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->pop = NULL;
}

static void
_gad_popup_do(Instance *inst)
{
   Evas_Object *o;

   if (inst->pop) return;

   inst->pop = o = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(o, "noblock");
   evas_object_smart_callback_add(o, "dismissed", _gad_popup_dismiss, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _gad_popup_del, inst);

   inst->popcontent = o = ebluez5_popup_content_add(e_comp->elm, inst);
   elm_object_content_set(inst->pop, o);
   evas_object_show(o);

   e_gadget_util_ctxpopup_place(inst->o_bluez5, inst->pop, inst->o_bluez5);
   evas_object_show(inst->pop);
}

static void
_gad_mouse_down(void *data, Evas *evas EINA_UNUSED,
                Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 1) return;
   if (!inst->pop) _gad_popup_do(inst);
   else elm_ctxpopup_dismiss(inst->pop);
}

static void
_gad_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   instances = eina_list_remove(instances, inst);
   E_FREE(inst);
}

/* XXX: fill in later when we have gotten this far
static Evas_Object *
_gad_config(Evas_Object *g EINA_UNUSED)
{
   if (e_configure_registry_exists("extensions/bluez5"))
     e_configure_registry_call("extensions/bluez5", NULL, NULL);
   return NULL;
}
*/

static Evas_Object *
_gad_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient)
{
   Evas_Object *o;
   Instance *inst = E_NEW(Instance, 1);

   if (!inst) return NULL;
   inst->id = *id;
   inst->orient = orient;
   inst->o_bluez5 = o = elm_layout_add(parent);
   _mod_icon_set(o, EINA_TRUE);
   evas_object_size_hint_aspect_set(o, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
// XXX: fill in later when we have gotten this far   
//   e_gadget_configure_cb_set(o, _gad_config);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN, _gad_mouse_down, inst);
   if (*id != -1)
     evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _gad_del, inst);
   instances = eina_list_append(instances, inst);
   return o;
}

/////////////////////////////////////////////////////////////////////////////

static void
_popup_del(Instance *inst)
{
   E_FREE_FUNC(inst->popup, e_object_del);
}

static void
_popup_del_cb(void *obj)
{
   _popup_del(e_object_data_get(obj));
}

static void
_popup_comp_del_cb(void *data, Evas_Object *obj EINA_UNUSED)
{
   _popup_del(data);
}

static void
_popup_new(Instance *inst)
{
   inst->popup = e_gadcon_popup_new(inst->gcc, 0);

   e_gadcon_popup_content_set(inst->popup, ebluez5_popup_content_add(e_comp->elm, inst));
   e_comp_object_util_autoclose(inst->popup->comp_object, _popup_comp_del_cb, NULL, inst);
   e_gadcon_popup_show(inst->popup);
   e_object_data_set(E_OBJECT(inst->popup), inst);
   E_OBJECT_DEL_SET(inst->popup, _popup_del_cb);
}

static void
_ebluez5_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 1) return;
   if (!inst->popup) _popup_new(inst);
   else _popup_del(inst);
}

static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Evas_Object *o;
   Instance *inst = E_NEW(Instance, 1);

   if (!inst) return NULL;
   inst->o_bluez5 = o = edje_object_add(gc->evas);
   _mod_icon_set(o, EINA_FALSE);
   inst->gcc = e_gadcon_client_new(gc, name, id, style, o);
   inst->gcc->data = inst;
   e_gadcon_client_util_menu_attach(inst->gcc);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN, _ebluez5_cb_mouse_down, inst);
   instances = eina_list_append(instances, inst);
   return inst->gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst = gcc->data;;

   if (!inst) return;
   instances = eina_list_remove(instances, inst);
   _popup_del(inst);
   E_FREE_FUNC(inst->o_bluez5, evas_object_del);
   E_FREE(inst);
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   static char tmpbuf[128];
   snprintf(tmpbuf, sizeof(tmpbuf), "bluez5.%d", eina_list_count(instances));
   return tmpbuf;
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
   return _("Bluez5");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o = NULL;
   char buf[4096];

   snprintf(buf, sizeof(buf), "%s/e-module-bluez5.edj", mod->dir);
   o = edje_object_add(evas);
   edje_object_file_set(o, buf, "icon");
   return o;
}

static const E_Gadcon_Client_Class _gc_class = {
   GADCON_CLIENT_CLASS_VERSION, "bluez5",
     {_gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL, NULL},
   E_GADCON_CLIENT_STYLE_PLAIN
};

/////////////////////////////////////////////////////////////////////////////
void
ebluez5_popups_show(void)
{
   Eina_List *l;
   Instance *inst;

   EINA_LIST_FOREACH(instances, l, inst)
     {
        if (inst->gcc)
          {
             if (!inst->popup) _popup_new(inst);
          }
        else
          {
             if (!inst->pop) _gad_popup_do(inst);
          }
     }
}

/////////////////////////////////////////////////////////////////////////////

/* Module Functions */
E_API void *
e_modapi_init(E_Module *m)
{
   mod = m;

   conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_VAL(D, T, lock_dev_addr, STR);
   E_CONFIG_VAL(D, T, unlock_dev_addr, STR);

   ebluez5_config = e_config_domain_load("module.ebluez5", conf_edd);
   if (!ebluez5_config) ebluez5_config = E_NEW(Config, 1);

   ebluze5_popup_init();
   bz_init();

   e_gadcon_provider_register(&_gc_class);
   e_gadget_type_add("Bluetooth", _gad_create, NULL);

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   E_CONFIG_DD_FREE(conf_edd);

   eina_stringshare_del(ebluez5_config->lock_dev_addr);
   eina_stringshare_del(ebluez5_config->unlock_dev_addr);
   free(ebluez5_config);
   ebluez5_config = NULL;

   bz_shutdown();
   ebluze5_popup_shutdown();

   e_gadget_type_del("Bluetooth");
   e_gadcon_provider_unregister(&_gc_class);
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.ebluez5", conf_edd, ebluez5_config);
   return 1;
}
