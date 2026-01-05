#include "utility.h"

#define TRIM_SPACES   0
#define TRIM_NEWLINES 1

static char *_strip_whitespace (char *str, int mode);
static int   _is_newline       (const int c);

static char *
_sanitize_ln(char *text, const unsigned int n, const int mode)
{
  char *ret = malloc(n + 1);
  char *temp = ret;
  unsigned int chr, i = 0;

  EINA_SAFETY_ON_NULL_RETURN_VAL(text, NULL);
  if (!ret) return NULL;

  if (mode) text = _strip_whitespace(text, TRIM_SPACES);
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
_strip_whitespace(char *str, int mode)
{
  char *end;
  int (*compare)(int);

  if (mode == TRIM_SPACES) compare = isspace;
  else compare = _is_newline;
  while ((*compare)(*str)) str++;
  if (*str == 0) return str; // empty string ?
  end = str + strlen(str) - 1;
  while ((end > str) && (*compare)(*end)) end--;
  *(end + 1) = 0; // write new null terminator
  return str;
}

static int
_is_newline(const int c)
{
  return ((c == '\n') || (c == '\r'));
}

Eina_Bool
set_clip_content(char **content, char *text, int mode)
{
  Eina_Bool ret = EINA_TRUE;
  char *temp, *trim;

  if (!text) // sanity check
    {
      WRN("ERROR: Text is NULL\n");
      text = "";
    }
  if (content)
    {
      switch (mode)
        {
         case 0: // don't trim
          temp = strdup(text);
          break;
         case 1: // trim new lines
          trim = _strip_whitespace(text, TRIM_NEWLINES);
          temp = strdup(trim);
          break;
         case 2: // trim all whitespace since white space includes new lines drop thru here
          EINA_FALLTHROUGH;
         case 3: // trim white space and new lines
          trim = _strip_whitespace(text, TRIM_SPACES);
          temp = strdup(trim);
          break;
         default: // error don't trim
          WRN("ERROR: Invalid strip_mode %d\n", mode);
          temp = strdup(text);
          break;
        }
      if (!temp)
        { // this is bad, leave it to calling function
          CRI("ERROR: Memory allocation Failed!!");
          ret = EINA_FALSE;
        }
      *content = temp;
    }
  else ERR("Error: Clip content pointer is Null!!");
  return ret;
}

Eina_Bool
set_clip_name(char **name, char *text, int mode, int n)
{
  Eina_Bool ret = EINA_TRUE;

  if (!text) // sanity check
    {
      WRN("ERROR: Text is NULL\n");
      text = "";
    }
  // to be continued later
  if (name) *name = _sanitize_ln(text, n, mode);
  else
    {
      ERR("Error: Clip name pointer is Null!!");
      return EINA_FALSE;
    }
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
