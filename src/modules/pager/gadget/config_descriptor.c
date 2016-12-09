#include <e.h>
#include "config_descriptor.h"

static E_Config_DD *conf_edd = NULL;

void
config_descriptor_init(void)
{
   conf_edd = E_CONFIG_DD_NEW("Pager_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_VAL(D, T, popup, UINT);
   E_CONFIG_VAL(D, T, popup_speed, DOUBLE);
   E_CONFIG_VAL(D, T, popup_urgent, UINT);
   E_CONFIG_VAL(D, T, popup_urgent_stick, UINT);
   E_CONFIG_VAL(D, T, popup_urgent_speed, DOUBLE);
   E_CONFIG_VAL(D, T, show_desk_names, UINT);
   E_CONFIG_VAL(D, T, popup_height, INT);
   E_CONFIG_VAL(D, T, popup_act_height, INT);
   E_CONFIG_VAL(D, T, drag_resist, UINT);
   E_CONFIG_VAL(D, T, btn_drag, UCHAR);
   E_CONFIG_VAL(D, T, btn_noplace, UCHAR);
   E_CONFIG_VAL(D, T, btn_desk, UCHAR);
   E_CONFIG_VAL(D, T, flip_desk, UCHAR);
}

void
config_descriptor_shutdown(void)
{
   E_CONFIG_DD_FREE(conf_edd);
}

E_Config_DD *
config_descriptor_get(void)
{
   return conf_edd;
}


