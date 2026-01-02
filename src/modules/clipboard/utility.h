#ifndef CLIPBOARD_UTILITY_H
#define CLIPBOARD_UTILITY_H

#include <string.h>
#include <common.h>

Eina_Bool set_clip_content(char **content, char* text, int mode);
Eina_Bool set_clip_name(char **name, char * text, int mode, int n);
Eina_Bool is_empty(const char *str);

#endif
