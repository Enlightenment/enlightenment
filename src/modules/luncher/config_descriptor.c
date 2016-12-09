#include <e.h>
#include "config_descriptor.h"

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;

void
config_descriptor_init(void)
{
conf_item_edd = E_CONFIG_DD_NEW("Luncher_Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, style, STR);
   E_CONFIG_VAL(D, T, dir, STR);

   conf_edd = E_CONFIG_DD_NEW("Luncher_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);
}

void
config_descriptor_shutdown(void)
{
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);
}

E_Config_DD *
config_descriptor_get(void)
{
   return conf_edd;
}
