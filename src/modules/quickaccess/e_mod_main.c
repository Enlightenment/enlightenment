#include "e_mod_main.h"

EINTERN int _e_quick_access_log_dom = -1;
static E_Config_DD *conf_edd = NULL;
Mod *qa_mod = NULL;
Config *qa_config = NULL;

/**
 * in priority order:
 *
 * @todo config (see e_mod_config.c)
 *
 * @todo custom border based on E_Quick_Access_Entry_Mode/E_Gadcon_Orient
 *
 * @todo show/hide effects:
 *        - fullscreen
 *        - centered
 *        - slide from top, bottom, left or right
 *
 * @todo match more than one, doing tabs (my idea is to do another
 *       tabbing module first, experiment with that, maybe use/reuse
 *       it here)
 */

EAPI E_Module_Api e_modapi = {E_MODULE_API_VERSION, "Quickaccess"};

//////////////////////////////
EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[PATH_MAX];
   E_Configure_Option *co;

   snprintf(buf, sizeof(buf), "%s/e-module-quickaccess.edj", e_module_dir_get(m));
   e_configure_registry_category_add("launcher", 80, _("Launcher"), NULL,
                                     "modules-launcher");
   e_configure_registry_item_add("launcher/quickaccess", 1, _("Quickaccess"), NULL,
                                 buf, e_int_config_qa_module);

   qa_mod = E_NEW(Mod, 1);
   qa_mod->module = m;
   m->data = qa_mod;
   e_module_delayed_set(m, 0);
   conf_edd = e_qa_config_dd_new();
   qa_config = e_config_domain_load("module.quickaccess", conf_edd);
   if (qa_config)
     {
        if (!e_util_module_config_check(_("Quickaccess"), qa_config->config_version, MOD_CONFIG_FILE_VERSION))
          {
             e_qa_config_free(qa_config);
             qa_config = NULL;
          }
     }

   if (!qa_config) qa_config = e_qa_config_new();
   qa_config->config_version = MOD_CONFIG_FILE_VERSION;

   _e_quick_access_log_dom = eina_log_domain_register("quickaccess", EINA_COLOR_ORANGE);
   eina_log_domain_level_set("quickaccess", EINA_LOG_LEVEL_DBG);

   if (!e_qa_init())
     {
        e_modapi_shutdown(NULL);
        return NULL;
     }

   e_configure_option_domain_current_set("quickaccess");

   E_CONFIGURE_OPTION_ADD_CUSTOM(co, "settings", _("Quickaccess settings panel"), _("quickaccess"), _("border"));
   co->info = eina_stringshare_add("launcher/quickaccess");
   E_CONFIGURE_OPTION_ICON(co, buf);
   E_CONFIGURE_OPTION_ADD(co, BOOL, hide_when_behind, qa_config, _("Hide windows on activate instead of raising"), _("quickaccess"), _("border"));
   E_CONFIGURE_OPTION_HELP(co, _("By default, activating a Quickaccess binding when the window is behind other windows will raise the window. "
                           "This option changes that behavior to hide the window instead."));
   E_CONFIGURE_OPTION_ADD(co, BOOL, autohide, qa_config, _("Hide windows when focus is lost"), _("quickaccess"), _("border"), _("focus"));
   E_CONFIGURE_OPTION_HELP(co, _("This option causes Quickaccess windows to automatically hide when they lose focus"));
   E_CONFIGURE_OPTION_ADD(co, BOOL, skip_taskbar, qa_config, _("Skip taskbar"), _("quickaccess"), _("border"));
   E_CONFIGURE_OPTION_HELP(co, _("This option causes Quickaccess windows to not show up in taskbars"));
   E_CONFIGURE_OPTION_ADD(co, BOOL, skip_pager, qa_config, _("Skip pager"), _("quickaccess"), _("border"));
   E_CONFIGURE_OPTION_HELP(co, _("This option causes Quickaccess windows to not show up in pagers"));

   e_configure_option_category_tag_add(_("windows"), _("quickaccess"));
   e_configure_option_category_tag_add(_("quickaccess"), _("quickaccess"));
   e_configure_option_category_icon_set(_("quickaccess"), buf);

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   e_qa_shutdown();

   conf_edd = e_qa_config_dd_free();
   eina_log_domain_unregister(_e_quick_access_log_dom);
   _e_quick_access_log_dom = -1;

   e_configure_registry_item_del("launcher/quickaccess");
   e_configure_registry_category_del("launcher");

   e_configure_option_domain_clear("quickaccess");
   e_configure_option_category_tag_del(_("quickaccess"), _("quickaccess"));
   e_configure_option_category_tag_del(_("windows"), _("quickaccess"));

   e_qa_config_free(qa_config);
   E_FREE(qa_mod);
   qa_config = NULL;
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   e_config_domain_save("module.quickaccess", conf_edd, qa_config);
   return 1;
}

