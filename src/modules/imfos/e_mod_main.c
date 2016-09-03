#include "e_mod_main.h"
#include "e_mod_config.h"
#include "imfos_v4l.h"
#include "imfos_devices.h"

#define IMFOS_TIMEOUT 10

static E_Config_DD *_conf_edd = NULL;
static E_Config_DD *_conf_edd_device = NULL;
static Ecore_Timer *_imfos_timer = NULL;

Imfos_Config *imfos_config = NULL;
static E_Config_DD *imfos_config_edd = NULL;

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Imfos" };

static void _e_mod_imfos(void);
static Eina_Bool _run(void *);

static Eina_Bool
_run(void *data EINA_UNUSED)
{
//   imfos_v4l_run(imfos_config);
}

EAPI void *
e_modapi_init(E_Module *m)
{
   printf("Loading ...\n");
   _conf_edd = E_CONFIG_DD_NEW("Config", Imfos_Config);
   _conf_edd_device = E_CONFIG_DD_NEW("Device", Imfos_Device);
#undef T
#undef D
#define T Imfos_Device
#define D _conf_edd_device
   E_CONFIG_VAL(D, T, enabled, INT);
   E_CONFIG_VAL(D, T, name, STR);
   E_CONFIG_VAL(D, T, dev_name, STR);
   E_CONFIG_VAL(D, T, dev_name_locked, INT);
   E_CONFIG_VAL(D, T, capacities, INT);
   E_CONFIG_VAL(D, T, powersave_min_state, INT);
   E_CONFIG_VAL(D, T, priority, INT);
   E_CONFIG_VAL(D, T, async, INT);
   E_CONFIG_VAL(D, T, timeout, INT);
   E_CONFIG_VAL(D, T, auto_screen_on, INT);
   E_CONFIG_VAL(D, T, auto_screen_off, INT);
   E_CONFIG_VAL(D, T, auto_logout, INT);
   E_CONFIG_VAL(D, T, auto_login, INT);
   E_CONFIG_VAL(D, T, catch_time_average, INT);
   E_CONFIG_VAL(D, T, catch_count, INT);
   E_CONFIG_VAL(D, T, param.v4l.flip, INT);
   E_CONFIG_VAL(D, T, param.v4l.init_time_average, INT);
   E_CONFIG_VAL(D, T, param.wifi.ssid, STR);
   E_CONFIG_VAL(D, T, param.bluetooth.id, STR);
#undef T
#undef D
#define T Imfos_Config
#define D _conf_edd
   E_CONFIG_VAL(D, T, config_version, UINT);
   E_CONFIG_LIST(D, T, devices, _conf_edd_device);
   imfos_config =  e_config_domain_load("module.imfos", _conf_edd);
   if ((!imfos_config)
       || (imfos_config->config_version != IMFOS_CONFIG_VERSION))
     E_FREE(imfos_config);
   if (!imfos_config)
     {
        imfos_config = calloc(1, sizeof(Imfos_Config));
        imfos_config->config_version = IMFOS_CONFIG_VERSION;
     }
     /*
   E_CONFIG_LIMIT(imfos_config->poll_interval, 1, 3600);
   E_CONFIG_LIMIT(imfos_config->poll_interval_max, 1, 3600);
   E_CONFIG_LIMIT(imfos_config->poll_interval_powersave, 1, 3600);
   */
   imfos_devices_init();
   imfos_v4l_init();


   e_configure_registry_category_add("advanced", 80, _("Advanced"), NULL,
                                     "preferences-advanced");
   e_configure_registry_item_add("advanced/imfos", 20, "Imfos", NULL,
                                 "preference-imfos", e_mod_config_imfos);
   e_config_domain_save("module.imfos", _conf_edd, imfos_config);

//   _imfos_timer = ecore_timer_add(50.0, _run, NULL);

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   e_configure_registry_item_del("advanced/imfos");
   e_configure_registry_category_del("advanced");

   ecore_timer_del(_imfos_timer);

   E_CONFIG_DD_FREE(_conf_edd);
   E_CONFIG_DD_FREE(_conf_edd_device);
   free(imfos_config);
   imfos_v4l_shutdown();
   imfos_devices_shutdown();

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.imfos", _conf_edd, imfos_config);
   return 1;
}

