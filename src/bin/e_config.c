#include "e.h"

#if ((E_PROFILE >= LOWRES_PDA) && (E_PROFILE <= HIRES_PDA))
#define DEF_MENUCLICK             1.25
#else
#define DEF_MENUCLICK             0.25
#endif

E_API E_Config *e_config = NULL;
E_API E_Config_Bindings *e_bindings = NULL;

static int _e_config_revisions = 9;

/* local subsystem functions */
static void      _e_config_save_cb(void *data);
static void      _e_config_free(E_Config *cfg);
static Eina_Bool _e_config_cb_timer(void *data);
static int       _e_config_eet_close_handle(Eet_File *ef, char *file);

/* local subsystem globals */
static int _e_config_save_block = 0;
static E_Powersave_Deferred_Action *_e_config_save_defer = NULL;
static const char *_e_config_profile = NULL;

E_API int E_EVENT_CONFIG_ICON_THEME = 0;
E_API int E_EVENT_CONFIG_MODE_CHANGED = 0;
E_API int E_EVENT_CONFIG_LOADED = 0;

static E_Dialog *_e_config_error_dialog = NULL;
static Eina_List *handlers = NULL;

static void
_e_config_binding_mouse_add(E_Binding_Context ctxt, int button, E_Binding_Modifier mod, int any_mod, const char *action, const char *params)
{
   E_Config_Binding_Mouse *binding;

   binding = calloc(1, sizeof(E_Config_Binding_Mouse));
   binding->context = ctxt;
   binding->button = button;
   binding->modifiers = mod;
   binding->any_mod = any_mod;
   binding->action = eina_stringshare_add(action);
   binding->params = eina_stringshare_add(params);
   e_bindings->mouse_bindings = eina_list_append(e_bindings->mouse_bindings, binding);
}

static void
_e_config_binding_wheel_add(E_Binding_Context ctxt, int direction, int z, E_Binding_Modifier mod, int any_mod, const char *action, const char *params)
{
   E_Config_Binding_Wheel *binding;

   binding = calloc(1, sizeof(E_Config_Binding_Wheel));
   binding->context = ctxt;
   binding->direction = direction;
   binding->z = z;
   binding->modifiers = mod;
   binding->any_mod = any_mod;
   binding->action = eina_stringshare_add(action);
   binding->params = eina_stringshare_add(params);
   e_bindings->wheel_bindings = eina_list_append(e_bindings->wheel_bindings, binding);
}

static Eina_Bool
_e_config_cb_efreet_cache_update(void *data EINA_UNUSED, int type EINA_UNUSED, void *ev EINA_UNUSED)
{
   if (e_config)
     {
        if (e_config->icon_theme)
          {
             if (!efreet_icon_theme_find(e_config->icon_theme))
               {
                  eina_stringshare_replace(&e_config->icon_theme, "hicolor");
                  e_config_save_queue();
               }
          }
     }
   return ECORE_CALLBACK_RENEW;
}


static void
_e_config_error_dialog_cb_delete(void *dia)
{
   if (dia == _e_config_error_dialog)
     _e_config_error_dialog = NULL;
}

static const char *
_e_config_profile_name_get(Eet_File *ef)
{
   /* profile config exists */
   char *data;
   const char *s = NULL;
   int data_len = 0;

   data = eet_read(ef, "config", &data_len);
   if ((data) && (data_len > 0))
     {
        int ok = 1;

        for (s = data; s < (data + data_len); s++)
          {
             // if profile is not all ascii (valid printable ascii - no
             // control codes etc.) or it contains a '/' (invalid as its a
             // directory delimiter) - then it's invalid
             if ((*s < ' ') || (*s > '~') || (*s == '/'))
               {
                  ok = 0;
                  break;
               }
          }
        s = NULL;
        if (ok)
          s = eina_stringshare_add_length(data, data_len);
        free(data);
     }
   return s;
}

/* externally accessible functions */
EINTERN int
e_config_init(void)
{
   E_EVENT_CONFIG_ICON_THEME = ecore_event_type_new();
   E_EVENT_CONFIG_MODE_CHANGED = ecore_event_type_new();
   E_EVENT_CONFIG_LOADED = ecore_event_type_new();

   /* if environment var set - use this profile name */
   _e_config_profile = eina_stringshare_add(getenv("E_CONF_PROFILE"));

   if (!_e_config_profile)
     {
        Eet_File *ef;
        char buf[PATH_MAX];

        /* try user profile config */
        e_user_dir_concat_static(buf, "config/profile.cfg");
        ef = eet_open(buf, EET_FILE_MODE_READ);
        if (ef)
          {
             _e_config_profile = _e_config_profile_name_get(ef);
             eet_close(ef);
             ef = NULL;
          }
        if (!_e_config_profile)
          {
             int i;

             for (i = 1; i <= _e_config_revisions; i++)
               {
                  e_user_dir_snprintf(buf, sizeof(buf), "config/profile.%i.cfg", i);
                  ef = eet_open(buf, EET_FILE_MODE_READ);
                  if (ef)
                    {
                       _e_config_profile = _e_config_profile_name_get(ef);
                       eet_close(ef);
                       ef = NULL;
                       if (_e_config_profile) break;
                    }
               }
             if (!_e_config_profile)
               {
                  /* use system if no user profile config */
                  e_prefix_data_concat_static(buf, "data/config/profile.cfg");
                  ef = eet_open(buf, EET_FILE_MODE_READ);
               }
          }
        if (ef)
          {
             _e_config_profile = _e_config_profile_name_get(ef);
             eet_close(ef);
             ef = NULL;
          }
        if (!_e_config_profile)
          {
             /* no profile config - try other means */
             char *lnk = NULL;

             /* check symlink - if default is a symlink to another dir */
             e_prefix_data_concat_static(buf, "data/config/default");
             lnk = ecore_file_readlink(buf);
             /* if so use just the filename as the profile - must be a local link */
             if (lnk)
               {
                  _e_config_profile = eina_stringshare_add(ecore_file_file_get(lnk));
                  free(lnk);
               }
             else
               _e_config_profile = eina_stringshare_add("default");
          }
        if (!getenv("E_CONF_PROFILE"))
          e_util_env_set("E_CONF_PROFILE", _e_config_profile);
     }

   e_config_descriptor_init(EINA_FALSE);

   e_config_load();

   e_config_save_queue();

   E_LIST_HANDLER_APPEND(handlers, EFREET_EVENT_DESKTOP_CACHE_UPDATE,
                         _e_config_cb_efreet_cache_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, EFREET_EVENT_ICON_CACHE_UPDATE,
                         _e_config_cb_efreet_cache_update, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CONFIG_ICON_THEME,
                         _e_config_cb_efreet_cache_update, NULL);

   return 1;
}

EINTERN int
e_config_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
   eina_stringshare_del(_e_config_profile);
   e_config_descriptor_shutdown();
   return 1;
}

E_API void
e_config_load(void)
{
   int reload = 0;

   e_config = e_config_domain_load("e", e_config_descriptor_get());
   if (e_config)
     {
        /* major version change - that means wipe and restart */
        if ((e_config->config_version) < E_CONFIG_FILE_EPOCH * 1000000)
          {
             /* your config is too old - need new defaults */
             _e_config_free(e_config);
             e_config = NULL;
             reload = 1;
             ecore_timer_add(1.0, _e_config_cb_timer,
                             _("Settings data needed upgrading. Your old settings have<br>"
                               "been wiped and a new set of defaults initialized. This<br>"
                               "will happen regularly during development, so don't report a<br>"
                               "bug. This simply means Enlightenment needs new settings<br>"
                               "data by default for usable functionality that your old<br>"
                               "settings simply lack. This new set of defaults will fix<br>"
                               "that by adding it in. You can re-configure things now to your<br>"
                               "liking. Sorry for the hiccup in your settings.<br>"));
          }
        /* config is too new? odd! suspect corruption? */
        else if (e_config->config_version > E_CONFIG_FILE_VERSION)
          {
             /* your config is too new - what the fuck??? */
             _e_config_free(e_config);
             e_config = NULL;
             reload = 1;
             ecore_timer_add(1.0, _e_config_cb_timer,
                             _("Your settings are NEWER than Enlightenment. This is very<br>"
                               "strange. This should not happen unless you downgraded<br>"
                               "Enlightenment or copied the settings from a place where<br>"
                               "a newer version of Enlightenment was running. This is bad and<br>"
                               "as a precaution your settings have been now restored to<br>"
                               "defaults. Sorry for the inconvenience.<br>"));
          }
        if (reload)
          {
             e_config_profile_del(e_config_profile_get());
             e_config_profile_set("default");
             e_config = e_config_domain_load("e", e_config_descriptor_get());
          }
     }
   while (!e_config)
     {
        e_config_descriptor_shutdown();
        e_config_descriptor_init(EINA_TRUE);
        e_config = e_config_domain_load("e", e_config_descriptor_get());
        /* I made a c&p error here and fucked the world, so this ugliness
         * will be my public mark of shame until E :/
         * -zmike, 2013
         */
        if (e_config)
          {
             Eina_List *l;
             E_Config_XKB_Layout *cl;
             int set = 0;

             /* this is essentially CONFIG_VERSION_CHECK(7) */
             INF("Performing config upgrade to %d.%d", 1, 7);
             e_config_descriptor_shutdown();
             e_config_descriptor_init(EINA_FALSE);
             set += !!e_config->xkb.current_layout;
             set += !!e_config->xkb.sel_layout;
             set += !!e_config->xkb.lock_layout;
             EINA_LIST_FOREACH(e_config->xkb.used_layouts, l, cl)
               {
                  if (e_config->xkb.current_layout && (e_config->xkb.current_layout->name == cl->name))
                    {
                       e_config->xkb.current_layout->model = eina_stringshare_ref(cl->model);
                       e_config->xkb.current_layout->variant = eina_stringshare_ref(cl->variant);
                       set--;
                    }
                  if (e_config->xkb.sel_layout && (e_config->xkb.sel_layout->name == cl->name))
                    {
                       e_config->xkb.sel_layout->model = eina_stringshare_ref(cl->model);
                       e_config->xkb.sel_layout->variant = eina_stringshare_ref(cl->variant);
                       set--;
                    }
                  if (e_config->xkb.lock_layout && (e_config->xkb.lock_layout->name == cl->name))
                    {
                       e_config->xkb.lock_layout->model = eina_stringshare_ref(cl->model);
                       e_config->xkb.lock_layout->variant = eina_stringshare_ref(cl->variant);
                       set--;
                    }
                  if (!set) break;
               }
             break;
          }
#undef T
#undef D
        e_config_profile_set("default");
        if (!reload) e_config_profile_del(e_config_profile_get());
        e_config_save_block_set(1);
        ecore_app_restart();
        //e_sys_action_do(E_SYS_RESTART, NULL);
        return;
     }

#define CONFIG_VERSION_CHECK(VERSION) \
  if (e_config->config_version - (E_CONFIG_FILE_EPOCH * 1000000) < (VERSION))

#define CONFIG_VERSION_UPDATE_INFO(VERSION) \
  INF("Performing config upgrade for %d.%d", E_CONFIG_FILE_EPOCH, VERSION);

   /* this should be 6, an xkb fix fucked up the ordering and this is now really broken */
   CONFIG_VERSION_CHECK(8)
     {
        /* e_bindings config domain didn't exist before this version, so we have to do this
         * check before we load or else we wipe configs :(
         */
#undef SET
#define SET(X) e_bindings->X = e_config->X, e_config->X = NULL

        //CONFIG_VERSION_UPDATE_INFO(6);
        if (e_config->mouse_bindings || e_config->key_bindings || e_config->edge_bindings ||
            e_config->signal_bindings || e_config->wheel_bindings || e_config->acpi_bindings)
          {
             e_bindings = E_NEW(E_Config_Bindings, 1);
             SET(mouse_bindings);
             SET(key_bindings);
             SET(edge_bindings);
             SET(signal_bindings);
             SET(wheel_bindings);
             SET(acpi_bindings);
             e_config_domain_save("e_bindings", e_config_binding_descriptor_get(), e_bindings);
#undef SET
          }
        else
          e_bindings = e_config_domain_load("e_bindings", e_config_binding_descriptor_get());
     }
   else
     e_bindings = e_config_domain_load("e_bindings", e_config_binding_descriptor_get());

   if ((!e_bindings) || (e_bindings->config_version != E_CONFIG_BINDINGS_VERSION))
     {
        Eina_Stringshare *prof;

        e_config_bindings_free(e_bindings);
        prof = eina_stringshare_ref(e_config_profile_get());
        e_config_profile_set("standard");
        e_bindings = e_config_domain_system_load("e_bindings", e_config_binding_descriptor_get());
        e_config_profile_set(prof);
        eina_stringshare_del(prof);
        ecore_timer_add(1.0, _e_config_cb_timer,
                        _("Your bindings settings version does not match the current settings version.<br>"
                          "As a result, all bindings have been reloaded from defaults.<br>"
                          "Sorry for the inconvenience.<br>"));
     }

   if (e_config->config_version < E_CONFIG_FILE_VERSION)
     {
        CONFIG_VERSION_CHECK(5)
          {
             E_Config_XKB_Layout *cl;
             Eina_List *l;

            CONFIG_VERSION_UPDATE_INFO(5);
            if (e_config->xkb.cur_layout || e_config->xkb.selected_layout || e_config->xkb.desklock_layout)
              {
                 EINA_LIST_FOREACH(e_config->xkb.used_layouts, l, cl)
                   {
                      if (cl->name == e_config->xkb.cur_layout)
                        e_config->xkb.current_layout = e_config_xkb_layout_dup(cl);
                      if (cl->name == e_config->xkb.selected_layout)
                        e_config->xkb.sel_layout = e_config_xkb_layout_dup(cl);
                      if (cl->name == e_config->xkb.desklock_layout)
                        e_config->xkb.lock_layout = e_config_xkb_layout_dup(cl);
                      if (((!!e_config->xkb.current_layout) == (!!e_config->xkb.cur_layout)) &&
                          ((!!e_config->xkb.sel_layout) == (!!e_config->xkb.selected_layout)) &&
                          ((!!e_config->xkb.lock_layout) == (!!e_config->xkb.desklock_layout)))
                        break;
                   }
              }
          }
/* this gets done above but I'm leaving it here so it can be seen
        CONFIG_VERSION_CHECK(6)
          {
             CONFIG_VERSION_UPDATE_INFO(6);
             e_bindings = E_NEW(E_Config_Bindings, 1);
#undef SET
#define SET(X) e_bindings->X = e_config->X, e_config->X = NULL

             SET(mouse_bindings);
             SET(key_bindings);
             SET(edge_bindings);
             SET(signal_bindings);
             SET(wheel_bindings);
             SET(acpi_bindings);
#undef SET
             e_config_domain_save("e_bindings", e_config_binding_descriptor_get(), e_bindings);
          }
*/
        CONFIG_VERSION_CHECK(8)
          {
             CONFIG_VERSION_UPDATE_INFO(8);
             if (!e_config->config_type)
               {
                  /* I guess this probably isn't great, but whatever */
                  if (eina_list_count(e_bindings->key_bindings) > 2)
                    e_config->config_type = E_CONFIG_PROFILE_TYPE_DESKTOP;
                  else
                    e_config->config_type = E_CONFIG_PROFILE_TYPE_TABLET;
               }
          }
        CONFIG_VERSION_CHECK(10)
          {
             int do_conf = 0;
             Eina_List *l, *ll;
             E_Config_Module *em;
             int enabled = 0, delayed = 0, priority = 0;

             CONFIG_VERSION_UPDATE_INFO(10);
             EINA_LIST_FOREACH_SAFE(e_config->modules, l, ll, em)
               {
                  Eina_Bool do_free = EINA_FALSE;

                  if (!e_util_strcmp(em->name, "comp"))
                    do_free = EINA_TRUE;
                  else if ((!e_util_strcmp(em->name, "conf_keybindings")) || (!e_util_strcmp(em->name, "conf_edgebindings")))
                    {
                       do_conf += do_free = EINA_TRUE;
                       enabled |= em->enabled;
                       delayed |= em->delayed;
                       priority = MIN(priority, em->priority);
                    }
                  if (do_free)
                    {
                       e_config->modules = eina_list_remove_list(e_config->modules, l);
                       eina_stringshare_del(em->name);
                       free(em);
                    }
                  if (do_conf == 2) break;
               }
             if (do_conf)
               {
                  em = E_NEW(E_Config_Module, 1);
                  em->name = eina_stringshare_add("conf_bindings");
                  em->enabled = !!enabled;
                  em->delayed = !!delayed;
                  em->priority = priority;
                  e_config->modules = eina_list_append(e_config->modules, em);
               }
          }
        CONFIG_VERSION_CHECK(11)
          {
             CONFIG_VERSION_UPDATE_INFO(11);
             e_config->pointer_warp_speed = e_config->winlist_warp_speed;
             e_config->winlist_warp_speed = 0;

             if (e_config->focus_policy == E_FOCUS_CLICK)
               {
                  if (e_config->border_raise_on_focus)
                    {
                       /* approximate expected behavior from removed option */
                       e_config->always_click_to_focus = 1;
                       e_config->always_click_to_raise = 1;
                    }
               }
          }
        CONFIG_VERSION_CHECK(12)
          {
             CONFIG_VERSION_UPDATE_INFO(12);
             switch (e_config->desk_flip_animate_mode)
               {
                case 1: //pane
                  e_config->desk_flip_animate_type = eina_stringshare_add("auto/pane");
                  break;
                case 2: //zoom, now known as diagonal
                  e_config->desk_flip_animate_type = eina_stringshare_add("auto/diagonal");
                  break;
                default:
                  break;
               }
          }
        CONFIG_VERSION_CHECK(14)
          {
             Eina_List *files, *l;
             Eina_Bool fail = EINA_FALSE;
             E_Config_Shelf *cf_es;
             E_Remember *rem;
             char buf[PATH_MAX], buf2[PATH_MAX], *f;

             CONFIG_VERSION_UPDATE_INFO(14);

             EINA_LIST_FOREACH(e_config->shelves, l, cf_es)
               {
                  if (cf_es->popup)
                    {
                       if (cf_es->layer)
                         cf_es->layer = E_LAYER_CLIENT_ABOVE;
                       else
                         cf_es->layer = E_LAYER_CLIENT_DESKTOP;
                    }
                  else if (!cf_es->layer)
                    cf_es->layer = E_LAYER_DESKTOP; //redundant, but whatever
                  cf_es->popup = 0;
               }

             /* E layer values are higher */
             EINA_LIST_FOREACH(e_config->remembers, l, rem)
               if (rem->apply & E_REMEMBER_APPLY_LAYER)
                 rem->prop.layer += 100;

             // copy all of ~/.e/e/themes/* into ~/.elementary/themes
             // and delete original data in ~/.e/e/themes
             ecore_file_mkpath(elm_theme_user_dir_get());
             snprintf(buf, sizeof(buf), "%s/themes", e_user_dir_get());
             files = ecore_file_ls(buf);
             EINA_LIST_FREE(files, f)
               {
                  snprintf(buf, sizeof(buf), "%s/themes/%s",
                           e_user_dir_get(), f);
                  snprintf(buf2, sizeof(buf2), "%s/%s",
                           elm_theme_user_dir_get(), f);
                  if (!ecore_file_cp(buf, buf2)) fail = EINA_TRUE;
               }
             if (!fail)
               {
                  snprintf(buf, sizeof(buf), "%s/themes", e_user_dir_get());
                  ecore_file_recursive_rm(buf);
               }
          }
        CONFIG_VERSION_CHECK(15)
          {
             E_Config_Module *em;
             Eina_List *l;
             Eina_Bool found = EINA_FALSE;

             CONFIG_VERSION_UPDATE_INFO(15);
             if (e_config->desklock_use_custom_desklock)
               e_config->desklock_auth_method = E_DESKLOCK_AUTH_METHOD_EXTERNAL;

             EINA_LIST_FOREACH(e_config->modules, l, em)
               if (!strcmp(em->name, "lokker"))
                 {
                    found = EINA_TRUE;
                    break;
                 }
             if (!found)
               {
                  /* add new desklock module */
                  em = E_NEW(E_Config_Module, 1);
                  em->name = eina_stringshare_add("lokker");
                  em->enabled = 1;
                  em->delayed = 0;
                  e_config->modules = eina_list_append(e_config->modules, em);
               }
          }
        CONFIG_VERSION_CHECK(17)
          {
             E_Config_Module *em;
             Eina_List *l;

             CONFIG_VERSION_UPDATE_INFO(17);

             EINA_LIST_FOREACH(e_config->modules, l, em)
               if (!strcmp(em->name, "pager16"))
                 {
                    eina_stringshare_replace(&em->name, "pager");
                    break;
                 }
          }
        CONFIG_VERSION_CHECK(18)
          {
             E_Color_Class *ecc;

             CONFIG_VERSION_UPDATE_INFO(18);
             EINA_LIST_FREE(e_config->color_classes, ecc)
               {
                  elm_config_color_overlay_set(ecc->name, ecc->r, ecc->g, ecc->b, ecc->a, ecc->r2, ecc->g2, ecc->b2, ecc->a2, ecc->r3, ecc->g3, ecc->b3, ecc->a3);
                  eina_stringshare_del(ecc->name);
                  free(ecc);
               }
          }
        CONFIG_VERSION_CHECK(19)
          {
             CONFIG_VERSION_UPDATE_INFO(19);

             /* set (400, 25) as the default values of repeat delay, rate */
             e_config->keyboard.repeat_delay = 400;
             e_config->keyboard.repeat_rate = 25;
          }
        CONFIG_VERSION_CHECK(20)
          {
             Eina_List *l;
             E_Config_Binding_Mouse *ebm;
             E_Config_Module *em, *module;

             CONFIG_VERSION_UPDATE_INFO(20);

             EINA_LIST_FOREACH(e_bindings->mouse_bindings, l, ebm)
               {
                  if (eina_streq(ebm->action, "window_move"))
                    {
                       _e_config_binding_mouse_add(E_BINDING_CONTEXT_ANY, ebm->button, ebm->modifiers,
                                            ebm->any_mod, "gadget_move", NULL);
                    }
                  else if (eina_streq(ebm->action, "window_resize"))
                    {
                       _e_config_binding_mouse_add(E_BINDING_CONTEXT_ANY, ebm->button, ebm->modifiers,
                                            ebm->any_mod, "gadget_resize", NULL);
                    }
                  else if (eina_streq(ebm->action, "window_menu"))
                    {
                       _e_config_binding_mouse_add(E_BINDING_CONTEXT_ANY, ebm->button, ebm->modifiers,
                                            ebm->any_mod, "gadget_menu", NULL);
                       _e_config_binding_mouse_add(E_BINDING_CONTEXT_ANY, ebm->button, ebm->modifiers,
                                            ebm->any_mod, "bryce_menu", NULL);
                    }
               }
             _e_config_binding_wheel_add(E_BINDING_CONTEXT_ANY, 0, 1, E_BINDING_MODIFIER_CTRL, 0, "bryce_resize", NULL);
             _e_config_binding_wheel_add(E_BINDING_CONTEXT_ANY, 0, -1, E_BINDING_MODIFIER_CTRL, 0, "bryce_resize", NULL);

             EINA_LIST_FOREACH(e_config->modules, l, em)
               {
                  if (!em->enabled) continue;
                  if (eina_streq(em->name, "connman"))
                    {
                       module = E_NEW(E_Config_Module, 1);
                       module->name = eina_stringshare_add("wireless");
                       module->enabled = 1;
                       e_config->modules = eina_list_append(e_config->modules, module);
                    }
                  else if (eina_streq(em->name, "clock"))
                    {
                       module = E_NEW(E_Config_Module, 1);
                       module->name = eina_stringshare_add("time");
                       module->enabled = 1;
                       e_config->modules = eina_list_append(e_config->modules, module);
                    }
               }
          }

          CONFIG_VERSION_CHECK(21)
            {
               CONFIG_VERSION_UPDATE_INFO(21);

               e_config->window_maximize_animate = 1;
               e_config->window_maximize_transition = E_EFX_EFFECT_SPEED_SINUSOIDAL;
               e_config->window_maximize_time = 0.15;
            }
     }
   if (!e_config->remember_internal_fm_windows)
     e_config->remember_internal_fm_windows = !!(e_config->remember_internal_windows & E_REMEMBER_INTERNAL_FM_WINS);

   e_bindings->config_version = E_CONFIG_BINDINGS_VERSION;
   e_config->config_version = E_CONFIG_FILE_VERSION;

   /* limit values so they are sane */
   E_CONFIG_LIMIT(e_config->menus_scroll_speed, 1.0, 20000.0);
   E_CONFIG_LIMIT(e_config->show_splash, 0, 1);
   E_CONFIG_LIMIT(e_config->menus_fast_mouse_move_threshhold, 1.0, 2000.0);
   E_CONFIG_LIMIT(e_config->menus_click_drag_timeout, 0.0, 10.0);
   E_CONFIG_LIMIT(e_config->window_maximize_animate, 0, 1);
   E_CONFIG_LIMIT(e_config->window_maximize_transition, 0, E_EFX_EFFECT_SPEED_SINUSOIDAL);
   E_CONFIG_LIMIT(e_config->window_maximize_time, 0.0, 1.0);
   E_CONFIG_LIMIT(e_config->border_shade_animate, 0, 1);
   E_CONFIG_LIMIT(e_config->border_shade_transition, 0, 8);
   E_CONFIG_LIMIT(e_config->border_shade_speed, 1.0, 20000.0);
   E_CONFIG_LIMIT(e_config->framerate, 1.0, 200.0);
   E_CONFIG_LIMIT(e_config->priority, 0, 19);
   E_CONFIG_LIMIT(e_config->zone_desks_x_count, 1, 64);
   E_CONFIG_LIMIT(e_config->zone_desks_y_count, 1, 64);
   E_CONFIG_LIMIT(e_config->show_desktop_icons, 0, 1);
   E_CONFIG_LIMIT(e_config->edge_flip_dragging, 0, 1);
   E_CONFIG_LIMIT(e_config->window_placement_policy, E_WINDOW_PLACEMENT_SMART, E_WINDOW_PLACEMENT_MANUAL);
   E_CONFIG_LIMIT(e_config->window_grouping, 0, 1);
   E_CONFIG_LIMIT(e_config->focus_policy, 0, 2);
   E_CONFIG_LIMIT(e_config->focus_setting, 0, 3);
   E_CONFIG_LIMIT(e_config->pass_click_on, 0, 1);
   E_CONFIG_LIMIT(e_config->window_activehint_policy, E_ACTIVEHINT_POLICY_IGNORE, E_ACTIVEHINT_POLICY_LAST - 1);
   E_CONFIG_LIMIT(e_config->always_click_to_raise, 0, 1);
   E_CONFIG_LIMIT(e_config->always_click_to_focus, 0, 1);
   E_CONFIG_LIMIT(e_config->use_auto_raise, 0, 1);
   E_CONFIG_LIMIT(e_config->auto_raise_delay, 0.0, 9.9);
   E_CONFIG_LIMIT(e_config->use_resist, 0, 1);
   E_CONFIG_LIMIT(e_config->drag_resist, 0, 100);
   E_CONFIG_LIMIT(e_config->desk_resist, 0, 100);
   E_CONFIG_LIMIT(e_config->window_resist, 0, 100);
   E_CONFIG_LIMIT(e_config->gadget_resist, 0, 100);
   E_CONFIG_LIMIT(e_config->geometry_auto_move, 0, 1);
   E_CONFIG_LIMIT(e_config->geometry_auto_resize_limit, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_warp_while_selecting, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_warp_at_end, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_no_warp_on_direction, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_warp_speed, 0.0, 1.0);
   E_CONFIG_LIMIT(e_config->winlist_scroll_animate, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_scroll_speed, 0.0, 1.0);
   E_CONFIG_LIMIT(e_config->winlist_list_show_iconified, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_list_show_other_desk_iconified, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_list_show_other_screen_iconified, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_list_show_other_desk_windows, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_list_show_other_screen_windows, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_list_uncover_while_selecting, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_list_jump_desk_while_selecting, 0, 1);
   E_CONFIG_LIMIT(e_config->winlist_pos_align_x, 0.0, 1.0);
   E_CONFIG_LIMIT(e_config->winlist_pos_align_y, 0.0, 1.0);
   E_CONFIG_LIMIT(e_config->winlist_pos_size_w, 0.0, 1.0);
   E_CONFIG_LIMIT(e_config->winlist_pos_size_h, 0.0, 1.0);
   E_CONFIG_LIMIT(e_config->winlist_pos_min_w, 0, 4000);
   E_CONFIG_LIMIT(e_config->winlist_pos_min_h, 0, 4000);
   E_CONFIG_LIMIT(e_config->winlist_pos_max_w, 8, 4000);
   E_CONFIG_LIMIT(e_config->winlist_pos_max_h, 8, 4000);
   E_CONFIG_LIMIT(e_config->maximize_policy, E_MAXIMIZE_FULLSCREEN, E_MAXIMIZE_DIRECTION);
   E_CONFIG_LIMIT(e_config->allow_manip, 0, 1);
   E_CONFIG_LIMIT(e_config->border_fix_on_shelf_toggle, 0, 1);
   E_CONFIG_LIMIT(e_config->allow_above_fullscreen, 0, 1);
   E_CONFIG_LIMIT(e_config->kill_if_close_not_possible, 0, 1);
   E_CONFIG_LIMIT(e_config->kill_process, 0, 1);
   E_CONFIG_LIMIT(e_config->kill_timer_wait, 0.0, 120.0);
   E_CONFIG_LIMIT(e_config->ping_clients, 0, 1);
   E_CONFIG_LIMIT(e_config->move_info_follows, 0, 1);
   E_CONFIG_LIMIT(e_config->resize_info_follows, 0, 1);
   E_CONFIG_LIMIT(e_config->move_info_visible, 0, 1);
   E_CONFIG_LIMIT(e_config->resize_info_visible, 0, 1);
   E_CONFIG_LIMIT(e_config->focus_last_focused_per_desktop, 0, 1);
   E_CONFIG_LIMIT(e_config->focus_revert_on_hide_or_close, 0, 1);
   E_CONFIG_LIMIT(e_config->focus_revert_allow_sticky, 0, 1);
   E_CONFIG_LIMIT(e_config->pointer_slide, 0, 1);
   E_CONFIG_LIMIT(e_config->disable_all_pointer_warps, 0, 1);
   E_CONFIG_LIMIT(e_config->pointer_warp_speed, 0.0, 1.0);
   E_CONFIG_LIMIT(e_config->show_cursor, 0, 1);
   E_CONFIG_LIMIT(e_config->use_e_cursor, 0, 1);
   E_CONFIG_LIMIT(e_config->cursor_size, 0, 1024);
   E_CONFIG_LIMIT(e_config->menu_autoscroll_margin, 0, 50);
   E_CONFIG_LIMIT(e_config->menu_autoscroll_cursor_margin, 0, 50);
   E_CONFIG_LIMIT(e_config->menu_eap_name_show, 0, 1);
   E_CONFIG_LIMIT(e_config->menu_eap_generic_show, 0, 1);
   E_CONFIG_LIMIT(e_config->menu_eap_comment_show, 0, 1);
   E_CONFIG_LIMIT(e_config->use_app_icon, 0, 1);
   E_CONFIG_LIMIT(e_config->cnfmdlg_disabled, 0, 1);
   E_CONFIG_LIMIT(e_config->cfgdlg_auto_apply, 0, 1);
   E_CONFIG_LIMIT(e_config->cfgdlg_default_mode, 0, 1);
   E_CONFIG_LIMIT(e_config->font_hinting, 0, 2);
   E_CONFIG_LIMIT(e_config->desklock_login_box_zone, -2, 1000);
   E_CONFIG_LIMIT(e_config->desklock_autolock_screensaver, 0, 1);
   E_CONFIG_LIMIT(e_config->desklock_post_screensaver_time, 0.0, 300.0);
   E_CONFIG_LIMIT(e_config->desklock_autolock_idle, 0, 1);
   E_CONFIG_LIMIT(e_config->desklock_autolock_idle_timeout, 1.0, 5400.0);
   E_CONFIG_LIMIT(e_config->desklock_use_custom_desklock, 0, 1);
   E_CONFIG_LIMIT(e_config->desklock_ask_presentation, 0, 1);
   E_CONFIG_LIMIT(e_config->desklock_ask_presentation_timeout, 1.0, 300.0);
   E_CONFIG_LIMIT(e_config->border_raise_on_mouse_action, 0, 1);
   E_CONFIG_LIMIT(e_config->border_raise_on_focus, 0, 1);
   E_CONFIG_LIMIT(e_config->raise_on_revert_focus, 0, 1);
   E_CONFIG_LIMIT(e_config->desk_flip_wrap, 0, 1);
   E_CONFIG_LIMIT(e_config->fullscreen_flip, 0, 1);
   E_CONFIG_LIMIT(e_config->icon_theme_overrides, 0, 1);
   E_CONFIG_LIMIT(e_config->remember_internal_windows, 0, 3);
   E_CONFIG_LIMIT(e_config->remember_internal_fm_windows, 0, 1);
   E_CONFIG_LIMIT(e_config->remember_internal_fm_windows_globally, 0, 1);
   E_CONFIG_LIMIT(e_config->desk_auto_switch, 0, 1);

   E_CONFIG_LIMIT(e_config->screen_limits, 0, 2);

   E_CONFIG_LIMIT(e_config->dpms_enable, 0, 1);
   E_CONFIG_LIMIT(e_config->dpms_standby_enable, 0, 1);
   E_CONFIG_LIMIT(e_config->dpms_suspend_enable, 0, 1);
   E_CONFIG_LIMIT(e_config->dpms_off_enable, 0, 1);
   E_CONFIG_LIMIT(e_config->dpms_standby_timeout, 30, 5400);
   E_CONFIG_LIMIT(e_config->dpms_suspend_timeout, 30, 5400);
   E_CONFIG_LIMIT(e_config->dpms_off_timeout, 30, 5400);

   E_CONFIG_LIMIT(e_config->backlight.timer, 1, 3600);

   E_CONFIG_LIMIT(e_config->screensaver_timeout, 30, 5400);
   E_CONFIG_LIMIT(e_config->screensaver_interval, 0, 5400);
   E_CONFIG_LIMIT(e_config->screensaver_blanking, 0, 2);
   E_CONFIG_LIMIT(e_config->screensaver_expose, 0, 2);
   E_CONFIG_LIMIT(e_config->screensaver_ask_presentation, 0, 1);
   E_CONFIG_LIMIT(e_config->screensaver_ask_presentation_timeout, 1.0, 300.0);

   E_CONFIG_LIMIT(e_config->screensaver_wake_on_notify, 0, 1);
   E_CONFIG_LIMIT(e_config->screensaver_wake_on_urgent, 0, 1);

   E_CONFIG_LIMIT(e_config->clientlist_group_by, 0, 2);
   E_CONFIG_LIMIT(e_config->clientlist_include_all_zones, 0, 1);
   E_CONFIG_LIMIT(e_config->clientlist_separate_with, 0, 2);
   E_CONFIG_LIMIT(e_config->clientlist_sort_by, 0, 3);
   E_CONFIG_LIMIT(e_config->clientlist_separate_iconified_apps, 0, 2);
   E_CONFIG_LIMIT(e_config->clientlist_warp_to_iconified_desktop, 0, 1);
   E_CONFIG_LIMIT(e_config->mouse_hand, 0, 1);
   E_CONFIG_LIMIT(e_config->clientlist_limit_caption_len, 0, 1);
   E_CONFIG_LIMIT(e_config->clientlist_max_caption_len, 2, E_CLIENTLIST_MAX_CAPTION_LEN);

   E_CONFIG_LIMIT(e_config->mouse_accel_numerator, 1, 30);
   E_CONFIG_LIMIT(e_config->mouse_accel_denominator, 1, 10);
   E_CONFIG_LIMIT(e_config->mouse_accel_threshold, 0, 10);

   E_CONFIG_LIMIT(e_config->menu_favorites_show, 0, 1);
   E_CONFIG_LIMIT(e_config->menu_apps_show, 0, 1);
   E_CONFIG_LIMIT(e_config->menu_gadcon_client_toplevel, 0, 1);

   E_CONFIG_LIMIT(e_config->ping_clients_interval, 16, 1024);

   E_CONFIG_LIMIT(e_config->mode.presentation, 0, 1);
   E_CONFIG_LIMIT(e_config->mode.offline, 0, 1);

   E_CONFIG_LIMIT(e_config->exec.expire_timeout, 0.1, 1000);
   E_CONFIG_LIMIT(e_config->exec.show_run_dialog, 0, 1);
   E_CONFIG_LIMIT(e_config->exec.show_exit_dialog, 0, 1);

   E_CONFIG_LIMIT(e_config->null_container_win, 0, 1);

   E_CONFIG_LIMIT(e_config->powersave.none, 0.01, 5400.00);
   E_CONFIG_LIMIT(e_config->powersave.low, 0.01, 5400.00);
   E_CONFIG_LIMIT(e_config->powersave.medium, 0.01, 5400.00);
   E_CONFIG_LIMIT(e_config->powersave.high, 0.01, 5400.00);
   E_CONFIG_LIMIT(e_config->powersave.extreme, 0.01, 5400.00);
   E_CONFIG_LIMIT(e_config->powersave.min, E_POWERSAVE_MODE_NONE, E_POWERSAVE_MODE_EXTREME);
   E_CONFIG_LIMIT(e_config->powersave.max, E_POWERSAVE_MODE_NONE, E_POWERSAVE_MODE_EXTREME);

   E_CONFIG_LIMIT(e_config->border_keyboard.move.dx, 1, 255);
   E_CONFIG_LIMIT(e_config->border_keyboard.move.dy, 1, 255);
   E_CONFIG_LIMIT(e_config->border_keyboard.resize.dx, 1, 255);
   E_CONFIG_LIMIT(e_config->border_keyboard.resize.dy, 1, 255);

   E_CONFIG_LIMIT(e_config->multiscreen_flip, 0, 1);

   E_CONFIG_LIMIT(e_config->backlight.normal, 0.05, 1.0);
   E_CONFIG_LIMIT(e_config->backlight.dim, 0.05, 1.0);
   E_CONFIG_LIMIT(e_config->backlight.idle_dim, 0, 1);

   E_CONFIG_LIMIT(e_config->keyboard.repeat_delay, -1, 1000); // 1 second
   E_CONFIG_LIMIT(e_config->keyboard.repeat_rate, -1, 1000); // 1 second

   if (!e_config->icon_theme)
     e_config->icon_theme = eina_stringshare_add("hicolor");  // FDO default

   /* FIXME: disabled auto apply because it causes problems */
   e_config->cfgdlg_auto_apply = 0;

   ecore_event_add(E_EVENT_CONFIG_LOADED, NULL, NULL, NULL);
}

E_API int
e_config_save(void)
{
   E_FREE_FUNC(_e_config_save_defer, e_powersave_deferred_action_del);
   _e_config_save_cb(NULL);
   return e_config_domain_save("e", e_config_descriptor_get(), e_config);
}

E_API void
e_config_save_flush(void)
{
   if (_e_config_save_defer)
     {
        e_powersave_deferred_action_del(_e_config_save_defer);
        _e_config_save_defer = NULL;
        _e_config_save_cb(NULL);
     }
}

E_API void
e_config_save_queue(void)
{
   if (_e_config_save_defer)
     e_powersave_deferred_action_del(_e_config_save_defer);
   _e_config_save_defer = e_powersave_deferred_action_add(_e_config_save_cb,
                                                          NULL);
}

E_API const char *
e_config_profile_get(void)
{
   return _e_config_profile;
}

E_API void
e_config_profile_set(const char *prof)
{
   eina_stringshare_replace(&_e_config_profile, prof);
   e_util_env_set("E_CONF_PROFILE", _e_config_profile);
}

E_API char *
e_config_profile_dir_get(const char *prof)
{
   char buf[PATH_MAX];

   e_user_dir_snprintf(buf, sizeof(buf), "config/%s", prof);
   if (ecore_file_is_dir(buf)) return strdup(buf);
   e_prefix_data_snprintf(buf, sizeof(buf), "data/config/%s", prof);
   if (ecore_file_is_dir(buf)) return strdup(buf);
   return NULL;
}

static int
_cb_sort_files(char *f1, char *f2)
{
   return strcmp(f1, f2);
}

E_API Eina_List *
e_config_profile_list(void)
{
   Eina_List *files;
   char buf[PATH_MAX], *p;
   Eina_List *flist = NULL;
   size_t len;

   len = e_user_dir_concat_static(buf, "config");
   if (len + 1 >= (int)sizeof(buf))
     return NULL;

   files = ecore_file_ls(buf);

   buf[len] = '/';
   len++;

   p = buf + len;
   len = sizeof(buf) - len;
   if (files)
     {
        char *file;

        files = eina_list_sort(files, 0, (Eina_Compare_Cb)_cb_sort_files);
        EINA_LIST_FREE(files, file)
          {
             if (eina_strlcpy(p, file, len) >= len)
               {
                  free(file);
                  continue;
               }
             if (ecore_file_is_dir(buf))
               flist = eina_list_append(flist, file);
             else
               free(file);
          }
     }
   len = e_prefix_data_concat_static(buf, "data/config");
   if (len >= sizeof(buf))
     return NULL;

   files = ecore_file_ls(buf);

   buf[len] = '/';
   len++;

   p = buf + len;
   len = sizeof(buf) - len;
   if (files)
     {
        char *file;
        files = eina_list_sort(files, 0, (Eina_Compare_Cb)_cb_sort_files);
        EINA_LIST_FREE(files, file)
          {
             if (eina_strlcpy(p, file, len) >= len)
               {
                  free(file);
                  continue;
               }
             if (ecore_file_is_dir(buf))
               {
                  const Eina_List *l;
                  const char *tmp;
                  EINA_LIST_FOREACH(flist, l, tmp)
                    if (!strcmp(file, tmp)) break;

                  if (!l) flist = eina_list_append(flist, file);
                  else free(file);
               }
             else
               free(file);
          }
     }
   return flist;
}

E_API void
e_config_profile_add(const char *prof)
{
   char buf[4096];
   if (e_user_dir_snprintf(buf, sizeof(buf), "config/%s", prof) >= sizeof(buf))
     return;
   ecore_file_mkdir(buf);
}

E_API void
e_config_profile_del(const char *prof)
{
   char buf[4096];
   if (e_user_dir_snprintf(buf, sizeof(buf), "config/%s", prof) >= sizeof(buf))
     return;
   ecore_file_recursive_rm(buf);
}

E_API void
e_config_save_block_set(int block)
{
   _e_config_save_block = block;
}

E_API int
e_config_save_block_get(void)
{
   return _e_config_save_block;
}

/**
 * Loads configurations from file located in the working profile
 * The configurations are stored in a struct declated by the
 * macros E_CONFIG_DD_NEW and E_CONFIG_<b>TYPE</b>
 *
 * @param domain of the configuration file.
 * @param edd to struct definition
 * @return returns allocated struct on success, if unable to find config returns null
 */
E_API void *
e_config_domain_load(const char *domain, E_Config_DD *edd)
{
   Eet_File *ef;
   char buf[4096];
   void *data = NULL;
   int i;

   e_user_dir_snprintf(buf, sizeof(buf), "config/%s/%s.cfg",
                       _e_config_profile, domain);
   ef = eet_open(buf, EET_FILE_MODE_READ);
   if (ef)
     {
        data = eet_data_read(ef, edd, "config");
        eet_close(ef);
        if (data) return data;
     }

   for (i = 1; i <= _e_config_revisions; i++)
     {
        e_user_dir_snprintf(buf, sizeof(buf), "config/%s/%s.%i.cfg",
                            _e_config_profile, domain, i);
        ef = eet_open(buf, EET_FILE_MODE_READ);
        if (ef)
          {
             data = eet_data_read(ef, edd, "config");
             eet_close(ef);
             if (data) return data;
          }
     }
   return e_config_domain_system_load(domain, edd);
}

E_API void *
e_config_domain_system_load(const char *domain, E_Config_DD *edd)
{
   Eet_File *ef;
   char buf[4096];
   void *data = NULL;

   e_prefix_data_snprintf(buf, sizeof(buf), "data/config/%s/%s.cfg",
                          _e_config_profile, domain);
   ef = eet_open(buf, EET_FILE_MODE_READ);
   if (ef)
     {
        data = eet_data_read(ef, edd, "config");
        eet_close(ef);
        return data;
     }

   return data;
}

static void
_e_config_mv_error(const char *from, const char *to)
{
   E_Dialog *dia;
   char buf[8192];

   if (_e_config_error_dialog) return;

   dia = e_dialog_new(NULL, "E", "_sys_error_logout_slow");
   EINA_SAFETY_ON_NULL_RETURN(dia);

   e_dialog_title_set(dia, _("Enlightenment Settings Write Problems"));
   e_dialog_icon_set(dia, "dialog-error", 64);
   snprintf(buf, sizeof(buf),
            _("Enlightenment has had an error while moving config files<br>"
              "from:<br>"
              "%s<br>"
              "<br>"
              "to:<br>"
              "%s<br>"
              "<br>"
              "The rest of the write has been aborted for safety.<br>"),
            from, to);
   e_dialog_text_set(dia, buf);
   e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
   e_dialog_button_focus_num(dia, 0);
   elm_win_center(dia->win, 1, 1);
   e_object_del_attach_func_set(E_OBJECT(dia),
                                _e_config_error_dialog_cb_delete);
   e_dialog_show(dia);
   _e_config_error_dialog = dia;
}

E_API int
e_config_profile_save(void)
{
   Eet_File *ef;
   char buf[4096], buf2[4096];
   int ok = 0;

   /* FIXME: check for other sessions fo E running */
   e_user_dir_concat_static(buf, "config/profile.cfg");
   e_user_dir_concat_static(buf2, "config/profile.cfg.tmp");

   ef = eet_open(buf2, EET_FILE_MODE_WRITE);
   if (ef)
     {
        ok = eet_write(ef, "config", _e_config_profile,
                       strlen(_e_config_profile), 0);
        if (_e_config_eet_close_handle(ef, buf2))
          {
             Eina_Bool ret = EINA_TRUE;

             if (_e_config_revisions > 0)
               {
                  int i;
                  char bsrc[4096], bdst[4096];

                  for (i = _e_config_revisions; i > 1; i--)
                    {
                       e_user_dir_snprintf(bsrc, sizeof(bsrc), "config/profile.%i.cfg", i - 1);
                       e_user_dir_snprintf(bdst, sizeof(bdst), "config/profile.%i.cfg", i);
                       if ((ecore_file_exists(bsrc)) &&
                           (ecore_file_size(bsrc)))
                         {
                            ret = ecore_file_mv(bsrc, bdst);
                            if (!ret)
                              {
                                 _e_config_mv_error(bsrc, bdst);
                                 break;
                              }
                         }
                    }
                  if (ret)
                    {
                       e_user_dir_snprintf(bsrc, sizeof(bsrc), "config/profile.cfg");
                       e_user_dir_snprintf(bdst, sizeof(bdst), "config/profile.1.cfg");
                       ecore_file_mv(bsrc, bdst);
//                       if (!ret)
//                          _e_config_mv_error(bsrc, bdst);
                    }
               }
             ret = ecore_file_mv(buf2, buf);
             if (!ret) _e_config_mv_error(buf2, buf);
          }
        ecore_file_unlink(buf2);
     }
   return ok;
}

/**
 * Saves configurations to file located in the working profile
 * The configurations are read from a struct declated by the
 * macros E_CONFIG_DD_NEW and E_CONFIG_<b>TYPE</b>
 *
 * @param domain  name of the configuration file.
 * @param edd pointer to struct definition
 * @param data struct to save as configuration file
 * @return 1 if save success, 0 on failure
 */
E_API int
e_config_domain_save(const char *domain, E_Config_DD *edd, const void *data)
{
   Eet_File *ef;
   char buf[4096], buf2[4096];
   int ok = 0, ret;
   size_t len, len2;

   if (_e_config_save_block) return 0;
   /* FIXME: check for other sessions fo E running */
   len = e_user_dir_snprintf(buf, sizeof(buf), "config/%s", _e_config_profile);
   if (len + 1 >= sizeof(buf)) return 0;

   ecore_file_mkdir(buf);

   buf[len] = '/';
   len++;

   len2 = eina_strlcpy(buf + len, domain, sizeof(buf) - len);
   if (len2 + sizeof(".cfg") >= sizeof(buf) - len) return 0;

   len += len2;

   memcpy(buf + len, ".cfg", sizeof(".cfg"));
   len += sizeof(".cfg") - 1;

   if (len + sizeof(".tmp") >= sizeof(buf)) return 0;
   memcpy(buf2, buf, len);
   memcpy(buf2 + len, ".tmp", sizeof(".tmp"));

   ef = eet_open(buf2, EET_FILE_MODE_WRITE);
   if (ef)
     {
        ok = eet_data_write(ef, edd, "config", data, 1);
        if (_e_config_eet_close_handle(ef, buf2))
          {
             if (_e_config_revisions > 0)
               {
                  int i;
                  char bsrc[4096], bdst[4096];

                  for (i = _e_config_revisions; i > 1; i--)
                    {
                       e_user_dir_snprintf(bsrc, sizeof(bsrc), "config/%s/%s.%i.cfg", _e_config_profile, domain, i - 1);
                       e_user_dir_snprintf(bdst, sizeof(bdst), "config/%s/%s.%i.cfg", _e_config_profile, domain, i);
                       if ((ecore_file_exists(bsrc)) &&
                           (ecore_file_size(bsrc)))
                         {
                            ecore_file_mv(bsrc, bdst);
                         }
                    }
                  e_user_dir_snprintf(bsrc, sizeof(bsrc), "config/%s/%s.cfg", _e_config_profile, domain);
                  e_user_dir_snprintf(bdst, sizeof(bdst), "config/%s/%s.1.cfg", _e_config_profile, domain);
                  ecore_file_mv(bsrc, bdst);
               }
             ret = ecore_file_mv(buf2, buf);
             if (!ret)
               ERR("*** Error saving config. ***");
          }
        ecore_file_unlink(buf2);
     }
   return ok;
}

E_API E_Config_Binding_Mouse *
e_config_binding_mouse_match(E_Config_Binding_Mouse *eb_in)
{
   Eina_List *l;
   E_Config_Binding_Mouse *eb;

   EINA_LIST_FOREACH(e_bindings->mouse_bindings, l, eb)
     {
        if ((eb->context == eb_in->context) &&
            (eb->button == eb_in->button) &&
            (eb->modifiers == eb_in->modifiers) &&
            (eb->any_mod == eb_in->any_mod) &&
            (((eb->action) && (eb_in->action) && (!strcmp(eb->action, eb_in->action))) ||
             ((!eb->action) && (!eb_in->action))) &&
            (((eb->params) && (eb_in->params) && (!strcmp(eb->params, eb_in->params))) ||
             ((!eb->params) && (!eb_in->params))))
          return eb;
     }
   return NULL;
}

E_API E_Config_Binding_Key *
e_config_binding_key_match(E_Config_Binding_Key *eb_in)
{
   Eina_List *l;
   E_Config_Binding_Key *eb;

   EINA_LIST_FOREACH(e_bindings->mouse_bindings, l, eb)
     {
        if ((eb->context == eb_in->context) &&
            (eb->modifiers == eb_in->modifiers) &&
            (eb->any_mod == eb_in->any_mod) &&
            (((eb->key) && (eb_in->key) && (!strcmp(eb->key, eb_in->key))) ||
             ((!eb->key) && (!eb_in->key))) &&
            (((eb->action) && (eb_in->action) && (!strcmp(eb->action, eb_in->action))) ||
             ((!eb->action) && (!eb_in->action))) &&
            (((eb->params) && (eb_in->params) && (!strcmp(eb->params, eb_in->params))) ||
             ((!eb->params) && (!eb_in->params))))
          return eb;
     }
   return NULL;
}

E_API E_Config_Binding_Edge *
e_config_binding_edge_match(E_Config_Binding_Edge *eb_in)
{
   Eina_List *l;
   E_Config_Binding_Edge *eb;

   EINA_LIST_FOREACH(e_bindings->edge_bindings, l, eb)
     {
        if ((eb->context == eb_in->context) &&
            (eb->modifiers == eb_in->modifiers) &&
            (eb->any_mod == eb_in->any_mod) &&
            (eb->edge == eb_in->edge) &&
            (eb->delay == eb_in->delay) &&
            (eb->drag_only == eb_in->drag_only) &&
            (((eb->action) && (eb_in->action) && (!strcmp(eb->action, eb_in->action))) ||
             ((!eb->action) && (!eb_in->action))) &&
            (((eb->params) && (eb_in->params) && (!strcmp(eb->params, eb_in->params))) ||
             ((!eb->params) && (!eb_in->params))))
          return eb;
     }
   return NULL;
}

E_API E_Config_Binding_Signal *
e_config_binding_signal_match(E_Config_Binding_Signal *eb_in)
{
   Eina_List *l;
   E_Config_Binding_Signal *eb;

   EINA_LIST_FOREACH(e_bindings->signal_bindings, l, eb)
     {
        if ((eb->context == eb_in->context) &&
            (eb->modifiers == eb_in->modifiers) &&
            (eb->any_mod == eb_in->any_mod) &&
            (((eb->signal) && (eb_in->signal) && (!strcmp(eb->signal, eb_in->signal))) ||
             ((!eb->signal) && (!eb_in->signal))) &&
            (((eb->source) && (eb_in->source) && (!strcmp(eb->source, eb_in->source))) ||
             ((!eb->source) && (!eb_in->source))) &&
            (((eb->action) && (eb_in->action) && (!strcmp(eb->action, eb_in->action))) ||
             ((!eb->action) && (!eb_in->action))) &&
            (((eb->params) && (eb_in->params) && (!strcmp(eb->params, eb_in->params))) ||
             ((!eb->params) && (!eb_in->params))))
          return eb;
     }
   return NULL;
}

E_API E_Config_Binding_Wheel *
e_config_binding_wheel_match(E_Config_Binding_Wheel *eb_in)
{
   Eina_List *l;
   E_Config_Binding_Wheel *eb;

   EINA_LIST_FOREACH(e_bindings->wheel_bindings, l, eb)
     {
        if ((eb->context == eb_in->context) &&
            (eb->direction == eb_in->direction) &&
            (eb->z == eb_in->z) &&
            (eb->modifiers == eb_in->modifiers) &&
            (eb->any_mod == eb_in->any_mod) &&
            (((eb->action) && (eb_in->action) && (!strcmp(eb->action, eb_in->action))) ||
             ((!eb->action) && (!eb_in->action))) &&
            (((eb->params) && (eb_in->params) && (!strcmp(eb->params, eb_in->params))) ||
             ((!eb->params) && (!eb_in->params))))
          return eb;
     }
   return NULL;
}

E_API E_Config_Binding_Acpi *
e_config_binding_acpi_match(E_Config_Binding_Acpi *eb_in)
{
   Eina_List *l;
   E_Config_Binding_Acpi *eb;

   EINA_LIST_FOREACH(e_bindings->acpi_bindings, l, eb)
     {
        if ((eb->context == eb_in->context) &&
            (eb->type == eb_in->type) &&
            (eb->status == eb_in->status) &&
            (((eb->action) && (eb_in->action) &&
              (!strcmp(eb->action, eb_in->action))) ||
             ((!eb->action) && (!eb_in->action))) &&
            (((eb->params) && (eb_in->params) &&
              (!strcmp(eb->params, eb_in->params))) ||
             ((!eb->params) && (!eb_in->params))))
          return eb;
     }
   return NULL;
}

E_API void
e_config_mode_changed(void)
{
   ecore_event_add(E_EVENT_CONFIG_MODE_CHANGED, NULL, NULL, NULL);
}

E_API void
e_config_binding_acpi_free(E_Config_Binding_Acpi *eba)
{
   if (!eba) return;
   eina_stringshare_del(eba->action);
   eina_stringshare_del(eba->params);
   free(eba);
}

E_API void
e_config_binding_key_free(E_Config_Binding_Key *ebk)
{
   if (!ebk) return;
   eina_stringshare_del(ebk->key);
   eina_stringshare_del(ebk->action);
   eina_stringshare_del(ebk->params);
   free(ebk);
}

E_API void
e_config_binding_edge_free(E_Config_Binding_Edge *ebe)
{
   if (!ebe) return;
   eina_stringshare_del(ebe->action);
   eina_stringshare_del(ebe->params);
   free(ebe);
}

E_API void
e_config_binding_mouse_free(E_Config_Binding_Mouse *ebm)
{
   if (!ebm) return;
   eina_stringshare_del(ebm->action);
   eina_stringshare_del(ebm->params);
   free(ebm);
}

E_API void
e_config_binding_wheel_free(E_Config_Binding_Wheel *ebw)
{
   if (!ebw) return;
   eina_stringshare_del(ebw->action);
   eina_stringshare_del(ebw->params);
   free(ebw);
}

E_API void
e_config_binding_signal_free(E_Config_Binding_Signal *ebs)
{
   if (!ebs) return;
   eina_stringshare_del(ebs->signal);
   eina_stringshare_del(ebs->source);
   eina_stringshare_del(ebs->action);
   eina_stringshare_del(ebs->params);
   free(ebs);
}

E_API void
e_config_bindings_free(E_Config_Bindings *ecb)
{
   if (!ecb) return;
   E_FREE_LIST(ecb->mouse_bindings, e_config_binding_mouse_free);
   E_FREE_LIST(ecb->key_bindings, e_config_binding_key_free);
   E_FREE_LIST(ecb->edge_bindings, e_config_binding_edge_free);
   E_FREE_LIST(ecb->signal_bindings, e_config_binding_signal_free);
   E_FREE_LIST(ecb->wheel_bindings, e_config_binding_wheel_free);
   E_FREE_LIST(ecb->acpi_bindings, e_config_binding_acpi_free);
   free(ecb);
}

/* local subsystem functions */
static void
_e_config_save_cb(void *data EINA_UNUSED)
{
   EINTERN void e_gadget_save(void);
   EINTERN void e_bryce_save(void);

   e_config_profile_save();
   e_module_save_all();
   elm_config_save();
   e_config_domain_save("e", e_config_descriptor_get(), e_config);
   e_config_domain_save("e_bindings", e_config_binding_descriptor_get(), e_bindings);
   if (E_EFL_VERSION_MINIMUM(1, 17, 99))
     {
        e_gadget_save();
        e_bryce_save();
     }
   _e_config_save_defer = NULL;
}

static void
_e_config_free(E_Config *ecf)
{
   E_Config_Syscon_Action *sca;
   E_Font_Fallback *eff;
   E_Config_Module *em;
   E_Font_Default *efd;
   E_Color_Class *cc;
   E_Path_Dir *epd;
   E_Remember *rem;
   E_Config_Env_Var *evr;
   E_Config_XKB_Option *op;
   E_Int_Menu_Applications *ema;

   if (!ecf) return;

   eina_stringshare_del(ecf->xkb.default_model);

   E_FREE_LIST(ecf->xkb.used_layouts, e_config_xkb_layout_free);
   EINA_LIST_FREE(ecf->xkb.used_options, op)
     {
        eina_stringshare_del(op->name);
        E_FREE(op);
     }

   EINA_LIST_FREE(ecf->modules, em)
     {
        if (em->name) eina_stringshare_del(em->name);
        E_FREE(em);
     }
   EINA_LIST_FREE(ecf->font_fallbacks, eff)
     {
        if (eff->name) eina_stringshare_del(eff->name);
        E_FREE(eff);
     }
   EINA_LIST_FREE(ecf->font_defaults, efd)
     {
        if (efd->text_class) eina_stringshare_del(efd->text_class);
        if (efd->font) eina_stringshare_del(efd->font);
        E_FREE(efd);
     }
   EINA_LIST_FREE(ecf->path_append_data, epd)
     {
        if (epd->dir) eina_stringshare_del(epd->dir);
        E_FREE(epd);
     }
   EINA_LIST_FREE(ecf->path_append_images, epd)
     {
        if (epd->dir) eina_stringshare_del(epd->dir);
        E_FREE(epd);
     }
   EINA_LIST_FREE(ecf->path_append_fonts, epd)
     {
        if (epd->dir) eina_stringshare_del(epd->dir);
        E_FREE(epd);
     }
   EINA_LIST_FREE(ecf->path_append_init, epd)
     {
        if (epd->dir) eina_stringshare_del(epd->dir);
        E_FREE(epd);
     }
   EINA_LIST_FREE(ecf->path_append_icons, epd)
     {
        if (epd->dir) eina_stringshare_del(epd->dir);
        E_FREE(epd);
     }
   EINA_LIST_FREE(ecf->path_append_modules, epd)
     {
        if (epd->dir) eina_stringshare_del(epd->dir);
        E_FREE(epd);
     }
   EINA_LIST_FREE(ecf->path_append_backgrounds, epd)
     {
        if (epd->dir) eina_stringshare_del(epd->dir);
        E_FREE(epd);
     }
   EINA_LIST_FREE(ecf->path_append_messages, epd)
     {
        if (epd->dir) eina_stringshare_del(epd->dir);
        E_FREE(epd);
     }
   EINA_LIST_FREE(ecf->remembers, rem)
     {
        if (rem->name) eina_stringshare_del(rem->name);
        if (rem->class) eina_stringshare_del(rem->class);
        if (rem->title) eina_stringshare_del(rem->title);
        if (rem->role) eina_stringshare_del(rem->role);
        if (rem->prop.border) eina_stringshare_del(rem->prop.border);
        if (rem->prop.command) eina_stringshare_del(rem->prop.command);
        E_FREE(rem);
     }
   EINA_LIST_FREE(ecf->menu_applications, ema)
     {
        if (ema->orig_path) eina_stringshare_del(ema->orig_path);
        if (ema->try_exec) eina_stringshare_del(ema->try_exec);
        if (ema->exec) eina_stringshare_del(ema->exec);
        E_FREE(ema);
     }
   EINA_LIST_FREE(ecf->color_classes, cc)
     {
        if (cc->name) eina_stringshare_del(cc->name);
        E_FREE(cc);
     }
   if (ecf->desktop_default_background) eina_stringshare_del(ecf->desktop_default_background);
   if (ecf->desktop_default_name) eina_stringshare_del(ecf->desktop_default_name);
   if (ecf->language) eina_stringshare_del(ecf->language);
   eina_stringshare_del(ecf->desklock_language);
   eina_stringshare_del(ecf->xkb.selected_layout);
   eina_stringshare_del(ecf->xkb.cur_layout);
   eina_stringshare_del(ecf->xkb.desklock_layout);
   e_config_xkb_layout_free(ecf->xkb.current_layout);
   e_config_xkb_layout_free(ecf->xkb.sel_layout);
   e_config_xkb_layout_free(ecf->xkb.lock_layout);
   eina_stringshare_del(ecf->desk_flip_animate_type);
   if (ecf->transition_start) eina_stringshare_del(ecf->transition_start);
   if (ecf->transition_desk) eina_stringshare_del(ecf->transition_desk);
   if (ecf->transition_change) eina_stringshare_del(ecf->transition_change);
   if (ecf->input_method) eina_stringshare_del(ecf->input_method);
   if (ecf->exebuf_term_cmd) eina_stringshare_del(ecf->exebuf_term_cmd);
   if (ecf->icon_theme) eina_stringshare_del(ecf->icon_theme);
   if (ecf->desktop_environment) eina_stringshare_del(ecf->desktop_environment);
   if (ecf->wallpaper_import_last_dev) eina_stringshare_del(ecf->wallpaper_import_last_dev);
   if (ecf->wallpaper_import_last_path) eina_stringshare_del(ecf->wallpaper_import_last_path);
   if (ecf->theme_default_border_style) eina_stringshare_del(ecf->theme_default_border_style);
   if (ecf->desklock_custom_desklock_cmd) eina_stringshare_del(ecf->desklock_custom_desklock_cmd);
   EINA_LIST_FREE(ecf->syscon.actions, sca)
     {
        if (sca->action) eina_stringshare_del(sca->action);
        if (sca->params) eina_stringshare_del(sca->params);
        if (sca->button) eina_stringshare_del(sca->button);
        if (sca->icon) eina_stringshare_del(sca->icon);
        E_FREE(sca);
     }
   EINA_LIST_FREE(ecf->env_vars, evr)
     {
        if (evr->var) eina_stringshare_del(evr->var);
        if (evr->val) eina_stringshare_del(evr->val);
        E_FREE(evr);
     }
   if (ecf->xsettings.net_icon_theme_name)
     eina_stringshare_del(ecf->xsettings.net_icon_theme_name);
   if (ecf->xsettings.net_theme_name)
     eina_stringshare_del(ecf->xsettings.net_theme_name);
   if (ecf->xsettings.gtk_font_name)
     eina_stringshare_del(ecf->xsettings.gtk_font_name);
   if (ecf->backlight.sysdev)
     eina_stringshare_del(ecf->backlight.sysdev);

   E_FREE(ecf);
}

static Eina_Bool
_e_config_cb_timer(void *data)
{
   e_util_dialog_show(_("Settings Upgraded"), "%s", (char *)data);
   return 0;
}

static int
_e_config_eet_close_handle(Eet_File *ef, char *file)
{
   Eet_Error err;
   char *erstr = NULL;

   err = eet_close(ef);
   switch (err)
     {
      case EET_ERROR_NONE:
        /* all good - no error */
        break;

      case EET_ERROR_BAD_OBJECT:
        erstr = _("The EET file handle is bad.");
        break;

      case EET_ERROR_EMPTY:
        erstr = _("The file data is empty.");
        break;

      case EET_ERROR_NOT_WRITABLE:
        erstr = _("The file is not writable. Perhaps the disk is read-only<br>or you lost permissions to your files.");
        break;

      case EET_ERROR_OUT_OF_MEMORY:
        erstr = _("Memory ran out while preparing the write.<br>Please free up memory.");
        break;

      case EET_ERROR_WRITE_ERROR:
        erstr = _("This is a generic error.");
        break;

      case EET_ERROR_WRITE_ERROR_FILE_TOO_BIG:
        erstr = _("The settings file is too large.<br>It should be very small (a few hundred KB at most).");
        break;

      case EET_ERROR_WRITE_ERROR_IO_ERROR:
        erstr = _("You have I/O errors on the disk.<br>Maybe it needs replacing?");
        break;

      case EET_ERROR_WRITE_ERROR_OUT_OF_SPACE:
        erstr = _("You ran out of space while writing the file.");
        break;

      case EET_ERROR_WRITE_ERROR_FILE_CLOSED:
        erstr = _("The file was closed while writing.");
        break;

      case EET_ERROR_MMAP_FAILED:
        erstr = _("Memory-mapping (mmap) of the file failed.");
        break;

      case EET_ERROR_X509_ENCODING_FAILED:
        erstr = _("X509 Encoding failed.");
        break;

      case EET_ERROR_SIGNATURE_FAILED:
        erstr = _("Signature failed.");
        break;

      case EET_ERROR_INVALID_SIGNATURE:
        erstr = _("The signature was invalid.");
        break;

      case EET_ERROR_NOT_SIGNED:
        erstr = _("Not signed.");
        break;

      case EET_ERROR_NOT_IMPLEMENTED:
        erstr = _("Feature not implemented.");
        break;

      case EET_ERROR_PRNG_NOT_SEEDED:
        erstr = _("PRNG was not seeded.");
        break;

      case EET_ERROR_ENCRYPT_FAILED:
        erstr = _("Encryption failed.");
        break;

      case EET_ERROR_DECRYPT_FAILED:
        erstr = _("Decryption failed.");
        break;

      default: /* if we get here eet added errors we don't know */
        erstr = _("The error is unknown to Enlightenment.");
        break;
     }
   if (erstr)
     {
        /* delete any partially-written file */
        ecore_file_unlink(file);
        /* only show dialog for first error - further ones are likely */
        /* more of the same error */
        if (!_e_config_error_dialog)
          {
             E_Dialog *dia;

             dia = e_dialog_new(NULL, "E", "_sys_error_logout_slow");
             if (dia)
               {
                  char buf[8192];

                  e_dialog_title_set(dia, _("Enlightenment Settings Write Problems"));
                  e_dialog_icon_set(dia, "dialog-error", 64);
                  snprintf(buf, sizeof(buf),
                           _("Enlightenment has had an error while writing<br>"
                             "its config file.<br>"
                             "%s<br>"
                             "<br>"
                             "The file where the error occurred was:<br>"
                             "%s<br>"
                             "<br>"
                             "This file has been deleted to avoid corrupt data.<br>"),
                           erstr, file);
                  e_dialog_text_set(dia, buf);
                  e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
                  e_dialog_button_focus_num(dia, 0);
                  elm_win_center(dia->win, 1, 1);
                  e_object_del_attach_func_set(E_OBJECT(dia),
                                               _e_config_error_dialog_cb_delete);
                  e_dialog_show(dia);
                  _e_config_error_dialog = dia;
               }
          }
        return 0;
     }
   return 1;
}

