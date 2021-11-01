#include "main.h"

typedef enum
{
   ENTRY_NONE,
   ENTRY_CHANGE,
   ENTRY_ADD,
   ENTRY_DEL
} Entry_Type;

typedef struct
{
   double tim;
   const char *name;
   Entry_Type type;
   unsigned char r, g, b, a;
   unsigned char r2, g2, b2, a2;
} Entry;

static Eina_List *undo = NULL;
static Eina_List *list = NULL;
static int ignore = 0;

/*
static void
_dump(void)
{
   Entry *en;
   Eina_List *l;

   printf("--------------------------------------------------\n");
   EINA_LIST_FOREACH(list, l, en)
     {
        if (l == undo) printf("* ");
        else printf("  ");
        switch (en->type)
          {
           case ENTRY_NONE:
             printf("UNDO: none\n");
             break;
           case ENTRY_CHANGE:
             printf("UNDO: change %5.2f [%s] %3i %3i %3i %3i -> %3i %3i %3i %3i\n",
                    en->tim,
                    en->name,
                    en->r, en->g, en->b, en->a,
                    en->r2, en->g2, en->b2, en->a2
                   );
             break;
           case ENTRY_ADD:
             printf("UNDO: add    %5.2f [%s] %3i %3i %3i %3i\n",
                    en->tim,
                    en->name,
                    en->r, en->g, en->b, en->a
                   );
             break;
           case ENTRY_DEL:
             printf("UNDO: del    %5.2f [%s] %3i %3i %3i %3i\n",
                    en->tim,
                    en->name,
                    en->r, en->g, en->b, en->a
                   );
             break;
           default:
             break;
          }
     }
}
*/

static void
_undo_init(void)
{
   Entry *en;

   if (list) return;
   en = calloc(1, sizeof(Entry));
   if (!en) return;
   en->name = eina_stringshare_add("");
   en->tim = 0.0;
   en->type = ENTRY_NONE;
   list = eina_list_append(list, en);
   undo = list;
}

static void
_undo_remove(void)
{
   Entry *en;
   Eina_List *l, *ll;

   if (!undo) return;
   EINA_LIST_REVERSE_FOREACH_SAFE(list, l, ll, en)
     {
        if (l == undo) break;
        if (en->type == ENTRY_NONE) break;
        eina_stringshare_del(en->name);
        free(en);
        list = eina_list_remove_list(list, l);
     }
   undo = NULL;
}

static void
_undo_apply(Evas_Object *win, Entry *en)
{
   Elm_Palette *pal = evas_object_data_get(win, "pal");

   ignore++;
   switch (en->type)
     {
      case ENTRY_CHANGE:
        elm_config_palette_color_set(pal, en->name,
                                     en->r2, en->g2, en->b2, en->a2);
        palcols_fill(win);
        // XXX: update colorsel
        break;
      case ENTRY_ADD:
        elm_config_palette_color_unset(pal, en->name);
        palcols_fill(win);
        break;
      case ENTRY_DEL:
        elm_config_palette_color_set(pal, en->name,
                                     en->r, en->g, en->b, en->a);
        palcols_fill(win);
        break;
      default:
        break;
     }
   ignore--;
}

static void
_redo_apply(Evas_Object *win, Entry *en)
{
   Elm_Palette *pal = evas_object_data_get(win, "pal");

   ignore++;
   switch (en->type)
     {
      case ENTRY_CHANGE:
        elm_config_palette_color_set(pal, en->name,
                                     en->r2, en->g2, en->b2, en->a2);
        palcols_fill(win);
        // XXX: update colorsel
        break;
      case ENTRY_ADD:
        elm_config_palette_color_set(pal, en->name,
                                     en->r, en->g, en->b, en->a);
        palcols_fill(win);
        break;
      case ENTRY_DEL:
        elm_config_palette_color_unset(pal, en->name);
        palcols_fill(win);
        break;
      default:
        break;
     }
   ignore--;
}

void
undoredo_op_col_add(Evas_Object *win EINA_UNUSED,
                    const char *col,
                    int r, int g, int b, int a)
{
   Entry *en;
   double t;

   if (ignore) return;
   if (!col) return;
   t = ecore_time_get();
   _undo_remove();
   en = calloc(1, sizeof(Entry));
   if (!en) return;
   en->name = eina_stringshare_add(col);
   en->tim = t;
   en->type = ENTRY_ADD;
   en->r = r;
   en->g = g;
   en->b = b;
   en->a = a;
   _undo_init();
   list = eina_list_append(list, en);
   undo = NULL;
//   _dump();
}

void
undoredo_op_col_del(Evas_Object *win EINA_UNUSED,
                    const char *col,
                    int r, int g, int b, int a)
{
   Entry *en;
   double t;

   if (ignore) return;
   if (!col) return;
   t = ecore_time_get();
   _undo_remove();
   en = calloc(1, sizeof(Entry));
   if (!en) return;
   en->name = eina_stringshare_add(col);
   en->tim = t;
   en->type = ENTRY_DEL;
   en->r = r;
   en->g = g;
   en->b = b;
   en->a = a;
   _undo_init();
   list = eina_list_append(list, en);
   undo = NULL;
//   _dump();
}

void
undoredo_op_col_change(Evas_Object *win EINA_UNUSED,
                       const char *col,
                       int r_from, int g_from, int b_from, int a_from,
                       int r_to, int g_to, int b_to, int a_to)
{
   Entry *en, *en_last = NULL;
   Eina_List *l;
   double t;

   if (ignore) return;
   if (!col) return;
   if ((r_from == r_to) && (g_from == g_to) &&
       (b_from == b_to) && (a_from == a_to)) return;
   t = ecore_time_get();
   _undo_remove();
   l = eina_list_last(list);
   if (l)
     {
        en = l->data;
        if ((en->type == ENTRY_CHANGE) &&
            (!strcmp(col, en->name)) &&
            ((t - en->tim) < 0.5))
          {
             en->tim = t;
             en->r2 = r_to;
             en->g2 = g_to;
             en->b2 = b_to;
             en->a2 = a_to;
//             _dump();
             return;
          }
     }
   EINA_LIST_REVERSE_FOREACH(list, l, en)
     {
        if (en->type == ENTRY_CHANGE)
          {
             if (!strcmp(en->name, col)) en_last = en;
          }
        else break;
     }
   _undo_init();
   if (!en_last)
     {
        en = calloc(1, sizeof(Entry));
        if (!en) return;
        en->name = eina_stringshare_add(col);
        en->tim = t - 1.0;
        en->type = ENTRY_CHANGE;
        en->r = r_from;
        en->g = g_from;
        en->b = b_from;
        en->a = a_from;
        en->r2 = r_from;
        en->g2 = g_from;
        en->b2 = b_from;
        en->a2 = a_from;
        list = eina_list_append(list, en);
     }
   en = calloc(1, sizeof(Entry));
   if (!en) return;
   en->name = eina_stringshare_add(col);
   en->tim = t;
   en->type = ENTRY_CHANGE;
   en->r = r_from;
   en->g = g_from;
   en->b = b_from;
   en->a = a_from;
   en->r2 = r_to;
   en->g2 = g_to;
   en->b2 = b_to;
   en->a2 = a_to;
   _undo_init();
   list = eina_list_append(list, en);
   undo = NULL;
//   _dump();
}

void
undoredo_reset(Evas_Object *win EINA_UNUSED)
{
   Entry *en;

   EINA_LIST_FREE(list, en)
     {
        eina_stringshare_del(en->name);
        free(en);
     }
   undo = NULL;
}

void
underedo_undo(Evas_Object *win)
{
   Entry *en;

   if (undo)
     {
        en = undo->data;
        if (en->type == ENTRY_NONE) return;
        undo = undo->prev;
     }
   else undo = eina_list_last(list);
   if (!undo) return;

   en = undo->data;
   if (en->type == ENTRY_NONE) return;
   _undo_apply(win, en);
//   _dump();
}

void
underedo_redo(Evas_Object *win)
{
   Entry *en;

   if (!undo) return;
   undo = undo->next;
   if (!undo) return;
   en = undo->data;
   _redo_apply(win, en);
//   _dump();
}
