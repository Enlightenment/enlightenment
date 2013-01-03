#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H

#include <Elementary.h>

#define WEIGHT evas_object_size_hint_weight_set
#define ALIGN evas_object_size_hint_align_set
#define EXPAND(X) WEIGHT((X), EVAS_HINT_EXPAND, EVAS_HINT_EXPAND)
#define FILL(X) ALIGN((X), EVAS_HINT_FILL, EVAS_HINT_FILL)

/**
 * @addtogroup Optional_Conf
 * @{
 *
 * @defgroup Module_Conf2 Improved Configuration Dialog
 *
 * Show the main configuration dialog used to access other
 * configuration.
 *
 * @}
 */

typedef struct Config
{
} Config;

EINTERN void e_conf2_show(E_Container *con, const char *params);
EINTERN void e_conf2_hide(void);

#endif
