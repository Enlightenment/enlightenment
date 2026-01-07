#include "e_mod_main.h"

static char *_strip_whitespace(char *str);

static char *
_sanitize_ln(char *text, const unsigned int n)
{
  char *ret = malloc(n + 1);
  char *temp = ret;
  unsigned char chr;
  unsigned int i = 0;

  EINA_SAFETY_ON_NULL_RETURN_VAL(text, NULL);
  if (!ret) return NULL;

  text = _strip_whitespace(text);
  for (;;)
    {
      chr = *text;
      if (!chr) break; // end of string
      if (chr < ' ') // some kind of ascii whitespace/controls
        { // is it a tab
          if (chr == '\t')
            { // default tab
              for (; i + 4; i++)
                {
                  if (i == n) break;
                  *temp++ = ' ';
                }
              text++;
            }
          else text++;
        }
      else
        { // assume char is ok and add to temp buffer
          *temp++ = *text++;
          i++;
        }
      if (i == n) break;
    }
  *temp = 0;
  return ret;
}

/**
 * @brief Strips whitespace from a string.
 *
 * @param str char pointer to a string.
 *
 * @return a char pointer to a substring of the original string..
 *
 * If the given string was allocated dynamically, the caller must not overwrite
 *  that pointer with the returned value. The original pointer must be
 *  deallocated using the same allocator with which it was allocated.  The return
 *  value must NOT be deallocated using free etc.
 *
 * You have been warned!!
 */
char *
_strip_whitespace(char *str)
{
  char *end;

  while (isspace(str[0])) str++;
  if (str[0] == 0) return str; // empty string ?
  end = str + strlen(str) - 1;
  while ((end > str) && isspace(end[0])) end--;
  (end + 1)[0] = 0; // write new null terminator
  return str;
}

Eina_Bool
set_clip_name(char **name, const char *text, int max)
{ // this is dodgey.. do a proper processing into tb mrkup for genlist
  Eina_Bool ret = EINA_TRUE;
  char *text2;

  if (!text) // sanity check
    {
      WRN("ERROR: Text is NULL\n");
      text = "";
    }
  text2 = strdup(text);
  if (!text2) return EINA_FALSE;
  // to be continued later
  if (name) *name = _sanitize_ln(text2, max);
  else
    {
      free(text2);
      ERR("Error: Clip name pointer is Null!!");
      return EINA_FALSE;
    }
  free(text2);
  if (!*name)
    { // this is bad, leave it to calling function
      CRI("ERROR: Memory allocation Failed!!");
      ret = EINA_FALSE;
    }
  return ret;
}

Eina_Bool
is_empty(const char *str)
{
  EINA_SAFETY_ON_NULL_RETURN_VAL(str, EINA_TRUE);
  while ((isspace((unsigned char)*str)) && (*str++));
  return !*str;
}
