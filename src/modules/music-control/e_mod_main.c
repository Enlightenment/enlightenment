#include "private.h"

static E_Module *music_control_mod = NULL;

static char tmpbuf[4096]; /* general purpose buffer, just use immediately */

static const char _e_music_control_Name[] = "Music controller";


const char *
music_control_edj_path_get(void)
{
#define TF "/e-module-music-control.edj"
   size_t dirlen;

   dirlen = strlen(music_control_mod->dir);
   if (dirlen >= sizeof(tmpbuf) - sizeof(TF))
     return NULL;

   memcpy(tmpbuf, music_control_mod->dir, dirlen);
   memcpy(tmpbuf + dirlen, TF, sizeof(TF));

   return tmpbuf;
#undef TF
}

/* Gadcon Api Functions */
static E_Gadcon_Client *
_gc_init(E_Gadcon *gc, const char *name, const char *id, const char *style)
{
   E_Music_Control_Instance *inst;
   E_Music_Control_Module_Context *ctxt;

   EINA_SAFETY_ON_NULL_RETURN_VAL(music_control_mod, NULL);
   ctxt = music_control_mod->data;

   inst = calloc(1, sizeof(E_Music_Control_Instance));
   inst->ctxt = ctxt;
   inst->gadget = edje_object_add(gc->evas);
   edje_object_file_set(inst->gadget, music_control_edj_path_get(),
                        "modules/music-control/main");
   /*e_theme_edje_object_set(inst->gadget, "base/theme/modules/music-control",
                             "e/modules/music-control/main");
   TODO append theme to data/themes/default.edc*/

   inst->gcc = e_gadcon_client_new(gc, name, id, style, inst->gadget);
   inst->gcc->data = inst;

   evas_object_event_callback_add(inst->gadget, EVAS_CALLBACK_MOUSE_DOWN, music_control_mouse_down_cb, inst);

   ctxt->instances = eina_list_append(ctxt->instances, inst);

   return inst->gcc;
}

static void
_gc_shutdown(E_Gadcon_Client *gcc)
{
   E_Music_Control_Instance *inst;
   E_Music_Control_Module_Context *ctxt;

   EINA_SAFETY_ON_NULL_RETURN(music_control_mod);

   ctxt = music_control_mod->data;
   inst = gcc->data;

   evas_object_del(inst->gadget);
   if (inst->popup) music_control_popup_del(inst);
   ctxt->instances = eina_list_remove(ctxt->instances, inst);

   free(inst);
}

static void
_gc_orient(E_Gadcon_Client *gcc, E_Gadcon_Orient orient)
{
   e_gadcon_client_aspect_set(gcc, 16, 16);
   e_gadcon_client_min_size_set(gcc, 16, 16);
}

static const char *
_gc_label(const E_Gadcon_Client_Class *client_class)
{
   return _e_music_control_Name;
}

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class, Evas *evas)
{
   Evas_Object *o = edje_object_add(evas);
   edje_object_file_set(o, music_control_edj_path_get(), "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class)
{
   E_Music_Control_Module_Context *ctxt;
   EINA_SAFETY_ON_NULL_RETURN_VAL(music_control_mod, NULL);
   ctxt = music_control_mod->data;

   snprintf(tmpbuf, sizeof(tmpbuf), "music-control.%d",
            eina_list_count(ctxt->instances));
   return tmpbuf;
}

static const E_Gadcon_Client_Class _gc_class =
{
   GADCON_CLIENT_CLASS_VERSION, "music-control",
   {
      _gc_init, _gc_shutdown, _gc_orient, _gc_label, _gc_icon, _gc_id_new, NULL,
      e_gadcon_site_is_not_toolbar
   },
   E_GADCON_CLIENT_STYLE_PLAIN
};

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, _e_music_control_Name };

static void
cb_playback_status_get(void *data, EDBus_Pending *p, const char *propname, EDBus_Proxy *proxy, EDBus_Error_Info *error_info, const char *value)
{
   E_Music_Control_Module_Context *ctxt = data;

   if (error_info)
     {
        ERR("%s %s", error_info->error, error_info->message);
        return;
     }

   if (!strcmp(value, "Playing"))
     ctxt->playning = EINA_TRUE;
   else
     ctxt->playning = EINA_FALSE;
   music_control_state_update_all(ctxt);
}

static void
prop_changed(void *data, EDBus_Proxy *proxy, void *event_info)
{
   EDBus_Proxy_Event_Property_Changed *event = event_info;
   E_Music_Control_Module_Context *ctxt = data;

   if (!strcmp(event->name, "PlaybackStatus"))
     {
        const Eina_Value *value = event->value;
        const char *status;

        eina_value_get(value, &status);
        if (!strcmp(status, "Playing"))
          ctxt->playning = EINA_TRUE;
        else
          ctxt->playning = EINA_FALSE;
        music_control_state_update_all(ctxt);
     }
}

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Music_Control_Module_Context *ctxt;

   ctxt = calloc(1, sizeof(E_Music_Control_Module_Context));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctxt, NULL);
   edbus_init();
   ctxt->conn = edbus_connection_get(EDBUS_CONNECTION_TYPE_SESSION);
   EINA_SAFETY_ON_NULL_GOTO(ctxt->conn, error_dbus_bus_get);
   ctxt->mrpis2 = mpris_media_player2_proxy_get(ctxt->conn, "org.mpris.MediaPlayer2.gmusicbrowser", NULL);
   ctxt->mpris2_player = media_player2_player_proxy_get(ctxt->conn, "org.mpris.MediaPlayer2.gmusicbrowser", NULL);
   media_player2_player_playback_status_propget(ctxt->mpris2_player, cb_playback_status_get, ctxt);
   edbus_proxy_event_callback_add(ctxt->mpris2_player, EDBUS_PROXY_EVENT_PROPERTY_CHANGED, prop_changed, ctxt);
   music_control_mod = m;

   e_gadcon_provider_register(&_gc_class);

   return ctxt;

error_dbus_bus_get:
   free(ctxt);
   return NULL;
}

EAPI int
e_modapi_shutdown(E_Module *m)
{
   E_Music_Control_Module_Context *ctxt;
   EINA_SAFETY_ON_NULL_RETURN_VAL(music_control_mod, 0);
   ctxt = music_control_mod->data;

   media_player2_player_proxy_unref(ctxt->mpris2_player);
   mpris_media_player2_proxy_unref(ctxt->mrpis2);
   edbus_connection_unref(ctxt->conn);
   edbus_shutdown();

   e_gadcon_provider_unregister(&_gc_class);

   if (eina_list_count(ctxt->instances))
     ERR("Live instances.");

   free(ctxt);
   music_control_mod = NULL;

   return 1;
}

EAPI int
e_modapi_save(E_Module *m)
{
   E_Music_Control_Module_Context *ctxt;

   ctxt = m->data;
   if (!ctxt)
     return 0;
   return 1;
}
