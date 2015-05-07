#include "e_mod_main.h"
#include "sha1.h"

EINTERN int _e_teamwork_log_dom = -1;
EINTERN Mod *tw_mod = NULL;
EINTERN Teamwork_Config *tw_config = NULL;

E_API E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Teamwork"};

static E_Config_DD *conf_edd = NULL;

static E_Action *e_tw_toggle = NULL;
static const char _act_toggle[] = "tw_toggle";
static const char _e_tw_name[] = N_("Teamwork");
static const char _lbl_toggle[] = N_("Toggle Popup Visibility");

static const char *
_sha1_to_string(const unsigned char *hashout)
{
   const char hextab[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
   char sha1[41] = {0};
   unsigned int i = 0;

   for (i = 0; i < 20; i++)
     {
        sha1[2 * i] = hextab[(hashout[i] >> 4) & 0x0f];
        sha1[2 * i + 1] = hextab[hashout[i] & 0x0f];
     }
   return eina_stringshare_add(sha1);
}

const char *
sha1_encode(const unsigned char *data, size_t len)
{
   SHA_CTX2 ctx;
   unsigned char hashout[20];
   unsigned char *buf;

   if (EINA_UNLIKELY(len > 65000))
     buf = malloc(len);
   else
     buf = alloca(len);
   EINA_SAFETY_ON_NULL_RETURN_VAL(buf, NULL);
   memcpy(buf, data, len);

   SHA1_Init2(&ctx);
   SHA1_Update2(&ctx, buf, len);
   SHA1_Final2(hashout, &ctx);
   if (EINA_UNLIKELY(len > 65000)) free(buf);
   return _sha1_to_string(hashout);
}

//////////////////////////////
static Teamwork_Config *
e_tw_config_new(void)
{
   Teamwork_Config *cf;

   cf = E_NEW(Teamwork_Config, 1);
   cf->config_version = MOD_CONFIG_FILE_VERSION;

   cf->allowed_media_size = 10; // 10 megabytes
   cf->allowed_media_fetch_size = 5; // 5 megabytes
   cf->allowed_media_age = 3; // 3 days

   cf->mouse_out_delay = 0.0; // hide instantly
   cf->popup_size = 60.0; // 60% screen size
   cf->popup_opacity = 90.0; // 90% opacity

   return cf;
}

static E_Config_DD *
e_tw_config_dd_new(void)
{
   conf_edd = E_CONFIG_DD_NEW("Teamwork_Config", Teamwork_Config);

#undef T
#undef D
#define T Teamwork_Config
#define D conf_edd
   E_CONFIG_VAL(D, T, config_version, UINT);
   E_CONFIG_VAL(D, T, disable_media_fetch, UCHAR);
   E_CONFIG_VAL(D, T, disable_video, UCHAR);
   E_CONFIG_VAL(D, T, allowed_media_size, LL);
   E_CONFIG_VAL(D, T, allowed_media_fetch_size, LL);
   E_CONFIG_VAL(D, T, allowed_media_age, INT);

   E_CONFIG_VAL(D, T, mouse_out_delay, DOUBLE);
   E_CONFIG_VAL(D, T, popup_size, DOUBLE);
   E_CONFIG_VAL(D, T, popup_opacity, DOUBLE);
   return conf_edd;
}
//////////////////////////////
static void
e_tw_act_toggle_cb(E_Object *obj EINA_UNUSED, const char *params)
{
   if (tw_mod->pop)
     {
        if (tw_mod->sticky)
          tw_hide(NULL);
        tw_mod->sticky = !tw_mod->sticky;
        return;
     }
   if (params && params[0])
     {
        tw_uri_show(params);
        tw_mod->sticky = 1;
     }
}
//////////////////////////////
static void
_e_modapi_shutdown(void)
{
   e_tw_shutdown();

   E_CONFIG_DD_FREE(conf_edd);
   eina_log_domain_unregister(_e_teamwork_log_dom);
   _e_teamwork_log_dom = -1;

   e_configure_registry_item_del("applications/teamwork");
   e_configure_registry_category_del("applications");

   e_action_predef_name_del(_e_tw_name, _lbl_toggle);
   e_action_del(_act_toggle);
   e_tw_toggle = NULL;

   E_FREE(tw_config);
   E_FREE(tw_mod);
}

E_API void *
e_modapi_init(E_Module *m)
{
   char buf[PATH_MAX];

   snprintf(buf, sizeof(buf), "%s/e-module-teamwork.edj", e_module_dir_get(m));
   e_configure_registry_category_add("applications", 20, _("Apps"), NULL,
                                     "preferences-applications");
   e_configure_registry_item_add("applications/teamwork", 1, _("Teamwork"), NULL,
                                 buf, e_int_config_teamwork_module);

   tw_mod = E_NEW(Mod, 1);
   tw_mod->module = m;
   m->data = tw_mod;
   conf_edd = e_tw_config_dd_new();
   tw_config = e_config_domain_load("module.teamwork", conf_edd);
   if (tw_config)
     {
        if (!e_util_module_config_check(_("Teamwork"), tw_config->config_version, MOD_CONFIG_FILE_VERSION))
          E_FREE_FUNC(tw_config, free);
     }

   if (tw_config)
     {
        /* sanity checks */
        tw_config->mouse_out_delay = E_CLAMP(tw_config->mouse_out_delay, 0.0, 5.0);
        tw_config->popup_size = E_CLAMP(tw_config->popup_size, 10.0, 100.0);
        tw_config->popup_opacity = E_CLAMP(tw_config->popup_opacity, 10.0, 100.0);
        tw_config->allowed_media_fetch_size = E_CLAMP(tw_config->allowed_media_fetch_size, 1, 50);
     }
   else
     tw_config = e_tw_config_new();
   tw_config->config_version = MOD_CONFIG_FILE_VERSION;

   _e_teamwork_log_dom = eina_log_domain_register("teamwork", EINA_COLOR_ORANGE);
   eina_log_domain_level_set("teamwork", EINA_LOG_LEVEL_DBG);

   if (!e_tw_init())
     {
        _e_modapi_shutdown();
        return NULL;
     }
   e_tw_toggle = e_action_add(_act_toggle);
   e_tw_toggle->func.go = e_tw_act_toggle_cb;
   e_action_predef_name_set(_e_tw_name, _lbl_toggle, _act_toggle, NULL, NULL, 1);

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   _e_modapi_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.teamwork", conf_edd, tw_config);
   return 1;
}

