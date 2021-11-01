#ifndef _MAIN_H
#define _MAIN_H

#include <Elementary.h>

void         palimg_update(Evas_Object *img, Elm_Palette *pal);
Evas_Object *palimg_add(Evas_Object *win);

Evas_Object *palsel_add(Evas_Object *win);

void         palcols_fill(Evas_Object *win);
Evas_Object *palcols_add(Evas_Object *win);

void         colsel_update(Evas_Object *win);
Evas_Object *colsel_add(Evas_Object *win);

void         pal_load(Evas_Object *win);
void         pal_save(Evas_Object *win);

void
undoredo_op_col_add(Evas_Object *win,
                    const char *col,
                    int r, int g, int b, int a);
void
undoredo_op_col_del(Evas_Object *win,
                    const char *col,
                    int r, int g, int b, int a);
void
undoredo_op_col_change(Evas_Object *win,
                       const char *col,
                       int r_from, int g_from, int b_from, int a_from,
                       int r_to, int g_to, int b_to, int a_to);
void
undoredo_reset(Evas_Object *win);
void
underedo_undo(Evas_Object *win);
void
underedo_redo(Evas_Object *win);

#endif
