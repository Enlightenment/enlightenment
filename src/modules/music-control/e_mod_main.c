#include "private.h"

#define MUSIC_CONTROL_DOMAIN "module.music_control"

static E_Module *music_control_mod = NULL;

static Eina_Bool was_playing_before_lock = EINA_FALSE;

static const char _e_music_control_Name[] = N_("Music controller");

const Player music_player_players[] =
{
   {"gmusicbrowser", "org.mpris.MediaPlayer2.gmusicbrowser"},
   {"Banshee", "org.mpris.MediaPlayer2.banshee"},
   {"Clementine", "org.mpris.MediaPlayer2.clementine"},
   {"Audacious", "org.mpris.MediaPlayer2.audacious"},
   {"VLC", "org.mpris.MediaPlayer2.vlc"},
   {"BMP", "org.mpris.MediaPlayer2.bmp"},
   {"XMMS2", "org.mpris.MediaPlayer2.xmms2"},
   {"DeaDBeeF", "org.mpris.MediaPlayer2.deadbeef"},
   {"Rhythmbox", "org.gnome.Rhythmbox3"},
   {"Quod Libet", "org.mpris.MediaPlayer2.quodlibet"},
   {"MPD", "org.mpris.MediaPlayer2.mpd"},
   {"Emotion Media Center", "org.mpris.MediaPlayer2.epymc"},
   {"Pithos", "org.mpris.MediaPlayer2.pithos"},
   {"Tomahawk", "org.mpris.MediaPlayer2.tomahawk"},
   {NULL, NULL}
};

Eina_Bool
_desklock_cb(void *data, int type EINA_UNUSED, void *ev)
{
   E_Music_Control_Module_Context *ctxt;
   E_Event_Desklock *event;

   ctxt = data;
   event = ev;

   /* Lock with music on. Pause it */
   if (event->on && ctxt->playing)
     {
        media_player2_player_play_pause_call(ctxt->mpris2_player);
        was_playing_before_lock = EINA_TRUE;
        return ECORE_CALLBACK_PASS_ON;
     }

   /* Lock without music. Keep music off as state */
   if (event->on && (!ctxt->playing))
     {
        was_playing_before_lock = EINA_FALSE;
        return ECORE_CALLBACK_PASS_ON;
     }

   /* Unlock with music pause and playing before lock. Turn it back on */
   if ((!event->on) && (!ctxt->playing) && was_playing_before_lock)
     media_player2_player_play_pause_call(ctxt->mpris2_player);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_music_control(E_Object *obj EINA_UNUSED, const char *params)
{
   E_Music_Control_Module_Context *ctxt;
   EINA_SAFETY_ON_NULL_RETURN(music_control_mod->data);
   ctxt = music_control_mod->data;
   if (!strcmp(params, "play"))
     media_player2_player_play_pause_call(ctxt->mpris2_player);
   else if (!strcmp(params, "next"))
     media_player2_player_next_call(ctxt->mpris2_player);
   else if (!strcmp(params, "previous"))
     media_player2_player_previous_call(ctxt->mpris2_player);
}

#define ACTION_NEXT "next_music"
#define ACTION_NEXT_NAME "Next Music"
#define ACTION_PLAY_PAUSE "playpause_music"
#define ACTION_PLAY_PAUSE_NAME "Play/Pause Music"
#define ACTION_PREVIOUS "previous_music"
#define ACTION_PREVIOUS_NAME "Previous Music"

static void
_actions_register(E_Music_Control_Module_Context *ctxt)
{
   E_Action *action;
   if (ctxt->actions_set) return;

   action = e_action_add(ACTION_NEXT);
   action->func.go = _music_control;
   e_action_predef_name_set(_e_music_control_Name, ACTION_NEXT_NAME,
                            ACTION_NEXT, "next", NULL, 0);

   action = e_action_add(ACTION_PLAY_PAUSE);
   action->func.go = _music_control;
   e_action_predef_name_set(_e_music_control_Name, ACTION_PLAY_PAUSE_NAME,
                            ACTION_PLAY_PAUSE, "play", NULL, 0);

   action = e_action_add(ACTION_PREVIOUS);
   action->func.go = _music_control;
   e_action_predef_name_set(_e_music_control_Name, ACTION_PREVIOUS_NAME,
                            ACTION_PREVIOUS, "previous", NULL, 0);

   ctxt->actions_set = EINA_TRUE;
}

static void
_actions_unregister(E_Music_Control_Module_Context *ctxt)
{
   if (!ctxt->actions_set) return;
   e_action_predef_name_del(ACTION_NEXT_NAME, ACTION_NEXT);
   e_action_del(ACTION_NEXT);
   e_action_predef_name_del(ACTION_PLAY_PAUSE_NAME, ACTION_PLAY_PAUSE);
   e_action_del(ACTION_PLAY_PAUSE);
   e_action_predef_name_del(ACTION_PREVIOUS_NAME, ACTION_PREVIOUS);
   e_action_del(ACTION_PREVIOUS);

   ctxt->actions_set = EINA_FALSE;
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
   e_theme_edje_object_set(inst->gadget, "base/theme/modules/music-control",
                           "e/modules/music-control/main");

   inst->gcc = e_gadcon_client_new(gc, name, id, style, inst->gadget);
   inst->gcc->data = inst;

   evas_object_event_callback_add(inst->gadget, EVAS_CALLBACK_MOUSE_DOWN, music_control_mouse_down_cb, inst);

   ctxt->instances = eina_list_append(ctxt->instances, inst);
   _actions_register(ctxt);

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
   if (!ctxt->instances)
     _actions_unregister(ctxt);

   free(inst);
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
   return _(_e_music_control_Name);
}

static char tmpbuf[1024]; /* general purpose buffer, just use immediately */

static Evas_Object *
_gc_icon(const E_Gadcon_Client_Class *client_class EINA_UNUSED, Evas *evas)
{
   Evas_Object *o;
   EINA_SAFETY_ON_NULL_RETURN_VAL(music_control_mod, NULL);
   snprintf(tmpbuf, sizeof(tmpbuf), "%s/e-module-music-control.edj",
            e_module_dir_get(music_control_mod));
   o = edje_object_add(evas);
   edje_object_file_set(o, tmpbuf, "icon");
   return o;
}

static const char *
_gc_id_new(const E_Gadcon_Client_Class *client_class EINA_UNUSED)
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
parse_metadata(E_Music_Control_Module_Context *ctxt, Eina_Value *array)
{
   unsigned i;

   E_FREE_FUNC(ctxt->meta_title, eina_stringshare_del);
   E_FREE_FUNC(ctxt->meta_album, eina_stringshare_del);
   E_FREE_FUNC(ctxt->meta_artist, eina_stringshare_del);
   E_FREE_FUNC(ctxt->meta_cover, eina_stringshare_del);
   // DBG("Metadata: %s", eina_value_to_string(array));

   for (i = 0; i < eina_value_array_count(array); i++)
     {
        const char *key, *str_val;
        char *str_markup;
        Eina_Value st, subst;
        Efreet_Uri *uri;

        eina_value_array_value_get(array, i, &st);
        eina_value_struct_get(&st, "arg0", &key);
        if (!strcmp(key, "xesam:title"))
          {
             eina_value_struct_value_get(&st, "arg1", &subst);
             eina_value_struct_get(&subst, "arg0", &str_val);
             str_markup = evas_textblock_text_utf8_to_markup(NULL, str_val);
             ctxt->meta_title = eina_stringshare_add(str_markup);
             free(str_markup);
             eina_value_flush(&subst);
          }
        else if (!strcmp(key, "xesam:album"))
          {
             eina_value_struct_value_get(&st, "arg1", &subst);
             eina_value_struct_get(&subst, "arg0", &str_val);
             str_markup = evas_textblock_text_utf8_to_markup(NULL, str_val);
             ctxt->meta_album = eina_stringshare_add(str_markup);
             free(str_markup);
             eina_value_flush(&subst);
          }
        else if (!strcmp(key, "xesam:artist"))
          {
             Eina_Value arr;
             eina_value_struct_value_get(&st, "arg1", &subst);
             eina_value_struct_value_get(&subst, "arg0", &arr);
             eina_value_array_get(&arr, 0, &str_val);
             str_markup = evas_textblock_text_utf8_to_markup(NULL, str_val);
             ctxt->meta_artist = eina_stringshare_add(str_markup);
             free(str_markup);
             eina_value_flush(&arr);
             eina_value_flush(&subst);
          }
        else if (!strcmp(key, "mpris:artUrl"))
          {
             eina_value_struct_value_get(&st, "arg1", &subst);
             eina_value_struct_get(&subst, "arg0", &str_val);
             uri = efreet_uri_decode(str_val);
             if (uri && !strncmp(uri->protocol, "file", 4))
               ctxt->meta_cover = eina_stringshare_add(uri->path);
             E_FREE_FUNC(uri, efreet_uri_free);
             eina_value_flush(&subst);
          }
        eina_value_flush(&st);
     }
}

static void
cb_playback_status_get(void *data, Eldbus_Pending *p EINA_UNUSED,
                       const char *propname EINA_UNUSED,
                       Eldbus_Proxy *proxy EINA_UNUSED,
                       Eldbus_Error_Info *error_info, const char *value)
{
   E_Music_Control_Module_Context *ctxt = data;

   if (error_info)
     {
        fprintf(stderr, "MUSIC-CONTROL: %s %s", error_info->error, error_info->message);
        return;
     }

   if (!strcmp(value, "Playing"))
     ctxt->playing = EINA_TRUE;
   else
     ctxt->playing = EINA_FALSE;
   music_control_state_update_all(ctxt);
}

static void
cb_metadata_get(void *data, Eldbus_Pending *p EINA_UNUSED,
                const char *propname EINA_UNUSED,
                Eldbus_Proxy *proxy EINA_UNUSED,
                Eldbus_Error_Info *error_info EINA_UNUSED, Eina_Value *value)
{
   E_Music_Control_Module_Context *ctxt = data;
   parse_metadata(ctxt, value);
   music_control_metadata_update_all(ctxt);
}

static void
prop_changed(void *data, Eldbus_Proxy *proxy EINA_UNUSED, void *event_info)
{
   Eldbus_Proxy_Event_Property_Changed *event = event_info;
   E_Music_Control_Module_Context *ctxt = data;

   if (!strcmp(event->name, "PlaybackStatus"))
     {
        const Eina_Value *value = event->value;
        const char *status;

        eina_value_get(value, &status);
        if (!strcmp(status, "Playing"))
          ctxt->playing = EINA_TRUE;
        else
          ctxt->playing = EINA_FALSE;
        music_control_state_update_all(ctxt);
     }
   else if (!strcmp(event->name, "Metadata"))
     {
        parse_metadata(ctxt, (Eina_Value*)event->value);
        music_control_metadata_update_all(ctxt);
     }
}

static void
cb_name_owner_has(void *data, const Eldbus_Message *msg,
                  Eldbus_Pending *pending EINA_UNUSED)
{
   E_Music_Control_Module_Context *ctxt = data;
   Eina_Bool owner_exists;

   if (eldbus_message_error_get(msg, NULL, NULL))
     return;
   if (!eldbus_message_arguments_get(msg, "b", &owner_exists))
     return;
   if (owner_exists)
     {
        media_player2_player_playback_status_propget(ctxt->mpris2_player,
                                                  cb_playback_status_get, ctxt);
        media_player2_player_metadata_propget(ctxt->mpris2_player,
                                              cb_metadata_get, ctxt);
     }
}

Eina_Bool
music_control_dbus_init(E_Music_Control_Module_Context *ctxt, const char *bus)
{
   eldbus_init();
   ctxt->conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctxt->conn, EINA_FALSE);

   ctxt->mrpis2 = mpris_media_player2_proxy_get(ctxt->conn, bus, NULL);
   ctxt->mpris2_player = media_player2_player_proxy_get(ctxt->conn, bus, NULL);
   eldbus_proxy_event_callback_add(ctxt->mpris2_player, ELDBUS_PROXY_EVENT_PROPERTY_CHANGED,
                                  prop_changed, ctxt);
   eldbus_name_owner_has(ctxt->conn, bus, cb_name_owner_has, ctxt);
   return EINA_TRUE;
}

EAPI void *
e_modapi_init(E_Module *m)
{
   E_Music_Control_Module_Context *ctxt;

   ctxt = calloc(1, sizeof(E_Music_Control_Module_Context));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctxt, NULL);
   music_control_mod = m;

   ctxt->conf_edd = E_CONFIG_DD_NEW("music_control_config", Music_Control_Config);
   #undef T
   #undef D
   #define T Music_Control_Config
   #define D ctxt->conf_edd
   E_CONFIG_VAL(D, T, player_selected, INT);
   E_CONFIG_VAL(D, T, pause_on_desklock, INT);
   ctxt->config = e_config_domain_load(MUSIC_CONTROL_DOMAIN, ctxt->conf_edd);
   if (!ctxt->config)
     ctxt->config = calloc(1, sizeof(Music_Control_Config));

   if (!music_control_dbus_init(ctxt, music_player_players[ctxt->config->player_selected].dbus_name))
     goto error_dbus_bus_get;
   music_control_mod = m;

   e_gadcon_provider_register(&_gc_class);

   if (ctxt->config->pause_on_desklock)
     desklock_handler = ecore_event_handler_add(E_EVENT_DESKLOCK, _desklock_cb, ctxt);

   return ctxt;

error_dbus_bus_get:
   free(ctxt);
   return NULL;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   E_Music_Control_Module_Context *ctxt;
   EINA_SAFETY_ON_NULL_RETURN_VAL(music_control_mod, 0);
   ctxt = music_control_mod->data;

   E_FREE_FUNC(ctxt->meta_title, eina_stringshare_del);
   E_FREE_FUNC(ctxt->meta_album, eina_stringshare_del);
   E_FREE_FUNC(ctxt->meta_artist, eina_stringshare_del);
   E_FREE_FUNC(ctxt->meta_cover, eina_stringshare_del);

   free(ctxt->config);
   E_CONFIG_DD_FREE(ctxt->conf_edd);

   E_FREE_FUNC(desklock_handler, ecore_event_handler_del);

   media_player2_player_proxy_unref(ctxt->mpris2_player);
   mpris_media_player2_proxy_unref(ctxt->mrpis2);
   eldbus_connection_unref(ctxt->conn);
   eldbus_shutdown();

   e_gadcon_provider_unregister(&_gc_class);

   if (eina_list_count(ctxt->instances))
     fprintf(stderr, "MUSIC-CONTROL: Live instances.");

   free(ctxt);
   music_control_mod = NULL;

   return 1;
}

EAPI int
e_modapi_save(E_Module *m)
{
   E_Music_Control_Module_Context *ctxt = m->data;
   e_config_domain_save(MUSIC_CONTROL_DOMAIN, ctxt->conf_edd, ctxt->config);
   return 1;
}
