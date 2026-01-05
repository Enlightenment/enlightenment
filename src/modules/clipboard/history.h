#ifndef CLIPBOARD_HISTORY_H
#define CLIPBOARD_HISTORY_H

#include "e_mod_main.h"

Eet_Error read_history(Eina_List **items, unsigned int ignore_ws, unsigned int label_length);
Eet_Error save_history(Eina_List *items);

#endif
