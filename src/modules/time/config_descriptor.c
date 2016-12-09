#include <e.h>
#include "config_descriptor.h"

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;


void
config_descriptor_init(void)
{
   conf_item_edd = E_CONFIG_DD_NEW("Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, weekend.start, INT);
   E_CONFIG_VAL(D, T, weekend.len, INT);
   E_CONFIG_VAL(D, T, week.start, INT);
   E_CONFIG_VAL(D, T, digital_clock, UCHAR);
   E_CONFIG_VAL(D, T, digital_24h, UCHAR);
   E_CONFIG_VAL(D, T, show_seconds, UCHAR);
   E_CONFIG_VAL(D, T, show_date, UCHAR);
   E_CONFIG_VAL(D, T, advanced, UCHAR);
   E_CONFIG_VAL(D, T, timezone, STR);
   E_CONFIG_VAL(D, T, time_str[0], STR);
   E_CONFIG_VAL(D, T, time_str[1], STR);
   E_CONFIG_VAL(D, T, colorclass[0], STR);
   E_CONFIG_VAL(D, T, colorclass[1], STR);

   conf_edd = E_CONFIG_DD_NEW("Config", Config);
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


