#include "e.h"
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "e_mod_comp.h"

static Eina_Inlist *cfg_opts = NULL;
//static Ecore_Event_Handler *init_done_handler = NULL;

//static int
//_e_init_done(void *data, int type, void *event)
//{
//   ecore_event_handler_del(init_done_handler);
//   init_done_handler = NULL;
//   if (!e_mod_comp_init())
//     {
//        // FIXME: handle if comp init fails
//     }
//   return 1;
//}

/* module private routines */
Mod *_comp_mod = NULL;

/* public module routines. all modules must have these */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Composite"
};

static Eina_List *
_e_mod_engine_info_cb(E_Configure_Option *co)
{
   Eina_List *ret = NULL;
   E_Configure_Option_Info *oi;
   int x;
   const char *name[] =
   {
    "Software",
    NULL
   };

   if (!getenv("ECORE_X_NO_XLIB"))
     {
        if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_OPENGL_X11))
          {
             name[1] = "OpenGL";
          }
     }

   for (x = ENGINE_SW; x <= ENGINE_GL; x++)
     {
        if (!name[x - 1]) continue;
        oi = e_configure_option_info_new(co, _(name[x - 1]), (intptr_t*)(long)x);
        oi->current = (*(int*)co->valptr == x);
        ret = eina_list_append(ret, oi);
     }
   return ret;
}

EAPI void *
e_modapi_init(E_Module *m)
{
   Mod *mod;
   char buf[4096];

   mod = calloc(1, sizeof(Mod));
   m->data = mod;

   mod->module = m;
   snprintf(buf, sizeof(buf), "%s/e-module-comp.edj", e_module_dir_get(m));
   e_configure_registry_category_add("appearance", 10, _("Look"), NULL,
                                     "preferences-look");
   e_configure_registry_item_add("appearance/comp", 120, _("Composite"), NULL,
                                 buf, e_int_config_comp_module);

   e_mod_comp_cfdata_edd_init(&(mod->conf_edd),
                              &(mod->conf_match_edd));

   mod->conf = e_config_domain_load("module.comp", mod->conf_edd);
   if (mod->conf)
     {
        mod->conf->max_unmapped_pixels = 32 * 1024;
        mod->conf->keep_unmapped = 1;
     }
   else _e_mod_config_new(m);
   
   /* force some config vals off */
   mod->conf->lock_fps = 0;
   mod->conf->indirect = 0;

   if (!e_config->use_composite)
     {
        e_config->use_composite = 1;
        e_config_save_queue();
     }

   /* XXX: update old configs. add config versioning */
   if (mod->conf->first_draw_delay == 0)
     mod->conf->first_draw_delay = 0.20;

   _comp_mod = mod;

   if (!e_mod_comp_init())
     {
        // FIXME: handle if comp init fails
     }

   e_module_delayed_set(m, 0);
   e_module_priority_set(m, -1000);

   {
      E_Configure_Option *co;

      E_CONFIGURE_OPTION_ADD(co, CUSTOM, engine, mod->conf, "Composite settings panel", _("composite"), _("border"));
      co->info = eina_stringshare_add("appearance/comp");
      E_CONFIGURE_OPTION_ICON(co, buf);
      cfg_opts = eina_inlist_append(cfg_opts, EINA_INLIST_GET(co));
      E_CONFIGURE_OPTION_ADD(co, BOOL, vsync, mod->conf, "Tear-free compositing (VSYNC)", _("composite"), _("border"));
      co->requires_restart = 1;
      cfg_opts = eina_inlist_append(cfg_opts, EINA_INLIST_GET(co));
      E_CONFIGURE_OPTION_ADD(co, BOOL, smooth_windows, mod->conf, "Smooth scaling of composited window content", _("composite"), _("border"));
      co->funcs[1].none = co->funcs[0].none = e_mod_comp_shadow_set;
      cfg_opts = eina_inlist_append(cfg_opts, EINA_INLIST_GET(co));
      E_CONFIGURE_OPTION_ADD(co, BOOL, nocomp_fs, mod->conf, "Don't composite fullscreen windows", _("composite"), _("border"));
      co->funcs[1].none = co->funcs[0].none = e_mod_comp_shadow_set;
      cfg_opts = eina_inlist_append(cfg_opts, EINA_INLIST_GET(co));
      E_CONFIGURE_OPTION_ADD(co, ENUM, engine, mod->conf, "Compositing engine", _("composite"), _("border"));
      co->info_cb = _e_mod_engine_info_cb;
      co->requires_restart = 1;
      cfg_opts = eina_inlist_append(cfg_opts, EINA_INLIST_GET(co));

      e_configure_option_category_tag_add(_("windows"), _("composite"));
      e_configure_option_category_tag_add(_("composite"), _("composite"));
      e_configure_option_category_icon_set(_("composite"), buf);
   }
   
   return mod;
}

void
_e_mod_config_new(E_Module *m)
{
   Mod *mod = m->data;

   mod->conf = e_mod_comp_cfdata_config_new();
}

void
_e_mod_config_free(E_Module *m)
{
   Mod *mod = m->data;

   e_mod_cfdata_config_free(mod->conf);
   mod->conf = NULL;
}

EAPI int
e_modapi_shutdown(E_Module *m)
{
   Mod *mod = m->data;

   e_mod_comp_shutdown();

   e_configure_registry_item_del("appearance/comp");
   e_configure_registry_category_del("appearance");

   if (mod->config_dialog)
     {
        e_object_del(E_OBJECT(mod->config_dialog));
        mod->config_dialog = NULL;
     }

   E_CONFIGURE_OPTION_LIST_CLEAR(cfg_opts);
   e_configure_option_category_tag_del(_("composite"), _("composite"));
   e_configure_option_category_tag_del(_("windows"), _("composite"));
   
   _e_mod_config_free(m);

   E_CONFIG_DD_FREE(mod->conf_match_edd);
   E_CONFIG_DD_FREE(mod->conf_edd);
   free(mod);

   if (mod == _comp_mod) _comp_mod = NULL;

   return 1;
}

EAPI int
e_modapi_save(E_Module *m)
{
   Mod *mod = m->data;
   e_config_domain_save("module.comp", mod->conf_edd, mod->conf);
   return 1;
}

