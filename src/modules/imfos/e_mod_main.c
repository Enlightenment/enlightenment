#include <e.h>
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "imfos_v4l.h"

#define IMFOS_TIMEOUT 10

static E_Config_DD *_imfos_conf = NULL;

static Imfos_Config *_imfos_config = NULL;
static E_Config_DD *_imfos_config_edd = NULL;

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Imfos" };

static void _e_mod_imfos(void);

static Eina_Bool
_run(void *data EINA_UNUSED)
{
   _e_mod_imfos();
}

static void
_e_mod_imfos(void)
{
   Imfos_V4l_Conf *conf;

   conf = calloc(1, sizeof(Imfos_V4l_Conf));
   conf->timeout = IMFOS_TIMEOUT;
   imfos_v4l_run(conf);
}

EAPI void *
e_modapi_init(E_Module *m)
{
   printf("Loading ...\n");
   imfos_v4l_init();

   _imfos_config_edd = E_CONFIG_DD_NEW("Config", Imfos_Config);
   E_CONFIG_VAL(_imfos_config_edd, Imfos_Config, orient, INT);
   e_configure_registry_category_add("advanced", 80, _("Advanced"), NULL, "preferences-advanced");
   e_configure_registry_item_add("advanced/imfos", 20, "Imfos", NULL, "preference-imfos", e_mod_config_imfos);
   ecore_timer_add(50.0, _run, NULL);
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   E_CONFIG_DD_FREE(_imfos_config_edd);
   free(_imfos_config);
   imfos_v4l_shutdown();
   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}

