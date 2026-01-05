#ifndef CLIPBOARD_UTILITY_H
#define CLIPBOARD_UTILITY_H

#include "e_mod_main.h"

Eina_Bool set_clip_content(char **content, char *text, int mode);
Eina_Bool set_clip_name(char **name, char *text, int mode, int n);
Eina_Bool is_empty(const char *str);

#endif
