#include "e_mod_main.h"

static const char _Name[] = "Systray";
static const char _name[] = "systray";
static const char _sig_source[] = "e";

E_Module *systray_mod = NULL;
static char tmpbuf[4096]; /* general purpose buffer, just use immediately */

#define SYSTRAY_MIN_W 16
#define SYSTRAY_MIN_H 8

static const char *
_systray_theme_path(void)
{
#define TF "/e-module-systray.edj"
   unsigned int dirlen;
   const char *moddir = e_module_dir_get(systray_mod);

   dirlen = strlen(moddir);
   if (dirlen >= sizeof(tmpbuf) - sizeof(TF))
     return NULL;

   memcpy(tmpbuf, moddir, dirlen);
   memcpy(tmpbuf + dirlen, TF, sizeof(TF));

   return tmpbuf;
#undef TF
}

static void
_systray_menu_new(Instance *inst, Evas_Event_Mouse_Down *ev)
{
   E_Zone *zone;
   E_Menu *m;
   //E_Menu_Item *mi;
   int x, y;

   zone = e_zone_current_get();

   m = e_menu_new();
   //mi = e_menu_item_new(m);
   //e_menu_item_label_set(mi, _("Settings"));
   //e_util_menu_item_theme_icon_set(mi, "configure");
   //e_menu_item_callback_set(mi, _cb_menu_cfg, inst);
   m = e_gadcon_client_util_menu_items_append(inst->gcc, m, 0);
   e_gadcon_canvas_zone_geometry_get(inst->gcc->gadcon, &x, &y, NULL, NULL);
   e_menu_activate_mouse(m, zone, x + ev->output.x, y + ev->output.y,
                         1, 1, E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
   evas_event_feed_mouse_up(e_comp->evas, ev->button,
                            EVAS_BUTTON_NONE, ev->timestamp, NULL);
}

static void
_systray_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->button == 3)
     _systray_menu_new(inst, ev);
}

void
_hack_get_me_the_correct_min_size(Edje_Object *obj)
{
   Evas_Coord w, h;
   E_Gadcon_Client *client;

   client = evas_object_data_get(obj, "gadcon");
   evas_object_size_hint_min_get(obj, &w, &h);
   e_gadcon_client_min_size_set(client, MAX(w, SYSTRAY_MIN_W), MAX(h, SYSTRAY_MIN_H));
}


static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   Instance *inst;
   Evas_Object *content;
   // fprintf(stderr, "SYSTRAY: init name=%s, id=%s, style=%s\n", name, id, style);

   inst = E_NEW(Instance, 1);
   if (!inst)
     return NULL;

   inst->ui.gadget = content = systray_notifier_host_new(gc->shelf ? gc->shelf->style : NULL, style);

   inst->gcc = e_gadcon_client_new(gc, name, id, style, content);

   if (!inst->gcc)
     {
        evas_object_del(inst->ui.gadget);
        E_FREE(inst);
        return NULL;
     }

   evas_object_data_set(content, "gadcon", inst->gcc);

   e_gadcon_client_min_size_set(inst->gcc, SYSTRAY_MIN_W, SYSTRAY_MIN_H);

   inst->gcc->data = inst;

   evas_object_event_callback_add(inst->ui.gadget, EVAS_CALLBACK_MOUSE_DOWN,
                                  _systray_cb_mouse_down, inst);

   return inst->gcc;
}

/* Called when Gadget_Container says stop */
static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   Instance *inst = gcc->data;

   // fprintf(stderr, "SYSTRAY: shutdown %p, inst=%p\n", gcc, inst);

   if (!inst)
     return;
   evas_object_del(inst->ui.gadget);

   if (inst->job.size_apply)
     ecore_job_del(inst->job.size_apply);

   E_FREE(inst);
   gcc->data = NULL;
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   Instance *inst = gcc->data;
   const char *sig;

   if (!inst)
     return;

   switch (orient)
     {
      case E_GADCON_ORIENT_FLOAT:
        sig = "e,action,orient,float";
        break;

      case E_GADCON_ORIENT_HORIZ:
        sig = "e,action,orient,horiz";
        break;

      case E_GADCON_ORIENT_VERT:
        sig = "e,action,orient,vert";
        break;

      case E_GADCON_ORIENT_LEFT:
        sig = "e,action,orient,left";
        break;

      case E_GADCON_ORIENT_RIGHT:
        sig = "e,action,orient,right";
        break;

      case E_GADCON_ORIENT_TOP:
        sig = "e,action,orient,top";
        break;

      case E_GADCON_ORIENT_BOTTOM:
        sig = "e,action,orient,bottom";
        break;

      case E_GADCON_ORIENT_CORNER_TL:
        sig = "e,action,orient,corner_tl";
        break;

      case E_GADCON_ORIENT_CORNER_TR:
        sig = "e,action,orient,corner_tr";
        break;

      case E_GADCON_ORIENT_CORNER_BL:
        sig = "e,action,orient,corner_bl";
        break;

      case E_GADCON_ORIENT_CORNER_BR:
        sig = "e,action,orient,corner_br";
        break;

      case E_GADCON_ORIENT_CORNER_LT:
        sig = "e,action,orient,corner_lt";
        break;

      case E_GADCON_ORIENT_CORNER_RT:
        sig = "e,action,orient,corner_rt";
        break;

      case E_GADCON_ORIENT_CORNER_LB:
        sig = "e,action,orient,corner_lb";
        break;

      case E_GADCON_ORIENT_CORNER_RB:
        sig = "e,action,orient,corner_rb";
        break;

      default:
        sig = "e,action,orient,horiz";
     }

   edje_object_signal_emit(inst->ui.gadget, sig, _sig_source);
   edje_object_message_signal_process(inst->ui.gadget);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _("Systray");
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;

   o = edje_object_add(evas);
   edje_object_file_set(o, _systray_theme_path(), "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
{
   return _name;
}

static const E_Gadcon_Client_Class _gc_class =
{
   GADCON_CLIENT_CLASS_VERSION, _name,
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      NULL
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

E_API E_Module_Api e_modapi = {E_MODULE_API_VERSION, _Name};

E_API void *
e_modapi_init(E_Module *m)
{
   systray_mod = m;

   e_gadcon_provider_register(&_gc_class);

   systray_notifier_host_init();

   //start the watcher service
   char buf[PATH_MAX];

   snprintf(buf, sizeof(buf), "%s/%s/watcher", e_module_dir_get(m), MODULE_ARCH);

   if (!ecore_exe_run(buf, NULL))
     {
        printf("Starting watcher failed\n");
     }

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   e_gadcon_provider_unregister(&_gc_class);
   systray_mod = NULL;

   systray_notifier_host_shutdown();

   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   //e_config_domain_save(_name, ctx->conf_edd, ctx->config);
   return 1;
}