#include <e.h>

static Eina_Bool _ask_user_migration(void *data);
static Eina_Bool _ask_user_valid(void *data);
static void _migrate_user_cb(void *data, E_Dialog *dia);
static void _migrate_user_ok_cb(void *data, E_Dialog *dia);
static void _migrate_user_cancel_cb(void *data, E_Dialog *dia);

typedef enum _Bryce_Migration_Step
{
   BRYCE_MIGRATION_ASK = 0,
   BRYCE_MIGRATION_RUN,
   BRYCE_MIGRATION_DONE
} Bryce_Migration_Step;

typedef struct _Config
{
   Bryce_Migration_Step step;
} Config;

static Config *_bryce_migration_config = NULL;
static E_Config_DD *_conf_edd = NULL;


E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Bryce migration"
};

E_API void *
e_modapi_init(E_Module *m)
{
   _conf_edd = E_CONFIG_DD_NEW("Config", Config);
#undef T
#undef D
#define T Config
#define D _conf_edd
   E_CONFIG_VAL(D, T, step, UINT);
   _bryce_migration_config =  e_config_domain_load("module.bryce_migration", _conf_edd);
   if (!_bryce_migration_config)
     _bryce_migration_config = E_NEW(Config, 1);



   switch(_bryce_migration_config->step)
   {
    case BRYCE_MIGRATION_ASK:
       ecore_timer_add(1.0, _ask_user_migration, NULL);
       break;
    case BRYCE_MIGRATION_RUN:
       ecore_timer_add(1.0, _ask_user_valid, NULL);
       break;
    case BRYCE_MIGRATION_DONE:
       fprintf(stderr, "migration done\n");
       break;
   }


   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   E_FREE(_bryce_migration_config);
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.bryce_migration", _conf_edd, _bryce_migration_config);
   return 1;
}

static Eina_Bool
_ask_user_migration(void *data)
{
   E_Dialog *dia;

   dia = e_dialog_new(NULL, "E", "_module_bryce_migration");

   e_dialog_title_set(dia, "Bryce Migration");
   e_dialog_icon_set(dia, "enlightenment", 64);
   e_dialog_text_set(dia, "Hi, shelf are deprecatede</br>We Will try to update your config to the new gadget container (bryce). Hope this run works well for you !");
   e_dialog_button_add(dia, _("Migrate"), NULL, _migrate_user_cb, NULL);
   e_dialog_button_add(dia, _("Later"), NULL, NULL, NULL);
   elm_win_center(dia->win, 1, 1);
   e_win_no_remember_set(dia->win, 1);
   e_dialog_show(dia);

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_ask_user_valid(void *data)
{
   E_Dialog *dia;

   dia = e_dialog_new(NULL, "E", "_module_bryce_migration");

   e_dialog_title_set(dia, "Bryce Migration");
   e_dialog_icon_set(dia, "enlightenment", 64);
   e_dialog_text_set(dia, _("Would you keep this setup ?"));
   e_dialog_button_add(dia, _("Keep this config"), NULL, _migrate_user_ok_cb, NULL);
   e_dialog_button_add(dia, _("Reset to my old Config"), NULL, _migrate_user_cancel_cb, NULL);
   elm_win_center(dia->win, 1, 1);
   e_win_no_remember_set(dia->win, 1);
   e_dialog_show(dia);

   return ECORE_CALLBACK_CANCEL;
}



static void
_migrate_user_cb(void *data, E_Dialog *dia)
{
   char buf[4096];
   char buf2[4096];
   char cmd[PATH_MAX];

   e_object_del(E_OBJECT(dia));
   e_user_dir_snprintf(buf, sizeof(buf), "config/%s", e_config_profile_get());
   e_user_dir_snprintf(buf2, sizeof(buf2), "config/bryce_backup");
   snprintf(cmd, sizeof(cmd), "rm -rf %s", buf2);
   fprintf(stderr, "exec -> %s\n", cmd);
   system(cmd);
   snprintf(cmd, sizeof(cmd), "/usr/bin/cp -a %s %s", buf, buf2);
   fprintf(stderr, "exec -> %s\n", cmd);
   system(cmd);
   fprintf(stderr, "Backup current profile %s\n", cmd);
   _bryce_migration_config->step = BRYCE_MIGRATION_RUN;
   e_config_save();
   e_config_save_flush();
   e_config_save_block_set(EINA_TRUE);

   snprintf(cmd, sizeof(cmd),
            "/usr/lib/enlightenment/utils/enlightenment_bryce_migration "
            "--path %s ", buf);
   system(cmd);
   kill(getpid(), SIGKILL);
}

static void
_migrate_user_ok_cb(void *data, E_Dialog *dia)
{
   E_Module *m;
   e_object_del(E_OBJECT(dia));
   _bryce_migration_config->step = BRYCE_MIGRATION_DONE;
   m = e_module_find("bryce_migration");
   e_module_disable(m);
   e_config_save_queue();
}

static void
_migrate_user_cancel_cb(void *data, E_Dialog *dia)
{
   char buf[4096];
   char buf2[4096];
   char cmd[PATH_MAX];

   _bryce_migration_config->step = BRYCE_MIGRATION_ASK;
   e_config_save();
   e_config_save_flush();
   e_object_del(E_OBJECT(dia));
   e_config_save_block_set(EINA_TRUE);
   fprintf(stderr, "Reset config\n");
   e_user_dir_snprintf(buf, sizeof(buf), "config/%s", e_config_profile_get());
   e_user_dir_snprintf(buf2, sizeof(buf2), "config/bryce_backup");
   fprintf(stderr, "exec -> %s\n", cmd);
   snprintf(cmd, sizeof(cmd), "rm -rf %s", buf);
   system(cmd);
   snprintf(cmd, sizeof(cmd), "/usr/bin/cp -a %s %s", buf2, buf);
   system(cmd);
   snprintf(cmd, sizeof(cmd), "rm -rf %s", buf2);
   system(cmd);
   kill(getpid(), SIGKILL);
}

